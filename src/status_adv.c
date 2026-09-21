/*
 * CLine46 ステータスブロードキャスト。
 *
 * 右手（Central）が自分の状態を BLE の非接続広告に載せて流す。受信側
 * （M5Stack など）はスキャンするだけでよく、接続もペアリングも要らないので、
 * BLE プロファイル（5個）も BT_MAX_CONN も消費しない。
 *
 * ZMK 自身の広告（プロファイル用／Studio の directed advertising）と衝突
 * させないため、拡張広告のセットを 1 つ別に確保して、そこにレガシーの
 * 非接続 PDU を流している（CONFIG_BT_EXT_ADV）。ZMK 側の広告は
 * bt_le_adv_start() の従来 API のままセット 0 を使う。
 *
 * ペイロードの中身は include/cline46/status_adv.h、バイト配置の説明は
 * docs/status-advertisement.md を参照。
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <stdint.h>
#include <string.h>

#include <zmk/activity.h>
#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/keymap.h>
#if IS_ENABLED(CONFIG_ZMK_USB)
#include <zmk/usb.h>
#endif

#include <zmk/events/activity_state_changed.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/layer_state_changed.h>

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#include <zmk/split/central.h>
#include <zmk/split/transport/central.h>
#endif

#if IS_ENABLED(CONFIG_ZMK_STUDIO)
#include <zmk/studio/core.h>
#endif

#if IS_ENABLED(CONFIG_ZMK_OS_DETECTION)
#include <cormoran/os-detection/os_detection.h>
#endif

#if IS_ENABLED(CONFIG_ZMK_DEFAULT_LAYER)
#include <cormoran/default-layer/default_layer.h>
#endif

#if IS_ENABLED(CONFIG_ZMK_WATCHDOG)
#include <cormoran/zmk/watchdog.h>
#endif

#if IS_ENABLED(CONFIG_CLINE46_STATUS_PERIPHERAL_VOLTAGE)
#include <cline46/peripheral_voltage.h>
#endif

#include <cline46/battery_mv.h>
#include <cline46/status_adv.h>

LOG_MODULE_REGISTER(cline46_status_adv, CONFIG_ZMK_LOG_LEVEL);

/* 広告間隔の単位は 0.625ms */
#define MS_TO_ADV_INTERVAL(ms) ((uint16_t)((ms) * 8 / 5))

/* イベントで即時更新するときの合流待ち。レイヤーのホールドなどで
 * イベントが連続しても、広告の書き換えは 1 回にまとめる */
#define REFRESH_DEBOUNCE_MS 50

static struct bt_le_ext_adv *adv_set;
static struct k_work_delayable adv_work;
static uint32_t current_interval_ms;

static struct cline46_status_adv_payload payload;

/* イベントでしか取れない値はここに持っておく */
static uint8_t peripheral_pct = CLINE46_STATUS_PCT_UNKNOWN;
static uint8_t reset_reason = CLINE46_STATUS_RESET_UNKNOWN;
static uint8_t keyboard_id;

static const struct bt_data adv_data[] = {
    BT_DATA(BT_DATA_MANUFACTURER_DATA, (const uint8_t *)&payload, sizeof(payload)),
};

static uint8_t map_reset_reason(uint32_t cause) {
    /* 複数ビットが同時に立つことがあるので、原因として知りたい順に見る */
    if (cause & RESET_WATCHDOG) {
        return CLINE46_STATUS_RESET_WATCHDOG;
    }
    if (cause & RESET_BROWNOUT) {
        return CLINE46_STATUS_RESET_BROWNOUT;
    }
    if (cause & RESET_SOFTWARE) {
        return CLINE46_STATUS_RESET_SOFTWARE;
    }
    if (cause & RESET_PIN) {
        return CLINE46_STATUS_RESET_PIN;
    }
    if (cause & RESET_LOW_POWER_WAKE) {
        return CLINE46_STATUS_RESET_LOW_POWER_WAKE;
    }
    if (cause & RESET_DEBUG) {
        return CLINE46_STATUS_RESET_DEBUG;
    }
    if (cause & RESET_POR) {
        return CLINE46_STATUS_RESET_POWER_ON;
    }
    return cause ? CLINE46_STATUS_RESET_OTHER : CLINE46_STATUS_RESET_UNKNOWN;
}

/*
 * 左手と繋がっているか。
 *
 * ZMK の zmk_split_peripheral_status_changed は Peripheral 側でしか上がらない
 * （app/src/split/bluetooth/peripheral.c）ので、Central では split transport に
 * 繋がっている source の数を数えて判断する。get_available_source_ids() は
 * PERIPHERAL_SLOT_STATE_CONNECTED のものだけを返す。
 */
static bool split_peripheral_connected(void) {
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) && ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT > 0
    uint8_t sources[ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT];

    STRUCT_SECTION_FOREACH(zmk_split_transport_central, transport) {
        if (transport->api == NULL || transport->api->get_available_source_ids == NULL) {
            continue;
        }
        if (transport->api->get_available_source_ids(sources) > 0) {
            return true;
        }
    }
#endif
    return false;
}

#if IS_ENABLED(CONFIG_ZMK_WATCHDOG)
/*
 * hwinfo のリセット原因は当てにならないことがある。nRF52 では電源投入で
 * RESETREAS が 0 のままだし、UF2 ブートローダが消してしまう場合もある。
 * そのときは watchdog に残っている記録のうち一番新しいものを手掛かりにする。
 * 記録は RAM 上の配列に載っているので、毎回読んでも負荷にならない。
 */
static uint8_t reason_from_watchdog(void) {
    struct zmk_watchdog_incident_record newest = {0};
    uint16_t count = zmk_watchdog_store_count();
    bool found = false;

    for (uint16_t i = 0; i < count; i++) {
        struct zmk_watchdog_incident_record rec;
        if (zmk_watchdog_store_get(i, &rec) < 0) {
            continue;
        }
        /* boot_ordinal は記録のたびに増える。同じ起動内は uptime で比べる */
        if (!found || rec.boot_ordinal > newest.boot_ordinal ||
            (rec.boot_ordinal == newest.boot_ordinal && rec.uptime_s > newest.uptime_s)) {
            newest = rec;
            found = true;
        }
    }

    if (!found) {
        return CLINE46_STATUS_RESET_UNKNOWN;
    }

    switch (newest.type) {
    case ZMK_WATCHDOG_INCIDENT_FREEZE:
        return CLINE46_STATUS_RESET_FREEZE;
    case ZMK_WATCHDOG_INCIDENT_FATAL:
        return CLINE46_STATUS_RESET_FAULT;
    case ZMK_WATCHDOG_INCIDENT_RESET_CAUSE:
        return map_reset_reason(newest.detail.reset.cause_bits);
    default:
        return CLINE46_STATUS_RESET_UNKNOWN;
    }
}
#endif

static uint8_t current_reset_reason(void) {
#if IS_ENABLED(CONFIG_ZMK_WATCHDOG)
    if (reset_reason == CLINE46_STATUS_RESET_UNKNOWN) {
        return reason_from_watchdog();
    }
#endif
    return reset_reason;
}

static uint8_t incident_count(void) {
#if IS_ENABLED(CONFIG_ZMK_WATCHDOG)
    uint16_t count = zmk_watchdog_store_count();
    return count > UINT8_MAX ? UINT8_MAX : (uint8_t)count;
#else
    return 0;
#endif
}

static uint8_t current_os(void) {
#if IS_ENABLED(CONFIG_ZMK_OS_DETECTION)
    return (uint8_t)zmk_os_detection_current() & 0x0F;
#else
    return CLINE46_STATUS_OS_UNKNOWN;
#endif
}

static uint8_t current_default_layer(void) {
#if IS_ENABLED(CONFIG_ZMK_DEFAULT_LAYER)
    int32_t layer = zmk_default_layer_resolve_current();
    if (layer < 0 || layer >= CLINE46_STATUS_DEFAULT_LAYER_NONE) {
        return CLINE46_STATUS_DEFAULT_LAYER_NONE;
    }
    return (uint8_t)layer;
#else
    return CLINE46_STATUS_DEFAULT_LAYER_NONE;
#endif
}

static uint8_t current_flags(bool peripheral_connected) {
    uint8_t flags = 0;

#if IS_ENABLED(CONFIG_ZMK_USB)
    if (zmk_usb_is_powered()) {
        flags |= CLINE46_STATUS_FLAG_USB_POWERED;
    }
    if (zmk_usb_is_hid_ready()) {
        flags |= CLINE46_STATUS_FLAG_USB_HID_READY;
    }
#endif
    if (zmk_endpoint_get_selected().transport == ZMK_TRANSPORT_BLE) {
        flags |= CLINE46_STATUS_FLAG_OUTPUT_BLE;
    }
#if IS_ENABLED(CONFIG_ZMK_STUDIO)
    if (zmk_studio_core_get_lock_state() == ZMK_STUDIO_CORE_LOCK_STATE_UNLOCKED) {
        flags |= CLINE46_STATUS_FLAG_STUDIO_UNLOCKED;
    }
#endif
    if (peripheral_connected) {
        flags |= CLINE46_STATUS_FLAG_SPLIT_CONNECTED;
    }
    if (zmk_activity_get_state() == ZMK_ACTIVITY_IDLE) {
        flags |= CLINE46_STATUS_FLAG_IDLE;
    }

    return flags;
}

static void fill_layer(void) {
    zmk_keymap_layer_index_t index = zmk_keymap_highest_layer_active();
    zmk_keymap_layer_id_t id = zmk_keymap_layer_index_to_id(index);
    const char *name = zmk_keymap_layer_name(id);

    payload.layer_index = (uint8_t)index;

    memset(payload.layer_name, 0, sizeof(payload.layer_name));
    if (name != NULL) {
        /* 4文字ちょうどなら終端は入らない。受信側は長さ4として読む */
        strncpy(payload.layer_name, name, sizeof(payload.layer_name));
    }
}

static void build_payload(void) {
    payload.company_id = CLINE46_STATUS_ADV_COMPANY_ID;
    payload.magic[0] = CLINE46_STATUS_ADV_MAGIC_0;
    payload.magic[1] = CLINE46_STATUS_ADV_MAGIC_1;
    payload.version = CLINE46_STATUS_ADV_VERSION;
    payload.keyboard_id = keyboard_id;

    fill_layer();

    payload.central_mv = cline46_battery_mv();
    payload.central_pct = zmk_battery_state_of_charge();

    /* 左手が切れているときに古い値を出し続けないよう、まとめて不明にする */
    bool peripheral_connected = split_peripheral_connected();
#if IS_ENABLED(CONFIG_CLINE46_STATUS_PERIPHERAL_VOLTAGE)
    payload.peripheral_mv =
        peripheral_connected ? cline46_peripheral_voltage_mv() : CLINE46_STATUS_MV_UNKNOWN;
#else
    payload.peripheral_mv = CLINE46_STATUS_MV_UNKNOWN;
#endif
    payload.peripheral_pct = peripheral_connected ? peripheral_pct : CLINE46_STATUS_PCT_UNKNOWN;

    payload.os_default_layer = (current_os() << 4) | current_default_layer();

    int profile = zmk_ble_active_profile_index();
    payload.profile = (uint8_t)(profile < 0 ? 0 : profile) & CLINE46_STATUS_PROFILE_INDEX_MASK;
    if (zmk_ble_active_profile_is_connected()) {
        payload.profile |= CLINE46_STATUS_PROFILE_CONNECTED;
    }
    if (zmk_ble_active_profile_is_open()) {
        payload.profile |= CLINE46_STATUS_PROFILE_OPEN;
    }

    payload.flags = current_flags(peripheral_connected);

    int64_t minutes = k_uptime_get() / 60000;
    payload.uptime_min = minutes > UINT16_MAX ? UINT16_MAX : (uint16_t)minutes;

    payload.reset_reason = current_reset_reason();
    payload.incident_count = incident_count();
}

static void adv_stop(void) {
    if (adv_set == NULL) {
        return;
    }

    int err = bt_le_ext_adv_stop(adv_set);
    if (err < 0 && err != -EALREADY) {
        LOG_WRN("Failed to stop status advertising (%d)", err);
    }
    current_interval_ms = 0;
}

/* 広告セットが無ければ作り、間隔が変わっていれば作り直してから流し始める */
static int adv_ensure_started(uint32_t interval_ms) {
    struct bt_le_adv_param param = {
        .id = BT_ID_DEFAULT,
        .sid = 0,
        .secondary_max_skip = 0,
        .options = 0, /* 非接続・非スキャン応答のレガシー広告 */
        .interval_min = MS_TO_ADV_INTERVAL(interval_ms),
        .interval_max = MS_TO_ADV_INTERVAL(interval_ms) + 16,
        .peer = NULL,
    };
    int err;

    if (adv_set == NULL) {
        err = bt_le_ext_adv_create(&param, NULL, &adv_set);
        if (err < 0) {
            LOG_ERR("Failed to create status advertising set (%d)", err);
            adv_set = NULL;
            return err;
        }
        current_interval_ms = 0;
    }

    if (current_interval_ms == interval_ms) {
        return 0;
    }

    /* 間隔の変更は広告を止めてからでないと通らない */
    bt_le_ext_adv_stop(adv_set);

    err = bt_le_ext_adv_update_param(adv_set, &param);
    if (err < 0) {
        LOG_ERR("Failed to update status advertising param (%d)", err);
        return err;
    }

    err = bt_le_ext_adv_start(adv_set, BT_LE_EXT_ADV_START_DEFAULT);
    if (err < 0) {
        LOG_ERR("Failed to start status advertising (%d)", err);
        /* スリープ復帰直後などでセットが無効になっていることがあるので、
         * 作り直せるように捨てる */
        bt_le_ext_adv_delete(adv_set);
        adv_set = NULL;
        current_interval_ms = 0;
        return err;
    }

    current_interval_ms = interval_ms;
    LOG_DBG("Status advertising at %u ms", interval_ms);

    return 0;
}

static void adv_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    if (!bt_is_ready()) {
        k_work_schedule(&adv_work, K_SECONDS(1));
        return;
    }

    enum zmk_activity_state activity = zmk_activity_get_state();
    if (activity == ZMK_ACTIVITY_SLEEP) {
        /* ディープスリープ中は無線ごと止まる。起きたら
         * activity_state_changed で戻ってくる */
        adv_stop();
        return;
    }

    uint32_t interval_ms = (activity == ZMK_ACTIVITY_IDLE)
                               ? CONFIG_CLINE46_STATUS_ADV_IDLE_INTERVAL_MS
                               : CONFIG_CLINE46_STATUS_ADV_INTERVAL_MS;

    if (adv_ensure_started(interval_ms) < 0) {
        k_work_schedule(&adv_work, K_SECONDS(5));
        return;
    }

    build_payload();

    int err = bt_le_ext_adv_set_data(adv_set, adv_data, ARRAY_SIZE(adv_data), NULL, 0);
    if (err < 0) {
        LOG_WRN("Failed to set status advertising data (%d)", err);
    }

    k_work_schedule(&adv_work, K_MSEC(interval_ms));
}

static void refresh_soon(void) { k_work_reschedule(&adv_work, K_MSEC(REFRESH_DEBOUNCE_MS)); }

static int status_adv_listener(const zmk_event_t *eh) {
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    const struct zmk_peripheral_battery_state_changed *peripheral_battery =
        as_zmk_peripheral_battery_state_changed(eh);
    if (peripheral_battery != NULL) {
        /* 左手は 1 台だけ。それ以外の source は無視する */
        if (peripheral_battery->source == 0) {
            peripheral_pct = peripheral_battery->state_of_charge;
        }
        refresh_soon();
        return ZMK_EV_EVENT_BUBBLE;
    }

#endif

    refresh_soon();
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(cline46_status_adv, status_adv_listener);
ZMK_SUBSCRIPTION(cline46_status_adv, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(cline46_status_adv, zmk_battery_state_changed);
ZMK_SUBSCRIPTION(cline46_status_adv, zmk_ble_active_profile_changed);
ZMK_SUBSCRIPTION(cline46_status_adv, zmk_endpoint_changed);
ZMK_SUBSCRIPTION(cline46_status_adv, zmk_activity_state_changed);

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
ZMK_SUBSCRIPTION(cline46_status_adv, zmk_peripheral_battery_state_changed);
#endif

#if IS_ENABLED(CONFIG_ZMK_STUDIO)
ZMK_SUBSCRIPTION(cline46_status_adv, zmk_studio_core_lock_state_changed);
#endif

#if IS_ENABLED(CONFIG_ZMK_OS_DETECTION)
ZMK_SUBSCRIPTION(cline46_status_adv, zmk_os_changed);
#endif

#if IS_ENABLED(CONFIG_CLINE46_STATUS_PERIPHERAL_VOLTAGE)
ZMK_SUBSCRIPTION(cline46_status_adv, cline46_peripheral_voltage_changed);
#endif

static int cline46_status_adv_init(void) {
    uint32_t cause = 0;
    uint8_t device_id[8];

    if (hwinfo_get_reset_cause(&cause) == 0) {
        reset_reason = map_reset_reason(cause);
    }

    /* 同じ広告を出すキーボードが複数あっても区別できるようにしておく */
    if (hwinfo_get_device_id(device_id, sizeof(device_id)) > 0) {
        keyboard_id = device_id[0];
    }

    k_work_init_delayable(&adv_work, adv_work_handler);
    k_work_schedule(&adv_work, K_SECONDS(CONFIG_CLINE46_STATUS_ADV_START_DELAY_S));

    return 0;
}

/*
 * zmk-feature-watchdog が APPLICATION/CONFIG_APPLICATION_INIT_PRIORITY で
 * hwinfo_clear_reset_cause() を呼ぶ（src/watchdog_reset_cause.c）。同じ優先度だと
 * どちらが先かはリンク順次第で、後になるとリセット原因が読めない（「不明」になる）。
 * そのため既定より小さい優先度を使って必ず先に読む
 */
SYS_INIT(cline46_status_adv_init, APPLICATION, CONFIG_CLINE46_STATUS_ADV_INIT_PRIORITY);

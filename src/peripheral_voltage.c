/*
 * 左手（Peripheral）の電池電圧を右手（Central）へ中継する。
 *
 * 左手: ZMK が電池を測るたびに自分の電圧をイベントとして上げる。あとは
 *       ZMK_RELAY_EVENT_PERIPHERAL_TO_CENTRAL が split 接続に相乗りして
 *       右手へ運ぶ（既存の接続を使うので電波は増えない）。
 * 右手: ZMK_RELAY_EVENT_HANDLE が受け取って同じイベントとして再発火させる。
 *       それを拾って覚えておき、ステータス広告に載せる。
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#include <zmk/split/central.h>
#else
#include <zephyr/init.h>
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/split/peripheral.h>
#endif

#include <cline46/battery_mv.h>
#include <cline46/peripheral_voltage.h>

LOG_MODULE_REGISTER(cline46_peripheral_voltage, CONFIG_ZMK_LOG_LEVEL);

ZMK_EVENT_IMPL(cline46_peripheral_voltage_changed);

/* 中継の識別子。CONFIG_ZMK_SPLIT_RELAY_EVENT_TYPE_NAME_LEN が既定 4 バイトなので
 * 終端込みで 4 文字まで＝3 文字。watchdog が wdq / wdp を使っているので clv にする */

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

static uint16_t peripheral_mv;

uint16_t cline46_peripheral_voltage_mv(void) { return peripheral_mv; }

ZMK_RELAY_EVENT_HANDLE(cline46_peripheral_voltage_changed, clv, source);

/* 切断したときに古い値を出し続けない仕掛けは status_adv.c 側にある
 * （左手が切れているかは split transport を見て判断する。ZMK の
 * zmk_split_peripheral_status_changed は Peripheral 側でしか上がらない） */
static int central_listener(const zmk_event_t *eh) {
    const struct cline46_peripheral_voltage_changed *voltage =
        as_cline46_peripheral_voltage_changed(eh);
    if (voltage != NULL && voltage->source != ZMK_RELAY_EVENT_SOURCE_SELF) {
        peripheral_mv = voltage->mv;
        LOG_DBG("Peripheral %u voltage %u mV", voltage->source, voltage->mv);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(cline46_peripheral_voltage, central_listener);
ZMK_SUBSCRIPTION(cline46_peripheral_voltage, cline46_peripheral_voltage_changed);

#else /* Peripheral（左手） */

ZMK_RELAY_EVENT_PERIPHERAL_TO_CENTRAL(cline46_peripheral_voltage_changed, clv, source);

/* 接続してすぐは送信経路が整っていないことがあるので、少し待ってから送る */
#define RECONNECT_SEND_DELAY_MS 2000

static void send_voltage(void) {
    uint16_t mv = cline46_battery_mv();
    if (mv == 0) {
        /* まだ一度も測っていない（起動直後）。次の測定で送られる */
        return;
    }

    struct cline46_peripheral_voltage_changed voltage = {
        .source = ZMK_RELAY_EVENT_SOURCE_SELF,
        .mv = mv,
    };
    raise_cline46_peripheral_voltage_changed(voltage);
}

static void reconnect_work_handler(struct k_work *work) {
    ARG_UNUSED(work);
    send_voltage();
}

static K_WORK_DELAYABLE_DEFINE(reconnect_work, reconnect_work_handler);

static int peripheral_listener(const zmk_event_t *eh) {
    if (as_zmk_battery_state_changed(eh) != NULL) {
        /* 残量(%)の更新と同じタイミング = ちょうど測り終わったところ */
        send_voltage();
        return ZMK_EV_EVENT_BUBBLE;
    }

    const struct zmk_split_peripheral_status_changed *status =
        as_zmk_split_peripheral_status_changed(eh);
    if (status != NULL && status->connected) {
        /* 繋がった直後にも 1 回送る。これが無いと、右手が再起動したあと
         * 次の電池測定（既定60秒ごと）まで左手の電圧が「不明」のままになる */
        k_work_reschedule(&reconnect_work, K_MSEC(RECONNECT_SEND_DELAY_MS));
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(cline46_peripheral_voltage, peripheral_listener);
ZMK_SUBSCRIPTION(cline46_peripheral_voltage, zmk_battery_state_changed);
ZMK_SUBSCRIPTION(cline46_peripheral_voltage, zmk_split_peripheral_status_changed);

#endif /* CONFIG_ZMK_SPLIT_ROLE_CENTRAL */

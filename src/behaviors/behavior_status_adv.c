/*
 * ステータス広告のオン/オフを切り替えるビヘイビア（&status_adv）。
 *
 * 受信側（M5Stack）を使っていないときは広告を出す意味が無いので、
 * キーから止められるようにしてある。非接続の広告には「誰か聞いているか」を
 * 知る手段が無く、自動で止めることはできないため、明示的な切り替えにした。
 *
 * locality は既定の BEHAVIOR_LOCALITY_CENTRAL。広告を出しているのは
 * Central（右手）なので、左手のキーに割り当てても Central 側で実行される。
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT cline46_behavior_status_adv

#include <drivers/behavior.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <zmk/behavior.h>

#include <cline46/status_adv_control.h>
#include <dt-bindings/cline46/status_adv.h>

LOG_MODULE_DECLARE(cline46_status_adv, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    switch (binding->param1) {
    case CLINE46_STATUS_ADV_CMD_OFF:
        return cline46_status_adv_set_enabled(false);
    case CLINE46_STATUS_ADV_CMD_ON:
        return cline46_status_adv_set_enabled(true);
    case CLINE46_STATUS_ADV_CMD_TOGGLE:
        return cline46_status_adv_set_enabled(!cline46_status_adv_is_enabled());
    default:
        LOG_ERR("Unknown status_adv command: %d", binding->param1);
    }

    return -ENOTSUP;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

/* DYA Studio のキーマップエディタに出す選択肢 */
static const struct behavior_parameter_value_metadata std_values[] = {
    {
        .display_name = "Toggle Status Broadcast",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = CLINE46_STATUS_ADV_CMD_TOGGLE,
    },
    {
        .display_name = "Status Broadcast On",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = CLINE46_STATUS_ADV_CMD_ON,
    },
    {
        .display_name = "Status Broadcast Off",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = CLINE46_STATUS_ADV_CMD_OFF,
    },
};

static const struct behavior_parameter_metadata_set std_set = {
    .param1_values = std_values,
    .param1_values_len = ARRAY_SIZE(std_values),
};

static const struct behavior_parameter_metadata metadata = {
    .sets_len = 1,
    .sets = &std_set,
};

#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static const struct behavior_driver_api behavior_status_adv_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &metadata,
#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_status_adv_driver_api);

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */

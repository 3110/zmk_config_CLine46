/*
 * 左手（Peripheral）の電池電圧を右手（Central）へ運ぶイベント。
 *
 * ZMK の split が中継する電池情報は残量(%)だけで、電圧は含まれていない
 * （zmk_peripheral_battery_state_changed の中身は source と state_of_charge のみ）。
 * そこで cormoran fork の汎用イベント中継（CONFIG_ZMK_SPLIT_RELAY_EVENT）に
 * 相乗りして、左手が測った電圧をそのまま右手へ送る。
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <zmk/event_manager.h>

struct cline46_peripheral_voltage_changed {
    /* 左手が上げるときは ZMK_RELAY_EVENT_SOURCE_SELF。右手側で中継を受けて
     * 再発火するときは、中継元の番号 + 1（0 は Central）が入る */
    uint8_t source;
    uint16_t mv;
};

ZMK_EVENT_DECLARE(cline46_peripheral_voltage_changed);

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
/* 直近に中継されてきた左手の電圧。未受信・切断中は 0（不明） */
uint16_t cline46_peripheral_voltage_mv(void);
#endif

/*
 * 電池電圧の読み出し。左右どちらでも使う。
 *
 * ZMK の battery.c が CONFIG_ZMK_BATTERY_REPORT_INTERVAL_S（既定60秒）ごとに
 * sensor_sample_fetch() を済ませているので、ここは最後に測った値を読むだけに
 * している。広告や中継のために ADC を回さないので、電池を余計に食わない。
 * 一度も測定されていない起動直後は 0（不明）が返る。
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>

static inline uint16_t cline46_battery_mv(void) {
#if DT_HAS_CHOSEN(zmk_battery)
    static const struct device *const battery_dev = DEVICE_DT_GET(DT_CHOSEN(zmk_battery));
    struct sensor_value value;

    if (!device_is_ready(battery_dev)) {
        return 0;
    }

    /* 非LiPo モジュール（zmk,non-lipo-battery）は GAUGE_VOLTAGE で mV を返す */
    if (sensor_channel_get(battery_dev, SENSOR_CHAN_GAUGE_VOLTAGE, &value) < 0) {
        return 0;
    }

    int32_t mv = value.val1 * 1000 + value.val2 / 1000;
    if (mv <= 0 || mv > UINT16_MAX) {
        return 0;
    }

    return (uint16_t)mv;
#else
    return 0;
#endif
}

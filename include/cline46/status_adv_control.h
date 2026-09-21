/*
 * ステータス広告のオン/オフ。
 *
 * ペイロードの定義（include/cline46/status_adv.h）は受信側の Arduino
 * ライブラリにもそのままコピーして使うため、Zephyr に依存するものは
 * こちらに分けてある。
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>

/** 広告を出している最中なら true。 */
bool cline46_status_adv_is_enabled(void);

/**
 * 広告のオン/オフを切り替える。
 *
 * 実際の開始・停止は広告ワークに任せるので、どのスレッドから呼んでも
 * 競合しない。CONFIG_CLINE46_STATUS_ADV_PERSIST が有効なら、少し遅れて
 * 設定に保存され、次の起動でも引き継がれる。
 *
 * @retval 0 状態を変えた、または既にその状態だった
 */
int cline46_status_adv_set_enabled(bool enabled);

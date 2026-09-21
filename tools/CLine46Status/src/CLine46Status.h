/*
 * CLine46Status - CLine46 が BLE 広告で流す状態を受信するライブラリ。
 *
 * 接続もペアリングもしない（広告を聞くだけ）ので、キーボードと PC の接続には
 * 影響しない。表示は含めていないので、機種に合わせて自由に作れる。
 *
 * 使い方:
 *
 *   #include <CLine46Status.h>
 *   CLine46Status keyboard;
 *
 *   void setup() {
 *     Serial.begin(115200);
 *     keyboard.begin();
 *   }
 *
 *   void loop() {
 *     if (keyboard.poll()) {        // 新しい広告を取り込んだら true
 *       keyboard.printTo(Serial);   // 自前で描くなら各アクセサを使う
 *     }
 *   }
 *
 * 広告の中身は src/cline46/status_adv.h（ファーム側のヘッダのコピー）。
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "cline46/status_adv.h"

class CLine46Status {
  public:
    /* poll() から呼ばれるコールバック。BLE のタスクではなく loop() の文脈で
     * 呼ばれるので、中で画面描画などをしても安全 */
    typedef void (*Handler)(CLine46Status &status);

    /* 受信が途切れたと判断するまでの既定時間。キーボードは操作中1秒・
     * アイドル中10秒ごとに広告を出すので、アイドル中は別の（長い）値を使う。
     * 同じ15秒だと、広告を1回取りこぼしただけで見失った扱いになってしまう */
    static const uint32_t DEFAULT_TIMEOUT_MS = 15000;
    static const uint32_t DEFAULT_IDLE_TIMEOUT_MS = 35000;

    CLine46Status();

    /* NimBLE を初期化してスキャンを始める。二重に呼んでも害はない。
     * NimBLE を他でも使う場合は、先に NimBLEDevice::init() を済ませてから
     * 呼べばそちらの初期化が尊重される */
    bool begin();

    /* スキャンを止める。begin() でまた始められる */
    void end();

    /* loop() から呼ぶ。新しい広告を取り込んだら true。
     * 受信の途切れ（onLost）の判定もここで行う */
    bool poll();

    /* 受信ごと / 途切れたときに呼ばれるコールバックを登録する */
    void onUpdate(Handler handler) { update_handler_ = handler; }
    void onLost(Handler handler) { lost_handler_ = handler; }

    /* 受信が途切れたと判断するまでの時間を変える。
     * アイドル中（キーボードが広告を間引いている間）は idle 側が使われる */
    void setTimeout(uint32_t ms) { timeout_ms_ = ms; }
    void setIdleTimeout(uint32_t ms) { idle_timeout_ms_ = ms; }

    /* --- 状態 --- */

    /* 一度でも広告を受信したか */
    bool available() const { return available_; }
    /* 直近 setTimeout() 以内に受信しているか */
    bool alive() const;
    /* 最後に受信してからの経過ミリ秒 */
    uint32_t ageMs() const;
    /* 直近の電波強度 */
    int rssi() const { return rssi_; }

    /* 受信した生データ。自前で解釈したいとき用 */
    const cline46_status_adv_payload &raw() const { return current_; }

    /* --- キーボードの状態 --- */

    uint8_t layerIndex() const { return current_.layer_index; }
    /* レイヤー名（display-name の先頭4文字）。終端付きの文字列を返す */
    const char *layerName() const { return layer_name_; }

    uint16_t centralMv() const { return current_.central_mv; }
    uint8_t centralPercent() const { return current_.central_pct; }
    uint16_t peripheralMv() const { return current_.peripheral_mv; }
    uint8_t peripheralPercent() const { return current_.peripheral_pct; }
    /* 電池の値が有効か（起動直後や未接続では取れない） */
    static bool validMv(uint16_t mv) { return mv != CLINE46_STATUS_MV_UNKNOWN; }
    static bool validPercent(uint8_t pct) { return pct != CLINE46_STATUS_PCT_UNKNOWN; }

    /* OS 判別結果（enum cline46_status_os）と、その表示名 */
    uint8_t os() const { return current_.os_default_layer >> 4; }
    const char *osName() const;
    /* 現在の既定レイヤー。未設定なら -1 */
    int defaultLayer() const;

    uint8_t profileIndex() const { return current_.profile & CLINE46_STATUS_PROFILE_INDEX_MASK; }
    bool profileConnected() const { return current_.profile & CLINE46_STATUS_PROFILE_CONNECTED; }
    bool profileOpen() const { return current_.profile & CLINE46_STATUS_PROFILE_OPEN; }

    bool usbPowered() const { return flag(CLINE46_STATUS_FLAG_USB_POWERED); }
    bool usbHidReady() const { return flag(CLINE46_STATUS_FLAG_USB_HID_READY); }
    bool outputBle() const { return flag(CLINE46_STATUS_FLAG_OUTPUT_BLE); }
    bool studioUnlocked() const { return flag(CLINE46_STATUS_FLAG_STUDIO_UNLOCKED); }
    bool splitConnected() const { return flag(CLINE46_STATUS_FLAG_SPLIT_CONNECTED); }
    bool idle() const { return flag(CLINE46_STATUS_FLAG_IDLE); }

    uint16_t uptimeMinutes() const { return current_.uptime_min; }
    uint8_t resetReason() const { return current_.reset_reason; }
    const char *resetReasonName() const;
    /* watchdog に残っている記録の件数。0 なら異常記録なし */
    uint8_t incidentCount() const { return current_.incident_count; }
    /* 個体識別。同じ広告を出すキーボードが複数あるときの区別に使う */
    uint8_t keyboardId() const { return current_.keyboard_id; }

    /* --- 表示の助け --- */

    /* フラグを "UH-S-I" のような6文字にする（buffer は7バイト以上）。
     * 順に USB給電 / USB HID / 出力先BLE / Studio解除 / 左手接続 / アイドル */
    void flagsText(char *buffer, size_t size) const;
    /* "1284mV/72%" のような文字列にする。不明な値は "----mV" / "--%" */
    static void batteryText(uint16_t mv, uint8_t percent, char *buffer, size_t size);

    /* 1行にまとめて出力する。シリアルにも画面にも使える（Print を取る） */
    void printTo(Print &out) const;

    /* --- 形式の食い違い --- */

    /* キーボードのファームが、このライブラリと違う形式で流していたら true。
     * そのときは seenVersion() に相手の version が入る */
    bool versionMismatch() const { return mismatch_version_ != 0; }
    uint8_t seenVersion() const { return mismatch_version_; }

    /* ライブラリが期待する形式のバージョン */
    static uint8_t expectedVersion() { return CLINE46_STATUS_ADV_VERSION; }

    /* --- 内部用（BLE コールバックから呼ばれる） --- */
    void ingest(const uint8_t *data, size_t size, int rssi);

  private:
    bool flag(uint8_t bit) const { return (current_.flags & bit) != 0; }

    cline46_status_adv_payload current_;
    cline46_status_adv_payload pending_;
    char layer_name_[CLINE46_STATUS_LAYER_NAME_LEN + 1];

    volatile bool has_pending_;
    volatile uint32_t last_seen_ms_;
    volatile int pending_rssi_;
    volatile uint8_t mismatch_version_;

    bool available_;
    bool lost_reported_;
    int rssi_;
    uint32_t timeout_ms_;
    uint32_t idle_timeout_ms_;
    bool scanning_;

    Handler update_handler_;
    Handler lost_handler_;
};

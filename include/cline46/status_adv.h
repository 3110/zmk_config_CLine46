/*
 * CLine46 ステータスブロードキャスト（BLE アドバタイズ）のペイロード定義。
 *
 * 右手（Central）が 1 秒ごとに非接続の広告を出し、M5Stack などの受信側は
 * スキャンするだけで状態を読める。接続もペアリングも不要で、BLE プロファイル
 * （5個）も消費しない。
 *
 * このヘッダは受信側にもそのまま使える形にしてある（Zephyr 依存なし）。
 * バイト配置の説明は docs/status-advertisement.md を参照。
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

/* Bluetooth SIG が「内部用・テスト用」として予約している company ID。
 * 製品として配るものではないのでこれを使う */
#define CLINE46_STATUS_ADV_COMPANY_ID 0xFFFF

/* company ID だけでは他人の機器と区別できないので、マジックで絞り込む */
#define CLINE46_STATUS_ADV_MAGIC_0 'C'
#define CLINE46_STATUS_ADV_MAGIC_1 'L'

/* ペイロードの形式。フィールドを増やしたら上げる */
#define CLINE46_STATUS_ADV_VERSION 1

/* 値が取れなかったときに入る値 */
#define CLINE46_STATUS_PCT_UNKNOWN 0xFF
#define CLINE46_STATUS_MV_UNKNOWN 0
#define CLINE46_STATUS_LAYER_NAME_LEN 4
#define CLINE46_STATUS_DEFAULT_LAYER_NONE 0x0F

/* flags のビット */
#define CLINE46_STATUS_FLAG_USB_POWERED (1 << 0)  /* USB から給電されている */
#define CLINE46_STATUS_FLAG_USB_HID_READY (1 << 1) /* USB HID が使える状態 */
#define CLINE46_STATUS_FLAG_OUTPUT_BLE (1 << 2)   /* キー入力の出力先が BLE */
#define CLINE46_STATUS_FLAG_STUDIO_UNLOCKED (1 << 3) /* ZMK Studio のロック解除中 */
#define CLINE46_STATUS_FLAG_SPLIT_CONNECTED (1 << 4) /* 左手と繋がっている */
#define CLINE46_STATUS_FLAG_IDLE (1 << 5)         /* アイドル状態 */

/* profile のビット */
#define CLINE46_STATUS_PROFILE_CONNECTED (1 << 7) /* 選択中プロファイルが接続済み */
#define CLINE46_STATUS_PROFILE_OPEN (1 << 6)      /* 選択中プロファイルが未ペアリング */
#define CLINE46_STATUS_PROFILE_INDEX_MASK 0x07

/* os_default_layer の上位4ビット。zmk-feature-os-detection の enum zmk_os と同じ値 */
enum cline46_status_os {
    CLINE46_STATUS_OS_UNKNOWN = 0,
    CLINE46_STATUS_OS_WINDOWS = 1,
    CLINE46_STATUS_OS_MACOS = 2,
    CLINE46_STATUS_OS_LINUX = 3,
    CLINE46_STATUS_OS_IOS = 4,
    CLINE46_STATUS_OS_ANDROID = 5,
};

/* reset_reason。hwinfo_get_reset_cause() を丸めたもの */
enum cline46_status_reset_reason {
    CLINE46_STATUS_RESET_UNKNOWN = 0,
    CLINE46_STATUS_RESET_POWER_ON = 1,  /* 電池を入れた */
    CLINE46_STATUS_RESET_PIN = 2,       /* リセットボタン */
    CLINE46_STATUS_RESET_SOFTWARE = 3,  /* &sys_reset やファーム書き込み */
    CLINE46_STATUS_RESET_WATCHDOG = 4,  /* フリーズ検出による再起動 */
    CLINE46_STATUS_RESET_BROWNOUT = 5,  /* 電圧低下 */
    CLINE46_STATUS_RESET_LOW_POWER_WAKE = 6, /* ディープスリープからの復帰 */
    CLINE46_STATUS_RESET_DEBUG = 7,
    CLINE46_STATUS_RESET_OTHER = 8,
};

/*
 * 広告の manufacturer specific data に載せる中身（22バイト）。
 * 数値はすべてリトルエンディアン。詰め物が入らないようにフィールドを並べてある。
 */
struct cline46_status_adv_payload {
    uint16_t company_id;   /* CLINE46_STATUS_ADV_COMPANY_ID */
    uint8_t magic[2];      /* 'C', 'L' */
    uint8_t version;       /* CLINE46_STATUS_ADV_VERSION */
    uint8_t keyboard_id;   /* 個体識別（hwinfo のデバイスIDの先頭1バイト） */
    uint8_t layer_index;   /* 最上位のアクティブレイヤー番号 */
    char layer_name[CLINE46_STATUS_LAYER_NAME_LEN]; /* display-name の先頭4文字。
                                                     * 4文字ちょうどのときは終端無し */
    uint16_t central_mv;   /* 右手の電池電圧 mV（0 = 不明） */
    uint8_t central_pct;   /* 右手の電池残量 %（0xFF = 不明） */
    uint8_t peripheral_pct;/* 左手の電池残量 %（0xFF = 不明）。
                            * 左手の電圧は Central に中継されないので % のみ */
    uint8_t os_default_layer; /* [7:4] = enum cline46_status_os
                               * [3:0] = 既定レイヤー（0x0F = 未設定） */
    uint8_t profile;       /* [7] 接続済み [6] 未ペアリング [2:0] プロファイル番号 */
    uint8_t flags;         /* CLINE46_STATUS_FLAG_* */
    uint16_t uptime_min;   /* 起動からの経過分（65535 で頭打ち） */
    uint8_t reset_reason;  /* enum cline46_status_reset_reason */
    uint8_t incident_count;/* watchdog に残っている記録の件数（255 で頭打ち） */
} __attribute__((packed));

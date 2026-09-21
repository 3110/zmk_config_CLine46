/*
 * CLine46 ステータスモニタ（シリアル出力版）
 *
 * CLine46（右手・Central）が流している BLE 広告を受信して、レイヤーや電池
 * などをシリアルに出す。接続もペアリングもしないので、キーボードと PC の
 * 接続には一切影響しない。
 *
 * 画面を使わないので、BLE が載っている M5Stack ならどれでも動く
 * （Basic / Core2 / CoreS3 / StickC / StickC Plus / Atom / AtomS3 …）。
 *
 * 必要なライブラリ: NimBLE-Arduino（2.x 推奨。1.4 系でも動くよう分岐あり）
 * 広告の中身: status_adv.h / 詳しくは docs/status-advertisement.md
 *
 * SPDX-License-Identifier: MIT
 */

#include <Arduino.h>
#include <NimBLEDevice.h>

#include "status_adv.h"

/* NimBLE-Arduino 2.x 以降には NimBLECppVersion.h がある。1.4 系には無い */
#if __has_include(<NimBLECppVersion.h>)
#define CL_NIMBLE_V2 1
#else
#define CL_NIMBLE_V2 0
#endif

/* 1.4 系の NimBLEAdvertisedDevice は const メソッドになっていないので、
 * 受け取る型をバージョンで切り替える */
#if CL_NIMBLE_V2
using AdvertisedDevice = const NimBLEAdvertisedDevice;
#else
using AdvertisedDevice = NimBLEAdvertisedDevice;
#endif

/* この秒数だけ受信が途切れたら「見失った」と表示する。
 * キーボードは操作中1秒・アイドル中10秒ごとに広告を出す */
static const uint32_t LOST_TIMEOUT_MS = 15000;

/* 広告のたびに出すと多いので、この間隔より短い更新は表示しない（0 で全部出す）*/
static const uint32_t PRINT_INTERVAL_MS = 1000;

static cline46_status_adv_payload g_status;
static bool g_have_status = false;
static uint32_t g_last_seen_ms = 0;
static uint32_t g_last_print_ms = 0;
static bool g_pending_print = false;
static bool g_reported_lost = true;
static int g_last_rssi = 0;

static const char *os_name(uint8_t os) {
  switch (os) {
    case CLINE46_STATUS_OS_WINDOWS: return "Windows";
    case CLINE46_STATUS_OS_MACOS:   return "macOS";
    case CLINE46_STATUS_OS_LINUX:   return "Linux";
    case CLINE46_STATUS_OS_IOS:     return "iOS";
    case CLINE46_STATUS_OS_ANDROID: return "Android";
    default:                        return "unknown";
  }
}

static const char *reset_name(uint8_t reason) {
  switch (reason) {
    case CLINE46_STATUS_RESET_POWER_ON:       return "電源投入";
    case CLINE46_STATUS_RESET_PIN:            return "リセットピン";
    case CLINE46_STATUS_RESET_SOFTWARE:       return "ソフトリセット";
    case CLINE46_STATUS_RESET_WATCHDOG:       return "watchdog";
    case CLINE46_STATUS_RESET_BROWNOUT:       return "電圧低下";
    case CLINE46_STATUS_RESET_LOW_POWER_WAKE: return "スリープ復帰";
    case CLINE46_STATUS_RESET_DEBUG:          return "デバッガ";
    case CLINE46_STATUS_RESET_OTHER:          return "その他";
    default:                                  return "不明";
  }
}

/* 立っているフラグを USB/HID/BLE/Studio/Split/Idle の頭文字で並べる */
static void format_flags(uint8_t flags, char *out, size_t len) {
  snprintf(out, len, "%c%c%c%c%c%c",
           (flags & CLINE46_STATUS_FLAG_USB_POWERED)     ? 'U' : '-',
           (flags & CLINE46_STATUS_FLAG_USB_HID_READY)   ? 'H' : '-',
           (flags & CLINE46_STATUS_FLAG_OUTPUT_BLE)      ? 'B' : '-',
           (flags & CLINE46_STATUS_FLAG_STUDIO_UNLOCKED) ? 'S' : '-',
           (flags & CLINE46_STATUS_FLAG_SPLIT_CONNECTED) ? 'L' : '-',
           (flags & CLINE46_STATUS_FLAG_IDLE)            ? 'I' : '-');
}

static void format_battery(uint16_t mv, uint8_t pct, char *out, size_t len) {
  char pct_text[8];
  if (pct == CLINE46_STATUS_PCT_UNKNOWN) {
    snprintf(pct_text, sizeof(pct_text), "--%%");
  } else {
    snprintf(pct_text, sizeof(pct_text), "%u%%", pct);
  }

  if (mv == CLINE46_STATUS_MV_UNKNOWN) {
    snprintf(out, len, "----mV/%s", pct_text);
  } else {
    snprintf(out, len, "%4umV/%s", mv, pct_text);
  }
}

static void print_status(void) {
  char layer[CLINE46_STATUS_LAYER_NAME_LEN + 1] = {0};
  memcpy(layer, g_status.layer_name, CLINE46_STATUS_LAYER_NAME_LEN);

  char flags[8];
  format_flags(g_status.flags, flags, sizeof(flags));

  char right[24];
  char left[24];
  format_battery(g_status.central_mv, g_status.central_pct, right, sizeof(right));
  format_battery(g_status.peripheral_mv, g_status.peripheral_pct, left, sizeof(left));

  uint8_t default_layer = g_status.os_default_layer & 0x0F;
  char default_layer_text[8];
  if (default_layer == CLINE46_STATUS_DEFAULT_LAYER_NONE) {
    snprintf(default_layer_text, sizeof(default_layer_text), "-");
  } else {
    snprintf(default_layer_text, sizeof(default_layer_text), "%u", default_layer);
  }

  Serial.printf("L%u:%-4s  右 %s  左 %s  %s  OS:%s(既定%s)  prof:%u%s  稼働%umin  前回:%s",
                g_status.layer_index, layer, right, left, flags,
                os_name(g_status.os_default_layer >> 4), default_layer_text,
                g_status.profile & CLINE46_STATUS_PROFILE_INDEX_MASK,
                (g_status.profile & CLINE46_STATUS_PROFILE_CONNECTED) ? "接続" : "未接続",
                g_status.uptime_min, reset_name(g_status.reset_reason));

  if (g_status.incident_count > 0) {
    Serial.printf("  記録%u件", g_status.incident_count);
  }
  Serial.printf("  RSSI:%ddBm  ID:%02X\n", g_last_rssi, g_status.keyboard_id);
}

static void handle_advertisement(AdvertisedDevice *device) {
  if (!device->haveManufacturerData()) {
    return;
  }

  std::string data = device->getManufacturerData();
  if (data.size() < sizeof(cline46_status_adv_payload)) {
    return;
  }

  cline46_status_adv_payload payload;
  memcpy(&payload, data.data(), sizeof(payload));

  /* 他人の機器を弾く。アドレスは定期的に変わるので MAC では絞れない */
  if (payload.company_id != CLINE46_STATUS_ADV_COMPANY_ID) return;
  if (payload.magic[0] != CLINE46_STATUS_ADV_MAGIC_0) return;
  if (payload.magic[1] != CLINE46_STATUS_ADV_MAGIC_1) return;

  if (payload.version != CLINE46_STATUS_ADV_VERSION) {
    static uint8_t warned_version = 0xFF;
    if (warned_version != payload.version) {
      warned_version = payload.version;
      Serial.printf("[警告] 広告の形式が違います（キーボード: version %u / このスケッチ: %u）。"
                    "status_adv.h を更新してください\n",
                    payload.version, CLINE46_STATUS_ADV_VERSION);
    }
    return;
  }

  g_status = payload;
  g_have_status = true;
  g_pending_print = true;
  g_last_seen_ms = millis();
  g_last_rssi = device->getRSSI();

  if (g_reported_lost) {
    g_reported_lost = false;
    Serial.println("--- キーボードを見つけました ---");
    g_last_print_ms = 0; /* 見つけた直後は間引かずに出す */
  }
}

#if CL_NIMBLE_V2
class ScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice *device) override { handle_advertisement(device); }
};
#else
class ScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice *device) override { handle_advertisement(device); }
};
#endif

static ScanCallbacks g_scan_callbacks;

void setup() {
  Serial.begin(115200);
  delay(500); /* USB シリアルが繋がるまでの猶予 */

  Serial.println();
  Serial.println("CLine46 ステータスモニタ");
  Serial.printf("NimBLE-Arduino %s 系で動作\n", CL_NIMBLE_V2 ? "2.x/3.x" : "1.4");
  Serial.println("広告を待っています…");

  NimBLEDevice::init("");

  NimBLEScan *scan = NimBLEDevice::getScan();

  /* 非接続広告なのでスキャン応答は要らない。active scan にしないほうが
   * 受信側の消費電力も少ない */
  scan->setActiveScan(false);
  scan->setInterval(100);
  scan->setWindow(99);

#if CL_NIMBLE_V2
  /* 同じ端末からの2回目以降を捨てないようにする（捨てると更新が止まる）*/
  scan->setScanCallbacks(&g_scan_callbacks, /*wantDuplicates=*/true);
  scan->setDuplicateFilter(false);
  scan->start(0, false); /* 0 = 無期限 */
#else
  scan->setAdvertisedDeviceCallbacks(&g_scan_callbacks, /*wantDuplicates=*/true);
  scan->start(0, nullptr, false);
#endif
}

void loop() {
  uint32_t now = millis();

  if (g_have_status && (now - g_last_seen_ms) > LOST_TIMEOUT_MS) {
    if (!g_reported_lost) {
      g_reported_lost = true;
      Serial.println("--- 受信が途切れました（スリープ中か圏外）---");
    }
  } else if (g_pending_print && !g_reported_lost) {
    if (PRINT_INTERVAL_MS == 0 || (now - g_last_print_ms) >= PRINT_INTERVAL_MS) {
      g_last_print_ms = now;
      g_pending_print = false;
      print_status();
    }
  }

  delay(50);
}

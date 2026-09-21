/*
 * CLine46 ステータス表示（M5AtomS3R / AtomS3 の 128x128 画面、ホーム画面）
 *
 * 「今どのレイヤーか」と「電池はあとどれくらいか」だけに絞った画面。
 *
 *   ┌──────────────────┐
 *   │ BT0        macOS │ ヘッダ（出力先と OS）
 *   │      SYMB        │ レイヤー名（特大・色分け）
 *   │ R ███████░ 1.28V │ 右手
 *   │ L █████░░░ 1.23V │ 左手
 *   │ 3d 04h         · │ 稼働時間と受信インジケータ
 *   └──────────────────┘
 *
 * 操作: 画面を短押し=明るさ切替 / 長押し=画面オフ
 *
 * 必要なもの: M5Unified, NimBLE-Arduino, CLine46Status
 * SPDX-License-Identifier: MIT
 */

#include <CLine46Status.h>
#include <M5Unified.h>

CLine46Status keyboard;

static M5Canvas canvas(&M5.Display);

static const int16_t SCREEN_W = 128;
static const int16_t SCREEN_H = 128;

/* レイアウト（y 座標） */
static const int16_t HEADER_Y = 2;
static const int16_t LAYER_CY = 38;   /* レイヤー名の中心 */
static const int16_t ROW_R_Y = 64;    /* 右手の行 */
static const int16_t ROW_L_Y = 88;    /* 左手の行 */
static const int16_t FOOTER_Y = 114;
static const int16_t BAR_X = 16;
static const int16_t BAR_W = 66;
static const int16_t BAR_H = 10;

/* 明るさの段階。長押しで 0（消灯）にもできる */
static const uint8_t BRIGHTNESS[] = {180, 80, 30};
static uint8_t brightness_index = 0;
static bool display_off = false;

/* 画面を描き直す間隔。広告が来なくても稼働時間と「受信なし」を更新する */
static const uint32_t REDRAW_INTERVAL_MS = 1000;
static uint32_t last_draw_ms = 0;
static bool blink = false;

/* レイヤーごとの色。キーマップの並び（BASE/SYMBOL/MOUSE/SCROLL/予約） */
static uint16_t layerColor(uint8_t index) {
  switch (index) {
    case 0:  return TFT_WHITE;
    case 1:  return TFT_CYAN;
    case 2:  return TFT_YELLOW;
    case 3:  return TFT_MAGENTA;
    default: return TFT_LIGHTGREY;
  }
}

/* 電池の色。NiMH 単セルなので電圧で見るほうが分かりやすい
 * （CONFIG_ZMK_NON_LIPO_LOW_MV=1000 で電源が落ちる） */
static uint16_t batteryColor(uint16_t mv, uint8_t percent) {
  if (CLine46Status::validMv(mv)) {
    if (mv < 1050) return TFT_RED;
    if (mv < 1100) return TFT_ORANGE;
    return TFT_GREEN;
  }
  if (!CLine46Status::validPercent(percent)) return TFT_DARKGREY;
  if (percent < 15) return TFT_RED;
  if (percent < 35) return TFT_ORANGE;
  return TFT_GREEN;
}

static void drawBatteryRow(int16_t y, const char *label, uint16_t mv, uint8_t percent,
                           bool connected) {
  canvas.setFont(&fonts::Font2);
  canvas.setTextDatum(top_left);
  canvas.setTextColor(TFT_LIGHTGREY);
  canvas.drawString(label, 2, y);

  if (!connected) {
    canvas.setTextColor(TFT_RED);
    canvas.drawString("-- OFF --", BAR_X, y);
    return;
  }

  /* バーの枠と中身。中身は残量(%)、数字は電圧 */
  int16_t bar_y = y + 3;
  canvas.drawRoundRect(BAR_X, bar_y, BAR_W, BAR_H, 2, TFT_DARKGREY);
  if (CLine46Status::validPercent(percent)) {
    int16_t fill = (int16_t)((BAR_W - 2) * percent / 100);
    canvas.fillRect(BAR_X + 1, bar_y + 1, fill, BAR_H - 2, batteryColor(mv, percent));
  }

  canvas.setTextDatum(top_right);
  canvas.setTextColor(batteryColor(mv, percent));
  char value[12];
  if (CLine46Status::validMv(mv)) {
    snprintf(value, sizeof(value), "%u.%02uV", mv / 1000, (mv % 1000) / 10);
    canvas.drawString(value, SCREEN_W - 2, y);
  } else if (CLine46Status::validPercent(percent)) {
    snprintf(value, sizeof(value), "%u%%", percent);
    canvas.drawString(value, SCREEN_W - 2, y);
  } else {
    canvas.setTextColor(TFT_DARKGREY);
    canvas.drawString("--", SCREEN_W - 2, y);
  }
}

static void drawHeader() {
  canvas.setFont(&fonts::Font2);
  canvas.setTextDatum(top_left);

  /* 出力先。BLE ならプロファイル番号まで出す */
  if (keyboard.usbHidReady()) {
    canvas.setTextColor(TFT_GREEN);
    canvas.drawString("USB", 2, HEADER_Y);
  } else {
    char profile[8];
    snprintf(profile, sizeof(profile), "BT%u", keyboard.profileIndex());
    canvas.setTextColor(keyboard.profileConnected() ? TFT_CYAN : TFT_DARKGREY);
    canvas.drawString(profile, 2, HEADER_Y);
  }

  canvas.setTextDatum(top_right);
  canvas.setTextColor(TFT_LIGHTGREY);
  canvas.drawString(keyboard.osName(), SCREEN_W - 2, HEADER_Y);
}

static void drawFooter() {
  canvas.setFont(&fonts::Font2);
  canvas.setTextDatum(top_left);
  canvas.setTextColor(TFT_DARKGREY);

  uint16_t minutes = keyboard.uptimeMinutes();
  char uptime[16];
  if (minutes >= 60 * 24) {
    snprintf(uptime, sizeof(uptime), "%ud %02uh", minutes / (60 * 24), (minutes / 60) % 24);
  } else {
    snprintf(uptime, sizeof(uptime), "%uh %02um", minutes / 60, minutes % 60);
  }
  canvas.drawString(uptime, 2, FOOTER_Y);

  /* watchdog の記録が残っていたら知らせる */
  if (keyboard.incidentCount() > 0) {
    char records[8];
    snprintf(records, sizeof(records), "!%u", keyboard.incidentCount());
    canvas.setTextDatum(top_center);
    canvas.setTextColor(TFT_ORANGE);
    canvas.drawString(records, SCREEN_W / 2, FOOTER_Y);
  }

  /* 受信のたびに点滅する。止まれば一目で分かる */
  if (blink) {
    canvas.fillCircle(SCREEN_W - 6, FOOTER_Y + 8, 3, TFT_DARKGREEN);
  }
}

static void drawHome() {
  canvas.fillSprite(TFT_BLACK);

  drawHeader();

  canvas.setFont(&fonts::FreeSansBold18pt7b);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(layerColor(keyboard.layerIndex()));
  canvas.drawString(keyboard.layerName(), SCREEN_W / 2, LAYER_CY);

  drawBatteryRow(ROW_R_Y, "R", keyboard.centralMv(), keyboard.centralPercent(), true);
  drawBatteryRow(ROW_L_Y, "L", keyboard.peripheralMv(), keyboard.peripheralPercent(),
                 keyboard.splitConnected());

  drawFooter();

  /* Studio が解除されたままだと設定変更を受け付ける状態なので目立たせる */
  if (keyboard.studioUnlocked()) {
    canvas.fillRect(0, 0, SCREEN_W, 18, TFT_RED);
    canvas.setFont(&fonts::Font2);
    canvas.setTextDatum(top_center);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString("STUDIO UNLOCKED", SCREEN_W / 2, HEADER_Y);
  }

  canvas.pushSprite(0, 0);
}

static void drawNoSignal() {
  canvas.fillSprite(TFT_BLACK);

  canvas.setFont(&fonts::Font2);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("NO SIGNAL", SCREEN_W / 2, SCREEN_H / 2 - 12);

  if (keyboard.available()) {
    /* いつから受信できていないか。スリープ中なのか圏外なのかの判断材料 */
    char age[16];
    snprintf(age, sizeof(age), "%us ago", keyboard.ageMs() / 1000);
    canvas.drawString(age, SCREEN_W / 2, SCREEN_H / 2 + 12);
  } else {
    canvas.drawString("waiting...", SCREEN_W / 2, SCREEN_H / 2 + 12);
  }

  canvas.pushSprite(0, 0);
}

static void applyBrightness() {
  if (display_off) {
    M5.Display.setBrightness(0);
    return;
  }
  /* キーボードがアイドルなら画面も落として焼き付きと消費を抑える */
  uint8_t value = BRIGHTNESS[brightness_index];
  M5.Display.setBrightness(keyboard.idle() ? value / 3 : value);
}

static void onUpdate(CLine46Status &status) {
  blink = !blink;
  drawHome();
  applyBrightness();
  last_draw_ms = millis();
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  M5.Display.setRotation(0);
  M5.Display.fillScreen(TFT_BLACK);
  applyBrightness();

  canvas.setColorDepth(16);
  canvas.createSprite(SCREEN_W, SCREEN_H);

  keyboard.onUpdate(onUpdate);
  keyboard.begin();

  drawNoSignal();
}

void loop() {
  M5.update();

  if (M5.BtnA.wasClicked()) {
    if (display_off) {
      display_off = false;
    } else {
      brightness_index = (brightness_index + 1) % (sizeof(BRIGHTNESS) / sizeof(BRIGHTNESS[0]));
    }
    applyBrightness();
  }
  if (M5.BtnA.wasHold()) {
    display_off = !display_off;
    applyBrightness();
  }

  keyboard.poll(); /* 受信したら onUpdate() から描き直す */

  /* 広告が来ない間も、稼働時間と「受信なし」の表示は進める */
  if (millis() - last_draw_ms >= REDRAW_INTERVAL_MS) {
    last_draw_ms = millis();
    if (keyboard.alive()) {
      drawHome();
    } else {
      drawNoSignal();
    }
  }

  delay(20);
}

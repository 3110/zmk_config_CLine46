/*
 * CLine46 ステータス表示（M5AtomS3R / AtomS3 の 128x128 画面）
 *
 * 画面は3つ。短押しで切り替える。
 *   1. HOME       今どのレイヤーか・電池はあとどれくらいか
 *   2. CONNECTION 接続先と OS、Studio のロック状態
 *   3. HEALTH     稼働時間・前回のリセット理由・電圧の推移
 *
 *   ┌──────────────────┐
 *   │ BT0        macOS │ ヘッダ（出力先と OS）
 *   │      SYMB        │ レイヤー名（特大・色分け）
 *   │ L▮▮▮▯62%  R▮▮▮▮75%│ 左右の電池（アイコンと残量）
 *   │   1.23V     1.28V│ 電圧（NiMH はこちらが判断しやすい）
 *   │ 3d 04h         · │ 稼働時間と受信インジケータ
 *   └──────────────────┘
 *
 * 操作: 画面を短押し=画面切替 / 長押し=明るさ切替（最後まで行くと消灯）
 *
 * 必要なもの: M5Unified, NimBLE-Arduino, CLine46Status
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include <CLine46Status.h>
#include <M5Unified.h>

CLine46Status keyboard;

static M5Canvas canvas(&M5.Display);

static const int16_t SCREEN_W = 128;
static const int16_t SCREEN_H = 128;

/* 画面の向き。0=標準 / 1=90度 / 2=180度 / 3=270度。
 * USB ケーブルを上に出して置くなら 2（180度回転） */
static const uint8_t SCREEN_ROTATION = 2;

/* レイアウト（y 座標） */
static const int16_t HEADER_Y = 2;
static const int16_t LAYER_CY = 44;  /* レイヤー名の中心 */
static const int16_t BATT_Y = 70;    /* 電池アイコンと残量(%) */
static const int16_t VOLT_Y = 90;    /* 電圧 */
static const int16_t FOOTER_Y = 110;

/* 電池は左右で1行。画面を左右に二分して、それぞれに
 * ラベル・アイコン・残量(%)を置く */
static const int16_t BLOCK_W = 64;
static const int16_t ICON_X = 10;  /* ブロック先頭からの位置 */
static const int16_t ICON_W = 20;
static const int16_t ICON_H = 13;

/* 画面2（接続と診断）・画面3（健康状態）の行 */
static const int16_t TITLE_Y = 0;
static const int16_t ROW_Y0 = 18;
static const int16_t ROW_H = 15;

/* 画面3のグラフ。電圧は NiMH 単セルの範囲で固定目盛りにして、
 * 減り方の傾きがそのまま見えるようにする */
static const int16_t GRAPH_X = 2;
static const int16_t GRAPH_Y = 68;
static const int16_t GRAPH_W = 124;
static const int16_t GRAPH_H = 46;
static const uint16_t GRAPH_MIN_MV = 1000;
static const uint16_t GRAPH_MAX_MV = 1400;
static const uint16_t GRAPH_LOW_MV = 1050; /* この線を割ったら交換どき */

/* 電圧の履歴。1分ごとに1点、画面の横幅ぶん（約2時間） */
static const uint8_t HISTORY_SIZE = GRAPH_W;
static const uint32_t SAMPLE_INTERVAL_MS = 60000;
static uint16_t history_r[HISTORY_SIZE];
static uint16_t history_l[HISTORY_SIZE];
static uint8_t history_count = 0;
static uint32_t last_sample_ms = 0;

enum Screen : uint8_t { SCREEN_HOME = 0, SCREEN_CONNECTION, SCREEN_HEALTH, SCREEN_COUNT };
static uint8_t screen = SCREEN_HOME;

/* 明るさの段階。長押しで一段ずつ下げ、最後まで行くと消灯 */
static const uint8_t BRIGHTNESS[] = {180, 80, 30};
static const uint8_t BRIGHTNESS_COUNT = sizeof(BRIGHTNESS) / sizeof(BRIGHTNESS[0]);
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

/* 電池アイコン。枠と端子を描いて、残量ぶんを塗る */
static void drawBatteryIcon(int16_t x, int16_t y, uint16_t color, uint8_t percent,
                            bool connected) {
  canvas.drawRoundRect(x, y, ICON_W, ICON_H, 2, color);
  canvas.fillRect(x + ICON_W, y + 4, 2, ICON_H - 8, color);

  if (!connected) {
    /* 切断中は斜線で潰す */
    canvas.drawLine(x, y + ICON_H - 1, x + ICON_W, y, color);
    return;
  }

  if (CLine46Status::validPercent(percent)) {
    int16_t fill = (int16_t)((ICON_W - 4) * percent / 100);
    if (fill > 0) {
      canvas.fillRect(x + 2, y + 2, fill, ICON_H - 4, color);
    }
  }
}

/* 左右それぞれの電池。上の行にアイコンと残量(%)、下の行に電圧 */
static void drawBatteryBlock(int16_t x0, const char *label, uint16_t mv, uint8_t percent,
                             bool connected) {
  uint16_t color = connected ? batteryColor(mv, percent) : TFT_DARKGREY;
  char text[12];

  canvas.setFont(&fonts::Font2);
  canvas.setTextDatum(top_left);
  canvas.setTextColor(connected ? TFT_LIGHTGREY : TFT_DARKGREY);
  canvas.drawString(label, x0 + 1, BATT_Y);

  drawBatteryIcon(x0 + ICON_X, BATT_Y + 2, color, percent, connected);

  canvas.setTextDatum(top_right);
  canvas.setTextColor(color);
  if (connected && CLine46Status::validPercent(percent)) {
    snprintf(text, sizeof(text), "%u%%", percent);
  } else {
    snprintf(text, sizeof(text), "--");
  }
  canvas.drawString(text, x0 + BLOCK_W - 2, BATT_Y);

  canvas.setTextColor(connected ? color : TFT_DARKGREY);
  if (connected && CLine46Status::validMv(mv)) {
    snprintf(text, sizeof(text), "%u.%02uV", mv / 1000, (mv % 1000) / 10);
  } else {
    snprintf(text, sizeof(text), "----");
  }
  canvas.drawString(text, x0 + BLOCK_W - 2, VOLT_Y);
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

static void formatUptime(char *out, size_t size) {
  uint16_t minutes = keyboard.uptimeMinutes();
  if (minutes >= 60 * 24) {
    snprintf(out, size, "%ud %02uh", minutes / (60 * 24), (minutes / 60) % 24);
  } else {
    snprintf(out, size, "%uh %02um", minutes / 60, minutes % 60);
  }
}

static void drawFooter() {
  canvas.setFont(&fonts::Font2);
  canvas.setTextDatum(top_left);
  canvas.setTextColor(TFT_DARKGREY);

  char uptime[16];
  formatUptime(uptime, sizeof(uptime));
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

  /* 実物と同じ並びで、左手を左に、右手を右に */
  drawBatteryBlock(0, "L", keyboard.peripheralMv(), keyboard.peripheralPercent(),
                   keyboard.splitConnected());
  drawBatteryBlock(BLOCK_W, "R", keyboard.centralMv(), keyboard.centralPercent(), true);

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

/* 画面2・3の共通部品 */
static void drawTitle(const char *title) {
  canvas.setFont(&fonts::Font2);
  canvas.setTextDatum(top_center);
  canvas.setTextColor(TFT_LIGHTGREY);
  canvas.drawString(title, SCREEN_W / 2, TITLE_Y);
  canvas.fillRect(0, TITLE_Y + 16, SCREEN_W, 1, TFT_DARKGREY);
}

static void drawRow(int16_t index, const char *label, const char *value, uint16_t color) {
  int16_t y = ROW_Y0 + index * ROW_H;

  canvas.setFont(&fonts::Font2);
  canvas.setTextDatum(top_left);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString(label, 2, y);

  canvas.setTextDatum(top_right);
  canvas.setTextColor(color);
  canvas.drawString(value, SCREEN_W - 2, y);
}

/* リセット理由は日本語フォントを積まずに済むよう、この画面では英字で出す */
static const char *resetReasonText() {
  switch (keyboard.resetReason()) {
    case CLINE46_STATUS_RESET_POWER_ON:       return "POWER ON";
    case CLINE46_STATUS_RESET_PIN:            return "RESET PIN";
    case CLINE46_STATUS_RESET_SOFTWARE:       return "SOFT RESET";
    case CLINE46_STATUS_RESET_WATCHDOG:       return "WATCHDOG";
    case CLINE46_STATUS_RESET_BROWNOUT:       return "BROWNOUT";
    case CLINE46_STATUS_RESET_LOW_POWER_WAKE: return "WAKE";
    case CLINE46_STATUS_RESET_DEBUG:          return "DEBUG";
    case CLINE46_STATUS_RESET_FREEZE:         return "FREEZE";
    case CLINE46_STATUS_RESET_FAULT:          return "FAULT";
    case CLINE46_STATUS_RESET_OTHER:          return "OTHER";
    default:                                  return "UNKNOWN";
  }
}

/* 画面2: 接続と診断 */
static void drawConnection() {
  char value[20];

  canvas.fillSprite(TFT_BLACK);
  drawTitle("CONNECTION");

  snprintf(value, sizeof(value), "BT%u", keyboard.profileIndex());
  drawRow(0, "Profile", value,
          keyboard.profileConnected() ? TFT_GREEN
                                      : (keyboard.profileOpen() ? TFT_ORANGE : TFT_DARKGREY));

  drawRow(1, "Output", keyboard.outputBle() ? "BLE" : "USB",
          keyboard.outputBle() ? TFT_CYAN : TFT_GREEN);

  drawRow(2, "OS", keyboard.osName(),
          keyboard.os() == CLINE46_STATUS_OS_UNKNOWN ? TFT_DARKGREY : TFT_LIGHTGREY);

  if (keyboard.defaultLayer() < 0) {
    snprintf(value, sizeof(value), "--");
  } else {
    snprintf(value, sizeof(value), "L%d", keyboard.defaultLayer());
  }
  drawRow(3, "Default", value, TFT_LIGHTGREY);

  drawRow(4, "Split", keyboard.splitConnected() ? "OK" : "OFF",
          keyboard.splitConnected() ? TFT_GREEN : TFT_RED);

  drawRow(5, "Studio", keyboard.studioUnlocked() ? "UNLOCKED" : "LOCKED",
          keyboard.studioUnlocked() ? TFT_RED : TFT_DARKGREY);

  snprintf(value, sizeof(value), "%ddBm", keyboard.rssi());
  drawRow(6, "RSSI", value, TFT_DARKGREY);

  canvas.pushSprite(0, 0);
}

static int16_t mvToY(uint16_t mv) {
  if (mv < GRAPH_MIN_MV) mv = GRAPH_MIN_MV;
  if (mv > GRAPH_MAX_MV) mv = GRAPH_MAX_MV;
  uint32_t span = GRAPH_MAX_MV - GRAPH_MIN_MV;
  return GRAPH_Y + GRAPH_H - 1 - (int16_t)((uint32_t)(mv - GRAPH_MIN_MV) * (GRAPH_H - 1) / span);
}

/* 1系統ぶんの折れ線。値の無い区間（0）は繋がない */
static void drawHistoryLine(const uint16_t *history, uint16_t color) {
  bool has_previous = false;
  int16_t previous_x = 0;
  int16_t previous_y = 0;

  for (uint8_t i = 0; i < history_count; i++) {
    uint16_t mv = history[i];
    if (mv == 0) {
      has_previous = false;
      continue;
    }

    int16_t x = GRAPH_X + i;
    int16_t y = mvToY(mv);
    if (has_previous) {
      canvas.drawLine(previous_x, previous_y, x, y, color);
    } else {
      canvas.fillRect(x, y, 1, 1, color);
    }
    previous_x = x;
    previous_y = y;
    has_previous = true;
  }
}

/* 画面3: 健康状態 */
static void drawHealth() {
  char value[20];

  canvas.fillSprite(TFT_BLACK);
  drawTitle("HEALTH");

  formatUptime(value, sizeof(value));
  drawRow(0, "Uptime", value, TFT_LIGHTGREY);

  drawRow(1, "Reset", resetReasonText(),
          (keyboard.resetReason() == CLINE46_STATUS_RESET_FREEZE ||
           keyboard.resetReason() == CLINE46_STATUS_RESET_FAULT ||
           keyboard.resetReason() == CLINE46_STATUS_RESET_WATCHDOG)
              ? TFT_ORANGE
              : TFT_LIGHTGREY);

  snprintf(value, sizeof(value), "%u", keyboard.incidentCount());
  drawRow(2, "Records", value, keyboard.incidentCount() > 0 ? TFT_ORANGE : TFT_DARKGREY);

  /* 枠と、電池が尽きる手前の目安線 */
  canvas.drawRoundRect(GRAPH_X - 1, GRAPH_Y - 1, GRAPH_W + 2, GRAPH_H + 2, 2, TFT_DARKGREY);
  int16_t low_y = mvToY(GRAPH_LOW_MV);
  for (int16_t x = GRAPH_X; x < GRAPH_X + GRAPH_W; x += 4) {
    canvas.fillRect(x, low_y, 2, 1, TFT_MAROON);
  }

  drawHistoryLine(history_l, TFT_CYAN);
  drawHistoryLine(history_r, TFT_GREEN);

  /* 凡例と目盛り。1点1分なので、横幅いっぱいで約2時間ぶん */
  canvas.setFont(&fonts::Font2);
  canvas.setTextDatum(top_left);
  canvas.setTextColor(TFT_CYAN);
  canvas.drawString("L", 2, GRAPH_Y + GRAPH_H + 2);
  canvas.setTextColor(TFT_GREEN);
  canvas.drawString("R", 16, GRAPH_Y + GRAPH_H + 2);
  canvas.setTextDatum(top_right);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("1.0-1.4V/2h", SCREEN_W - 2, GRAPH_Y + GRAPH_H + 2);

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

static void drawScreen() {
  if (!keyboard.alive()) {
    drawNoSignal();
    return;
  }

  switch (screen) {
    case SCREEN_CONNECTION: drawConnection(); break;
    case SCREEN_HEALTH:     drawHealth(); break;
    default:                drawHome(); break;
  }
}

/* 1分ごとに電圧を1点ずつ足す。いっぱいになったら古いほうから捨てる */
static void sampleHistory() {
  uint32_t now = millis();
  if (history_count > 0 && (now - last_sample_ms) < SAMPLE_INTERVAL_MS) {
    return;
  }
  last_sample_ms = now;

  if (history_count >= HISTORY_SIZE) {
    memmove(history_r, history_r + 1, (HISTORY_SIZE - 1) * sizeof(history_r[0]));
    memmove(history_l, history_l + 1, (HISTORY_SIZE - 1) * sizeof(history_l[0]));
    history_count = HISTORY_SIZE - 1;
  }

  history_r[history_count] = keyboard.centralMv();
  history_l[history_count] = keyboard.splitConnected() ? keyboard.peripheralMv() : 0;
  history_count++;
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
  sampleHistory();
  drawScreen();
  applyBrightness();
  last_draw_ms = millis();
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  M5.Display.setRotation(SCREEN_ROTATION);
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
      /* 消灯中はまず点ける */
      display_off = false;
      applyBrightness();
    } else {
      screen = (screen + 1) % SCREEN_COUNT;
      drawScreen();
    }
  }
  if (M5.BtnA.wasHold()) {
    /* 明るさを一段ずつ下げ、最後まで行ったら消灯 */
    if (display_off) {
      display_off = false;
      brightness_index = 0;
    } else if (brightness_index + 1 < BRIGHTNESS_COUNT) {
      brightness_index++;
    } else {
      display_off = true;
    }
    applyBrightness();
  }

  keyboard.poll(); /* 受信したら onUpdate() から描き直す */

  /* 広告が来ない間も、稼働時間と「受信なし」の表示は進める */
  if (millis() - last_draw_ms >= REDRAW_INTERVAL_MS) {
    last_draw_ms = millis();
    drawScreen();
  }

  delay(20);
}

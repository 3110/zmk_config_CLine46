/*
 * CLine46 キーマップビューワー（M5Stack Tab5 の 1280x720 画面）
 *
 * キーボードの右手が流している状態広告を受けて、今のレイヤーのキーマップを
 * 実物と同じ並びで描く。レイヤーが変わると画面全体が切り替わる。
 *
 *   ┌────────────────────────────────────────────────────────┐
 *   │ ▌SYMBOL  [0 BASE][1 SYMBOL][2 MOUSE][3 SCROLL]  BT0 ... │ ヘッダ
 *   │   左手                         右手                      │
 *   │  [^][=][7][8][9][@]         [(]["][)][;][!][?]          │
 *   │  ...                                                    │
 *   │     [Esc][0][.][▽][Space][▽]  [Enter][BS] (●) [Del][▽]  │ 親指とボール
 *   │ ┌ このレイヤー ──────────────┐ ┌ 受信 ──────────────┐   │
 *   │ └───────────────────────────┘ └───────────────────┘   │
 *   └────────────────────────────────────────────────────────┘
 *
 * 表示の遅れ: 今のファームは操作中1秒ごとに広告を出すので、レイヤーの
 * 切り替わりは最大で約1秒遅れて映る（変化直後だけ間隔を詰める改修で縮める予定）。
 *
 * 操作: ヘッダのレイヤー名をタッチすると、そのレイヤーを数秒だけ表示する
 * （キーボードに触らずに配置を確かめたいとき用）。
 *
 * 必要なもの: M5Unified, CLine46Status（BLE は Arduino コア内蔵のものを使う）
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include <CLine46Status.h>
#include <M5Unified.h>

#include "keymap.h"

#if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#include <esp32-hal-hosted.h>
#endif

using keymap::Key;
using keymap::KEY_COUNT;
using keymap::LAYER_COUNT;
using keymap::LAYERS;

CLine46Status keyboard;

/* 画面の向き。横長（1280x720）になる向きにする。上下が逆なら 1 と 3 を入れ替える */
static const uint8_t SCREEN_ROTATION = 3;

/* レイアウト（1280x720 の座標） */
static const int16_t SCREEN_W = 1280;
static const int16_t HEADER_H = 84;
static const int16_t UNIT = 80;      /* キー1つ分の間隔 */
static const int16_t KEY = 72;       /* キーの一辺 */
static const int16_t KEY_R = 9;      /* 角の丸み */
static const int16_t SHADOW = 4;     /* キーの下の影 */
static const int16_t KB_X = 44;      /* x=0 のキーの左端 */
static const int16_t KB_Y = 132;     /* 上段のキーの上端 */
static const int16_t HALF_LABEL_Y = 104;
static const int16_t PANEL_X = 36;
static const int16_t PANEL_Y = 478;
static const int16_t PANEL_W = SCREEN_W - PANEL_X * 2;
static const int16_t PANEL_H = 218;
static const int16_t PANEL_SPLIT = 716; /* 左の欄の幅 */
static const int16_t TAB_X = 400;
static const int16_t TAB_W = 118;
static const int16_t TAB_GAP = 6;

/* 色（RGB888。LovyanGFX は uint32_t を RGB888 として扱う） */
static const uint32_t COL_BG = 0x0B0E13u;
static const uint32_t COL_LINE = 0x1C222Bu;
static const uint32_t COL_KEY = 0x181E27u;
static const uint32_t COL_KEY_EDGE = 0x242C38u;
static const uint32_t COL_KEY_SHADOW = 0x0E1218u;
static const uint32_t COL_TEXT = 0xE6EBF2u;
static const uint32_t COL_DIM = 0x4A5363u;
static const uint32_t COL_MUTED = 0x8F99A8u;
static const uint32_t COL_FAINT = 0x566072u;
static const uint32_t COL_PANEL = 0x11161Du;
static const uint32_t COL_TAB = 0x131820u;
static const uint32_t COL_GOOD = 0x3FD08Au;
static const uint32_t COL_WARN = 0xF2A33Cu;
static const uint32_t COL_UNKNOWN_LAYER = 0xB8C0CCu;

/* レイヤーをタッチで覗いたときに表示しておく時間 */
static const uint32_t PREVIEW_MS = 4000;
/* 受信が無くてもヘッダ（経過時間など）を描き直す間隔 */
static const uint32_t STATUS_REDRAW_MS = 500;

static M5Canvas keycap(&M5.Display);
static M5Canvas header(&M5.Display);
static M5Canvas panel(&M5.Display);

/* 画面に出しているもの。変わったときだけ描き直す */
static int shown_layer = -1;
static int shown_mode = -1;
static uint32_t last_status_draw_ms = 0;

/* 受信の記録（最初の検証用に、広告の間隔を画面に出す） */
static uint32_t last_rx_ms = 0;
static uint32_t rx_interval_ms = 0;
static uint32_t rx_count = 0;

/* タッチで覗いているレイヤー。-1 なら覗いていない */
static int preview_layer = -1;
static uint32_t preview_until_ms = 0;

enum Mode : uint8_t { MODE_SEARCHING = 0, MODE_LIVE, MODE_LOST, MODE_OFF, MODE_MISMATCH };

/* ------------------------------------------------------------------ */

static uint32_t mix(uint32_t a, uint32_t b, uint8_t amount_of_a) {
  uint32_t out = 0;
  for (int shift = 0; shift <= 16; shift += 8) {
    uint32_t ca = (a >> shift) & 0xFF, cb = (b >> shift) & 0xFF;
    out |= ((ca * amount_of_a + cb * (255 - amount_of_a)) / 255) << shift;
  }
  return out;
}

static uint32_t layerColor(int layer) {
  return (layer >= 0 && layer < LAYER_COUNT) ? LAYERS[layer].color : COL_UNKNOWN_LAYER;
}

/* 入る大きさのフォントを大きい順に探す */
static const lgfx::IFont *fitFont(LovyanGFX &gfx, const char *text, int16_t max_w,
                                  const lgfx::IFont *const *fonts, size_t count) {
  for (size_t i = 0; i < count; i++) {
    gfx.setFont(fonts[i]);
    if (gfx.textWidth(text) <= max_w) {
      return fonts[i];
    }
  }
  return fonts[count - 1];
}

static const lgfx::IFont *const LEGEND_FONTS[] = {
    &fonts::lgfxJapanGothicP_28, &fonts::lgfxJapanGothicP_24, &fonts::lgfxJapanGothicP_20,
    &fonts::lgfxJapanGothicP_16, &fonts::lgfxJapanGothicP_12,
};

static Mode currentMode() {
  if (keyboard.versionMismatch()) return MODE_MISMATCH;
  if (!keyboard.available()) return MODE_SEARCHING;
  if (keyboard.broadcastOff()) return MODE_OFF;
  if (!keyboard.alive()) return MODE_LOST;
  return MODE_LIVE;
}

/* 画面に出すレイヤー。受信できていないときは BASE を出しておく */
static int displayedLayer() {
  if (preview_layer >= 0) return preview_layer;
  return currentMode() == MODE_LIVE ? keyboard.layerIndex() : 0;
}

/* ------------------------------------------------------------------ */

static void drawKey(uint8_t i, int layer) {
  const bool known = layer < LAYER_COUNT;
  const bool active_key = known && layer > 0 && LAYERS[layer].activator == i;

  uint8_t from = 0;
  const Key &key = active_key ? keymap::MAP[0][i]
                              : keymap::resolve(known ? layer : 0, i, from);
  /* 表に割り当てが無い（下のレイヤーから透けている）キーは暗く描く */
  const bool trans = !active_key && (!known || from != layer);
  const bool changed = known && layer > 0 && !trans && !active_key;
  const uint32_t lc = layerColor(layer);

  keycap.fillScreen(COL_BG);
  keycap.fillRoundRect(0, SHADOW, KEY, KEY, KEY_R, active_key ? mix(lc, 0, 128) : COL_KEY_SHADOW);
  keycap.fillRoundRect(0, 0, KEY, KEY, KEY_R, active_key ? lc : COL_KEY);
  if (!active_key) {
    keycap.drawRoundRect(0, 0, KEY, KEY, KEY_R, changed ? mix(lc, COL_KEY_EDGE, 115) : COL_KEY_EDGE);
  }

  const char *hold = active_key ? "押下中" : key.hold;
  const int16_t cx = KEY / 2;
  const int16_t legend_cy = hold ? KEY / 2 - 8 : KEY / 2;

  fitFont(keycap, key.tap, KEY - 10, LEGEND_FONTS, sizeof(LEGEND_FONTS) / sizeof(LEGEND_FONTS[0]));
  keycap.setTextDatum(middle_center);
  keycap.setTextColor(active_key ? COL_BG : trans ? COL_DIM : COL_TEXT);
  keycap.drawString(key.tap, cx, legend_cy);

  if (hold) {
    keycap.setFont(&fonts::lgfxJapanGothicP_12);
    uint32_t hold_color = active_key ? COL_BG
                          : key.layer >= 0 ? layerColor(key.layer)
                                           : COL_MUTED;
    if (trans) hold_color = mix(hold_color, COL_KEY, 110);
    keycap.setTextColor(hold_color);
    keycap.drawString(hold, cx, KEY - 14);
  }

  if (trans && known && layer > 0) {
    keycap.setFont(&fonts::lgfxJapanGothic_12);
    keycap.setTextDatum(top_right);
    keycap.setTextColor(0x3C4553u);
    keycap.drawString("▽", KEY - 5, 3);
  }

  keycap.pushSprite(KB_X + (int16_t)(keymap::keyX(i) * UNIT), KB_Y + keymap::keyY(i) * UNIT);
}

static void drawBall(int layer) {
  const int16_t r = KEY / 2;
  const int16_t cx = KB_X + (int16_t)(keymap::BALL_X * UNIT) + r;
  const int16_t cy = KB_Y + keymap::BALL_Y * UNIT + r;
  const bool scroll = layer == 3;
  const uint32_t lc = layerColor(layer);

  M5.Display.fillCircle(cx, cy, r, scroll ? lc : 0x1E2530u);
  M5.Display.fillCircle(cx - 10, cy - 12, r / 3, scroll ? mix(0xFFFFFFu, lc, 90) : 0x2E3746u);
  M5.Display.drawCircle(cx, cy, r, scroll ? lc : 0x2B3340u);
  M5.Display.setFont(&fonts::lgfxJapanGothicP_12);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(scroll ? COL_BG : 0x7E8898u);
  M5.Display.drawString(scroll ? "SCROLL" : "POINTER", cx, cy + 14);
}

static void drawKeyboard(int layer) {
  M5.Display.fillRect(0, HEADER_H, SCREEN_W, PANEL_Y - HEADER_H, COL_BG);

  M5.Display.setFont(&fonts::lgfxJapanGothicP_16);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextColor(COL_FAINT, COL_BG);
  M5.Display.drawString("左手", KB_X, HALF_LABEL_Y);
  M5.Display.drawString("右手", KB_X + 9 * UNIT, HALF_LABEL_Y);

  for (uint8_t i = 0; i < KEY_COUNT; i++) {
    drawKey(i, layer);
  }
  drawBall(layer);
}

/* ------------------------------------------------------------------ */

static void drawBattery(int16_t x, int16_t y, const char *label, uint8_t percent) {
  header.setFont(&fonts::lgfxJapanGothicP_16);
  header.setTextDatum(middle_left);
  header.setTextColor(0x9AA4B3u);
  header.drawString(label, x, y);
  x += 16;
  header.drawRoundRect(x, y - 7, 28, 14, 3, 0x6D7686u);
  header.fillRect(x + 28, y - 3, 2, 6, 0x6D7686u);
  if (CLine46Status::validPercent(percent)) {
    int16_t w = (int16_t)(24 * (percent > 100 ? 100 : percent) / 100);
    header.fillRect(x + 2, y - 5, w, 10, percent < 20 ? COL_WARN : 0x7FD1A8u);
  }
  char text[8];
  if (CLine46Status::validPercent(percent)) {
    snprintf(text, sizeof(text), "%u%%", percent);
  } else {
    snprintf(text, sizeof(text), "--%%");
  }
  header.drawString(text, x + 36, y);
}

static void drawHeader(int layer, Mode mode) {
  const uint32_t lc = layerColor(layer);
  const int16_t cy = HEADER_H / 2;

  header.fillScreen(COL_BG);
  header.drawFastHLine(0, HEADER_H - 1, SCREEN_W, COL_LINE);

  /* 今のレイヤー */
  header.fillRoundRect(36, cy - 24, 14, 48, 4, lc);
  header.setFont(&fonts::lgfxJapanGothicP_40);
  header.setTextDatum(baseline_left);
  header.setTextColor(lc);
  char name[16];
  if (layer < LAYER_COUNT) {
    snprintf(name, sizeof(name), "%s", LAYERS[layer].name);
  } else {
    snprintf(name, sizeof(name), "%s", keyboard.layerName());
  }
  header.drawString(name, 64, cy + 6);

  char sub[48];
  switch (mode) {
  case MODE_SEARCHING: snprintf(sub, sizeof(sub), "キーボードを探しています"); break;
  case MODE_LOST:      snprintf(sub, sizeof(sub), "受信が途切れています"); break;
  case MODE_OFF:       snprintf(sub, sizeof(sub), "キーボード側で広告がオフ"); break;
  case MODE_MISMATCH:  snprintf(sub, sizeof(sub), "広告の形式が違います"); break;
  default:
    if (preview_layer >= 0) {
      snprintf(sub, sizeof(sub), "レイヤー %d を表示中（タッチ）", layer);
    } else {
      snprintf(sub, sizeof(sub), "レイヤー %d", layer);
    }
    break;
  }
  if (mode != MODE_LIVE && preview_layer >= 0) {
    snprintf(sub, sizeof(sub), "レイヤー %d を表示中（タッチ）", layer);
  }
  header.setFont(&fonts::lgfxJapanGothicP_16);
  header.setTextDatum(top_left);
  header.setTextColor(mode == MODE_LIVE || preview_layer >= 0 ? COL_MUTED : COL_WARN);
  header.drawString(sub, 64, cy + 12);

  /* レイヤーのタブ */
  for (int l = 0; l < LAYER_COUNT; l++) {
    const int16_t x = TAB_X + l * (TAB_W + TAB_GAP);
    const bool on = l == layer;
    header.fillRoundRect(x, cy - 18, TAB_W, 36, 6, on ? LAYERS[l].color : COL_TAB);
    if (!on) header.drawRoundRect(x, cy - 18, TAB_W, 36, 6, 0x1E252Fu);
    header.setFont(&fonts::lgfxJapanGothicP_16);
    header.setTextDatum(middle_center);
    header.setTextColor(on ? COL_BG : 0x6D7686u);
    char tab[16];
    snprintf(tab, sizeof(tab), "%d %s", l, LAYERS[l].name);
    header.drawString(tab, x + TAB_W / 2, cy);
  }

  /* 接続先・電池・受信 */
  if (keyboard.available()) {
    header.setFont(&fonts::lgfxJapanGothicP_16);
    header.setTextDatum(middle_left);
    header.setTextColor(0x9AA4B3u);
    char where[24];
    if (keyboard.outputBle()) {
      snprintf(where, sizeof(where), "BT%u", keyboard.profileIndex());
    } else {
      snprintf(where, sizeof(where), "USB");
    }
    header.drawString(where, 912, cy);
    header.drawString(keyboard.osName(), 962, cy);
    drawBattery(1050, cy, "L", keyboard.peripheralPercent());
    drawBattery(1140, cy, "R", keyboard.centralPercent());
  }
  const bool fresh = mode == MODE_LIVE && keyboard.ageMs() < 300;
  header.fillCircle(SCREEN_W - 44, cy, 6,
                    mode == MODE_LIVE ? (fresh ? COL_GOOD : mix(COL_GOOD, COL_BG, 110)) : COL_WARN);

  header.pushSprite(0, 0);
}

/* ------------------------------------------------------------------ */

static void drawPanel(int layer, Mode mode) {
  panel.fillScreen(COL_BG);

  /* 左: このレイヤーの説明 */
  panel.fillRoundRect(0, 0, PANEL_SPLIT, PANEL_H, 12, COL_PANEL);
  panel.drawRoundRect(0, 0, PANEL_SPLIT, PANEL_H, 12, COL_LINE);
  panel.setFont(&fonts::lgfxJapanGothicP_16);
  panel.setTextDatum(top_left);
  panel.setTextColor(0x6F7989u);
  panel.drawString("このレイヤー", 20, 16);

  panel.setFont(&fonts::lgfxJapanGothicP_24);
  panel.setTextColor(0xC9D1DCu);
  panel.setTextWrap(false);
  const char *how = layer < LAYER_COUNT ? LAYERS[layer].how
                                         : "このレイヤーの割り当ては表示用の表（keymap.h）に未登録です";
  /* 1行に収まらない説明は、欄の幅で折り返す */
  {
    const int16_t max_w = PANEL_SPLIT - 40;
    int16_t y = 48;
    const char *p = how;
    char line[160];
    while (*p && y < PANEL_H - 30) {
      size_t len = 0;
      size_t fit = 0;
      while (p[len]) {
        size_t step = 1;
        uint8_t c = (uint8_t)p[len];
        if (c >= 0xF0) step = 4; else if (c >= 0xE0) step = 3; else if (c >= 0xC0) step = 2;
        if (len + step >= sizeof(line)) break;
        memcpy(line, p, len + step);
        line[len + step] = '\0';
        if (panel.textWidth(line) > max_w) break;
        len += step;
        fit = len;
      }
      if (fit == 0) break;
      memcpy(line, p, fit);
      line[fit] = '\0';
      panel.drawString(line, 20, y);
      p += fit;
      y += 36;
    }
  }

  /* 右: 受信の様子 */
  const int16_t rx = PANEL_SPLIT + 16;
  const int16_t rw = PANEL_W - rx;
  panel.fillRoundRect(rx, 0, rw, PANEL_H, 12, COL_PANEL);
  panel.drawRoundRect(rx, 0, rw, PANEL_H, 12, COL_LINE);
  panel.setFont(&fonts::lgfxJapanGothicP_16);
  panel.setTextColor(0x6F7989u);
  panel.drawString("受信", rx + 20, 16);

  char line1[64];
  char line2[64];
  char line3[64];
  line2[0] = line3[0] = '\0';
  uint32_t color1 = 0xC9D1DCu;
  switch (mode) {
  case MODE_SEARCHING:
    snprintf(line1, sizeof(line1), "キーボードを探しています");
    snprintf(line2, sizeof(line2), "右手の電源と、広告がオンかを確認");
    snprintf(line3, sizeof(line3), "（SCROLL + 右手上段 I で切替）");
    color1 = COL_WARN;
    break;
  case MODE_OFF:
    snprintf(line1, sizeof(line1), "広告がオフになっています");
    snprintf(line2, sizeof(line2), "SCROLL + 右手上段 I でオンに戻せます");
    color1 = COL_WARN;
    break;
  case MODE_MISMATCH:
    snprintf(line1, sizeof(line1), "広告の形式が違います（v%u）", keyboard.seenVersion());
    snprintf(line2, sizeof(line2), "ファームとこのアプリを同じリリースに");
    color1 = COL_WARN;
    break;
  case MODE_LOST:
  case MODE_LIVE:
  default: {
    uint32_t age = keyboard.ageMs();
    if (mode == MODE_LOST) {
      snprintf(line1, sizeof(line1), "途切れて %lu 秒", (unsigned long)(age / 1000));
      color1 = COL_WARN;
    } else {
      snprintf(line1, sizeof(line1), "受信中  %lu 件", (unsigned long)rx_count);
    }
    snprintf(line2, sizeof(line2), "間隔 %lums  前回から %lums",
             (unsigned long)rx_interval_ms, (unsigned long)(age > 99999 ? 99999 : age));
    snprintf(line3, sizeof(line3), "RSSI %ddBm  ID %02X%s", keyboard.rssi(), keyboard.keyboardId(),
             keyboard.idle() ? "  アイドル" : "");
    break;
  }
  }
  panel.setFont(&fonts::lgfxJapanGothicP_24);
  panel.setTextColor(color1);
  panel.drawString(line1, rx + 20, 48);
  panel.setFont(&fonts::lgfxJapanGothicP_20);
  panel.setTextColor(COL_MUTED);
  panel.drawString(line2, rx + 20, 92);
  panel.drawString(line3, rx + 20, 124);
  panel.setFont(&fonts::lgfxJapanGothicP_16);
  panel.setTextColor(COL_FAINT);
  panel.drawString("レイヤーの反映は最大1秒ほど遅れます", rx + 20, PANEL_H - 34);

  panel.pushSprite(PANEL_X, PANEL_Y);
}

/* ------------------------------------------------------------------ */

static void handleTouch() {
  if (M5.Touch.getCount() == 0) return;
  auto t = M5.Touch.getDetail();
  if (!t.wasPressed()) return;
  if (t.y >= HEADER_H) return;
  for (int l = 0; l < LAYER_COUNT; l++) {
    const int16_t x = TAB_X + l * (TAB_W + TAB_GAP);
    if (t.x >= x && t.x < x + TAB_W) {
      preview_layer = l;
      preview_until_ms = millis() + PREVIEW_MS;
      return;
    }
  }
}

static void onUpdate(CLine46Status &status) {
  const uint32_t now = millis();
  if (rx_count > 0) rx_interval_ms = now - last_rx_ms;
  last_rx_ms = now;
  rx_count++;
  status.printTo(Serial);
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);

  M5.Display.setRotation(SCREEN_ROTATION);
  M5.Display.fillScreen(COL_BG);
  if (M5.Display.width() != SCREEN_W) {
    Serial.printf("画面が %dx%d です。SCREEN_ROTATION を見直してください\n",
                  (int)M5.Display.width(), (int)M5.Display.height());
  }

  keycap.setColorDepth(16);
  keycap.createSprite(KEY, KEY + SHADOW);
  header.setColorDepth(16);
  header.setPsram(true);
  header.createSprite(SCREEN_W, HEADER_H);
  panel.setColorDepth(16);
  panel.setPsram(true);
  panel.createSprite(PANEL_W, PANEL_H);

  keyboard.onUpdate(onUpdate);
  if (!keyboard.begin()) {
    Serial.println("BLE のスキャンを始められませんでした");
  }

#if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
  /* BLE は ESP32-C6 側で動く。受信できないときの切り分け用に、C6 との
   * 通信の版を出しておく（C6 側が古いと BLE が使えないことがある） */
  uint32_t major = 0, minor = 0, patch = 0;
  hostedGetHostVersion(&major, &minor, &patch);
  Serial.printf("ESP-Hosted: P4 側 %lu.%lu.%lu", (unsigned long)major, (unsigned long)minor,
                (unsigned long)patch);
  hostedGetSlaveVersion(&major, &minor, &patch);
  Serial.printf(" / %s 側 %lu.%lu.%lu%s\n", hostedGetSlaveTargetName(), (unsigned long)major,
                (unsigned long)minor, (unsigned long)patch,
                hostedHasUpdate() ? "（C6 側に更新あり）" : "");
#endif
}

void loop() {
  M5.update();
  handleTouch();
  if (preview_layer >= 0 && (int32_t)(millis() - preview_until_ms) >= 0) {
    preview_layer = -1;
  }

  const bool updated = keyboard.poll();
  const Mode mode = currentMode();
  const int layer = displayedLayer();
  const uint32_t now = millis();

  if (layer != shown_layer) {
    drawHeader(layer, mode);
    drawKeyboard(layer);
    drawPanel(layer, mode);
    shown_layer = layer;
    shown_mode = mode;
    last_status_draw_ms = now;
  } else if (updated || mode != shown_mode || now - last_status_draw_ms >= STATUS_REDRAW_MS) {
    drawHeader(layer, mode);
    drawPanel(layer, mode);
    shown_mode = mode;
    last_status_draw_ms = now;
  }

  delay(5);
}

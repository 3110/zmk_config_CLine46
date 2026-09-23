/*
 * 表示用のキーマップ。config/CLine46.keymap の工場出荷値を写したもの。
 *
 * キーボードの広告にはレイヤー番号しか載っていないので、各キーに何が
 * 割り当たっているかはこちらで持っている。DYA Studio でキーマップを
 * 変えた場合は、この表も合わせて直すこと（将来は自動生成にする予定）。
 *
 * 凡例は keymap-drawer（keymap_drawer.config.yaml）と同じく US 配列の
 * 意味で書いている。
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

namespace keymap {

static const uint8_t KEY_COUNT = 46;
static const uint8_t LAYER_COUNT = 4;

struct Key {
  /* 表に出す文字。nullptr は &trans（下のレイヤーの割り当てを使う） */
  const char *tap;
  /* ホールド時の動作（ホールドタップのキーだけ）。nullptr なら無し */
  const char *hold;
  /* 押している間に有効になるレイヤー（&lt / &mo）。-1 なら無し */
  int8_t layer;
};

#define K(t) {t, nullptr, -1}
#define T {nullptr, nullptr, -1}
#define LT(t, h, l) {t, h, l}

/* 並びは .keymap と同じ（左上から右へ、上段・中段・下段各12キー、親指10キー） */
static const Key MAP[LAYER_COUNT][KEY_COUNT] = {
  /* 0: BASE */
  {
    K("Tab"),   K("Q"), K("W"), K("E"), K("R"), K("T"),     K("Y"), K("U"), K("I"), K("O"), K("P"), K("["),
    K("Ctrl"),  K("A"), K("S"), K("D"), K("F"), K("G"),     K("H"), K("J"), K("K"), K("L"), K("-"), K("]"),
    K("Shift"), K("Z"), K("X"), K("C"), K("V"), K("B"),     K("N"), K("M"), K(","), K("."), LT("SCROLL", "押す間", 3), K("Shift"),
    K("Esc"), K("GUI"), K("Alt"), LT("無変換", "MOUSE", 2), LT("Space", "SYMBOL", 1), LT("変換", "Shift", -1),
    K("Enter"), K("BS"), K("9"), K("Del"),
  },
  /* 1: SYMBOL（Space 長押し） */
  {
    K("^"), K("="), K("7"), K("8"), K("9"), K("@"),     K("("), K("\""), K(")"), K(";"), K("!"), K("?"),
    K("-"), K("+"), K("4"), K("5"), K("6"), K("#"),     K("["), K("'"),  K("]"), K(":"), K("&"), K("|"),
    K("/"), K("*"), K("1"), K("2"), K("3"), K("_"),     K("/"), K("\\"), K(","), K("."), K("$"), K("%"),
    K("Esc"), K("0"), K("."), T, T, T,
    K("Enter"), K("BS"), K("Del"), T,
  },
  /* 2: MOUSE（無変換 長押し） */
  {
    T, T, K("F7"), K("F8"), K("F9"), K("F10"),     K("Ctrl+Y"), K("Ctrl+C"), K("Ctrl+V"), K("Ctrl+X"), K("Ctrl+P"), T,
    T, T, K("F4"), K("F5"), K("F6"), K("F11"),     K("Ctrl+Z"), K("左クリック"), K("↑"), K("右クリック"), K("Ctrl+F"), T,
    T, T, K("F1"), K("F2"), K("F3"), K("F12"),     K("Ctrl+A"), K("←"), K("↓"), K("→"), T, T,
    T, T, T, T, T, T,
    K("Enter"), K("BS"), K("Home"), K("End"),
  },
  /* 3: SCROLL（右手 . の右隣を押している間） */
  {
    K("BT消去"), T, T, T, K("RESET"), K("BOOT"),     K("BOOT"), K("RESET"), K("広告切替"), T, T, T,
    K("BT全消去"), T, K("BT 4"), T, T, T,            T, T, T, T, T, T,
    T, T, K("BT 1"), K("BT 2"), K("BT 3"), T,        T, T, T, T, T, T,
    T, K("BT 0"), T, T, T, T,
    K("Studio解除"), T, T, T,
  },
};

#undef K
#undef T
#undef LT

/* 物理配置（boards/shields/CLine46/CLine46.dtsi）。単位はキー1つ分。
 * 上3段は左6キー・右6キーで、右手は x=9 から始まる */
static inline float keyX(uint8_t i) {
  static const uint8_t BOTTOM_X[10] = {1, 2, 3, 4, 5, 6, 8, 9, 12, 13};
  if (i < 36) {
    uint8_t c = i % 12;
    return c < 6 ? c : c + 3;
  }
  return BOTTOM_X[i - 36];
}
static inline uint8_t keyY(uint8_t i) { return i < 36 ? i / 12 : 3; }

/* トラックボール（CLine46_R.overlay の trackball_layout: x=1050, y=300） */
static const float BALL_X = 10.5f;
static const uint8_t BALL_Y = 3;

struct LayerInfo {
  const char *name;
  uint32_t color; /* RGB888 */
  /* そのレイヤーを有効にしているキーの位置。-1 なら無し */
  int8_t activator;
  const char *how;  /* 下の欄に出す説明 */
};

static const LayerInfo LAYERS[LAYER_COUNT] = {
  {"BASE", 0x9DB4D3u, -1,
   "無変換 長押し → MOUSE ／ Space 長押し → SYMBOL ／ 右手 . の右隣を押す間 → SCROLL"},
  {"SYMBOL", 0xF4B942u, 40,
   "Space を押している間だけ有効。数字は左手に 7 8 9 / 4 5 6 / 1 2 3 のテンキー並び、0 は GUI の位置"},
  {"MOUSE", 0x3FC1B0u, 39,
   "無変換を押している間だけ有効。K の位置が ↑、M , . が ← ↓ →。左手は F1〜F12"},
  {"SCROLL", 0xF2796Bu, 34,
   "トラックボールがスクロールに変わる。BT の切替は左手、RESET と BOOT は押した側の半分だけに効く"},
};

/* 指定したレイヤーで position i に効く割り当てを探す。&trans は下へたどる。
 * from_layer に実際に割り当てを持っていたレイヤーが入る */
static inline const Key &resolve(uint8_t layer, uint8_t i, uint8_t &from_layer) {
  for (int l = layer; l >= 0; l--) {
    if (MAP[l][i].tap != nullptr) {
      from_layer = (uint8_t)l;
      return MAP[l][i];
    }
  }
  from_layer = 0;
  return MAP[0][i];
}

} // namespace keymap

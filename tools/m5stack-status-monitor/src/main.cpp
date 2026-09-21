/* CLine46Status ライブラリの examples/SerialMonitor と同じ内容（PlatformIO 用）。*/
/*
 * CLine46 の状態をシリアルに出すだけの例。
 *
 * 画面を使わないので、BLE が載っている M5Stack ならどの機種でも動く。
 * 自分の機種の画面に出したいときは、printTo() の代わりに keyboard の
 * 各アクセサ（layerName() / centralMv() / studioUnlocked() など）を使う。
 *
 * SPDX-License-Identifier: MIT
 */

#include <CLine46Status.h>

CLine46Status keyboard;

static void onUpdate(CLine46Status &status) { status.printTo(Serial); }

static void onLost(CLine46Status &status) {
  Serial.println("--- 受信が途切れました（スリープ中か圏外）---");
}

void setup() {
  Serial.begin(115200);
  delay(500); /* USB シリアルが繋がるまでの猶予 */

  Serial.println();
  Serial.println("CLine46 ステータスモニタ");

  keyboard.onUpdate(onUpdate);
  keyboard.onLost(onLost);

  if (!keyboard.begin()) {
    Serial.println("BLE の初期化に失敗しました");
    return;
  }
  Serial.println("広告を待っています…");
}

void loop() {
  keyboard.poll();

  /* 形式が食い違っていたら一度だけ知らせる */
  static bool warned = false;
  if (!warned && keyboard.versionMismatch()) {
    warned = true;
    Serial.printf("[警告] 広告の形式が違います（キーボード: version %u / ライブラリ: %u）。"
                  "ライブラリの src/cline46/status_adv.h を更新してください\n",
                  keyboard.seenVersion(), CLine46Status::expectedVersion());
  }

  delay(50);
}

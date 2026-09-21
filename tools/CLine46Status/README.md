# CLine46Status

CLine46 キーボードが BLE 広告で流す状態を受信する Arduino ライブラリです。
**接続もペアリングもしません**（広告を聞くだけ）。キーボードと PC の接続や
DYA Studio の動作には影響しません。

**表示は含んでいません。** 機種ごとに違う画面まわりは、このライブラリの
アクセサを使って自分で書く前提です（シリアルに出すだけの例は
`examples/SerialMonitor`）。

対応: ESP32 系（M5Stack 各機種）/ NimBLE-Arduino 2.x（1.4 系でも動きます）

## 使い方

```cpp
#include <CLine46Status.h>

CLine46Status keyboard;

void setup() {
  Serial.begin(115200);
  keyboard.begin();
}

void loop() {
  if (keyboard.poll()) {          // 新しい広告を取り込んだら true
    Serial.printf("%s %umV\n", keyboard.layerName(), keyboard.centralMv());
  }
  delay(50);
}
```

BLE のコールバックは NimBLE のタスクから呼ばれますが、**取り込みと通知は
`poll()` の中（＝`loop()` の文脈）で行います**。画面描画をコールバックの中で
しても安全なのはこのためです。

## API

| 種類 | メソッド |
|---|---|
| 開始・停止 | `begin()` / `end()` / `poll()` |
| コールバック | `onUpdate(handler)` / `onLost(handler)` / `setTimeout(ms)` |
| 受信状態 | `available()` / `alive()` / `ageMs()` / `rssi()` |
| レイヤー | `layerIndex()` / `layerName()` |
| 電池 | `centralMv()` / `centralPercent()` / `peripheralMv()` / `peripheralPercent()` / `validMv()` / `validPercent()` |
| 接続 | `profileIndex()` / `profileConnected()` / `profileOpen()` / `usbPowered()` / `usbHidReady()` / `outputBle()` / `splitConnected()` |
| OS・レイヤー既定 | `os()` / `osName()` / `defaultLayer()` |
| その他 | `studioUnlocked()` / `idle()` / `uptimeMinutes()` / `resetReason()` / `resetReasonName()` / `incidentCount()` / `keyboardId()` |
| 表示の助け | `flagsText()` / `batteryText()` / `printTo(Print&)` |
| 形式の確認 | `versionMismatch()` / `seenVersion()` / `expectedVersion()` |
| 生データ | `raw()` |

電池が取れていないとき（起動直後・左手未接続）は `centralMv()` が `0`、
`centralPercent()` が `0xFF` になります。判定には `validMv()` / `validPercent()`
を使ってください。`defaultLayer()` は未設定なら `-1` を返します。

## 導入

### Arduino IDE

1. ライブラリマネージャで **NimBLE-Arduino** を入れる
2. この `CLine46Status` フォルダを `~/Documents/Arduino/libraries/` にコピー
   （またはシンボリックリンク）
3. **ファイル → スケッチ例 → CLine46Status → SerialMonitor** で例を開く

### PlatformIO

```ini
lib_deps =
    h2zero/NimBLE-Arduino@^2.5.1
    symlink://../CLine46Status   ; パスは自分のプロジェクトからの相対
```

このリポジトリには、そのまま使えるプロジェクトが2つあります。

| プロジェクト | 内容 |
|---|---|
| [../m5stack-status-monitor](../m5stack-status-monitor) | シリアルに出すだけ（機種を選ばない） |
| [../atoms3r-status-display](../atoms3r-status-display) | AtomS3R / AtomS3 の画面に出す |

同じものが `examples/SerialMonitor` と `examples/AtomS3RHome` に入っています
（Arduino IDE 用）。

## ファーム側との同期

`src/cline46/status_adv.h` は**ファーム側 `include/cline46/status_adv.h` の
コピー**です。ファーム側を変えたら、このファイルも差し替えてください。

```sh
cp include/cline46/status_adv.h tools/CLine46Status/src/cline46/status_adv.h
```

食い違ったまま動かすと、`versionMismatch()` が `true` になり、受信データは
取り込まれません（古い形式を誤って読まないため）。

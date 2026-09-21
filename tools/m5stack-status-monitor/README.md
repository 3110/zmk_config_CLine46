# CLine46 ステータスモニタ（M5Stack / シリアル出力）

CLine46 が流している BLE 広告を受信して、レイヤーや電池残量などをシリアルに出す
スケッチです。**接続もペアリングもしない**ので、キーボードと PC の接続には
一切影響しません。

画面を使わないため、**BLE が載っている M5Stack ならどれでも動きます**
（Basic / Gray / Core2 / CoreS3 / StickC / StickC Plus / Atom / AtomS3 など）。

前提として、キーボード側に `CONFIG_CLINE46_STATUS_ADV=y` のファームを書き込んでおきます
(→ [../../docs/status-advertisement.md](../../docs/status-advertisement.md))。

## 出力例

```
CLine46 ステータスモニタ
NimBLE-Arduino 2.x/3.x 系で動作
広告を待っています…
--- キーボードを見つけました ---
L0:BASE  右 1284mV/72%  左 1301mV/78%  UH-L--  OS:macOS(既定4)  prof:0接続  稼働37min  前回:電源投入  RSSI:-54dBm  ID:9C
L1:SYMB  右 1284mV/72%  左 1301mV/78%  UH-L--  OS:macOS(既定4)  prof:0接続  稼働37min  前回:電源投入  RSSI:-56dBm  ID:9C
```

フラグは `USB給電 / USB HID ready / 出力先BLE / Studioロック解除中 / 左手接続 / アイドル`
の順で、立っていれば `U H B S L I`、落ちていれば `-` が出ます。上の例の `UH-L--` は
「USB 給電あり・HID 有効・出力先は USB・Studio はロック中・左手と接続済み・操作中」です。

電池が `----mV` や `--%` になるのは、まだ測定前（起動直後）か左手が切れているときです。
キーボードは電池を **60秒ごと**にしか測らないので、電圧の更新もその周期です。

## ビルドと書き込み

### Arduino IDE

1. ボードマネージャに **esp32**（Espressif Systems）を入れる
2. ライブラリマネージャで **NimBLE-Arduino** を入れる（2.x 推奨。1.4 系でも動きます）
3. このフォルダごと開いて（`m5stack-status-monitor.ino`）、機種に合うボードを選ぶ
4. **S3 系（AtomS3 / CoreS3 / StickC Plus2）は「USB CDC On Boot」を Enabled** にする
   （シリアルモニタに何も出ない場合はここを疑ってください）
5. 書き込んで、シリアルモニタを **115200 bps** で開く

### PlatformIO

```
cd tools/m5stack-status-monitor
pio run -e m5stick-c -t upload -t monitor      # 機種に合わせて -e を変える
```

## 中身

| ファイル | 内容 |
|---|---|
| `m5stack-status-monitor.ino` | スキャンと表示。NimBLE 1.4 系 / 2.x 系の API 差は `CL_NIMBLE_V2` で吸収 |
| `status_adv.h` | **`include/cline46/status_adv.h` のコピー**。ファーム側を変えたらここも更新する |
| `platformio.ini` | PlatformIO 用。Arduino IDE では不要 |

`status_adv.h` にはペイロードの構造体が入っているだけなので、受信した 24 バイトを
そのまま `memcpy` して読んでいます。フィルタは company ID `0xFFFF` とマジック `"CL"`、
それに `version` です。**アドレス（MAC）では絞れません** — 非接続広告なので
アドレスは定期的に変わります。

キーボードを複数台持っている場合は `keyboard_id`（個体ごとに固定）で区別できます。

## つまずきやすいところ

- **何も出ない**: キーボードがスリープしていないか確認してください（広告は操作中1秒・
  アイドル10秒ごと、ディープスリープ中は停止）。S3 系なら「USB CDC On Boot」も確認
- **一度出たきり更新されない**: NimBLE の重複フィルタが効いています。このスケッチでは
  `setDuplicateFilter(false)` と `wantDuplicates=true` で無効化済みですが、
  自分で書き換えるときは注意してください
- **`[警告] 広告の形式が違います`**: ファームとスケッチで `status_adv.h` の version が
  食い違っています。ファーム側のヘッダをこのフォルダにコピーし直してください

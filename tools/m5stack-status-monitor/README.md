# CLine46 ステータスモニタ（シリアル出力）

CLine46 が流している BLE 広告を受信して、レイヤーや電池残量などをシリアルに
出すだけの PlatformIO プロジェクトです。画面を使わないので、**BLE が載っている
M5Stack ならどれでも動きます**。

受信そのものは [../CLine46Status](../CLine46Status) のライブラリが行います。
画面に出したい場合は、このプロジェクトをコピーしてライブラリのアクセサから
自分の機種向けに描いてください。

前提として、キーボード側に `CONFIG_CLINE46_STATUS_ADV=y` のファームを
書き込んでおきます(→ [../../docs/status-advertisement.md](../../docs/status-advertisement.md))。

## 出力例

```
CLine46 ステータスモニタ
広告を待っています…
L0:BASE  右 1284mV/72%  左 1301mV/78%  UH-L--  OS:macOS(既定4)  prof:0接続  稼働37min  前回:電源投入  RSSI:-54dBm  ID:9C
L1:SYMB  右 1284mV/72%  左 1301mV/78%  UH-L--  OS:macOS(既定4)  prof:0接続  稼働37min  前回:電源投入  RSSI:-56dBm  ID:9C
```

フラグは `USB給電 / USB HID ready / 出力先BLE / Studioロック解除中 / 左手接続 / アイドル`
の順で、立っていれば `U H B S L I`、落ちていれば `-` です。

電池が `----mV` や `--%` になるのは、まだ測定前（起動直後）か左手が切れているとき。
キーボードは電池を **60秒ごと**にしか測らないので、電圧の更新もその周期です。

## ビルドと書き込み

機種ごとの設定は [m5stack-platformio-config](https://github.com/3110/m5stack-platformio-config)
を submodule として使っています。**初回は submodule の取得が必要**です。

```sh
git submodule update --init tools/config

cd tools/m5stack-status-monitor
pio run -e m5stack-stop-watch                      # ビルドだけ
pio run -e m5stack-stop-watch -t upload -t monitor # 書き込んでシリアルを開く
```

用意してある env:

| env | 機種 |
|---|---|
| `m5stack-stop-watch` | M5Stack StopWatch |
| `m5stack-basic-4MB` | M5Stack Basic |
| `m5stack-core2` | M5Stack Core2 |
| `m5stack-cores3` | M5Stack CoreS3 |
| `m5stick-c` | M5StickC |
| `m5stick-c-plus2` | M5StickC Plus2 |
| `m5stack-atoms3` | ATOMS3 |

他の機種を足すときは、共通 ini（`../config/platformio-m5stack.ini`）にある
セクション名をそのまま `extends` に書きます。

```ini
[env:m5stack-cardputer]
extends = m5stack-cardputer
```

共通設定を更新するには `git submodule update --remote tools/config`。

### Arduino IDE で使う場合

PlatformIO を使わない場合は、`../CLine46Status` を `~/Documents/Arduino/libraries/`
に置いて、**ファイル → スケッチ例 → CLine46Status → SerialMonitor** を開いてください
（このプロジェクトの `src/main.cpp` と同じ内容です）。
S3 系の機種では「USB CDC On Boot」を Enabled にしないとシリアルに何も出ません。

## 構成

| パス | 内容 |
|---|---|
| `platformio.ini` | プロジェクト固有の設定。機種は共通 ini の `extends` だけ |
| `../config/` | submodule（機種ごとの設定の共通リポジトリ） |
| `src/main.cpp` | ライブラリを呼ぶだけの本体 |

## つまずきやすいところ

- **何も出ない**: キーボードがスリープしていないか確認してください（広告は操作中1秒・
  アイドル10秒ごと、ディープスリープ中は停止）。S3 系なら「USB CDC On Boot」も確認
- **`[警告] 広告の形式が違います`**: ファームとライブラリで `status_adv.h` の version が
  食い違っています。ファーム側のヘッダを
  `tools/CLine46Status/src/cline46/status_adv.h` にコピーし直してください

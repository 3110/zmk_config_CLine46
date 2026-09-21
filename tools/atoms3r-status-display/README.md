# CLine46 ステータス表示（M5AtomS3R / AtomS3）

128×128 の画面に、CLine46 の**レイヤーと電池**を出すホーム画面です。
受信は [../CLine46Status](../CLine46Status) のライブラリが行い、このプロジェクトは
描画だけを持ちます。接続もペアリングもしないので、キーボードと PC の接続には
影響しません。

```
┌──────────────────┐
│ BT0        macOS │ 出力先（BT番号 / USB）と OS 判別結果
│                  │
│      SYMB        │ レイヤー名（特大・レイヤーごとに色分け）
│                  │
│ R ███████░ 1.28V │ 右手：残量バー＋電圧
│ L █████░░░ 1.23V │ 左手：同上（切断中は -- OFF --）
│ 3d 04h    !2   · │ 稼働時間／watchdog 記録／受信インジケータ
└──────────────────┘
```

| 表示 | 意味 |
|---|---|
| レイヤー名の色 | BASE=白 / SYMBOL=シアン / MOUSE=黄 / SCROLL=マゼンタ / 予約=灰 |
| 電池の色 | 1.10V 未満で橙、1.05V 未満で赤（`CONFIG_ZMK_NON_LIPO_LOW_MV=1000` で電源が落ちます） |
| `-- OFF --` | 左手が切れている |
| `!2` | watchdog に記録が2件ある（Troubleshooting タブで中身を確認） |
| 右下の点 | 広告を受けるたびに点滅。止まれば受信が途切れています |
| 赤帯 `STUDIO UNLOCKED` | Studio のロックが解除されたまま（設定変更を受け付ける状態） |
| `NO SIGNAL` | 受信が途切れた。最後に受けてからの秒数も出ます |

電池が `--` になるのは、キーボードが起動してから最初の測定（60秒ごと）が
終わるまでの間です。

## 操作

| 操作 | 動作 |
|---|---|
| 画面を短押し | 明るさ切替（3段階）。消灯中なら点灯 |
| 画面を長押し | 画面オフ／オン |

キーボードがアイドルに入ると、画面も自動的に暗くなります。

## ビルドと書き込み

```sh
git submodule update --init tools/config     # 初回のみ

cd tools/atoms3r-status-display
pio run -e m5stack-atoms3r -t upload -t monitor
```

AtomS3（無印）は `-e m5stack-atoms3` を使います。画面サイズは同じ 128×128 なので
表示は変わりません。

### Arduino IDE で使う場合

`../CLine46Status` を `~/Documents/Arduino/libraries/` に置いて、
**ファイル → スケッチ例 → CLine46Status → AtomS3RHome** を開きます
（このプロジェクトの `src/main.cpp` はそれを include しているだけです）。
M5Unified と NimBLE-Arduino もライブラリマネージャから入れてください。
ボード設定で「USB CDC On Boot」を Enabled にします。

## 構成

| パス | 内容 |
|---|---|
| `platformio.ini` | 機種は共通 ini（`../config/`）の `m5stack-atoms3r` と `m5unified` を extends |
| `src/main.cpp` | ライブラリのサンプルを include するだけ |
| `../CLine46Status/examples/AtomS3RHome/` | **画面の実装本体** |

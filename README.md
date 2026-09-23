# CLine46 ZMK ファームウェア

トラックボール付き左右分割キーボード **CLine46** のファームウェアと、
キーボードの状態を手元の M5Stack に映すツール一式です。

ビルド済みのファームウェアは [Releases](https://github.com/3110/zmk_config_CLine46/releases/latest)
に置いてあるので、**uf2 をコピーするだけで使い始められます**。

![keymap](keymap-drawer/CLine46.svg)

## できること

### ファームを焼き直さずに設定を変えられる

[DYA Studio](https://studio.dya.cormoran.works)（ブラウザまたはデスクトップアプリ）から、
**キー割り当て・レイヤー・マクロ・コンボ・トラックボールの感度や CPI・BLE プロファイル・
OS ごとの既定レイヤー**まで変更できます。変更はその場で効き、保存すれば再起動しても残ります。
ビルド待ちも書き込みもありません。

### 右手のトラックボールがそのまま使える

PMW3610 トラックボールを右手側に搭載。SCROLL レイヤーを押している間は**スクロールに変わります**。
感度・回転・軸の反転は Studio から調整できます。

### キーボードの状態が手元の画面で見える

右手が、**レイヤー名・左右の電池電圧・接続先・OS 判別結果・稼働時間**などを
BLE の広告に載せて流します。受信側（M5Stack）は**スキャンするだけ**でよく、接続も
ペアリングも要りません。BLE プロファイルを消費しないので、PC との接続や DYA Studio の
動作には一切影響しません。使わないときはキー1つで止められます。

AtomS3R に出る画面です。上から出力先と OS、レイヤー名、左右の電池、電圧、稼働時間。
`!2` は watchdog に残っている記録の件数、右下の `*` は広告を受けるたびに点滅する受信インジケータです。

```
+------------------------+
| BT0              macOS |
|          SYMB          |
| L[###.]62%  R[####]75% |
|   1.23V       1.28V    |
| 3d 04h     !2        * |
+------------------------+
```

### 落ちた理由が残る

watchdog を左右に入れてあるので、フリーズや再起動の記録が残ります。
前回のリセット理由は状態表示にも出るので、「いつのまにか再起動していた」に気づけます。

## 必要なもの

| | |
|---|---|
| キーボード | CLine46 本体（Seeed XIAO BLE ×2、右手に PMW3610 トラックボール、電池は NiMH 単セル） |
| 設定を変えるとき | Chrome / Edge、または DYA Studio のデスクトップ版（macOS / Windows） |
| 状態表示（任意） | **M5Stack AtomS3R / AtomS3**（画面に出す）、または BLE 付きの M5Stack 各機種（シリアルに出す） |

状態表示を使わなくても、キーボードは普通に使えます。

## 導入

### 1. ファームウェアを書き込む

1. [Releases](https://github.com/3110/zmk_config_CLine46/releases/latest) から
   `CLine46_R.uf2`（右手）と `CLine46_L.uf2`（左手）をダウンロードする
2. XIAO BLE の**リセットボタンを素早く2回**押す。`XIAO-SENSE` ドライブがマウントされる
3. それぞれの uf2 をドライブにコピーする。書き込むと自動で再起動します

**右手が Central（親機）です。** PC とつなぐのも、Studio の窓口も、状態広告を出すのも右手で、
左手の情報は右手経由で流れます。

> **初めて書き込むとき**、または**ペアリングがおかしいとき**は、先に `settings_reset.uf2` を
> 左右の両方に書き込んでから、上の手順で焼き直してください。

> **すでに DYA Studio でキーマップを編集している場合**は、書き込んだあと Studio で
> **Restore Stock Settings** を実行してください。Studio の編集内容は設定領域に保存され、
> ファームウェアに焼き込まれたキーマップより優先されるため、実行しないと新しいキーマップが
> 反映されません。**Studio で作ったマクロはこの操作で消えます。**

### 2. PC とつなぐ

右手を USB でつなぐか、BLE でペアリングします。BLE のプロファイルは5個あり、
SCROLL レイヤー（右手 `,`/`.` の隣を押しっぱなし）を押しながら切り替えます。

| SCROLL + | 動作 |
|---|---|
| 左手 親指 `LGUI` の位置 | プロファイル 0 を選ぶ |
| 左手 `X` / `C` / `V` | プロファイル 1 / 2 / 3 を選ぶ |
| 左手 `S` | プロファイル 4 を選ぶ |
| 左手 `TAB` | 今のプロファイルのペアリングを消す |
| 左手 `LCTRL` | 全プロファイルのペアリングを消す |

USB と BLE のどちらに出力するかは、DYA Studio の Connection タブから切り替えられます。

### 3. DYA Studio で設定を変える

1. キーボードで **`&studio_unlock`**（SCROLL + 右手親指の `RET`）を押してロックを解除する
2. [DYA Studio](https://studio.dya.cormoran.works) を Chrome / Edge で開く
   （またはデスクトップアプリを起動する）
3. USB なら **Serial**、無線なら **Bluetooth** を選んで **CLine46** につなぐ

> **BLE でつなぐときは USB ケーブルを抜いてください。** USB がつながっていると
> 応答が返らず `Connection timed out` になります。理由と回避策は
> [DYA Studio の使い方](docs/dya-studio.md#ble-で接続する)にあります。

読むだけならロックされたままでもほぼ見えますが、**編集にはロック解除が必要**です。
何ができるかは [DYA Studio の使い方](docs/dya-studio.md) にまとめてあります。

### 4. M5Stack で状態を表示する（任意）

画面に出すなら **AtomS3R / AtomS3**、シリアルに出すだけならどの M5Stack でも動きます。

```sh
git clone https://github.com/3110/zmk_config_CLine46.git
cd zmk_config_CLine46
git submodule update --init tools/config

# 画面に出す（AtomS3R / AtomS3）
cd tools/atoms3r-status-display
pio run -e m5stack-atoms3r -t upload -t monitor

# キーマップを出す（Tab5）
cd tools/tab5-keymap-viewer
pio run -e m5stack-tab5 -t upload -t monitor
```

シリアルに出すだけなら [tools/m5stack-status-monitor](tools/m5stack-status-monitor) を使います
（M5Stack Basic / Core2 / CoreS3 / StickC など、対応 env が一通り用意してあります）。
PlatformIO を使わず Arduino IDE から入れる手順は各プロジェクトの README にあります。

受信部分は Arduino ライブラリ [tools/CLine46Status](tools/CLine46Status) に切り出してあるので、
**別の機種の画面も自分で書けます**。

## 使い方

### レイヤー

| # | 名前 | 呼び出し方 | 内容 |
|---|---|---|---|
| 0 | BASE | — | 通常のキー入力 |
| 1 | SYMBOL | 右親指 `SPACE` をホールド | 数字・記号 |
| 2 | MOUSE | 左親指 `無変換` をホールド | F1〜F12、マウスクリック、矢印、Ctrl 系ショートカット |
| 3 | SCROLL | 右手 `,`/`.` の隣をホールド | **トラックボールがスクロールに変化**。BT プロファイル切替、リセット、状態広告の切替、Studio 解除 |
| 4-6 | （予約） | — | 空き。DYA Studio から自由に使えます |

### よく使うキー

| キー | 動作 |
|---|---|
| 左親指 `無変換` | タップ = 無変換 / ホールド = MOUSE レイヤー |
| 右親指 `SPACE` | タップ = SPACE / ホールド = SYMBOL レイヤー |
| 右親指 `変換` | タップ = 変換 / ホールド = 左 Shift |
| `Q` + `W` 同時押し | Tab |
| `W` + `E` 同時押し | Shift + Tab |
| SCROLL + 右手親指 `RET` | ZMK Studio のロック解除 |
| SCROLL + 右手上段 `I` | **状態広告のオン/オフ** |
| SCROLL + `T` / `Y` | ブートローダーに入る（**押した側の半分だけ**） |
| SCROLL + `R` / `U` | 再起動 |

同時押し（コンボ）は誤爆しないよう、直前 125ms 打鍵していないときだけ効きます。
コンボは DYA Studio から追加・変更できます。

> ブートローダーが**押した側だけ**に効くので、左手を焼くのに本体のリセットボタンを
> 探さなくて済みます。

### 状態表示を止める / 再開する

SCROLL レイヤーで右手上段の `I` を押すたびに切り替わります。
**切り替えた状態は保存され、次の起動でも引き継ぎます。**

止めると M5Stack 側は待たされることなく橙の電源マークの画面に変わります
（止める直前に「これから止める」合図を流しているため）。もう一度押せば数秒で戻ります。

止める理由は電池ではありません（差はごくわずかです）。**使わないときに平文の広告を
出しっぱなしにしない**ためのものです。

### 画面の見かた（AtomS3R / AtomS3）

画面を**短押しで3画面を切り替え**、**長押しで明るさを一段下げ**ます。

| 画面 | 内容 |
|---|---|
| HOME | レイヤー名（色分け）、左右の電池（アイコン・%・電圧）、出力先、OS、稼働時間 |
| CONNECTION | プロファイル、出力先、OS、既定レイヤー、左手の接続、Studio のロック状態、電波強度 |
| HEALTH | 稼働時間、前回のリセット理由、watchdog の記録件数、電圧の推移グラフ（約2時間） |

赤帯の `STUDIO UNLOCKED` は、**設定変更を受け付ける状態のまま**という意味です。
詳しい表示の意味は [tools/atoms3r-status-display](tools/atoms3r-status-display) にあります。

## 困ったときは

| 症状 | 対処 |
|---|---|
| DYA Studio につながらない（BLE） | **USB を抜いて**から `&studio_unlock` を押し直して接続します（[理由](docs/dya-studio.md#ble-で接続する)） |
| キーマップを変えたのに反映されない | 書き込み後に Studio で **Restore Stock Settings** を実行します（Studio の設定が優先されるため） |
| M5Stack に何も出ない | キーボードがディープスリープ（2時間）に入っていないか、状態広告をオフにしていないか確認します。S3 系は「USB CDC On Boot」も確認 |
| M5Stack に「広告の形式が違います」と出る | ファームウェアと受信側ライブラリのバージョン違いです。両方を同じリリースに揃えてください |
| 電池が `--` のまま | 起動後の最初の測定（60秒ごと）が終わるまでの間はこうなります |
| ペアリングがおかしい | 左右に `settings_reset.uf2` を書き込んでから焼き直します |
| 左手がつながらない | 左手の電池と、右手が起動しているかを確認します。左手は右手にしかつながりません |

うまくいかないときは [既知の事項](docs/known-issues.md) も確認してください。

## もっと詳しく

| ドキュメント | 内容 |
|---|---|
| [DYA Studio の使い方](docs/dya-studio.md) | タブごとにできること、接続の作法、マクロとコンボの作り方、設定の保存と初期化、OS ごとの既定レイヤー |
| [状態広告の仕様](docs/status-advertisement.md) | BLE 広告のバイト配置、受信側の実装、キーによるオン/オフの仕組み |
| [ビルドとリリース](docs/build.md) | ブランチ運用、GitHub Actions でのビルド、ローカルビルド、リリースの作り方 |
| [実装とリポジトリ構成](docs/internals.md) | ファイル構成、モジュール構成、設定値のカスタマイズ、fork 元との違い |
| [既知の事項](docs/known-issues.md) | 制限、回避策、意図的にそうしている点 |
| [リリースノート](docs/release-notes/) | バージョンごとの変更点 |

受信側のツール:

| プロジェクト | 内容 |
|---|---|
| [tools/CLine46Status](tools/CLine46Status) | 受信用の Arduino ライブラリ（表示は含まない） |
| [tools/atoms3r-status-display](tools/atoms3r-status-display) | AtomS3R / AtomS3 の画面に出す |
| [tools/m5stack-status-monitor](tools/m5stack-status-monitor) | シリアルに出す（機種を選ばない） |
| [tools/tab5-keymap-viewer](tools/tab5-keymap-viewer) | Tab5 の画面に今のレイヤーのキーマップを出す（配置を覚える用） |

## このリポジトリについて

[takamaru-fpv/zmk_config_CLine46](https://github.com/takamaru-fpv/zmk_config_CLine46) からの
fork です（変更点は[実装とリポジトリ構成](docs/internals.md#fork-元との違い)）。
ZMK は DYA Studio 対応の [cormoran/zmk](https://github.com/cormoran/zmk) をベースにしています。

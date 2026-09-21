# zmk_config_CLine46

分割キーボード **CLine46** 用の ZMK ファームウェア設定(個人用)。

[takamaru-fpv/zmk_config_CLine46](https://github.com/takamaru-fpv/zmk_config_CLine46) からの fork です。

## 構成

| 項目 | 内容 |
|---|---|
| キー数 | 46(左右23キーずつ) |
| コントローラ | Seeed XIAO BLE (nRF52840) ×2 |
| キースキャン | Charlieplex 方式(D2〜D7、割り込み D1) |
| ポインティング | PMW3610 トラックボール(右手側、SPI0 / CS=P0.09 / IRQ=D8)。ドライバは [cormoran 版](https://github.com/cormoran/zmk-driver-pmw3610-with-custom-studio-rpc)(Studio RPC 付き) |
| 電池 | NiMH 単セル(1.02〜1.36V) |
| Central | **右手**(トラックボール側) |
| ZMK Studio | 対応(DYA Studio 拡張込み)。マクロ・コンボ・接続・診断まで編集可([一覧](#dya-studio-でできること)) |
| DYA Studio | `v2026.09.20.0` で確認([このバージョンで増えたこと](#新バージョンで増えたこと)) |
| ベース | [cormoran/zmk](https://github.com/cormoran/zmk) `main+dya` @ `e5c9b69`(Zephyr 4.1) |

## キーマップ

![keymap](keymap-drawer/CLine46.svg)

> 画像は Actions の **Draw Keymap** ワークフローで自動生成されます。

### レイヤー

| # | 名前 | 呼び出し方 | 内容 |
|---|---|---|---|
| 0 | BASE | — | 通常のキー入力 |
| 1 | SYMBOL | 右親指 SPACE をホールド | 数字・記号 |
| 2 | MOUSE | 左親指 無変換 をホールド | F1〜F12、マウスクリック、矢印、Ctrl系ショートカット |
| 3 | SCROLL | 右手 `,`/`.` の隣(位置34)を押しっぱなし | **トラックボールがスクロールに変化**。BT プロファイル切替、リセット、Studio 解除 |
| 4-6 | (予約) | — | `status = "reserved"`。ZMK Studio からのみ利用可能 |

### 主要バインド

| キー | 動作 |
|---|---|
| 左親指 無変換 | タップ=無変換 / ホールド=MOUSE レイヤー |
| 右親指 SPACE | タップ=SPACE / ホールド=SYMBOL レイヤー |
| 右親指 変換 | タップ=変換 / ホールド=左Shift |
| SCROLL + 位置42(RET) | `&studio_unlock`(ZMK Studio のロック解除) |
| SCROLL + 位置5 / 6 | `&bootloader`(**押した側の半分**がブートローダーに入る) |
| SCROLL + 位置4 / 7 | `&sys_reset` |

`&mt` と `&lt` はどちらも `flavor = "balanced"` に設定済み。既定の `tap-preferred` だと 200ms 待たないとレイヤーが有効にならず、MOUSE レイヤーのクリックが機能しないため。

### コンボ

| スロット | キー | 出力 | 位置 |
|---|---|---|---|
| 0 | Q + W | Tab | `<1 2>` |
| 1 | W + E | Shift + Tab | `<2 3>` |

いずれも `require-prior-idle-ms = <125>` 付き(タイピング中の誤爆防止)。

コンボは ZMK 標準の `zmk,combos` ではなく
[zmk-feature-runtime-combo](https://github.com/cormoran/zmk-feature-runtime-combo)
の `cormoran,runtime-combo-defaults` で定義しています。`zmk,combos` は
コンパイル時に固定されて DYA Studio から編集できないためです。

`.keymap` に書いた値は**工場出荷値**の扱いで、DYA Studio で上書きするとそちらが
優先され、**Reset to Default** で元に戻せます。追加のコンボは Studio 側から
空きスロットに作れます(上限は `CONFIG_ZMK_RUNTIME_COMBO_MAX_COMBOS`、既定 8)。

### マクロ

マクロは
[zmk-feature-runtime-macro](https://github.com/cormoran/zmk-feature-runtime-macro)
で実現していて、**すべて DYA Studio 上で作成・編集**します(`.keymap` には
`&rmacro` の定義を include してあるだけで、マクロの中身は書きません)。

1. DYA Studio の **Macro&Combo** タブで名前を付けて作成し、手順を編集する
2. 作成すると**スロット番号**が割り当てられるので、リストで番号を確認する
3. キーマップエディタで対象キーに **Runtime Macro** を選び、その番号を指定する

スロット番号は作成時に決まるもので、事前に選ぶものではありません。既定では
最大8個(`CONFIG_ZMK_RUNTIME_MACRO_COUNT`)、名前と本文の合計で 1024 バイト
(`CONFIG_ZMK_RUNTIME_MACRO_POOL_BYTES`)まで保持できます。

### キーポジション番号

コンボの `key-positions` で使う番号。キーマップの `bindings` の記述順と一致します。

```
 0  1  2  3  4  5   |   6  7  8  9 10 11
12 13 14 15 16 17   |  18 19 20 21 22 23
24 25 26 27 28 29   |  30 31 32 33 34 35
   36 37 38 39 40 41 | 42 43     44 45
```

## DYA Studio でできること

[DYA Studio](https://studio.dya.cormoran.works) は [ZMK Studio](https://zmk.studio/) の代替 Web UI です。
[cormoran](https://github.com/cormoran) 氏の ZMK fork とモジュール群に対応していて、
**ファームを焼き直さずに変えられる範囲が ZMK Studio より広い**のが利点です。
このリポジトリはそれを使う前提の構成にしてあります。

### 接続と前提

- ブラウザは **Chrome / Edge**(Web Serial / Web Bluetooth)。iOS は Bluefy、Android は Chrome(BLE のみ)
- 接続は **USB(右手を挿す)** か **BLE**。窓口は常に**右手**(Central)で、左手の情報は右手経由で中継されます
- 読むだけならロックされたままでも大半は見えますが、**編集には `&studio_unlock`** が必要です
  (SCROLL レイヤー + 位置42。「主要バインド」の表を参照)

#### BLE で接続する

> **USB ケーブルを抜いてから接続してください。**
> ZMK の Studio RPC は**キー入力の出力先(選択中のエンドポイント)と同じ経路にしか応答しません**
> (`zmk/app/src/studio/rpc.c` の `refresh_selected_transport()`)。USB が繋がっていると
> 出力先が USB になり、BLE の GATT に書き込んでも応答が返らず、ブラウザ側は
> **`Connection timed out: the device did not respond.`** で失敗します。
> USB を挿したまま使いたい場合は、Connection タブの**出力優先度を `BLE` に切り替え**ます
> (その間はキー入力も BLE 側に流れます)。

1. USB を抜く(または出力優先度を BLE にする)
2. キーボードで **`&studio_unlock`** を押す — アクティブなプロファイル宛に directed advertising が始まり、Web Bluetooth から見つけられる状態になります
3. DYA Studio のスプラッシュ画面で **Bluetooth** を選び、デバイス選択で **CLine46** を選ぶ

見つからないときは、**もう一度 `&studio_unlock` を押してから**接続し直してください
(アンロック時に広告が始まる仕組みなので、時間が経つと止まります)。
macOS の Chrome でダイアログに何も出ない場合は、システム設定 → プライバシーとセキュリティ →
Bluetooth で Chrome を許可してください。

BLE 接続中は応答を速くするため `CONFIG_ZMK_STUDIO_TRANSPORT_BLE_PREF_LATENCY=0` を指定しています。
既定の 10 のままだと接続間隔 15ms と合わせて 1 往復 165ms かかり、操作がもたつきます。
効くのは Studio が接続されている間だけなので、普段の電池持ちには影響しません。

### タブごとの機能

| タブ | できること | 効かせているモジュール |
|---|---|---|
| **Keymap** | キー割り当て、レイヤーの追加・並べ替え・リネーム。**Stream** スイッチを入れると押したキーがプレビュー上で光る。プレビューにはトラックボールも描かれる | ZMK Studio 標準 + `input-stream` / `fast-keymap` / `physical-layout` |
| **Macro&Combo** | マクロの作成・編集・リネーム・削除、コンボの位置/ビヘイビア/タイムアウト/対象レイヤーの編集 | `runtime-macro` / `runtime-combo` / `custom-settings` |
| **Trackball** | 感度(0.1〜10倍)、回転、軸スナップ、スクロール、オートマウスレイヤー。入力プロセッサ単位・レイヤー単位で指定。**PMW3610 Drivers** の欄では CPI・スマートアルゴリズム・ダウンシフト/サンプル時間・軸反転をセンサーに直接指定できる | `runtime-input-processor` / PMW3610 ドライバ(`custom-settings` 経由) |
| **Connection** | BLE プロファイルの改名・切替・ペアリング解除、USB と BLE の出力優先度、**OS 判別の確認と手動上書き**、**接続先ごと・OS ごとの既定レイヤー** | `ble-management` / `os-detection` / `default-layer` |
| **Settings** | 左右それぞれのアイドル/スリープのタイムアウト、詳細設定 | `settings-rpc`(左手の分は `custom-settings` の中継) |
| **Troubleshooting** | 電池残量、FW のビルド情報と稼働時間、キースイッチ診断(チャタリング)、**watchdog の記録(左右とも)**、**トラックボールのセンサー生画像(22×22)と診断値**、サポートレポートのコピー | `device-info` / `kscan-diagnostics` / `watchdog` / PMW3610 ドライバ |

### 新バージョンで増えたこと

DYA Studio `v2026.09.20.0`(2026-09-20)の変更のうち、この構成に関係するものです。
**いずれもブラウザ/アプリ側だけの変更で、ファームを焼き直す必要はありません。**
プロトコル(`proto/`)は前バージョンから変わっていないので、今のファームのままで全部使えます。

| 増えたこと | 内容 | この構成での扱い |
|---|---|---|
| **デスクトップアプリ** | macOS(`.dmg`)と Windows(`.exe`)版が [GitHub のリリース](https://github.com/cormoran/dya-studio/releases)に付くようになった。USB(シリアル)も BLE もアプリ側のダイアログで選べる | ブラウザ版と同じことができる。Chrome / Edge を開かずに済む |
| **キー編集のフローティングウィンドウ** | ビヘイビア選択がドラッグできる別窓になり、確定すると次のキーへ自動で進む。behavior 検索の改善、クイック選択のカスタマイズも入った | そのまま使える |
| **WebMCP 対応** | Settings タブを開いている間だけ、対応ブラウザの AI 機能(WebMCP クライアント)から電源管理と詳細設定を操作できる。登録されるのは `get_power_management_settings` / `set_power_management_timeouts` / `reset_all_keyboard_settings` / `list_advanced_keyboard_settings` / `set_advanced_keyboard_setting` の5つ | ファーム側は `settings-rpc` と `custom-settings` があれば足りるので、この構成なら追加設定なしで動く。非対応ブラウザでは何も起きない |
| **スマートフォン対応の改善** | スクロール範囲の整理、タッチ操作、縦画面の表示修正。Macro&Combo と Trackball はドロップダウンにまとまる | Android Chrome / iOS Bluefy から BLE で繋いだときに効く |
| **簡体字中国語とブラウザ言語の自動判定** | 英語・日本語・簡体字中国語の切り替え | — |
| ペリフェラル側 PMW3610 の検出 | 分割キーボードの**子機**に付いたセンサーも一覧・診断できるようになった(両手に `CONFIG_ZMK_PMW3610_SPLIT_RPC_RELAY` が要る) | **この構成には不要**。トラックボールは Central(右手)側に1つだけなので、中継せずそのまま見える |
| Demo セッションの復元 | 再読み込みしてもデモ接続とワークスペースが残る | — |

ファーム側では PMW3610 ドライバを cormoran 版へ差し替え(下記)、
`zmk-module-runtime-input-processor` をタグ
`zmk-v0.4.0.0` から `main` に上げました。後者で入力プロセッサ設定の保存先が
[zmk-feature-custom-settings](https://github.com/cormoran/zmk-feature-custom-settings)
になりました。Settings タブの詳細設定にも `cormoran_rip` として出てきます。
ほかのモジュールは 2026-09-20 時点で `main` の先頭と同じ SHA を指しているので、そのままです。

> 保存形式が変わるため、**このファームを焼いた最初の起動でトラックボールの保存済み設定は
> `CLine46_R.overlay` の既定値に戻ります**(スケーリングや回転を Studio で変えていた場合は
> 入れ直してください)。

### トラックボールのセンサー設定(PMW3610 ドライバ)

PMW3610 のドライバは badjeff 版から
[cormoran 版](https://github.com/cormoran/zmk-driver-pmw3610-with-custom-studio-rpc)に
差し替えてあります。センサー自体の設定を**焼き直さずに**変えられるのが目的です。

| 変えられるもの | 場所 |
|---|---|
| CPI(200〜3200、200 刻み)、X/Y 反転、XY 入れ替え、スマートアルゴリズム、常時稼働、ダウンシフト/サンプル時間、最小レポート間隔 | Trackball タブの **PMW3610 Drivers** |
| センサーの生画像(22×22)、製品 ID・リビジョン・SQUAL などの診断値 | Troubleshooting タブ |

- `CLine46_R.conf` の `CONFIG_PMW3610_*` は**工場出荷値**で、DYA Studio で保存した値が
  起動時にその上へ乗ります。戻すときは Studio 側の Reset を使います
- このサブシステムは**ロック対象**です。読むだけの操作でも `&studio_unlock`(SCROLL + 位置42)が
  要ります(生レジスタへの書き込みができるため、モジュール側がそう決めています)
- 保存キーは `CLine46_R.overlay` の `settings-id = "ball"` から `"<項目>@ball"` になります
- 感度(スケーリング)や回転、スクロール化は**今までどおり入力プロセッサ側**(`runtime-input-processor`)の
  担当です。センサーの CPI とは別物なので、混ぜないようにしてください

> 生画像の取得は `&spi0` に `cs-gpios` がある(CS を保持したまま連続読みできる)ことを前提にした
> 速い経路を使います。動きが取りこぼされるようなら、`trackball` ノードに `disable-burst-read;` を
> 足して遅い方の経路に切り替えます。

### 採用を検討できるもの(未採用)

DYA Studio が対応していて、このリポジトリではまだ使っていないものです。

| 候補 | 増えること | 必要な作業・注意 |
|---|---|---|
| [zmk-module-devtool](https://github.com/cormoran/zmk-module-devtool) | 画面右下に **Devtool** の小窓が出て、ファームの Zephyr ログをブラウザで流し読みできる。再起動・ブートローダー移行・ロック操作も窓から叩ける。デバッグプローブ無しで原因を追える | 開発用。イベントタップを有効にすると**打鍵内容を観測できてしまう**ので、常用ファームに入れるならログ取得だけにする。ログのリングバッファでメモリも食う |
| [zmk-feature-zephyr-setting-expose](https://github.com/cormoran/zmk-feature-zephyr-setting-expose) | 保存済み設定(NVS)を左右それぞれ一覧・編集・削除でき、残容量も見える | DYA Studio 本体にはこの画面が無く、モジュール側の別 Web UI を開く形になる |

### このファームではできないこと

| できないこと | 理由 |
|---|---|
| バッテリー履歴のグラフ | 無効にしています(「既知の事項」参照) |
| **左手側**のキー配線の表示 | ファームの中継は完成していますが Web UI 側が未実装。打鍵・チャタリングの統計は左右とも取れます |
| エンコーダーの回転割り当ての変更 | CLine46 にロータリーエンコーダーが載っていないため(`sensor-rotate` は入れていません) |

### プレビューのトラックボール

キーの右下に描かれるトラックボールは `CLine46_R.overlay` の
`trackball_layout` ノードで位置と大きさを決めています。見た目だけの情報で、
動作には影響しません。

| プロパティ | 現在値 | 意味 |
|---|---|---|
| `size` | `100` | 直径。キー1つと同じ大きさ |
| `x` / `y` | `1050` / `300` | 左上の座標。右手親指列の空き(x=1000〜1200)の中央 |

単位はキーの物理レイアウト(`CLine46.dtsi`)と同じで、**キー1つが 100**です。
実物とずれていたらこの3つを調整します。`linked-device-identifiers` は
描いたボールを Trackball タブの設定に結び付けるためのもので、
このキーボードの runtime input processor は `mouse` だけです
(スクロールは `zip_*` で組んでいて runtime input processor ではありません)。

### 保存の考え方

DYA Studio の変更は**2段階**です。ここを取り違えると「設定したのに再起動で消えた」ことになります。

1. **メモリへの書き込み** — 即時反映されるが電源を切ると消える。未保存の項目には緑のドットが付きます
2. **保存** — フラッシュへ永続化。再起動しても残ります

戻すときは3通りあります。

| 操作 | 効果 |
|---|---|
| Discard / 破棄 | 未保存の変更だけを捨てて、保存済みの値に戻す |
| **Reset to Default**(コンボなど項目単位) | その項目だけ `.keymap` に書いた既定値へ戻す |
| **Restore Stock Settings** | 設定領域を全消去してコンパイル時の状態へ。**マクロは設定領域にしか無いので消えます** |

### OS ごとに既定レイヤーを変える

1. Studio のキーマップエディタで、**予約してある 4〜6 のレイヤー**に OS ごとの差分キーを置く
   (レイヤー0は常に有効なので、**変えたいキーだけ**置けば足ります)
2. Connection タブで接続先の「デフォルトレイヤー」を **「OS 検出に従う」** にする
3. 「OS ごとのデフォルトレイヤー」で macOS → 4、Windows → 5 のように割り当てる

OS の判別は USB なら列挙時のやり取り、BLE なら GATT の読まれ方の癖から推定します。
接続のたびに一定時間(USB 200ms / BLE 1000ms)静まってから確定するので、
繋いだ直後の一瞬は unknown 扱いです。外したときは同じ画面で手動上書きできます。

> レイヤー1〜3(SYMBOL / MOUSE / SCROLL)はホールドして使うモーメンタリなレイヤーなので、
> 既定レイヤーには選ばないでください。

## 元リポジトリとの違い

fork 元の [takamaru-fpv/zmk_config_CLine46](https://github.com/takamaru-fpv/zmk_config_CLine46)
から意図的に変えている点です。**上流を取り込むときはここを潰さないよう注意**してください。

### キーマップ

| 違い | 効果・理由 |
|---|---|
| **SCROLL レイヤーに `&bootloader` と `&sys_reset` を配置**(位置4〜7) | 上流には無い。**押した側の半分だけ**がブートローダーに入る(この振る舞いは `locality = EVENT_SOURCE` による)。左手を焼くのにリセットボタンを2度押ししなくて済むので、書き込みが明らかに楽 |
| コンボを `cormoran,runtime-combo-defaults` で定義 | DYA Studio から編集できる。上流は `zmk,combos` のままで編集不可。位置も違う(こちらは Q+W / W+E、`require-prior-idle-ms = <125>` 付き) |
| `&lt` に `flavor = "balanced"` / `quick-tap-ms = <175>` | 既定の `tap-preferred` だと 200ms 待たないとレイヤーが有効にならず、MOUSE レイヤーのクリックが機能しない |
| レイヤーに `display-name`(BASE / SYMBOL) | Studio のタブに名前が出る |
| レイヤー4〜6を `status = "reserved"` | Studio から使う空きレイヤーとして確保。上流は `&trans` で埋めた実レイヤー |
| `behaviors/runtime_macro.dtsi` と `behaviors/default_layer.dtsi` を include | `&rmacro`(マクロ)と `&df`(既定レイヤー)をキーに割り当て可能にする |

### ファームウェア構成

| 違い | 効果・理由 |
|---|---|
| **左手にも custom-settings と split relay を残す** | 上流は左手から削除している。削ると Settings タブから左手のアイドル/スリープ設定を触れなくなる |
| **watchdog を左手にも入れる** | 左手自身のフリーズ・クラッシュの記録が残る。中継で右手経由から読める |
| トラックボールの physical-layout ノードを定義 | 上流はモジュールを入れているがノードが無く、プレビューに何も描かれない |
| **PMW3610 ドライバを cormoran 版に差し替え** | 上流は badjeff 版。cormoran 版は Studio RPC 付きで、CPI などを DYA Studio から変更・保存でき、センサーの生画像も取れる |
| `default-layer` は `codex/custom-rpc-rewrite`、`os-detection` も追加 | 上流が指す `main` にはビヘイビア(`&df`)しか無く、Connection タブから設定できない |
| バッテリー履歴を無効 | 見る手段が無いため(「既知の事項」参照) |
| kscan diagnostics の左手中継を無効 | Web UI 未実装のため。統計は左右とも取れる |
| `sensor-rotate` を入れない | ロータリーエンコーダー用のモジュールで、CLine46 には載っていない |
| `CONFIG_ZMK_STUDIO_TRANSPORT_BLE_PREF_LATENCY=0` | BLE 経由の Studio 操作を軽くする |
| `BT_PERIPHERAL_PREF_MIN_INT` は `12` のまま | 上流は `6`(7.5ms)に下げている。低遅延だが電池を食うため追随していない |
| PMW3610 の `RUN_DOWNSHIFT_TIME_MS` / `REST1_SAMPLE_TIME_MS` を残す | 上流は既定値に戻した。こちらは現状の挙動で問題が無いため維持(ドライバ差し替えに伴い `CONFIG_PMW3610_ALT_*` から `CONFIG_PMW3610_*` に改名) |
| `zephyr/module.yml` の名前、`CLine46.zmk.yml` の URL と features、`draw.yml` のパスを修正 | 上流は `CLine45` や `roBa` の残骸が残っている |

## ビルド

### GitHub Actions(通常はこちら)

1. `config/CLine46.keymap` などを編集して push
2. **Actions** タブでビルド完了を待つ
3. Artifacts から `firmware.zip` をダウンロード

生成される uf2:

| ファイル | 用途 |
|---|---|
| `CLine46_R.uf2` | 右手(Central、ZMK Studio 有効) |
| `CLine46_L.uf2` | 左手(Peripheral) |
| `settings_reset.uf2` | 設定領域の初期化用 |

> ファイル名は `build.yaml` の `artifact-name` で固定しています。指定が無いと
> `CLine46_R rgbled_adapter-xiao_ble_zmk-zmk.uf2` のようにボード名込みの
> 長い名前になり、ZMK のバージョンが上がるたびに変わります。

### ローカルビルド

このリポジトリはルートに `zephyr/module.yml` を持つため、**リポジトリ内で `west init` するとディレクトリが衝突します**。config だけを別ワークスペースにコピーしてビルドします。

```bash
git clone https://github.com/3110/zmk_config_CLine46.git
cd zmk_config_CLine46
mkdir -p ~/zmk-workspace

docker run --rm -it \
  -v "$PWD":/zmk-config \
  -v ~/zmk-workspace:/workspace \
  -w /workspace \
  zmkfirmware/zmk-build-arm:stable bash
```

コンテナ内:

```bash
# 初回のみ
mkdir -p config && cp -R /zmk-config/config/* config/
west init -l config
west update
west zephyr-export

# 右手(Central・Studio あり)
west build -s zmk/app -d build/right -b xiao_ble//zmk -S studio-rpc-usb-uart -- \
  -DSHIELD="CLine46_R rgbled_adapter" \
  -DZMK_CONFIG=/workspace/config \
  -DZMK_EXTRA_MODULES=/zmk-config

# 左手(Peripheral)
west build -s zmk/app -d build/left -b xiao_ble//zmk -- \
  -DSHIELD="CLine46_L rgbled_adapter" \
  -DZMK_CONFIG=/workspace/config \
  -DZMK_EXTRA_MODULES=/zmk-config
```

成果物は `build/right/zephyr/zmk.uf2`。キーマップだけ変えた場合は `cp -R` をやり直してから `west build -d build/right` を再実行(`west update` は不要)。

## 書き込み

1. XIAO BLE のリセットボタンを素早く2回押す(または SCROLL レイヤーの `&bootloader`)
2. `XIAO-SENSE` ドライブがマウントされる
3. uf2 をコピー

> **キーマップを変更したら、書き込み後に ZMK Studio で「Restore Stock Settings」を実行してください。**
> Studio でキーマップを編集すると設定領域に保存され、以降はコンパイル済みキーマップより優先されます。これを実行しないと `.keymap` の変更が反映されません。
> コンボも同じで、Studio で上書きした値は `.keymap` の既定値より優先されます
> (コンボ単位で戻すだけなら Studio の **Reset to Default** が使えます)。
> **DYA Studio で作ったマクロは設定領域にしか無いので、この操作で消えます。**

ペアリングがおかしいときは、両手に `settings_reset` を書き込んでから左右のファームを焼き直します。

## 外部機器への状態通知

右手(Central)が、レイヤーや電池残量などを **BLE の非接続広告**に載せて流します。
M5Stack などの受信側は**スキャンするだけ**でよく、接続もペアリングも要りません。
BLE プロファイル(5個)も `CONFIG_BT_MAX_CONN` も消費しないので、PC との接続や
DYA Studio の動作には影響しません。

流しているもの(24バイト):

| 内容 | 備考 |
|---|---|
| レイヤー番号と名前 | `display-name` の先頭4文字(BASE / SYMB / MOUS / SCRO) |
| **右手の電池電圧(mV)** と残量(%) | NiMH は 1.02〜1.36V の狭い幅なので、%より電圧が実用的 |
| **左手の電池電圧(mV)** と残量(%) | 電圧は ZMK の split 中継に入っていないので、独自のイベント中継で右手へ送っている |
| **OS 判別結果と現在の既定レイヤー** | 誤判定に気づける |
| BLE プロファイル番号・接続状態・USB/BLE の出力先 | |
| **Studio のロック解除中かどうか** | 解除しっぱなしに気づける |
| 左右の接続状態、アイドル状態 | |
| **起動からの経過分・リセット理由・watchdog の記録件数** | 「前回なぜ落ちたか」が常に見える |

広告間隔は操作中1秒 / アイドル中10秒で、ディープスリープ中は止まります。
電圧は ZMK が 60 秒ごとに測っている値を**読むだけ**なので、広告のために
電池を余計に食うことはありません。

> ZMK 自身もプロファイル用の広告と `&studio_unlock` 後の directed advertising を
> 出すため、**拡張広告のセットをもう1つ**確保して(`CONFIG_BT_EXT_ADV`)、そちらに
> 流しています。ZMK 側は従来どおりセット0を使うので互いに独立です。

バイト配置と M5Stack(NimBLE)側の実装例は
[docs/status-advertisement.md](docs/status-advertisement.md) にあります。
`include/cline46/status_adv.h` は Zephyr に依存していないので、受信側へ
そのままコピーして使えます。

> **広告は平文です。** 誰でも受信できるので打鍵内容は載せていません。
> Studio のロック状態も載せたくない場合は `flags` の bit3 を落としてください。

## リリース

タグとリリースは **Actions の Release ワークフロー**で作ります。手元で `git tag` を
打つ必要はありません(タグは対象コミット上に作られます)。

1. `docs/release-notes/<タグ名>.md` にリリースノートを書いて main に push する
   (例: `docs/release-notes/v1.1.0.md`)
2. Actions → **Release** → **Run workflow**
3. 入力する項目

   | 項目 | 内容 |
   |---|---|
   | `version` | タグ名。`v` から始める(例: `v1.1.0`) |
   | `title` | リリースのタイトル。省略するとタグ名だけになる |
   | `notes_path` | ノートの場所。省略すると `docs/release-notes/<タグ名>.md` |
   | `draft` | 下書きで作りたいときだけ `true` |
   | `retag` | すでにあるタグを別のコミットに打ち直すときだけ `true` |

タグの重複とノートの有無は**ビルド前**に確かめるので、入力を間違えても数分待たされません。
ビルドは `build.yml` と同じ手順で、できた uf2(左右と `settings_reset`)がそのまま
リリースに添付されます。

## ファイル構成

| パス | 役割 |
|---|---|
| `config/CLine46.keymap` | **キーマップ本体**。レイヤー、コンボの既定値、ビヘイビア |
| `config/west.yml` | 依存リポジトリの一覧(west マニフェスト) |
| `src/status_adv.c` | 状態を BLE 広告で流すコード([説明](#外部機器への状態通知)) |
| `include/cline46/status_adv.h` | 広告に載せるデータの定義。**受信側にもそのまま使える** |
| `CMakeLists.txt` / `Kconfig` | 上の C を Zephyr モジュールとしてビルドするための定義 |
| `docs/status-advertisement.md` | 広告のバイト配置と M5Stack 側の実装例 |
| `config/CLine46.json` | keymap-drawer 用の物理レイアウト定義 |
| `build.yaml` | ビルド対象の board / shield マトリクス |
| `keymap_drawer.config.yaml` | キーマップ図の描画設定 |
| `zephyr/module.yml` | このリポジトリを Zephyr モジュールとして宣言 |
| `boards/shields/CLine46/CLine46.dtsi` | 左右共通のハード定義(物理レイアウト、マトリクス、kscan、電池) |
| `boards/shields/CLine46/CLine46_R.overlay` | 右手固有。列オフセット、SPI、PMW3610、入力プロセッサ |
| `boards/shields/CLine46/CLine46_L.overlay` | 左手固有 |
| `boards/shields/CLine46/CLine46_R.conf` | 右手の Kconfig。PMW3610、ZMK Studio、DYA Studio 用モジュール一式 |
| `boards/shields/CLine46/CLine46_L.conf` | 左手の Kconfig。電池、スリープ、右手からの設定中継、watchdog |
| `boards/shields/CLine46/Kconfig.defconfig` | シールド選択時の既定 Kconfig |
| `.github/workflows/build.yml` | ファームウェアのビルド |
| `.github/workflows/draw.yml` | キーマップ図の生成(手動実行) |
| `.github/workflows/release.yml` | タグ作成とリリース公開(手動実行) |
| `docs/release-notes/` | リリースノート(`v1.1.0.md` のようにタグ名で置く) |

## カスタマイズのポイント

`boards/shields/CLine46/CLine46_R.overlay`:

| 項目 | 説明 |
|---|---|
| `cpi = <600>` | センサーの解像度。ここは**起動時の既定値**で、常用の変更は DYA Studio の PMW3610 Drivers から |
| `settings-id = "ball"` | DYA Studio 上でのセンサー名。保存キー(`<項目>@ball`)にも使われる |
| `zip_scroll_scaler 1 8` | スクロール量(現在1/8倍) |
| `zip_xy_transform INPUT_TRANSFORM_X_INVERT` | 軸の反転・入れ替え。他のパターンはコメントアウトで用意済み |
| `zip_temp_layer 2 1000` | **オートマウスレイヤー**。コメントを外すと、トラックボールを動かした瞬間に MOUSE レイヤーへ自動遷移 |

`boards/shields/CLine46/*.conf`:

| 項目 | 説明 |
|---|---|
| `CONFIG_ZMK_IDLE_TIMEOUT=30000` | アイドルまでの時間(30秒) |
| `CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=7200000` | ディープスリープまでの時間(2時間) |
| `CONFIG_ZMK_NON_LIPO_MAX_MV` / `MIN_MV` | NiMH の100%/0%に対応する電圧 |
| `CONFIG_ZMK_NON_LIPO_LOW_MV` | 保護のため電源を落とす電圧(0%より低い値が正常) |
| `CONFIG_BT_PERIPHERAL_PREF_MIN_INT` / `MAX_INT` | BLE 接続間隔。小さいほど低遅延だが電池を消費 |

## 既知の事項

- **キーマップ図にコンボが出ません。** keymap-drawer が `zmk,combos` しか読まないためで、
  このリポジトリはコンボを `cormoran,runtime-combo-defaults` で定義しています
- **バッテリー履歴(`zmk-module-battery-history`)は無効にしています。**
  DYA Studio 本体がこのサブシステムに未対応で、モジュールが申告する Web UI の
  URL は開発用サーバ(`http://localhost:5173`)がハードコードされているだけのため、
  記録しても見る手段がありません(DYA Studio 上ではリンクが繋がらない項目として
  見えるだけ)。見る場合はモジュールの `web/` を自分で動かします
- **キースイッチ診断に左手の配線が出ません**(`Devices: 1` のまま)。ファーム側の中継は
  完成していますが Web UI 側が未実装のためで、そのぶんの中継は切ってあります
  (`CONFIG_ZMK_KSCAN_DIAGNOSTICS_SPLIT=n`)。打鍵・チャタリングの統計は左右とも
  取れているので、診断の実用面は落ちていません。UI が対応したときに戻す手順は
  `CLine46_R.conf` のコメントにあります
- **watchdog の監視タイマーが BLE の無線タイミングと稀に干渉しうる**と、モジュールの
  DESIGN.md に書かれています。接続が不安定になったら Troubleshooting の記録を見て、
  FREEZE が記録されていれば watchdog が仕事をした結果、記録が空なのに再起動している
  なら干渉を疑い、`CONFIG_ZMK_WATCHDOG_FREEZE_MONITOR_LOWPRIO_QUEUE=n` で
  タイマー負荷を半分にします
- **`zmk-module-runtime-input-processor` を `main` に上げた回だけ、トラックボールの
  保存済み設定が初期化されます。** 保存先が custom-settings に移り、フラッシュ上の
  形式が変わったためです(モジュールの設計上、旧形式からの移行は行いません)。
  詳しくは[新バージョンで増えたこと](#新バージョンで増えたこと)を参照
- **`zmk-feature-default-layer` だけ `codex/custom-rpc-rewrite` ブランチを指しています。**
  `main` にはビヘイビア(`&df`)しか無く Studio RPC が入っていないため、Connection タブ
  から設定できません。`zmk-feature-os-detection` は機能を使う/使わないに関わらず、
  `default-layer` の `zephyr/module.yml` が `build.depends` で要求するので必須です
- **v0.3 系には戻せません。** マクロ/コンボのモジュールが Zephyr 3.7 以降でしか
  通らない書き方(`configdefault`、`zephyr_linker_sources` の `ROM_SECTIONS`)を
  使っているためです。v0.3 で動かすには
  [zmk-feature-custom-settings](https://github.com/cormoran/zmk-feature-custom-settings)
  にパッチを当てた fork が必要でした
- Actions で `Node.js 20 is deprecated` の警告が出ますが、ZMK 側の再利用可能ワークフロー(`build-user-config.yml@v0.3`)が `actions/checkout@v4` を使っているためで、**ビルドには影響しません**(このワークフローは ZMK 本体のバージョンとは無関係で、4.1 でもそのまま使えます)
- `&xiao_serial` は無効化しています(D6/D7 を kscan が使用中のため)。有効に戻すとキー入力が壊れます
- `spi0` の MOSI と MISO が同じ P1.15 に割り当てられていますが、PMW3610 の3線式 SPI 仕様のため正常です。
  PMW3610 ドライバの README が言う「3-wire フォールバック」は CS(`cs-gpios`)が無い配線のことで、
  この配線には CS があるので速い方(burst)の経路がそのまま使えます


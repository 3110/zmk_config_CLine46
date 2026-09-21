# DYA Studio の使い方

[DYA Studio](https://studio.dya.cormoran.works) は [ZMK Studio](https://zmk.studio/) の代替 Web UI です。
[cormoran](https://github.com/cormoran) 氏の ZMK fork とモジュール群に対応していて、
**ファームを焼き直さずに変えられる範囲が ZMK Studio より広い**のが利点です。
このリポジトリはそれを使う前提の構成にしてあります。

確認しているバージョンは `v2026.09.20.0` です。

## 接続と前提

- ブラウザは **Chrome / Edge**（Web Serial / Web Bluetooth）。iOS は Bluefy、Android は Chrome（BLE のみ）。
  macOS / Windows は[デスクトップアプリ](https://github.com/cormoran/dya-studio/releases)も使えます
- 接続は **USB（右手を挿す）** か **BLE**。窓口は常に**右手**（Central）で、左手の情報は右手経由で中継されます
- 読むだけならロックされたままでも大半は見えますが、**編集には `&studio_unlock`** が必要です
  （SCROLL レイヤー + 右手親指の `RET`）

### BLE で接続する

> **USB ケーブルを抜いてから接続してください。**
> ZMK の Studio RPC は**キー入力の出力先（選択中のエンドポイント）と同じ経路にしか応答しません**
> （`zmk/app/src/studio/rpc.c` の `refresh_selected_transport()`）。USB が繋がっていると
> 出力先が USB になり、BLE の GATT に書き込んでも応答が返らず、ブラウザ側は
> **`Connection timed out: the device did not respond.`** で失敗します。
> USB を挿したまま使いたい場合は、Connection タブの**出力優先度を `BLE` に切り替え**ます
> （その間はキー入力も BLE 側に流れます）。

1. USB を抜く（または出力優先度を BLE にする）
2. キーボードで **`&studio_unlock`** を押す — アクティブなプロファイル宛に directed advertising が始まり、Web Bluetooth から見つけられる状態になります
3. DYA Studio のスプラッシュ画面で **Bluetooth** を選び、デバイス選択で **CLine46** を選ぶ

見つからないときは、**もう一度 `&studio_unlock` を押してから**接続し直してください
（アンロック時に広告が始まる仕組みなので、時間が経つと止まります）。
macOS の Chrome でダイアログに何も出ない場合は、システム設定 → プライバシーとセキュリティ →
Bluetooth で Chrome を許可してください。

BLE 接続中は応答を速くするため `CONFIG_ZMK_STUDIO_TRANSPORT_BLE_PREF_LATENCY=0` を指定しています。
既定の 10 のままだと接続間隔 15ms と合わせて 1 往復 165ms かかり、操作がもたつきます。
効くのは Studio が接続されている間だけなので、普段の電池持ちには影響しません。

## タブごとの機能

| タブ | できること | 効かせているモジュール |
|---|---|---|
| **Keymap** | キー割り当て、レイヤーの追加・並べ替え・リネーム。**Stream** スイッチを入れると押したキーがプレビュー上で光る。プレビューにはトラックボールも描かれる | ZMK Studio 標準 + `input-stream` / `fast-keymap` / `physical-layout` |
| **Macro&Combo** | マクロの作成・編集・リネーム・削除、コンボの位置/ビヘイビア/タイムアウト/対象レイヤーの編集 | `runtime-macro` / `runtime-combo` / `custom-settings` |
| **Trackball** | 感度（0.1〜10倍）、回転、軸スナップ、スクロール、オートマウスレイヤー。入力プロセッサ単位・レイヤー単位で指定。**PMW3610 Drivers** の欄では CPI・スマートアルゴリズム・ダウンシフト/サンプル時間・軸反転をセンサーに直接指定できる | `runtime-input-processor` / PMW3610 ドライバ（`custom-settings` 経由） |
| **Connection** | BLE プロファイルの改名・切替・ペアリング解除、USB と BLE の出力優先度、**OS 判別の確認と手動上書き**、**接続先ごと・OS ごとの既定レイヤー** | `ble-management` / `os-detection` / `default-layer` |
| **Settings** | 左右それぞれのアイドル/スリープのタイムアウト、詳細設定 | `settings-rpc`（左手の分は `custom-settings` の中継） |
| **Troubleshooting** | 電池残量、FW のビルド情報と稼働時間、キースイッチ診断（チャタリング）、**watchdog の記録（左右とも）**、**トラックボールのセンサー生画像（22×22）と診断値**、サポートレポートのコピー | `device-info` / `kscan-diagnostics` / `watchdog` / PMW3610 ドライバ |

## 設定の保存と初期化

DYA Studio の変更は**2段階**です。ここを取り違えると「設定したのに再起動で消えた」ことになります。

1. **メモリへの書き込み** — 即時反映されるが電源を切ると消える。未保存の項目には緑のドットが付きます
2. **保存** — フラッシュへ永続化。再起動しても残ります

戻すときは3通りあります。

| 操作 | 効果 |
|---|---|
| Discard / 破棄 | 未保存の変更だけを捨てて、保存済みの値に戻す |
| **Reset to Default**（コンボなど項目単位） | その項目だけ `.keymap` に書いた既定値へ戻す |
| **Restore Stock Settings** | 設定領域を全消去してコンパイル時の状態へ。**マクロは設定領域にしか無いので消えます** |

`.keymap` に書いた値は**工場出荷値**の扱いで、Studio で上書きするとそちらが優先されます。
**新しいファームを書き込んでもキーマップが変わらない**のはこのためで、そのときは
**Restore Stock Settings** を実行します。

## マクロを作る

マクロは [zmk-feature-runtime-macro](https://github.com/cormoran/zmk-feature-runtime-macro)
で実現していて、**すべて DYA Studio 上で作成・編集**します（`.keymap` には `&rmacro` の
定義を include してあるだけで、マクロの中身は書きません）。

1. **Macro&Combo** タブで名前を付けて作成し、手順を編集する
2. 作成すると**スロット番号**が割り当てられるので、リストで番号を確認する
3. キーマップエディタで対象キーに **Runtime Macro** を選び、その番号を指定する

スロット番号は作成時に決まるもので、事前に選ぶものではありません。既定では
最大8個（`CONFIG_ZMK_RUNTIME_MACRO_COUNT`）、名前と本文の合計で 1024 バイト
（`CONFIG_ZMK_RUNTIME_MACRO_POOL_BYTES`）まで保持できます。

## コンボを追加する

既定では2つ入っています。

| スロット | キー | 出力 | 位置 |
|---|---|---|---|
| 0 | Q + W | Tab | `<1 2>` |
| 1 | W + E | Shift + Tab | `<2 3>` |

いずれも `require-prior-idle-ms = <125>` 付き（タイピング中の誤爆防止）です。

コンボは ZMK 標準の `zmk,combos` ではなく
[zmk-feature-runtime-combo](https://github.com/cormoran/zmk-feature-runtime-combo)
の `cormoran,runtime-combo-defaults` で定義しています。`zmk,combos` はコンパイル時に
固定されて DYA Studio から編集できないためです。追加のコンボは Studio 側から
空きスロットに作れます（上限は `CONFIG_ZMK_RUNTIME_COMBO_MAX_COMBOS`、既定 8）。

### キーポジション番号

コンボの `key-positions` で使う番号。キーマップの `bindings` の記述順と一致します。

```
 0  1  2  3  4  5   |   6  7  8  9 10 11
12 13 14 15 16 17   |  18 19 20 21 22 23
24 25 26 27 28 29   |  30 31 32 33 34 35
   36 37 38 39 40 41 | 42 43     44 45
```

## OS ごとに既定レイヤーを変える

1. キーマップエディタで、**予約してある 4〜6 のレイヤー**に OS ごとの差分キーを置く
   （レイヤー0は常に有効なので、**変えたいキーだけ**置けば足ります）
2. Connection タブで接続先の「デフォルトレイヤー」を **「OS 検出に従う」** にする
3. 「OS ごとのデフォルトレイヤー」で macOS → 4、Windows → 5 のように割り当てる

OS の判別は USB なら列挙時のやり取り、BLE なら GATT の読まれ方の癖から推定します。
接続のたびに一定時間（USB 200ms / BLE 1000ms）静まってから確定するので、
繋いだ直後の一瞬は unknown 扱いです。外したときは同じ画面で手動上書きできます。

> レイヤー1〜3（SYMBOL / MOUSE / SCROLL）はホールドして使うモーメンタリなレイヤーなので、
> 既定レイヤーには選ばないでください。

## トラックボールのセンサー設定（PMW3610 ドライバ）

PMW3610 のドライバは badjeff 版から
[cormoran 版](https://github.com/cormoran/zmk-driver-pmw3610-with-custom-studio-rpc)に
差し替えてあります。センサー自体の設定を**焼き直さずに**変えられるのが目的です。

| 変えられるもの | 場所 |
|---|---|
| CPI（200〜3200、200 刻み）、X/Y 反転、XY 入れ替え、スマートアルゴリズム、常時稼働、ダウンシフト/サンプル時間、最小レポート間隔 | Trackball タブの **PMW3610 Drivers** |
| センサーの生画像（22×22）、製品 ID・リビジョン・SQUAL などの診断値 | Troubleshooting タブ |

- `CLine46_R.conf` の `CONFIG_PMW3610_*` は**工場出荷値**で、DYA Studio で保存した値が
  起動時にその上へ乗ります。戻すときは Studio 側の Reset を使います
- このサブシステムは**ロック対象**です。読むだけの操作でも `&studio_unlock` が
  要ります（生レジスタへの書き込みができるため、モジュール側がそう決めています）
- 保存キーは `CLine46_R.overlay` の `settings-id = "ball"` から `"<項目>@ball"` になります
- 感度（スケーリング）や回転、スクロール化は**入力プロセッサ側**（`runtime-input-processor`）の
  担当です。センサーの CPI とは別物なので、混ぜないようにしてください

> 生画像の取得は `&spi0` に `cs-gpios` がある（CS を保持したまま連続読みできる）ことを前提にした
> 速い経路を使います。動きが取りこぼされるようなら、`trackball` ノードに `disable-burst-read;` を
> 足して遅い方の経路に切り替えます。

## プレビューに描かれるトラックボール

キーの右下に描かれるトラックボールは `CLine46_R.overlay` の
`trackball_layout` ノードで位置と大きさを決めています。見た目だけの情報で、
動作には影響しません。

| プロパティ | 現在値 | 意味 |
|---|---|---|
| `size` | `100` | 直径。キー1つと同じ大きさ |
| `x` / `y` | `1050` / `300` | 左上の座標。右手親指列の空き（x=1000〜1200）の中央 |

単位はキーの物理レイアウト（`CLine46.dtsi`）と同じで、**キー1つが 100** です。
実物とずれていたらこの3つを調整します。`linked-device-identifiers` は
描いたボールを Trackball タブの設定に結び付けるためのもので、
このキーボードの runtime input processor は `mouse` だけです
（スクロールは `zip_*` で組んでいて runtime input processor ではありません）。

## このファームではできないこと

| できないこと | 理由 |
|---|---|
| バッテリー履歴のグラフ | 無効にしています（[既知の事項](known-issues.md)） |
| **左手側**のキー配線の表示 | ファームの中継は完成していますが Web UI 側が未実装。打鍵・チャタリングの統計は左右とも取れます |
| エンコーダーの回転割り当ての変更 | CLine46 にロータリーエンコーダーが載っていないため（`sensor-rotate` は入れていません） |

## 新しいバージョンで増えたこと

DYA Studio `v2026.09.20.0`（2026-09-20）の変更のうち、この構成に関係するものです。
**いずれもブラウザ/アプリ側だけの変更で、ファームを焼き直す必要はありません。**
プロトコル（`proto/`）は前バージョンから変わっていないので、今のファームのままで全部使えます。

| 増えたこと | 内容 | この構成での扱い |
|---|---|---|
| **デスクトップアプリ** | macOS（`.dmg`）と Windows（`.exe`）版が [GitHub のリリース](https://github.com/cormoran/dya-studio/releases)に付くようになった。USB（シリアル）も BLE もアプリ側のダイアログで選べる | ブラウザ版と同じことができる。Chrome / Edge を開かずに済む |
| **キー編集のフローティングウィンドウ** | ビヘイビア選択がドラッグできる別窓になり、確定すると次のキーへ自動で進む。behavior 検索の改善、クイック選択のカスタマイズも入った | そのまま使える |
| **WebMCP 対応** | Settings タブを開いている間だけ、対応ブラウザの AI 機能（WebMCP クライアント）から電源管理と詳細設定を操作できる。登録されるのは `get_power_management_settings` / `set_power_management_timeouts` / `reset_all_keyboard_settings` / `list_advanced_keyboard_settings` / `set_advanced_keyboard_setting` の5つ | ファーム側は `settings-rpc` と `custom-settings` があれば足りるので、この構成なら追加設定なしで動く。非対応ブラウザでは何も起きない |
| **スマートフォン対応の改善** | スクロール範囲の整理、タッチ操作、縦画面の表示修正。Macro&Combo と Trackball はドロップダウンにまとまる | Android Chrome / iOS Bluefy から BLE で繋いだときに効く |
| **簡体字中国語とブラウザ言語の自動判定** | 英語・日本語・簡体字中国語の切り替え | — |
| ペリフェラル側 PMW3610 の検出 | 分割キーボードの**子機**に付いたセンサーも一覧・診断できるようになった（両手に `CONFIG_ZMK_PMW3610_SPLIT_RPC_RELAY` が要る） | **この構成には不要**。トラックボールは Central（右手）側に1つだけなので、中継せずそのまま見える |
| Demo セッションの復元 | 再読み込みしてもデモ接続とワークスペースが残る | — |

ファーム側では PMW3610 ドライバを cormoran 版へ差し替え、
`zmk-module-runtime-input-processor` をタグ `zmk-v0.4.0.0` から `main` に上げました。
後者で入力プロセッサ設定の保存先が
[zmk-feature-custom-settings](https://github.com/cormoran/zmk-feature-custom-settings)
になりました。Settings タブの詳細設定にも `cormoran_rip` として出てきます。
ほかのモジュールは 2026-09-20 時点で `main` の先頭と同じ SHA を指しているので、そのままです。

> 保存形式が変わるため、**このファームを焼いた最初の起動でトラックボールの保存済み設定は
> `CLine46_R.overlay` の既定値に戻ります**（スケーリングや回転を Studio で変えていた場合は
> 入れ直してください）。

## 採用を検討できるもの（未採用）

DYA Studio が対応していて、このリポジトリではまだ使っていないものです。

| 候補 | 増えること | 必要な作業・注意 |
|---|---|---|
| [zmk-module-devtool](https://github.com/cormoran/zmk-module-devtool) | 画面右下に **Devtool** の小窓が出て、ファームの Zephyr ログをブラウザで流し読みできる。再起動・ブートローダー移行・ロック操作も窓から叩ける。デバッグプローブ無しで原因を追える | 開発用。イベントタップを有効にすると**打鍵内容を観測できてしまう**ので、常用ファームに入れるならログ取得だけにする。ログのリングバッファでメモリも食う |
| [zmk-feature-zephyr-setting-expose](https://github.com/cormoran/zmk-feature-zephyr-setting-expose) | 保存済み設定（NVS）を左右それぞれ一覧・編集・削除でき、残容量も見える | DYA Studio 本体にはこの画面が無く、モジュール側の別 Web UI を開く形になる |

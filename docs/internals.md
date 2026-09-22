# 実装とリポジトリ構成

使い方は [README](../README.md)、Studio の操作は [DYA Studio の使い方](dya-studio.md)、
広告の仕様は [状態広告の仕様](status-advertisement.md) にあります。
ここはファームウェアの中身とリポジトリの構造です。

## ハードウェア構成

| 項目 | 内容 |
|---|---|
| キー数 | 46（左右23キーずつ） |
| コントローラ | Seeed XIAO BLE (nRF52840) ×2 |
| キースキャン | Charlieplex 方式（D2〜D7、割り込み D1） |
| ポインティング | PMW3610 トラックボール（右手側、SPI0 / CS=P0.09 / IRQ=D8）。ドライバは [cormoran 版](https://github.com/cormoran/zmk-driver-pmw3610-with-custom-studio-rpc)（Studio RPC 付き） |
| 電池 | NiMH 単セル（1.02〜1.36V） |
| Central | **右手**（トラックボール側） |
| ベース | [cormoran/zmk](https://github.com/cormoran/zmk) `main+dya` @ `e5c9b69`（Zephyr 4.1） |

## ファイル構成

| パス | 役割 |
|---|---|
| `config/CLine46.keymap` | **キーマップ本体**。レイヤー、コンボの既定値、ビヘイビア |
| `config/west.yml` | 依存リポジトリの一覧（west マニフェスト） |
| `config/CLine46.json` | keymap-drawer 用の物理レイアウト定義 |
| `src/status_adv.c` | 状態を BLE 広告で流すコード |
| `src/behaviors/behavior_status_adv.c` | 広告をキーでオン/オフするビヘイビア（`&status_adv`） |
| `src/peripheral_voltage.c` | 左手の電池電圧を右手へ中継する |
| `include/cline46/status_adv.h` | 広告に載せるデータの定義。**受信側にもそのまま使える** |
| `dts/behaviors/status_adv.dtsi` | `&status_adv` のノード定義 |
| `CMakeLists.txt` / `Kconfig` | 上の C を Zephyr モジュールとしてビルドするための定義 |
| `zephyr/module.yml` | このリポジトリを Zephyr モジュールとして宣言 |
| `boards/shields/CLine46/CLine46.dtsi` | 左右共通のハード定義（物理レイアウト、マトリクス、kscan、電池） |
| `boards/shields/CLine46/CLine46_R.overlay` | 右手固有。列オフセット、SPI、PMW3610、入力プロセッサ |
| `boards/shields/CLine46/CLine46_L.overlay` | 左手固有 |
| `boards/shields/CLine46/CLine46_R.conf` | 右手の Kconfig。PMW3610、ZMK Studio、DYA Studio 用モジュール一式 |
| `boards/shields/CLine46/CLine46_L.conf` | 左手の Kconfig。電池、スリープ、右手からの設定中継、watchdog |
| `boards/shields/CLine46/Kconfig.defconfig` | シールド選択時の既定 Kconfig |
| `build.yaml` | ビルド対象の board / shield マトリクス |
| `keymap_drawer.config.yaml` | キーマップ図の描画設定 |
| `.github/workflows/build.yml` | ファームウェアのビルド |
| `.github/workflows/draw.yml` | キーマップ図の生成（手動実行） |
| `.github/workflows/release.yml` | タグ作成とリリース公開（手動実行） |
| `docs/release-notes/` | リリースノート（`v1.1.0.md` のようにタグ名で置く） |
| `tools/CLine46Status/` | 受信側の Arduino ライブラリ（表示なし） |
| `tools/m5stack-status-monitor/` | シリアルに出すだけの PlatformIO プロジェクト |
| `tools/atoms3r-status-display/` | AtomS3R / AtomS3 の画面に出す PlatformIO プロジェクト |
| `tools/config/` | 機種ごとの PlatformIO 設定（submodule） |
| `tools/*/platformio-offline.ini` | レジストリを使わずビルドするための上書き設定（[説明](platformio-without-registry.md)） |
| `tools/pio-setup-no-registry.sh` | 上の ini を使うための PlatformIO 準備スクリプト |

## 状態広告の実装

右手（Central）が、レイヤー・電池・接続状態などを **BLE の非接続広告**（24バイト）に
載せて流します。バイト配置、キーによるオン/オフ、左手電圧の中継、受信側の実装例は
[状態広告の仕様](status-advertisement.md) にまとめてあります。

要点だけ:

- ZMK 自身もプロファイル用の広告と directed advertising を出すため、**拡張広告のセットを
  もう1つ**確保して（`CONFIG_BT_EXT_ADV`）そちらに流しています。ZMK 側はセット0のままで独立です
- BLE プロファイル（5個）も `CONFIG_BT_MAX_CONN` も消費しません
- 電圧は ZMK が 60 秒ごとに測っている値を**読むだけ**なので、広告のために電池を余計に食いません
- 左手の電圧は ZMK の split 中継に入っていないため、cormoran fork の汎用イベント中継に
  相乗りして独自に送っています（`src/peripheral_voltage.c`）
- **広告は平文です。** 誰でも受信できるので打鍵内容は載せていません

## 設定値のカスタマイズ

### `boards/shields/CLine46/CLine46_R.overlay`

| 項目 | 説明 |
|---|---|
| `cpi = <600>` | センサーの解像度。ここは**起動時の既定値**で、常用の変更は DYA Studio の PMW3610 Drivers から |
| `settings-id = "ball"` | DYA Studio 上でのセンサー名。保存キー（`<項目>@ball`）にも使われる |
| `zip_scroll_scaler 1 8` | スクロール量（現在1/8倍） |
| `zip_xy_transform INPUT_TRANSFORM_X_INVERT` | 軸の反転・入れ替え。他のパターンはコメントアウトで用意済み |
| `zip_temp_layer 2 1000` | **オートマウスレイヤー**。コメントを外すと、トラックボールを動かした瞬間に MOUSE レイヤーへ自動遷移 |

### `boards/shields/CLine46/*.conf`

| 項目 | 説明 |
|---|---|
| `CONFIG_ZMK_IDLE_TIMEOUT=30000` | アイドルまでの時間（30秒） |
| `CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=7200000` | ディープスリープまでの時間（2時間） |
| `CONFIG_ZMK_NON_LIPO_MAX_MV` / `MIN_MV` | NiMH の100%/0%に対応する電圧 |
| `CONFIG_ZMK_NON_LIPO_LOW_MV` | 保護のため電源を落とす電圧（0%より低い値が正常） |
| `CONFIG_BT_PERIPHERAL_PREF_MIN_INT` / `MAX_INT` | BLE 接続間隔。小さいほど低遅延だが電池を消費 |
| `CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE` | 設定をフラッシュへ書くまでの遅延（このリポジトリでは10秒） |
| `CONFIG_CLINE46_STATUS_ADV*` | 状態広告（間隔・お別れパケット・保存の有無）。[一覧](status-advertisement.md) |

### キーマップ側

`&mt` と `&lt` はどちらも `flavor = "balanced"` に設定済みです。既定の `tap-preferred` だと
200ms 待たないとレイヤーが有効にならず、MOUSE レイヤーのクリックが機能しないためです。

## fork 元との違い

fork 元の [takamaru-fpv/zmk_config_CLine46](https://github.com/takamaru-fpv/zmk_config_CLine46)
から意図的に変えている点です。**上流を取り込むときはここを潰さないよう注意**してください。

### キーマップ

| 違い | 効果・理由 |
|---|---|
| **SCROLL レイヤーに `&bootloader` と `&sys_reset` を配置** | 上流には無い。**押した側の半分だけ**がブートローダーに入る（この振る舞いは `locality = EVENT_SOURCE` による）。左手を焼くのにリセットボタンを2度押ししなくて済む |
| **SCROLL レイヤーに `&status_adv SADV_TOG` を配置** | 状態広告をキーで止められるようにするため（上流にはこの機能自体が無い） |
| コンボを `cormoran,runtime-combo-defaults` で定義 | DYA Studio から編集できる。上流は `zmk,combos` のままで編集不可。位置も違う（こちらは Q+W / W+E、`require-prior-idle-ms = <125>` 付き） |
| `&lt` に `flavor = "balanced"` / `quick-tap-ms = <175>` | 既定の `tap-preferred` だと 200ms 待たないとレイヤーが有効にならず、MOUSE レイヤーのクリックが機能しない |
| レイヤーに `display-name`（BASE / SYMBOL） | Studio のタブに名前が出る。状態広告のレイヤー名にも使う |
| レイヤー4〜6を `status = "reserved"` | Studio から使う空きレイヤーとして確保。上流は `&trans` で埋めた実レイヤー |
| `behaviors/runtime_macro.dtsi` と `behaviors/default_layer.dtsi` を include | `&rmacro`（マクロ）と `&df`（既定レイヤー）をキーに割り当て可能にする |

### ファームウェア構成

| 違い | 効果・理由 |
|---|---|
| **状態を BLE 広告で流す実装を追加** | 上流には無い。`src/status_adv.c` ほか |
| **左手にも custom-settings と split relay を残す** | 上流は左手から削除している。削ると Settings タブから左手のアイドル/スリープ設定を触れなくなる |
| **watchdog を左手にも入れる** | 左手自身のフリーズ・クラッシュの記録が残る。中継で右手経由から読める |
| トラックボールの physical-layout ノードを定義 | 上流はモジュールを入れているがノードが無く、プレビューに何も描かれない |
| **PMW3610 ドライバを cormoran 版に差し替え** | 上流は badjeff 版。cormoran 版は Studio RPC 付きで、CPI などを DYA Studio から変更・保存でき、センサーの生画像も取れる |
| `default-layer` は `codex/custom-rpc-rewrite`、`os-detection` も追加 | 上流が指す `main` にはビヘイビア（`&df`）しか無く、Connection タブから設定できない |
| バッテリー履歴を無効 | 見る手段が無いため（[既知の事項](known-issues.md)） |
| kscan diagnostics の左手中継を無効 | Web UI 未実装のため。統計は左右とも取れる |
| `sensor-rotate` を入れない | ロータリーエンコーダー用のモジュールで、CLine46 には載っていない |
| `CONFIG_ZMK_STUDIO_TRANSPORT_BLE_PREF_LATENCY=0` | BLE 経由の Studio 操作を軽くする |
| `BT_PERIPHERAL_PREF_MIN_INT` は `12` のまま | 上流は `6`（7.5ms）に下げている。低遅延だが電池を食うため追随していない |
| PMW3610 の `RUN_DOWNSHIFT_TIME_MS` / `REST1_SAMPLE_TIME_MS` を残す | 上流は既定値に戻した。こちらは現状の挙動で問題が無いため維持（ドライバ差し替えに伴い `CONFIG_PMW3610_ALT_*` から `CONFIG_PMW3610_*` に改名） |
| `zephyr/module.yml` の名前、`CLine46.zmk.yml` の URL と features、`draw.yml` のパスを修正 | 上流は `CLine45` や `roBa` の残骸が残っている |

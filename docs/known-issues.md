# 既知の事項

制限、回避策、意図的にそうしている点です。

## 表示・UI

- **キーマップ図にコンボが出ません。** keymap-drawer が `zmk,combos` しか読まないためで、
  このリポジトリはコンボを `cormoran,runtime-combo-defaults` で定義しています
  （[理由](dya-studio.md#コンボを追加する)）
- **バッテリー履歴（`zmk-module-battery-history`）は無効にしています。**
  DYA Studio 本体がこのサブシステムに未対応で、モジュールが申告する Web UI の
  URL は開発用サーバ（`http://localhost:5173`）がハードコードされているだけのため、
  記録しても見る手段がありません（DYA Studio 上ではリンクが繋がらない項目として
  見えるだけ）。見る場合はモジュールの `web/` を自分で動かします
- **キースイッチ診断に左手の配線が出ません**（`Devices: 1` のまま）。ファーム側の中継は
  完成していますが Web UI 側が未実装のためで、そのぶんの中継は切ってあります
  （`CONFIG_ZMK_KSCAN_DIAGNOSTICS_SPLIT=n`）。打鍵・チャタリングの統計は左右とも
  取れているので、診断の実用面は落ちていません。UI が対応したときに戻す手順は
  `CLine46_R.conf` のコメントにあります

## 動作

- **watchdog の監視タイマーが BLE の無線タイミングと稀に干渉しうる**と、モジュールの
  DESIGN.md に書かれています。接続が不安定になったら Troubleshooting の記録を見て、
  FREEZE が記録されていれば watchdog が仕事をした結果、記録が空なのに再起動している
  なら干渉を疑い、`CONFIG_ZMK_WATCHDOG_FREEZE_MONITOR_LOWPRIO_QUEUE=n` で
  タイマー負荷を半分にします
- **状態広告の切り替えは、切り替えてから10秒以内に電源を切ると保存前に戻ります。**
  フラッシュを余計に減らさないよう、保存を `CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE`
  （このリポジトリでは10秒）だけまとめているためです
- **電池の表示は起動後しばらく `--` です。** ZMK が電池を測るのは 60 秒ごとで、
  最初の測定が終わるまでは「不明」を送っています（残量0%と区別するため）
- `&xiao_serial` は無効化しています（D6/D7 を kscan が使用中のため）。有効に戻すとキー入力が壊れます

## アップグレード時

- **`zmk-module-runtime-input-processor` を `main` に上げた回（v1.1.0）だけ、トラックボールの
  保存済み設定が初期化されます。** 保存先が custom-settings に移り、フラッシュ上の
  形式が変わったためです（モジュールの設計上、旧形式からの移行は行いません）。
  詳しくは [DYA Studio の使い方](dya-studio.md#新しいバージョンで増えたこと) を参照
- **ファームと受信側ライブラリのバージョンが食い違うと、受信側は取り込みを止めます**
  （`versionMismatch()` が `true`）。古い形式を誤って読まないための仕様なので、
  両方を同じリリースに揃えてください

## 依存モジュール

- **`zmk-feature-default-layer` だけ `codex/custom-rpc-rewrite` ブランチを指しています。**
  `main` にはビヘイビア（`&df`）しか無く Studio RPC が入っていないため、Connection タブ
  から設定できません。`zmk-feature-os-detection` は機能を使う/使わないに関わらず、
  `default-layer` の `zephyr/module.yml` が `build.depends` で要求するので必須です
- **v0.3 系には戻せません。** マクロ/コンボのモジュールが Zephyr 3.7 以降でしか
  通らない書き方（`configdefault`、`zephyr_linker_sources` の `ROM_SECTIONS`）を
  使っているためです。v0.3 で動かすには
  [zmk-feature-custom-settings](https://github.com/cormoran/zmk-feature-custom-settings)
  にパッチを当てた fork が必要でした

## ビルド

- Actions で `Node.js 20 is deprecated` の警告が出ますが、ZMK 側の再利用可能ワークフロー
  （`build-user-config.yml@v0.3`）が `actions/checkout@v4` を使っているためで、
  **ビルドには影響しません**（このワークフローは ZMK 本体のバージョンとは無関係で、
  4.1 でもそのまま使えます）
- `spi0` の MOSI と MISO が同じ P1.15 に割り当てられていますが、PMW3610 の3線式 SPI 仕様のため正常です。
  PMW3610 ドライバの README が言う「3-wire フォールバック」は CS（`cs-gpios`）が無い配線のことで、
  この配線には CS があるので速い方（burst）の経路がそのまま使えます

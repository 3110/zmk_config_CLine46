# ビルドとリリース

ビルド済みの uf2 は [Releases](https://github.com/3110/zmk_config_CLine46/releases) に
付いています。ここはキーマップや構成を変えて**自分でビルドする**ときの手順です。

## ブランチ運用

| ブランチ | 役割 |
|---|---|
| `main` | リリース済みの状態だけが乗る。タグはここに打つ。更新は PR 経由のみ（直 push は禁止） |
| `develop` | 開発の本流。既定ブランチ |
| `feature/...` | 作業用。`develop` から切って `develop` に PR で戻す |

```
feature/xxx ──PR──> develop ──PR(merge commit)──> main ──タグ/リリース
                       ^                            |
                       └──────── 戻しマージ ─────────┘
```

作業するとき:

```bash
git switch develop && git pull
git switch -c feature/やること
# 編集してコミット
git push -u origin feature/やること
```

あとは GitHub で PR を作ります（ベースは `develop`）。ビルドが通ったら **Rebase and merge** で
マージして、作業ブランチを消します。ブランチ名は `feature/` `fix/` `docs/` のように、種類が
分かる程度の緩い規則です。

`main` に PR を出すのは[リリース](#リリース)のときだけです。`main` だけを緊急で直したいときは、
`main` から `hotfix/...` を切って `main` に PR を出し、リリースと同じようにタグを打ってから
`develop` に戻しマージします。

## GitHub Actions（通常はこちら）

1. `config/CLine46.keymap` などを編集し、作業ブランチを push して `develop` に PR を出す
2. **Actions** タブでビルド完了を待つ
3. Artifacts から `firmware.zip` をダウンロード

ビルドが走るのは、**PR を出したとき**と `main` / `develop` への push のときです。作業ブランチを
push しただけでは走りません。PR の前に試したいときは Actions → **Build** → **Run workflow** で
作業ブランチを選んでください。

生成される uf2:

| ファイル | 用途 |
|---|---|
| `CLine46_R.uf2` | 右手（Central、ZMK Studio 有効） |
| `CLine46_L.uf2` | 左手（Peripheral） |
| `settings_reset.uf2` | 設定領域の初期化用 |

> ファイル名は `build.yaml` の `artifact-name` で固定しています。指定が無いと
> `CLine46_R rgbled_adapter-xiao_ble_zmk-zmk.uf2` のようにボード名込みの
> 長い名前になり、ZMK のバージョンが上がるたびに変わります。

## ローカルビルド

このリポジトリはルートに `zephyr/module.yml` を持つため、**リポジトリ内で `west init` すると
ディレクトリが衝突します**。config だけを別ワークスペースにコピーしてビルドします。

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

成果物は `build/right/zephyr/zmk.uf2`。キーマップだけ変えた場合は `cp -R` をやり直してから
`west build -d build/right` を再実行（`west update` は不要）。

## 書き込み

1. XIAO BLE のリセットボタンを素早く2回押す（または SCROLL レイヤーの `&bootloader`）
2. `XIAO-SENSE` ドライブがマウントされる
3. uf2 をコピーする

> **キーマップを変更したら、書き込み後に DYA Studio で「Restore Stock Settings」を
> 実行してください。** Studio でキーマップを編集すると設定領域に保存され、以降は
> コンパイル済みキーマップより優先されます。これを実行しないと `.keymap` の変更が
> 反映されません。コンボも同じで、Studio で上書きした値は `.keymap` の既定値より
> 優先されます（コンボ単位で戻すだけなら Studio の **Reset to Default** が使えます）。
> **DYA Studio で作ったマクロは設定領域にしか無いので、この操作で消えます。**

ペアリングがおかしいときは、両手に `settings_reset` を書き込んでから左右のファームを焼き直します。

## キーマップ図の生成

`keymap-drawer/CLine46.svg` は Actions の **Draw Keymap** ワークフロー（手動実行）で
生成します。描画設定は `keymap_drawer.config.yaml`、物理レイアウトは `config/CLine46.json` です。

> 実行したブランチに図をコミットして push するので、**`develop` を選んでください**
> （[ブランチ運用](#ブランチ運用)）。

## リリース

タグとリリースは **Actions の Release ワークフロー**で作ります。手元で `git tag` を
打つ必要はありません（タグは対象コミット上に作られます）。

1. `docs/release-notes/<タグ名>.md` にリリースノートを書いて、通常の作業と同じように
   `develop` に入れる（例: `docs/release-notes/v1.1.0.md`）
2. `develop` → `main` の PR を作り、**Create a merge commit** でマージする

   > squash と rebase は使わないでください。`main` のコミットが `develop` と別物になり、
   > 手順 5 の戻しマージが毎回コンフリクトします。

3. Actions → **Release** → **Run workflow**。**Use workflow from** で `main` を選ぶ
   （ほかのブランチを選ぶと、入力の確認の時点で止まります）
4. 入力する項目

   | 項目 | 内容 |
   |---|---|
   | `version` | タグ名。`v` から始める（例: `v1.1.0`） |
   | `title` | リリースのタイトル。省略するとタグ名だけになる |
   | `notes_path` | ノートの場所。省略すると `docs/release-notes/<タグ名>.md` |
   | `draft` | 下書きで作りたいときだけ `true` |
   | `retag` | すでにあるタグを別のコミットに打ち直すときだけ `true` |

5. リリースを `main` から `develop` に反映する

   ```bash
   git fetch origin --tags
   git switch develop
   git merge origin/main   # 手順 2 のマージコミットを取り込む。通常は fast-forward
   git push
   ```

タグの重複とノートの有無は**ビルド前**に確かめるので、入力を間違えても数分待たされません。
ビルドは `build.yml` と同じ手順で、できた uf2（左右と `settings_reset`）がそのまま
リリースに添付されます。

`develop` は消さずにそのまま使い続けます。

## 受信側（M5Stack）のビルド

機種ごとの PlatformIO 設定は
[m5stack-platformio-config](https://github.com/3110/m5stack-platformio-config) を
submodule として使っています。**初回は submodule の取得が必要**です。

```sh
git submodule update --init tools/config

cd tools/atoms3r-status-display
pio run -e m5stack-atoms3r -t upload -t monitor
```

共通設定を更新するには `git submodule update --remote tools/config`。
対応機種の一覧と Arduino IDE での手順は
[tools/m5stack-status-monitor](../tools/m5stack-status-monitor) と
[tools/atoms3r-status-display](../tools/atoms3r-status-display) の README にあります。

> ファーム側の `include/cline46/status_adv.h` を変えたら、受信側ライブラリのコピーも
> 差し替えてください。食い違うと受信側が形式違いとして取り込みを止めます。
>
> ```sh
> cp include/cline46/status_adv.h tools/CLine46Status/src/cline46/status_adv.h
> ```

# PlatformIO レジストリを使わずに M5Stack 側をビルドする

`tools/` 以下の M5Stack 向けプロジェクトは、通常 PlatformIO レジストリから
プラットフォームとライブラリを取ってくる。ところがサンドボックス的な環境
（Claude Code on the web など）では `*.platformio.org` が egress ポリシーで
遮断されていて、`pio run` がパッケージ解決の時点で落ちる。

```
Platform Manager: Installing espressif32 @ ^7.0.1
HTTPClientError:
```

そういう環境向けに、**GitHub だけで完結させる**上書き設定を用意してある。
手元の PC で普通にビルドできているなら読む必要はない。

## 使い方

```bash
# 初回のみ。ツールチェーン一式(約 5.3GB)を取るので 5〜6 分かかる
bash tools/pio-setup-no-registry.sh
source "$HOME/.platformio/pio-env.sh"

git submodule update --init tools/config

cd tools/atoms3r-status-display
pio run -c platformio-offline.ini -e m5stack-atoms3r
```

`platformio.ini` には手を入れていない。`-c` で別の ini を指すだけなので、
手元の実機ビルド環境には影響しない。

用意してある env は4つ。

| プロジェクト | env |
|---|---|
| `tools/atoms3r-status-display` | `m5stack-atoms3r` / `m5stack-atoms3` |
| `tools/m5stack-status-monitor` | `m5stack-atoms3r` / `m5stack-atoms3` |

他の機種を足す場合は、`tools/config/platformio-m5stack.ini` のセクション名を
`platformio-offline.ini` に同じ形（`platform` と `platform_packages` を
上書きする形）で足す。

## 何をどう差し替えているか

| 取得対象 | 既定（レジストリ経由） | 代替（GitHub 経由） |
|---|---|---|
| platform | `espressif32@^7.0.1` | `pioarduino/platform-espressif32` を git で |
| ライブラリ | `M5Unified@^0.2.17` などの名前指定 | git URL + タグ直指定 |
| tool-scons | PlatformIO Core がレジストリから取る | `pioarduino/scons` のリリースを手で配置 |

pioarduino 版プラットフォームは、もともと全パッケージを GitHub Releases から
取るように作られている（`platform.json` の全エントリが
`https://github.com/pioarduino/registry/releases/download/...`）。
`tools/config/platformio-m5stack.ini` にも `[platform-pioarduino]` セクションが
あるので、方向性としては元からあるものに乗るかたち。

ライブラリはレジストリが引けないと `^` のバージョン解決ができないため、
タグを直に書いている。更新するときは手で上げること。

`tool-scons` だけは PlatformIO Core 自身が使うもので、プラットフォームを
差し替えても必ずレジストリを見にいく。`pioarduino/scons` のリリースにある
scons-local を展開し、`.piopm` を書いて「インストール済み」に見せている。

## 引っかかりどころ

### GitHub でも経路によって通ったり弾かれたりする

| 経路 | 結果 |
|---|---|
| `git clone` / `git fetch`（公開リポジトリ） | 通る |
| `raw.githubusercontent.com/...` | 通る |
| `github.com/<owner>/<repo>/releases/download/...` | 通る |
| `api.github.com/...` | 弾かれることがある |
| `codeload.github.com/...` | 弾かれることがある |
| `github.com/.../archive/refs/tags/...` | 弾かれることがある |

GitHub Releases のアセットは通るので、そこだけを使う pioarduino が刺さる。
逆に `archive/` や `codeload` を使う取り方（`lib_deps` にタグの tarball を
直接書く等）は避けること。

### pioarduino が CA 証明書の設定を上書きする

TLS を終端するプロキシの下では、`SSL_CERT_FILE` や `REQUESTS_CA_BUNDLE` で
そのプロキシの CA を信頼させている。ところが pioarduino の
`builder/penv_setup.py` の `_setup_certifi_env()` が

```python
os.environ["SSL_CERT_FILE"]      = cert_path
os.environ["REQUESTS_CA_BUNDLE"] = cert_path
os.environ["CURL_CA_BUNDLE"]     = cert_path
os.environ["GIT_SSL_CAINFO"]     = cert_path
```

と、専用 venv (penv) の certifi で上書きしてしまうので、設定が消えて

```
SSLError: certificate verify failed: self-signed certificate in certificate chain
```

で落ちる。セットアップスクリプトでは、検証を切るのではなく certifi の
`cacert.pem` に CA を追記して通している。penv は「最初のビルド」で作られるため、
捨てプロジェクトを1回ビルドして penv を作らせてから追記している。

### 大きいアセットでプロキシが 504 を返すことがある

一過性なので、落ちたらもう一度叩けば通る。pioarduino 側にも5回の再試行が入って
いるし、セットアップスクリプトにも3回の再試行を入れてある。

## ネットワークポリシー側で解決する場合

レジストリをそのまま使いたいなら、egress の許可リストに次を入れる。

```
api.registry.platformio.org      パッケージ解決（必須）
api.registry.nm1.platformio.org  上のミラー。PlatformIO が自動で切り替える（必須）
dl.registry.platformio.org       パッケージ本体のダウンロード（必須）
registry.platformio.org          疎通チェックに使われる（準必須）
collector.platformio.org         テレメトリ。要らなければ許可しない
```

Claude Code on the web の場合、これは環境を作るときのネットワークポリシー設定で
指定する（セッションの中からは変えられない）。
参照: https://code.claude.com/docs/en/claude-code-on-the-web

## 既定との違いに注意

pioarduino 55.03.39 は Arduino core 3.3.9 / ESP-IDF 5.5.4 系で、
既定の `espressif32@^7.0.1` + `framework-arduinoespressif32@^3.20017`
（Arduino core 2.0.17 系）とは世代が違う。上の4つの env はどれもビルドが
通ることを確認しているが、実機での挙動に差が出る可能性はある。
**最終的な書き込み用のファームは、手元の既定の設定で作ったものを使うこと。**

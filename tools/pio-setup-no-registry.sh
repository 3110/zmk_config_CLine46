#!/usr/bin/env bash
#
# PlatformIO を「PlatformIO レジストリを使わず GitHub だけ」で動かす準備をする。
#
# PlatformIO レジストリ (*.platformio.org) に出られないネットワーク
# （Claude Code on the web のサンドボックスなど）で、tools/ 以下の
# M5Stack 向けプロジェクトをビルドするためのもの。手元の PC で
# 普通にビルドできている場合は使う必要はない。
#
# 背景と引っかかりどころは docs/platformio-without-registry.md にまとめてある。
#
# 代替のしかた:
#   platform   espressif32@^7.0.1  → pioarduino 版 platform-espressif32 (git)
#                                    全パッケージを GitHub Releases から取る作り
#   lib_deps   レジストリ名        → git URL + タグ直指定
#   tool-scons PlatformIO Core が   → GitHub リリースの scons-local を手で配置
#              レジストリから取る
#
# 使い方:
#   bash tools/pio-setup-no-registry.sh
#   source "$HOME/.platformio/pio-env.sh"
#   cd tools/atoms3r-status-display
#   pio run -c platformio-offline.ini -e m5stack-atoms3r
#
# 初回はツールチェーン一式(約 5.3GB)を取得するので 5〜6 分かかる。
# 環境変数 PIO_VENV / PLATFORMIO_CORE_DIR で置き場所を変えられる。

set -euo pipefail

VENV="${PIO_VENV:-$HOME/.pio-venv}"
export PLATFORMIO_CORE_DIR="${PLATFORMIO_CORE_DIR:-$HOME/.platformio}"
CA_BUNDLE="${CCR_CA_BUNDLE:-/root/.ccr/ca-bundle.crt}"
PLATFORM_URL="https://github.com/pioarduino/platform-espressif32.git#55.03.39"
SCONS_URL="https://github.com/pioarduino/scons/releases/download/4.11.1/scons-local-4.11.1.tar.gz"
SCONS_VERSION="4.41101.0"

say() { printf '\n== %s\n' "$*"; }

# ---------------------------------------------------------------- 1. PlatformIO
say "PlatformIO Core を用意する ($VENV)"
if [ ! -x "$VENV/bin/pio" ]; then
  python3 -m venv "$VENV"
  "$VENV/bin/pip" install -q --upgrade pip platformio
fi
PIO="$VENV/bin/pio"
"$PIO" --version

# テレメトリの送信先 collector.platformio.org も遮断されているので黙らせる
"$PIO" settings set enable_telemetry false >/dev/null 2>&1 || true

# ------------------------------------------------------ 2. 証明書バンドルの追補
# TLS を終端するプロキシの下では、その CA を信頼する必要がある。
# ところが pioarduino の builder/penv_setup.py の _setup_certifi_env() が
# SSL_CERT_FILE / REQUESTS_CA_BUNDLE / CURL_CA_BUNDLE / GIT_SSL_CAINFO を
# penv 内の certifi で上書きしてしまい、せっかくの CA 設定が消える。
# 検証を切るのではなく、certifi の cacert.pem に CA を追記して通す（冪等）。
patch_certifi() {
  [ -f "$CA_BUNDLE" ] || { echo "  CA バンドルなし。追補は不要"; return 0; }
  local n=0 f
  while IFS= read -r f; do
    grep -q 'ccr-agent-proxy-marker' "$f" && continue
    { echo; echo '# ccr-agent-proxy-marker'; cat "$CA_BUNDLE"; } >> "$f"
    n=$((n + 1))
  done < <(find "$PLATFORMIO_CORE_DIR" "$VENV" -name cacert.pem 2>/dev/null)
  echo "  certifi バンドルに CA を追記: ${n} 件"
}
say "certifi バンドルにプロキシ CA を追記する"
patch_certifi

# -------------------------------------------------------------- 3. tool-scons
# PlatformIO Core 自身が使う tool-scons だけはレジストリからしか降ってこない。
# GitHub リリースの scons-local を展開し、.piopm を書いて「インストール済み」にする。
SCONS_DIR="$PLATFORMIO_CORE_DIR/packages/tool-scons"
if [ -f "$SCONS_DIR/scons.py" ]; then
  say "tool-scons は配置済み"
else
  say "tool-scons を GitHub リリースから配置する"
  mkdir -p "$SCONS_DIR"
  curl -fsSL --retry 5 --retry-all-errors -o "$SCONS_DIR/../scons-local.tar.gz" "$SCONS_URL"
  tar -xzf "$SCONS_DIR/../scons-local.tar.gz" -C "$SCONS_DIR"
  rm -f "$SCONS_DIR/../scons-local.tar.gz"
  cat > "$SCONS_DIR/package.json" <<EOF
{"name": "tool-scons", "version": "$SCONS_VERSION", "description": "SCons software construction tool", "license": "MIT"}
EOF
  cat > "$SCONS_DIR/.piopm" <<EOF
{"type": "tool", "name": "tool-scons", "version": "$SCONS_VERSION", "spec": {"owner": "platformio", "id": null, "name": "tool-scons", "requirements": null, "uri": "$SCONS_URL"}}
EOF
fi

# --------------------------------------------------------------- 4. platform
say "pioarduino 版 platform-espressif32 を入れる"
"$PIO" pkg install -g -p "$PLATFORM_URL"

# --------------------------------------------- 5. penv 作成とパッケージ先読み
# penv (pioarduino が作る専用 venv) は「最初のビルド」で作られ、そのときに
# 証明書が差し替わる。捨てプロジェクトを1回ビルドして penv を作らせ、
# CA を追記し直してからもう一度回して、ツールチェーンを全部落としておく。
BOOT="$PLATFORMIO_CORE_DIR/.bootstrap"
mkdir -p "$BOOT/src"
printf 'void setup() {}\nvoid loop() {}\n' > "$BOOT/src/main.cpp"
cat > "$BOOT/platformio.ini" <<EOF
[env:bootstrap]
platform = $PLATFORM_URL
platform_packages =
board = m5stack-atoms3
framework = arduino
EOF
say "penv を作らせる（1回目は証明書エラーで落ちてよい）"
"$PIO" run -d "$BOOT" >/dev/null 2>&1 || true
patch_certifi
say "ツールチェーンを取得する（約 5.3GB。数分かかる）"
for attempt in 1 2 3; do
  if "$PIO" run -d "$BOOT"; then break; fi
  echo "  取得に失敗（プロキシの 504 かもしれない）。再試行 $attempt/3"
  patch_certifi
done

# ----------------------------------------------------------------- 6. 環境変数
PIO_ENV_FILE="$PLATFORMIO_CORE_DIR/pio-env.sh"
cat > "$PIO_ENV_FILE" <<EOF
export PLATFORMIO_CORE_DIR="$PLATFORMIO_CORE_DIR"
export PATH="$VENV/bin:\$PATH"
EOF

say "準備完了"
cat <<EOF
  source "$PIO_ENV_FILE"
  cd tools/atoms3r-status-display
  pio run -c platformio-offline.ini -e m5stack-atoms3r

大きなファイルの取得でプロキシが 504 を返すことがある。一過性なので、
落ちたらもう一度 pio run を叩けば通る（pioarduino 側にも5回の再試行がある）。
EOF

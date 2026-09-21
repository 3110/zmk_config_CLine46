# この設定リポジトリは Zephyr モジュールとして読み込まれる。
# ビルドに足すのは有効にした機能のコードだけで、どちらも無効なビルド
# （settings_reset）では何も増えない。
if(CONFIG_CLINE46_STATUS_ADV OR CONFIG_CLINE46_STATUS_PERIPHERAL_VOLTAGE)
  zephyr_include_directories(include)
endif()

# 広告を出すのも、&status_adv でそれを切り替えるのも Central だけの仕事。
# キーマップを処理するのは Central だけなので、Peripheral 側にビヘイビアの
# 実体が無くても構わない（ZMK 本体の &ext_power などと同じ）
if(CONFIG_CLINE46_STATUS_ADV)
  target_sources(app PRIVATE
    src/status_adv.c
    src/behaviors/behavior_status_adv.c
  )
endif()

# 左手（送る側）と右手（受ける側）の両方でビルドする
if(CONFIG_CLINE46_STATUS_PERIPHERAL_VOLTAGE)
  target_sources(app PRIVATE src/peripheral_voltage.c)
endif()

# この設定リポジトリは Zephyr モジュールとして読み込まれる。
# ビルドに足すのはステータス広告のコードだけで、無効なビルド
# （左手・settings_reset）では何も増えない。
if(CONFIG_CLINE46_STATUS_ADV)
  zephyr_include_directories(include)
  target_sources(app PRIVATE src/status_adv.c)
endif()

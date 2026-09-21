# ステータスブロードキャスト（BLE 広告）

右手（Central）が自分の状態を BLE の**非接続広告**に載せて流します。受信側は
スキャンするだけでよく、接続もペアリングも要りません。BLE プロファイル（5個）も
`CONFIG_BT_MAX_CONN` も消費しないので、PC との接続や DYA Studio の動作に影響しません。

- 有効化: `CLine46_R.conf` の `CONFIG_CLINE46_STATUS_ADV=y`
- 実装: `src/status_adv.c` / 定義: `include/cline46/status_adv.h`
- 広告間隔: 操作中 1秒 / アイドル中 10秒（Kconfig で変更可）。ディープスリープ中は停止

## 仕組み

ZMK 自身もプロファイル用の広告と、`&studio_unlock` 後の directed advertising を
出します。レガシー広告は同時に1つしか出せないため、**拡張広告のセットをもう1つ
確保**して（`CONFIG_BT_EXT_ADV` / `BT_EXT_ADV_MAX_ADV_SET=2`）、そこにレガシーの
非接続 PDU を流しています。ZMK 側は従来どおりセット0を使うので、両者は独立です。

アドレスは非接続広告の既定（non-resolvable random address）で、一定時間ごとに
変わります。**受信側は MAC ではなくマジック（`0xFFFF` + `"CL"`）で絞り込んでください。**
同じ広告を出すキーボードが複数ある場合は `keyboard_id`（個体ごとに固定）で区別できます。

## キーでオン/オフする（`&status_adv`）

受信側（M5Stack）を使わないときは広告を出す意味が無いので、キーから止められます。

```dts
#include <behaviors/status_adv.dtsi>
#include <dt-bindings/cline46/status_adv.h>

// キーマップの中で
&status_adv SADV_TOG   // 押すたびに反転
&status_adv SADV_ON    // 出す
&status_adv SADV_OFF   // 止める
```

このリポジトリでは **SCROLL レイヤー（右手の上段、RESET の隣）** に
`&status_adv SADV_TOG` を置いてあります。DYA Studio のキーマップエディタにも
**Status Broadcast** として出るので、好きなキーに移せます。

- 止める直前に、`CLINE46_STATUS_FLAG_ADV_STOPPING` を立てた「お別れ」パケットを
  200ms 間隔で `CONFIG_CLINE46_STATUS_ADV_FAREWELL_MS`（既定1000ms）のあいだ流してから
  停止します。受信側はこれで**沈黙のタイムアウトを待たずに**「圏外」ではなく
  「意図的に止められた」と判断できます（ライブラリの `broadcastOff()`、
  AtomS3R 版は橙の電源マークの画面になります）。0 にすると即座に停止します
- お別れパケットが1つも届かなかった場合は、従来どおり沈黙から判断します
  （操作中なら約15秒、アイドル中なら最大45秒）
- オンに戻すと数秒で復帰します
- **切り替えた状態は保存され、次の起動でも引き継がれます**
  （`CONFIG_CLINE46_STATUS_ADV_PERSIST`、既定 y）。
  保存が無いときにどちらで始めるかは `CONFIG_CLINE46_STATUS_ADV_DEFAULT_ON`（既定 y）
- 保存は ZMK 本体と同じく `CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE` だけ遅らせて
  まとめます（ZMK の既定は60秒ですが、このリポジトリは `CLine46_R.conf` で
  **10秒**にしています）。フラッシュを余計に減らさないためですが、
  **切り替えてから10秒以内に電源を切ると保存前の状態に戻ります**
- 広告を出すのも切り替えを処理するのも右手（Central）です。ビヘイビアの locality は
  既定の `BEHAVIOR_LOCALITY_CENTRAL` なので、**左手のキーに割り当てても動きます**
- 消費電流の差はごくわずかです（1秒間隔・24バイトの非接続広告で、平均して数µA の
  オーダー）。**止める主な理由は電池ではなく、使わないときに平文の広告を
  出しっぱなしにしないこと**だと考えてください

非接続の広告には「誰かが受信しているか」を知る手段がありません（受信側は
スキャンするだけで、こちらには何も返しません）。そのため自動でのオン/オフは
作れず、明示的な切り替えにしてあります。

## 左手の電池電圧

ZMK の split が中継する電池情報は**残量(%)だけ**で、電圧は含まれていません
（`zmk_peripheral_battery_state_changed` の中身は `source` と `state_of_charge` のみ）。
そこで cormoran fork の汎用イベント中継（`CONFIG_ZMK_SPLIT_RELAY_EVENT`）に相乗りして、
左手が測った電圧をそのまま右手へ送っています（`src/peripheral_voltage.c`、中継の識別子は `clv`）。

- **左右の両方**で `CONFIG_CLINE46_STATUS_PERIPHERAL_VOLTAGE=y` が要ります
- 左手は電池を測ったタイミング（既定60秒ごと）にイベントを上げるだけで、**広告は出しません**。
  既存の split 接続に相乗りするので、電波も電池消費も増えません
- 左手が切断されると `peripheral_mv` は `0`（不明）に戻ります

## パケットの形

AD 構造は Manufacturer Specific Data（type `0xFF`）1つだけです。

```
AD length (1) | AD type 0xFF (1) | payload (24)  = 26 バイト（31 バイト以内）
```

payload（`struct cline46_status_adv_payload`、**すべてリトルエンディアン**）:

| offset | size | 名前 | 内容 |
|---|---|---|---|
| 0 | 2 | `company_id` | `0xFFFF`（SIG が内部用に予約している ID） |
| 2 | 2 | `magic` | `'C'`, `'L'` |
| 4 | 1 | `version` | ペイロード形式。現在 `2`（`1` は `peripheral_mv` が無い） |
| 5 | 1 | `keyboard_id` | 個体識別（hwinfo のデバイスIDの先頭1バイト） |
| 6 | 1 | `layer_index` | 最上位のアクティブレイヤー番号（0=BASE） |
| 7 | 4 | `layer_name` | `display-name` の先頭4文字。4文字ちょうどのときは**終端なし** |
| 11 | 2 | `central_mv` | 右手の電池電圧 mV（`0` = 不明） |
| 13 | 1 | `central_pct` | 右手の電池残量 %（`0xFF` = 不明） |
| 14 | 2 | `peripheral_mv` | 左手の電池電圧 mV（`0` = 不明・未接続） |
| 16 | 1 | `peripheral_pct` | 左手の電池残量 %（`0xFF` = 不明・未接続） |
| 17 | 1 | `os_default_layer` | 上位4bit = OS 判別結果、下位4bit = 既定レイヤー（`0x0F` = 未設定） |
| 18 | 1 | `profile` | bit7 接続済み / bit6 未ペアリング / bit2-0 プロファイル番号 |
| 19 | 1 | `flags` | 下表 |
| 20 | 2 | `uptime_min` | 起動からの経過分（65535 で頭打ち） |
| 22 | 1 | `reset_reason` | 下表 |
| 23 | 1 | `incident_count` | watchdog に残っている記録の件数 |

### flags

| bit | 意味 |
|---|---|
| 0 | USB から給電されている |
| 1 | USB HID が使える状態 |
| 2 | キー入力の出力先が BLE（落ちていれば USB） |
| 3 | **ZMK Studio がロック解除中** |
| 4 | 左手と接続できている |
| 5 | アイドル状態 |

### OS 判別結果（`os_default_layer >> 4`）

`0` unknown / `1` Windows / `2` macOS / `3` Linux / `4` iOS / `5` Android

### reset_reason

`0` 不明 / `1` 電源投入 / `2` リセットピン / `3` ソフトリセット（`&sys_reset`・書き込み）/
`4` **watchdog による再起動** / `5` 電圧低下 / `6` ディープスリープからの復帰 /
`7` デバッガ / `8` その他 / `9` **フリーズ検出** / `10` **フォールト**

基本は起動時の `hwinfo_get_reset_cause()` です。nRF52 は**電源投入ではどのビットも
立たない**（RESETREAS が 0 のまま）ので、値が取れて 0 だった場合は `1`（電源投入）
として扱います。`hwinfo` が `4`（watchdog）を返したときと、そもそも読めなかったときは、
watchdog に残っている一番新しい記録を見て `9`（フリーズ検出）/ `10`（フォールト）まで
絞り込みます。

## 受信側（M5Stack / ESP32）

受信用の Arduino ライブラリを [tools/CLine46Status](../tools/CLine46Status) に、
それを使ってシリアルに出すだけの PlatformIO プロジェクトを
[tools/m5stack-status-monitor](../tools/m5stack-status-monitor) に置いてあります。
ライブラリに表示は含めていないので、機種ごとの画面表示は別に書けます。

下は仕組みを示すための最小例です（実際にはライブラリを使うほうが簡単です）。

### 実装例

NimBLE-Arduino でのスキャン例です。`include/cline46/status_adv.h` をそのまま
コピーして使えます（Zephyr 依存はありません）。

```cpp
#include <NimBLEDevice.h>
#include "status_adv.h"   // include/cline46/status_adv.h をコピー

static cline46_status_adv_payload g_status;
static uint32_t g_last_seen_ms = 0;

class ScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* dev) override {
    if (!dev->haveManufacturerData()) return;
    std::string md = dev->getManufacturerData();
    if (md.size() < sizeof(cline46_status_adv_payload)) return;

    cline46_status_adv_payload p;
    memcpy(&p, md.data(), sizeof(p));
    if (p.company_id != CLINE46_STATUS_ADV_COMPANY_ID) return;
    if (p.magic[0] != CLINE46_STATUS_ADV_MAGIC_0) return;
    if (p.magic[1] != CLINE46_STATUS_ADV_MAGIC_1) return;
    if (p.version != CLINE46_STATUS_ADV_VERSION) return;

    g_status = p;
    g_last_seen_ms = millis();
  }
};

void setup() {
  NimBLEDevice::init("");
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(new ScanCallbacks(), /*wantDuplicates=*/true);
  scan->setActiveScan(false);   // 非接続広告なのでスキャン応答は要らない
  scan->setInterval(100);
  scan->setWindow(99);
  scan->start(0, false);        // 連続スキャン
}

void loop() {
  if (millis() - g_last_seen_ms > 5000) {
    // 5 秒受信が無い = キーボードがスリープ中か圏外
    return;
  }

  char layer[CLINE46_STATUS_LAYER_NAME_LEN + 1] = {0};
  memcpy(layer, g_status.layer_name, CLINE46_STATUS_LAYER_NAME_LEN);

  Serial.printf("%s  R:%umV/%u%%  L:%umV/%u%%  OS:%u  prof:%u  up:%umin\n",
                layer, g_status.central_mv, g_status.central_pct,
                g_status.peripheral_mv, g_status.peripheral_pct,
                g_status.os_default_layer >> 4,
                g_status.profile & CLINE46_STATUS_PROFILE_INDEX_MASK,
                g_status.uptime_min);
}
```

**注意**: `setScanCallbacks(..., wantDuplicates=true)` にしないと、同じアドレスからの
2 回目以降の広告が捨てられて更新が止まります。

## 制限と注意

- **左手の電圧は独自の中継**で運んでいます（上記）。左手のファームが古い（この機能が
  入っていない）場合は `peripheral_mv` が `0` のままになります。残量(%)は ZMK 標準の
  中継なので、そちらは従来どおり取れます
- **電圧は ZMK が定期取得した値の読み出し**です。`CONFIG_ZMK_BATTERY_REPORT_INTERVAL_S`
  （既定60秒）ごとにしか更新されず、起動直後の 1 回目までは `0`（不明）になります。
  広告のためだけに ADC を回さないのは電池を食わないためです。同じ理由で、最初の測定が
  終わるまでは残量(%)も `0xFF`（不明）にしています（`zmk_battery_state_of_charge()` は
  測定前に 0 を返すため、残量0%と区別が付かないので）
- **左手が切れると、覚えていた電圧と残量はその場で捨てます。** 再接続すると左手が
  数秒で送り直すので、古い値が一瞬だけ表示されることはありません
- **広告は平文**です。誰でも受信できるので、打鍵内容は載せていません。ただし
  `flags` の bit3（Studio ロック解除中）は「今このキーボードは設定変更を受け付ける」
  という情報でもあるので、それが気になる場合はこのビットを落としてください
- `reset_reason` は起動時の `hwinfo_get_reset_cause()` が基本で、そこが「不明」のときだけ
  watchdog の記録で補います（上記）。`zmk-feature-watchdog` も起動数秒後に原因をクリアする
  ため、こちらは `CONFIG_CLINE46_STATUS_ADV_INIT_PRIORITY`（既定50）で先に読んでいます

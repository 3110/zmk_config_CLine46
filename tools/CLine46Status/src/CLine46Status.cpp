/*
 * CLine46Status の実装。
 *
 * BLE のコールバックは NimBLE のタスクから呼ばれるため、そこでは受信した
 * 24 バイトを pending_ に置くだけにして、loop() から呼ばれる poll() で
 * current_ に取り込む。利用者のコールバックも poll() の文脈で呼ぶので、
 * 中で画面を描いても安全。
 *
 * SPDX-License-Identifier: MIT
 */

#include "CLine46Status.h"

#include <NimBLEDevice.h>
#include <string.h>

/* NimBLE-Arduino 2.x 以降には NimBLECppVersion.h がある。1.4 系には無い */
#if __has_include(<NimBLECppVersion.h>)
#define CL_NIMBLE_V2 1
#else
#define CL_NIMBLE_V2 0
#endif

/* 1.4 系の NimBLEAdvertisedDevice は const メソッドになっていない */
#if CL_NIMBLE_V2
typedef const NimBLEAdvertisedDevice CLAdvertisedDevice;
#else
typedef NimBLEAdvertisedDevice CLAdvertisedDevice;
#endif

namespace {

/* 受信した広告を CLine46Status へ渡すだけの薄いコールバック */
#if CL_NIMBLE_V2
class ScanBridge : public NimBLEScanCallbacks {
#else
class ScanBridge : public NimBLEAdvertisedDeviceCallbacks {
#endif
  public:
    explicit ScanBridge(CLine46Status &owner) : owner_(owner) {}

    void onResult(CLAdvertisedDevice *device) override {
        if (!device->haveManufacturerData()) {
            return;
        }
        std::string data = device->getManufacturerData();
        owner_.ingest(reinterpret_cast<const uint8_t *>(data.data()), data.size(),
                      device->getRSSI());
    }

  private:
    CLine46Status &owner_;
};

} // namespace

CLine46Status::CLine46Status()
    : has_pending_(false), last_seen_ms_(0), pending_rssi_(0), mismatch_version_(0),
      available_(false), lost_reported_(true), rssi_(0), timeout_ms_(DEFAULT_TIMEOUT_MS),
      scanning_(false), update_handler_(nullptr), lost_handler_(nullptr) {
    memset(&current_, 0, sizeof(current_));
    memset(&pending_, 0, sizeof(pending_));
    memset(layer_name_, 0, sizeof(layer_name_));
    current_.central_pct = CLINE46_STATUS_PCT_UNKNOWN;
    current_.peripheral_pct = CLINE46_STATUS_PCT_UNKNOWN;
}

bool CLine46Status::begin() {
    if (scanning_) {
        return true;
    }

    /* 他でも NimBLE を使っている場合は、そちらの初期化を尊重する。
     * 1.4 系は関数名も戻り値も違う（getInitialized / init は void） */
#if CL_NIMBLE_V2
    if (!NimBLEDevice::isInitialized() && !NimBLEDevice::init("")) {
        return false;
    }
#else
    if (!NimBLEDevice::getInitialized()) {
        NimBLEDevice::init("");
    }
#endif

    static ScanBridge *bridge = nullptr;
    if (bridge == nullptr) {
        bridge = new ScanBridge(*this);
    }

    NimBLEScan *scan = NimBLEDevice::getScan();
    if (scan == nullptr) {
        return false;
    }

    /* 非接続広告なのでスキャン応答は要らない。active scan にしないほうが
     * 受信側の消費電力も少ない */
    scan->setActiveScan(false);
    scan->setInterval(100);
    scan->setWindow(99);

#if CL_NIMBLE_V2
    /* 重複フィルタを切らないと、2回目以降の広告が捨てられて更新が止まる */
    scan->setScanCallbacks(bridge, /*wantDuplicates=*/true);
    scan->setDuplicateFilter(false);
    scanning_ = scan->start(0, false); /* 0 = 無期限 */
#else
    scan->setAdvertisedDeviceCallbacks(bridge, /*wantDuplicates=*/true);
    scanning_ = scan->start(0, nullptr, false);
#endif

    return scanning_;
}

void CLine46Status::end() {
    if (!scanning_) {
        return;
    }
    NimBLEScan *scan = NimBLEDevice::getScan();
    if (scan != nullptr) {
        scan->stop();
    }
    scanning_ = false;
}

void CLine46Status::ingest(const uint8_t *data, size_t size, int rssi) {
    if (data == nullptr || size < sizeof(cline46_status_adv_payload)) {
        return;
    }

    cline46_status_adv_payload payload;
    memcpy(&payload, data, sizeof(payload));

    /* 他人の機器を弾く。アドレスは定期的に変わるので MAC では絞れない */
    if (payload.company_id != CLINE46_STATUS_ADV_COMPANY_ID) {
        return;
    }
    if (payload.magic[0] != CLINE46_STATUS_ADV_MAGIC_0 ||
        payload.magic[1] != CLINE46_STATUS_ADV_MAGIC_1) {
        return;
    }

    if (payload.version != CLINE46_STATUS_ADV_VERSION) {
        /* 形式が違うものは取り込まない。利用者が気づけるよう記録だけする */
        mismatch_version_ = payload.version;
        return;
    }

    mismatch_version_ = 0;
    pending_ = payload;
    pending_rssi_ = rssi;
    last_seen_ms_ = millis();
    has_pending_ = true;
}

bool CLine46Status::poll() {
    bool updated = false;

    if (has_pending_) {
        has_pending_ = false;
        current_ = pending_;
        rssi_ = pending_rssi_;
        available_ = true;
        updated = true;

        memcpy(layer_name_, current_.layer_name, CLINE46_STATUS_LAYER_NAME_LEN);
        layer_name_[CLINE46_STATUS_LAYER_NAME_LEN] = '\0';

        if (lost_reported_) {
            lost_reported_ = false;
        }

        if (update_handler_ != nullptr) {
            update_handler_(*this);
        }
    }

    if (available_ && !lost_reported_ && !alive()) {
        lost_reported_ = true;
        if (lost_handler_ != nullptr) {
            lost_handler_(*this);
        }
    }

    return updated;
}

uint32_t CLine46Status::ageMs() const {
    if (!available_) {
        return UINT32_MAX;
    }
    return millis() - last_seen_ms_;
}

bool CLine46Status::alive() const { return available_ && ageMs() <= timeout_ms_; }

const char *CLine46Status::osName() const {
    switch (os()) {
    case CLINE46_STATUS_OS_WINDOWS:
        return "Windows";
    case CLINE46_STATUS_OS_MACOS:
        return "macOS";
    case CLINE46_STATUS_OS_LINUX:
        return "Linux";
    case CLINE46_STATUS_OS_IOS:
        return "iOS";
    case CLINE46_STATUS_OS_ANDROID:
        return "Android";
    default:
        return "unknown";
    }
}

int CLine46Status::defaultLayer() const {
    uint8_t layer = current_.os_default_layer & 0x0F;
    return layer == CLINE46_STATUS_DEFAULT_LAYER_NONE ? -1 : (int)layer;
}

const char *CLine46Status::resetReasonName() const {
    switch (current_.reset_reason) {
    case CLINE46_STATUS_RESET_POWER_ON:
        return "電源投入";
    case CLINE46_STATUS_RESET_PIN:
        return "リセットピン";
    case CLINE46_STATUS_RESET_SOFTWARE:
        return "ソフトリセット";
    case CLINE46_STATUS_RESET_WATCHDOG:
        return "watchdog";
    case CLINE46_STATUS_RESET_BROWNOUT:
        return "電圧低下";
    case CLINE46_STATUS_RESET_LOW_POWER_WAKE:
        return "スリープ復帰";
    case CLINE46_STATUS_RESET_DEBUG:
        return "デバッガ";
    case CLINE46_STATUS_RESET_OTHER:
        return "その他";
    case CLINE46_STATUS_RESET_FREEZE:
        return "フリーズ検出";
    case CLINE46_STATUS_RESET_FAULT:
        return "フォールト";
    default:
        return "不明";
    }
}

void CLine46Status::flagsText(char *buffer, size_t size) const {
    snprintf(buffer, size, "%c%c%c%c%c%c", usbPowered() ? 'U' : '-', usbHidReady() ? 'H' : '-',
             outputBle() ? 'B' : '-', studioUnlocked() ? 'S' : '-', splitConnected() ? 'L' : '-',
             idle() ? 'I' : '-');
}

void CLine46Status::batteryText(uint16_t mv, uint8_t percent, char *buffer, size_t size) {
    char percent_text[8];
    if (validPercent(percent)) {
        snprintf(percent_text, sizeof(percent_text), "%u%%", percent);
    } else {
        snprintf(percent_text, sizeof(percent_text), "--%%");
    }

    if (validMv(mv)) {
        snprintf(buffer, size, "%umV/%s", mv, percent_text);
    } else {
        snprintf(buffer, size, "----mV/%s", percent_text);
    }
}

void CLine46Status::printTo(Print &out) const {
    char flags[8];
    char right[24];
    char left[24];

    flagsText(flags, sizeof(flags));
    batteryText(current_.central_mv, current_.central_pct, right, sizeof(right));
    batteryText(current_.peripheral_mv, current_.peripheral_pct, left, sizeof(left));

    out.printf("L%u:%-4s  右 %s  左 %s  %s  OS:%s(既定%s)  prof:%u%s  稼働%umin  前回:%s",
               layerIndex(), layer_name_, right, left, flags, osName(),
               defaultLayer() < 0 ? "-" : String(defaultLayer()).c_str(), profileIndex(),
               profileConnected() ? "接続" : "未接続", uptimeMinutes(), resetReasonName());

    if (incidentCount() > 0) {
        out.printf("  記録%u件", incidentCount());
    }
    out.printf("  RSSI:%ddBm  ID:%02X\n", rssi_, keyboardId());
}

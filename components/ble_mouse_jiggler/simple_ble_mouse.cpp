#include "simple_ble_mouse.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace ble_mouse_jiggler {

static const char *const TAG = "ble_mouse_jiggler";

// HID Report Descriptor for a standard mouse
static const uint8_t hid_mouse_report_descriptor[] = {
    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x02,  // Usage (Mouse)
    0xA1, 0x01,  // Collection (Application)
    0x09, 0x01,  //   Usage (Pointer)
    0xA1, 0x00,  //   Collection (Physical)
    0x05, 0x09,  //     Usage Page (Button)
    0x19, 0x01,  //     Usage Minimum (Button 1)
    0x29, 0x03,  //     Usage Maximum (Button 3)
    0x15, 0x00,  //     Logical Minimum (0)
    0x25, 0x01,  //     Logical Maximum (1)
    0x95, 0x03,  //     Report Count (3)
    0x75, 0x01,  //     Report Size (1)
    0x81, 0x02,  //     Input (Data, Variable, Absolute)
    0x95, 0x01,  //     Report Count (1)
    0x75, 0x05,  //     Report Size (5)
    0x81, 0x03,  //     Input (Constant)
    0x05, 0x01,  //     Usage Page (Generic Desktop)
    0x09, 0x30,  //     Usage (X)
    0x09, 0x31,  //     Usage (Y)
    0x09, 0x38,  //     Usage (Wheel)
    0x15, 0x81,  //     Logical Minimum (-127)
    0x25, 0x7F,  //     Logical Maximum (127)
    0x75, 0x08,  //     Report Size (8)
    0x95, 0x03,  //     Report Count (3)
    0x81, 0x06,  //     Input (Data, Variable, Relative)
    0xC0,        //   End Collection
    0xC0         // End Collection
};

// PnP ID: Vendor ID Source, Vendor ID, Product ID, Product Version
static const uint8_t pnp_id[] = {0x02, 0x58, 0x25, 0x01, 0x00, 0x01, 0x00};

ServerCallbacks::ServerCallbacks(SimpleBLEMouse* mouse) : mouse_(mouse) {}

void ServerCallbacks::onConnect(BLEServer* pServer) {
    mouse_->onConnect();
}

void ServerCallbacks::onDisconnect(BLEServer* pServer) {
    mouse_->onDisconnect();
}

SimpleBLEMouse::SimpleBLEMouse(const std::string &device_name, const std::string &manufacturer, uint8_t battery_level)
    : device_name_(device_name), manufacturer_(manufacturer), battery_level_(battery_level) {}

void SimpleBLEMouse::begin() {
    BLEDevice::init(device_name_);
    p_server_ = BLEDevice::createServer();
    p_server_->setCallbacks(new ServerCallbacks(this));

    // HID Service
    BLEService* p_hid_service = p_server_->createService(BLEUUID((uint16_t)0x1812));

    p_hid_report_char_ = p_hid_service->createCharacteristic(
        BLEUUID((uint16_t)0x2A4D),
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    p_hid_report_char_->addDescriptor(new BLE2902());

    p_hid_service->createCharacteristic(BLEUUID((uint16_t)0x2A4B))
        ->setValue((uint8_t*)hid_mouse_report_descriptor, sizeof(hid_mouse_report_descriptor));

    // Device Information Service
    BLEService* p_dis_service = p_server_->createService(BLEUUID((uint16_t)0x180A));

    p_dis_service->createCharacteristic(BLEUUID((uint16_t)0x2A29), BLECharacteristic::PROPERTY_READ)
        ->setValue(manufacturer_);

    p_dis_service->createCharacteristic(BLEUUID((uint16_t)0x2A50), BLECharacteristic::PROPERTY_READ)
        ->setValue((uint8_t*)pnp_id, sizeof(pnp_id));

    // Battery Service
    BLEService* p_battery_service = p_server_->createService(BLEUUID((uint16_t)0x180F));

    p_battery_level_char_ = p_battery_service->createCharacteristic(
        BLEUUID((uint16_t)0x2A19),
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    p_battery_level_char_->addDescriptor(new BLE2902());
    setBatteryLevel(battery_level_);

    // Start services
    p_hid_service->start();
    p_dis_service->start();
    p_battery_service->start();

    // Advertising
    p_advertising_ = BLEDevice::getAdvertising();
    p_advertising_->addServiceUUID(p_hid_service->getUUID());
    p_advertising_->setAppearance(0x03C2); // Mouse appearance
    p_advertising_->start();

    ESP_LOGI(TAG, "BLE Mouse Jiggler started");
}

void SimpleBLEMouse::end() {
    // This can be expanded if needed
}

void SimpleBLEMouse::onConnect() {
    connected_ = true;
    ESP_LOGI(TAG, "Client connected");
}

void SimpleBLEMouse::onDisconnect() {
    connected_ = false;
    ESP_LOGI(TAG, "Client disconnected");
    // Restart advertising
    p_advertising_->start();
}


bool SimpleBLEMouse::isConnected() { return connected_; }

void SimpleBLEMouse::setBatteryLevel(uint8_t level) {
    battery_level_ = level;
    if (p_battery_level_char_ != nullptr) {
        p_battery_level_char_->setValue(&battery_level_, 1);
        if (connected_) {
            p_battery_level_char_->notify();
        }
    }
}

void SimpleBLEMouse::move(int8_t x, int8_t y, int8_t wheel) {
    if (!isConnected()) {
        return;
    }
    uint8_t report[4] = {0, (uint8_t)x, (uint8_t)y, (uint8_t)wheel};
    p_hid_report_char_->setValue(report, sizeof(report));
    p_hid_report_char_->notify();
}

void SimpleBLEMouse::press(uint8_t b) {
    if (!isConnected()) {
        return;
    }
    uint8_t report[4] = {b, 0, 0, 0};
    p_hid_report_char_->setValue(report, sizeof(report));
    p_hid_report_char_->notify();
}

void SimpleBLEMouse::release(uint8_t b) {
    if (!isConnected()) {
        return;
    }
    uint8_t report[4] = {0, 0, 0, 0};
    p_hid_report_char_->setValue(report, sizeof(report));
    p_hid_report_char_->notify();
}

void SimpleBLEMouse::click(uint8_t b) {
    press(b);
    delay(10);
    release(b);
}

} // namespace ble_mouse_jiggler
} // namespace esphome

#endif // USE_ESP32

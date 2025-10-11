#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// This is a workaround for a compilation error with ESP32 BLE Arduino library
#include "freertos/ringbuf.h"

namespace esphome {
namespace ble_mouse_jiggler {

class SimpleBLEMouse; // Forward declaration

class ServerCallbacks: public BLEServerCallbacks {
public:
    ServerCallbacks(SimpleBLEMouse* mouse);
    void onConnect(BLEServer* pServer) override;
    void onDisconnect(BLEServer* pServer) override;
private:
    SimpleBLEMouse* mouse_;
};

class SimpleBLEMouse {
public:
    SimpleBLEMouse(const std::string &device_name, const std::string &manufacturer, uint8_t battery_level);
    void begin();
    void end();
    void click(uint8_t b);
    void move(int8_t x, int8_t y, int8_t wheel = 0);
    void press(uint8_t b);
    void release(uint8_t b);
    bool isConnected();
    void setBatteryLevel(uint8_t level);

protected:
    friend class ServerCallbacks;
    void onConnect();
    void onDisconnect();

private:
    std::string device_name_;
    std::string manufacturer_;
    uint8_t battery_level_;
    bool connected_{false};

    BLEServer* p_server_{nullptr};
    BLECharacteristic* p_hid_report_char_{nullptr};
    BLECharacteristic* p_battery_level_char_{nullptr};
    BLEAdvertising* p_advertising_{nullptr};
};

} // namespace ble_mouse_jiggler
} // namespace esphome

#endif // USE_ESP32

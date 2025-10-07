#pragma once

// Simple wrapper for ESP32 BLE Mouse functionality
// This avoids external library dependencies

#ifdef USE_ESP32

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "nvs_flash.h"

class SimpleBLEMouse {
public:
    SimpleBLEMouse(const std::string& device_name = "ESP32 Mouse",
                   const std::string& manufacturer = "ESPHome",
                   uint8_t battery_level = 100);

    void begin();
    void end();
    bool isConnected();
    void move(int8_t x, int8_t y, int8_t wheel = 0);
    void click(uint8_t button = 1);
    void press(uint8_t button = 1);
    void release(uint8_t button = 1);

private:
    std::string device_name_;
    std::string manufacturer_;
    uint8_t battery_level_;
    bool connected_;

    void init_bluetooth_();
    void setup_hid_service_();
    void send_hid_report_(uint8_t* data, size_t length);

    static void gap_event_handler_(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param);
    static void gatts_event_handler_(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t* param);
};

#endif // USE_ESP32

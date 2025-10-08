#pragma once

// Enhanced BLE Mouse implementation supporting multiple instances
// This allows creating multiple virtual mice on one ESP32

#ifdef USE_ESP32

#include <string>
#include <vector>
#include <map>
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
                   uint8_t battery_level = 100,
                   uint8_t mouse_id = 0,
                   const std::string& pin_code = "");

    void begin();
    void end();
    bool isConnected();
    void move(int8_t x, int8_t y, int8_t wheel = 0);
    void click(uint8_t button = 1);
    void press(uint8_t button = 1);
    void release(uint8_t button = 1);

    uint8_t getMouseId() const { return mouse_id_; }
    std::string getDeviceName() const { return device_name_; }
    void setPinCode(const std::string& pin_code) { pin_code_ = pin_code; }
    bool hasPinCode() const { return !pin_code_.empty(); }

    // Pairing mode control
    void enablePairingMode(uint32_t duration_ms = 120000); // 2 minutes default
    void disablePairingMode();
    bool isPairingMode() const { return pairing_mode_; }

    // Static methods for managing multiple mice
    static void initBluetooth();
    static void deinitBluetooth();
    static SimpleBLEMouse* getMouseById(uint8_t mouse_id);
    static std::vector<SimpleBLEMouse*> getAllMice();

private:
    std::string device_name_;
    std::string manufacturer_;
    uint8_t battery_level_;
    uint8_t mouse_id_;
    bool connected_;
    std::string pin_code_;
    bool pairing_mode_; // New state variable for pairing mode

    // Instance-specific BLE handles
    uint16_t gatts_if_;
    uint16_t conn_id_;
    uint16_t service_handle_;
    uint16_t char_handle_;
    uint16_t app_id_;

    void setup_hid_service_();
    void add_hid_characteristics_();
    void setup_battery_service_();
    void send_hid_report_(uint8_t* data, size_t length);
    void start_advertising_();

    // Static management
    static std::map<uint8_t, SimpleBLEMouse*> mice_instances_;
    static std::map<uint16_t, SimpleBLEMouse*> app_to_mouse_map_;
    static bool bluetooth_initialized_;
    static uint16_t next_app_id_;

    static void gap_event_handler_(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param);
    static void gatts_event_handler_(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t* param);
};

#endif // USE_ESP32

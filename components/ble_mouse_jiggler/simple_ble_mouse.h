#pragma once

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

// Final, corrected state machine for service and characteristic creation
enum class CreationState {
    IDLE,
    CREATING_HID_SERVICE,
    STARTING_HID_SERVICE,
    ADDING_HID_INFO_CHAR,
    ADDING_HID_REPORT_MAP_CHAR,
    ADDING_HID_CONTROL_POINT_CHAR,
    ADDING_HID_REPORT_CHAR,
    ADDING_PROTOCOL_MODE_CHAR,
    CREATING_BATTERY_SERVICE,
    STARTING_BATTERY_SERVICE,
    ADDING_BATTERY_LEVEL_CHAR,
    CREATING_DIS_SERVICE,
    STARTING_DIS_SERVICE,
    ADDING_DIS_MANUFACTURER_CHAR,
    ADDING_DIS_MODEL_CHAR,
    ADDING_DIS_PNP_ID_CHAR,
    DONE
};

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
    void setBatteryLevel(uint8_t level);

    uint8_t getMouseId() const { return mouse_id_; }
    std::string getDeviceName() const { return device_name_; }

    static void initBluetooth();
    static SimpleBLEMouse* getMouseById(uint8_t mouse_id);

private:
    friend class BleMouseJiggler;

    std::string device_name_;
    std::string manufacturer_;
    uint8_t battery_level_;
    uint8_t mouse_id_;
    bool connected_;
    std::string pin_code_;

    uint16_t gatts_if_;
    uint16_t conn_id_;
    uint16_t app_id_;

    uint16_t hid_service_handle_;
    uint16_t battery_service_handle_;
    uint16_t dis_service_handle_;

    uint16_t hid_report_char_handle_;
    uint16_t battery_level_char_handle_;

    CreationState creation_state_;

    void execute_creation_step_();
    void send_hid_report_(uint8_t* data, size_t length);
    void start_advertising_();

    static std::map<uint8_t, SimpleBLEMouse*> mice_instances_;
    static std::map<uint16_t, SimpleBLEMouse*> app_to_mouse_map_;
    static bool bluetooth_initialized_;
    static uint16_t next_app_id_;

    static void gap_event_handler_(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param);
    static void gatts_event_handler_(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t* param);
};

#endif // USE_ESP32
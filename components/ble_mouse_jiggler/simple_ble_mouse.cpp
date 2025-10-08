#include "simple_ble_mouse.h"

#ifdef USE_ESP32

#include <string>
#include "esp_log.h"
#include <string.h>

static const char* TAG = "SimpleBLEMouse";

// HID Report Descriptor for Mouse
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
    0x81, 0x03,  //     Input (Constant, Variable, Absolute)
    0x05, 0x01,  //     Usage Page (Generic Desktop)
    0x09, 0x30,  //     Usage (X)
    0x09, 0x31,  //     Usage (Y)
    0x15, 0x81,  //     Logical Minimum (-127)
    0x25, 0x7F,  //     Logical Maximum (127)
    0x75, 0x08,  //     Report Size (8)
    0x95, 0x02,  //     Report Count (2)
    0x81, 0x06,  //     Input (Data, Variable, Relative)
    0x09, 0x38,  //     Usage (Wheel)
    0x15, 0x81,  //     Logical Minimum (-127)
    0x25, 0x7F,  //     Logical Maximum (127)
    0x75, 0x08,  //     Report Size (8)
    0x95, 0x01,  //     Report Count (1)
    0x81, 0x06,  //     Input (Data, Variable, Relative)
    0xC0,        //   End Collection
    0xC0         // End Collection
};

// Static member definitions
std::map<uint8_t, SimpleBLEMouse*> SimpleBLEMouse::mice_instances_;
std::map<uint16_t, SimpleBLEMouse*> SimpleBLEMouse::app_to_mouse_map_;
bool SimpleBLEMouse::bluetooth_initialized_ = false;
uint16_t SimpleBLEMouse::next_app_id_ = 0;

SimpleBLEMouse::SimpleBLEMouse(const std::string& device_name, const std::string& manufacturer, uint8_t battery_level, uint8_t mouse_id)
    : device_name_(device_name), manufacturer_(manufacturer), battery_level_(battery_level), mouse_id_(mouse_id), connected_(false),
      gatts_if_(ESP_GATT_IF_NONE), conn_id_(0), service_handle_(0), char_handle_(0), app_id_(next_app_id_++) {

    mice_instances_[mouse_id_] = this;
    app_to_mouse_map_[app_id_] = this;

    ESP_LOGI(TAG, "Created mouse instance %d: %s (app_id: %d)", mouse_id_, device_name_.c_str(), app_id_);
}

void SimpleBLEMouse::initBluetooth() {
    if (bluetooth_initialized_) {
        ESP_LOGW(TAG, "Bluetooth already initialized");
        return;
    }

    esp_err_t ret;

    ESP_LOGI(TAG, "Initializing Bluetooth for multiple mice");

    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Release classic BT memory
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    // Initialize BT controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "Initialize controller failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "Enable controller failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "Init bluetooth failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "Enable bluetooth failed: %s", esp_err_to_name(ret));
        return;
    }

    // Register callbacks
    esp_ble_gatts_register_callback(gatts_event_handler_);
    esp_ble_gap_register_callback(gap_event_handler_);

    bluetooth_initialized_ = true;
    ESP_LOGI(TAG, "Bluetooth initialized successfully");
}

void SimpleBLEMouse::deinitBluetooth() {
    if (!bluetooth_initialized_) return;

    ESP_LOGI(TAG, "Deinitializing Bluetooth");
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    bluetooth_initialized_ = false;
}

SimpleBLEMouse* SimpleBLEMouse::getMouseById(uint8_t mouse_id) {
    auto it = mice_instances_.find(mouse_id);
    return (it != mice_instances_.end()) ? it->second : nullptr;
}

std::vector<SimpleBLEMouse*> SimpleBLEMouse::getAllMice() {
    std::vector<SimpleBLEMouse*> mice;
    for (auto& pair : mice_instances_) {
        mice.push_back(pair.second);
    }
    return mice;
}

void SimpleBLEMouse::begin() {
    ESP_LOGI(TAG, "Starting mouse %d: %s", mouse_id_, device_name_.c_str());

    if (!bluetooth_initialized_) {
        initBluetooth();
    }

    // Register this mouse's GATT application
    esp_ble_gatts_app_register(app_id_);
}

void SimpleBLEMouse::end() {
    connected_ = false;
    ESP_LOGI(TAG, "Stopping mouse %d", mouse_id_);
}

bool SimpleBLEMouse::isConnected() {
    return connected_;
}

void SimpleBLEMouse::move(int8_t x, int8_t y, int8_t wheel) {
    if (!connected_) return;

    uint8_t report[4] = {0, (uint8_t)x, (uint8_t)y, (uint8_t)wheel};
    send_hid_report_(report, sizeof(report));
}

void SimpleBLEMouse::click(uint8_t button) {
    press(button);
    vTaskDelay(pdMS_TO_TICKS(10));
    release(button);
}

void SimpleBLEMouse::press(uint8_t button) {
    if (!connected_) return;

    uint8_t report[4] = {button, 0, 0, 0};
    send_hid_report_(report, sizeof(report));
}

void SimpleBLEMouse::release(uint8_t button) {
    if (!connected_) return;

    uint8_t report[4] = {0, 0, 0, 0};
    send_hid_report_(report, sizeof(report));
}

void SimpleBLEMouse::start_advertising_() {
    esp_ble_adv_params_t adv_params = {};
    adv_params.adv_int_min = 0x20;
    adv_params.adv_int_max = 0x40;
    adv_params.adv_type = ADV_TYPE_IND;
    adv_params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
    adv_params.channel_map = ADV_CHNL_ALL;
    adv_params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;

    // Set advertising data with device name
    esp_ble_adv_data_t adv_data = {};
    adv_data.set_scan_rsp = false;
    adv_data.include_name = true;
    adv_data.include_txpower = true;
    adv_data.min_interval = 0x20;
    adv_data.max_interval = 0x40;
    adv_data.appearance = 0x03C2; // Mouse appearance
    adv_data.manufacturer_len = 0;
    adv_data.p_manufacturer_data = nullptr;
    adv_data.service_data_len = 0;
    adv_data.p_service_data = nullptr;
    adv_data.service_uuid_len = 0;
    adv_data.p_service_uuid = nullptr;
    adv_data.flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);

    esp_ble_gap_config_adv_data(&adv_data);
    esp_ble_gap_start_advertising(&adv_params);

    ESP_LOGI(TAG, "Started advertising for mouse %d: %s", mouse_id_, device_name_.c_str());
}

void SimpleBLEMouse::gap_event_handler_(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            ESP_LOGD(TAG, "Advertising data set complete");
            break;
        default:
            break;
    }
}

void SimpleBLEMouse::gatts_event_handler_(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t* param) {
    SimpleBLEMouse* mouse = nullptr;

    // Find the mouse instance based on app_id or gatts_if
    if (event == ESP_GATTS_REG_EVT) {
        auto it = app_to_mouse_map_.find(param->reg.app_id);
        if (it != app_to_mouse_map_.end()) {
            mouse = it->second;
            mouse->gatts_if_ = gatts_if;
        }
    } else {
        // Find mouse by gatts_if
        for (auto& pair : mice_instances_) {
            if (pair.second->gatts_if_ == gatts_if) {
                mouse = pair.second;
                break;
            }
        }
    }

    if (!mouse) {
        ESP_LOGW(TAG, "No mouse found for event %d, gatts_if %d", event, gatts_if);
        return;
    }

    switch (event) {
        case ESP_GATTS_REG_EVT:
            ESP_LOGI(TAG, "GATT server registered for mouse %d, gatts_if %d", mouse->mouse_id_, gatts_if);
            mouse->setup_hid_service_();
            mouse->start_advertising_();
            break;
        case ESP_GATTS_CONNECT_EVT:
            mouse->conn_id_ = param->connect.conn_id;
            mouse->connected_ = true;
            ESP_LOGI(TAG, "Mouse %d (%s) connected", mouse->mouse_id_, mouse->device_name_.c_str());
            break;
        case ESP_GATTS_DISCONNECT_EVT: {
            mouse->connected_ = false;
            ESP_LOGI(TAG, "Mouse %d (%s) disconnected, restarting advertising", mouse->mouse_id_, mouse->device_name_.c_str());
            mouse->start_advertising_();
            break;
        }
        default:
            break;
    }
}

void SimpleBLEMouse::setup_hid_service_() {
    // Tworzenie serwisu HID BLE (UUID: 0x1812)
    esp_gatt_srvc_id_t hid_service_id = {};
    hid_service_id.is_primary = true;
    hid_service_id.id.inst_id = 0;
    hid_service_id.id.uuid.len = ESP_UUID_LEN_16;
    hid_service_id.id.uuid.uuid.uuid16 = 0x1812;

    esp_ble_gatts_create_service(gatts_if_, &hid_service_id, 8);

    // Dodanie charakterystyki HID Information
    esp_bt_uuid_t hid_info_uuid = {};
    hid_info_uuid.len = ESP_UUID_LEN_16;
    hid_info_uuid.uuid.uuid16 = 0x2A4A;
    uint8_t hid_info[4] = {0x01, 0x01, 0x00, 0x02}; // HID info: ver 1.1, country code 0, flags 2
    esp_attr_value_t hid_info_val = {};
    hid_info_val.attr_max_len = sizeof(hid_info);
    hid_info_val.attr_len = sizeof(hid_info);
    hid_info_val.attr_value = hid_info;
    esp_ble_gatts_add_char(service_handle_, &hid_info_uuid, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ, &hid_info_val, nullptr);

    // Dodanie charakterystyki HID Report Map
    esp_bt_uuid_t report_map_uuid = {};
    report_map_uuid.len = ESP_UUID_LEN_16;
    report_map_uuid.uuid.uuid16 = 0x2A4B;
    esp_attr_value_t report_map_val = {};
    report_map_val.attr_max_len = sizeof(hid_mouse_report_descriptor);
    report_map_val.attr_len = sizeof(hid_mouse_report_descriptor);
    report_map_val.attr_value = (uint8_t*)hid_mouse_report_descriptor;
    esp_ble_gatts_add_char(service_handle_, &report_map_uuid, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ, &report_map_val, nullptr);

    // Dodanie charakterystyki HID Control Point
    esp_bt_uuid_t control_point_uuid = {};
    control_point_uuid.len = ESP_UUID_LEN_16;
    control_point_uuid.uuid.uuid16 = 0x2A4C;
    uint8_t control_point = 0;
    esp_attr_value_t control_point_val = {};
    control_point_val.attr_max_len = sizeof(control_point);
    control_point_val.attr_len = sizeof(control_point);
    control_point_val.attr_value = &control_point;
    esp_ble_gatts_add_char(service_handle_, &control_point_uuid, ESP_GATT_PERM_WRITE, ESP_GATT_CHAR_PROP_BIT_WRITE_NR, &control_point_val, nullptr);

    // Dodanie charakterystyki HID Report
    esp_bt_uuid_t report_uuid = {};
    report_uuid.len = ESP_UUID_LEN_16;
    report_uuid.uuid.uuid16 = 0x2A4D;
    uint8_t report_val[4] = {0, 0, 0, 0};
    esp_attr_value_t report_attr_val = {};
    report_attr_val.attr_max_len = sizeof(report_val);
    report_attr_val.attr_len = sizeof(report_val);
    report_attr_val.attr_value = report_val;
    esp_ble_gatts_add_char(service_handle_, &report_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY, &report_attr_val, nullptr);

    // Dodanie charakterystyki Protocol Mode
    esp_bt_uuid_t protocol_mode_uuid = {};
    protocol_mode_uuid.len = ESP_UUID_LEN_16;
    protocol_mode_uuid.uuid.uuid16 = 0x2A4E;
    uint8_t protocol_mode = 1; // Report Protocol
    esp_attr_value_t protocol_mode_val = {};
    protocol_mode_val.attr_max_len = sizeof(protocol_mode);
    protocol_mode_val.attr_len = sizeof(protocol_mode);
    protocol_mode_val.attr_value = &protocol_mode;
    esp_ble_gatts_add_char(service_handle_, &protocol_mode_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE, &protocol_mode_val, nullptr);

    ESP_LOGI(TAG, "HID BLE service i charakterystyki utworzone dla myszy %d", mouse_id_);
}

void SimpleBLEMouse::send_hid_report_(uint8_t* data, size_t length) {
    if (!connected_ || gatts_if_ == ESP_GATT_IF_NONE) return;

    // In a full implementation, this would send the actual HID report
    // For now, this is a placeholder
    ESP_LOGV(TAG, "Mouse %d - Sending HID report: buttons=%d, x=%d, y=%d, wheel=%d",
             mouse_id_, data[0], (int8_t)data[1], (int8_t)data[2], (int8_t)data[3]);
}

#endif // USE_ESP32

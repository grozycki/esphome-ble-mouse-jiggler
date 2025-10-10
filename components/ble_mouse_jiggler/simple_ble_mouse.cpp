#include "simple_ble_mouse.h"

#ifdef USE_ESP32

#include <string>
#include "esp_log.h"
#include <string.h>

static const char* TAG = "SimpleBLEMouse";

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

// Static member definitions
std::map<uint8_t, SimpleBLEMouse*> SimpleBLEMouse::mice_instances_;
std::map<uint16_t, SimpleBLEMouse*> SimpleBLEMouse::app_to_mouse_map_;
bool SimpleBLEMouse::bluetooth_initialized_ = false;
uint16_t SimpleBLEMouse::next_app_id_ = 0;

SimpleBLEMouse::SimpleBLEMouse(const std::string& device_name, const std::string& manufacturer, uint8_t battery_level, uint8_t mouse_id, const std::string& pin_code)
    : device_name_(device_name), manufacturer_(manufacturer), battery_level_(battery_level), mouse_id_(mouse_id), connected_(false), pin_code_(pin_code),
      gatts_if_(ESP_GATT_IF_NONE), conn_id_(0), app_id_(next_app_id_++), hid_service_handle_(0), battery_service_handle_(0), dis_service_handle_(0),
      hid_report_char_handle_(0), battery_level_char_handle_(0), creation_state_(CreationState::IDLE) {
    mice_instances_[mouse_id_] = this;
    app_to_mouse_map_[app_id_] = this;
}

void SimpleBLEMouse::initBluetooth() {
    if (bluetooth_initialized_) return;
    ESP_LOGI(TAG, "Initializing Bluetooth");
    esp_bluedroid_init();
    esp_bluedroid_enable();
    esp_ble_gatts_register_callback(gatts_event_handler_);
    esp_ble_gap_register_callback(gap_event_handler_);
    bluetooth_initialized_ = true;
}

void SimpleBLEMouse::begin() {
    ESP_LOGI(TAG, "Starting mouse %d: %s", mouse_id_, device_name_.c_str());
    if (!bluetooth_initialized_) {
        initBluetooth();
    }
    esp_ble_gatts_app_register(app_id_);
}

void SimpleBLEMouse::gatts_event_handler_(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t* param) {
    SimpleBLEMouse* mouse = nullptr;
    if (event == ESP_GATTS_REG_EVT) {
        auto it = app_to_mouse_map_.find(param->reg.app_id);
        if (it != app_to_mouse_map_.end()) mouse = it->second;
    } else {
        for (auto& pair : mice_instances_) {
            if (pair.second->gatts_if_ == gatts_if) {
                mouse = pair.second;
                break;
            }
        }
    }
    if (!mouse) return;

    switch (event) {
        case ESP_GATTS_REG_EVT: {
            ESP_LOGI(TAG, "GATT app %d registered", mouse->app_id_);
            mouse->gatts_if_ = gatts_if;
            esp_ble_gap_set_device_name(mouse->device_name_.c_str());
            mouse->creation_state_ = CreationState::CREATING_HID_SERVICE;
            mouse->execute_creation_step_();
            break;
        }
        case ESP_GATTS_CREATE_EVT: {
            if (param->create.status == ESP_GATT_OK) {
                if (mouse->creation_state_ == CreationState::CREATING_HID_SERVICE) mouse->hid_service_handle_ = param->create.service_handle;
                else if (mouse->creation_state_ == CreationState::CREATING_BATTERY_SERVICE) mouse->battery_service_handle_ = param->create.service_handle;
                else if (mouse->creation_state_ == CreationState::CREATING_DIS_SERVICE) mouse->dis_service_handle_ = param->create.service_handle;
                mouse->execute_creation_step_();
            }
            break;
        }
        case ESP_GATTS_START_EVT: {
            if (param->start.status == ESP_GATT_OK) mouse->execute_creation_step_();
            break;
        }
        case ESP_GATTS_ADD_CHAR_EVT: {
            if (param->add_char.status == ESP_GATT_OK) {
                if (mouse->creation_state_ == CreationState::ADDING_HID_REPORT_CHAR) mouse->hid_report_char_handle_ = param->add_char.attr_handle;
                else if (mouse->creation_state_ == CreationState::ADDING_BATTERY_LEVEL_CHAR) mouse->battery_level_char_handle_ = param->add_char.attr_handle;
                mouse->execute_creation_step_();
            }
            break;
        }
        case ESP_GATTS_CONNECT_EVT: {
            mouse->connected_ = true;
            mouse->conn_id_ = param->connect.conn_id;
            ESP_LOGI(TAG, "Mouse %d connected", mouse->mouse_id_);
            esp_ble_gap_stop_advertising();
            break;
        }
        case ESP_GATTS_DISCONNECT_EVT: {
            mouse->connected_ = false;
            ESP_LOGI(TAG, "Mouse %d disconnected", mouse->mouse_id_);
            mouse->start_advertising_();
            break;
        }
        default: break;
    }
}

void SimpleBLEMouse::execute_creation_step_() {
    switch (creation_state_) {
        case CreationState::CREATING_HID_SERVICE: {
            esp_gatt_srvc_id_t srvc_id = {{ESP_UUID_LEN_16, {0x1812}}, true};
            esp_ble_gatts_create_service(gatts_if_, &srvc_id, 16);
            creation_state_ = CreationState::STARTING_HID_SERVICE;
            break;
        }
        case CreationState::STARTING_HID_SERVICE: esp_ble_gatts_start_service(hid_service_handle_); creation_state_ = CreationState::ADDING_HID_INFO_CHAR; break;
        case CreationState::ADDING_HID_INFO_CHAR: {
            uint8_t hid_info[] = {0x11, 0x01, 0x00, 0x02};
            esp_attr_value_t val = {sizeof(hid_info), sizeof(hid_info), hid_info};
            esp_bt_uuid_t uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A4A}};
            esp_ble_gatts_add_char(hid_service_handle_, &uuid, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ, &val, nullptr);
            creation_state_ = CreationState::ADDING_HID_REPORT_MAP_CHAR;
            break;
        }
        case CreationState::ADDING_HID_REPORT_MAP_CHAR: {
            esp_attr_value_t val = {sizeof(hid_mouse_report_descriptor), sizeof(hid_mouse_report_descriptor), (uint8_t*)hid_mouse_report_descriptor};
            esp_bt_uuid_t uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A4B}};
            esp_ble_gatts_add_char(hid_service_handle_, &uuid, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ, &val, nullptr);
            creation_state_ = CreationState::ADDING_HID_CONTROL_POINT_CHAR;
            break;
        }
        case CreationState::ADDING_HID_CONTROL_POINT_CHAR: {
            esp_bt_uuid_t uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A4C}};
            esp_ble_gatts_add_char(hid_service_handle_, &uuid, ESP_GATT_PERM_WRITE, ESP_GATT_CHAR_PROP_BIT_WRITE_NR, nullptr, nullptr);
            creation_state_ = CreationState::ADDING_HID_REPORT_CHAR;
            break;
        }
        case CreationState::ADDING_HID_REPORT_CHAR: {
            esp_bt_uuid_t uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A4D}};
            esp_ble_gatts_add_char(hid_service_handle_, &uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY, nullptr, nullptr);
            creation_state_ = CreationState::ADDING_PROTOCOL_MODE_CHAR;
            break;
        }
        case CreationState::ADDING_PROTOCOL_MODE_CHAR: {
            esp_bt_uuid_t uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A4E}};
            esp_ble_gatts_add_char(hid_service_handle_, &uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE, nullptr, nullptr);
            creation_state_ = CreationState::CREATING_BATTERY_SERVICE;
            break;
        }
        case CreationState::CREATING_BATTERY_SERVICE: {
            esp_gatt_srvc_id_t srvc_id = {{ESP_UUID_LEN_16, {0x180F}}, true};
            esp_ble_gatts_create_service(gatts_if_, &srvc_id, 4);
            creation_state_ = CreationState::STARTING_BATTERY_SERVICE;
            break;
        }
        case CreationState::STARTING_BATTERY_SERVICE: esp_ble_gatts_start_service(battery_service_handle_); creation_state_ = CreationState::ADDING_BATTERY_LEVEL_CHAR; break;
        case CreationState::ADDING_BATTERY_LEVEL_CHAR: {
            esp_bt_uuid_t uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A19}};
            esp_attr_value_t val = {1, 1, &this->battery_level_};
            esp_ble_gatts_add_char(battery_service_handle_, &uuid, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY, &val, nullptr);
            creation_state_ = CreationState::CREATING_DIS_SERVICE;
            break;
        }
        case CreationState::CREATING_DIS_SERVICE: {
            esp_gatt_srvc_id_t srvc_id = {{ESP_UUID_LEN_16, {0x180A}}, true};
            esp_ble_gatts_create_service(gatts_if_, &srvc_id, 10);
            creation_state_ = CreationState::STARTING_DIS_SERVICE;
            break;
        }
        case CreationState::STARTING_DIS_SERVICE: esp_ble_gatts_start_service(dis_service_handle_); creation_state_ = CreationState::ADDING_DIS_MANUFACTURER_CHAR; break;
        case CreationState::ADDING_DIS_MANUFACTURER_CHAR: {
            esp_attr_value_t val = {static_cast<uint16_t>(manufacturer_.length()), static_cast<uint16_t>(manufacturer_.length()), (uint8_t*)manufacturer_.c_str()};
            esp_bt_uuid_t uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A29}};
            esp_ble_gatts_add_char(dis_service_handle_, &uuid, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ, &val, nullptr);
            creation_state_ = CreationState::ADDING_DIS_MODEL_CHAR;
            break;
        }
        case CreationState::ADDING_DIS_MODEL_CHAR: {
            std::string model = "ESP32 BLE Mouse";
            esp_attr_value_t val = {static_cast<uint16_t>(model.length()), static_cast<uint16_t>(model.length()), (uint8_t*)model.c_str()};
            esp_bt_uuid_t uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A24}};
            esp_ble_gatts_add_char(dis_service_handle_, &uuid, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ, &val, nullptr);
            creation_state_ = CreationState::ADDING_DIS_PNP_ID_CHAR;
            break;
        }
        case CreationState::ADDING_DIS_PNP_ID_CHAR: {
            esp_attr_value_t val = {sizeof(pnp_id), sizeof(pnp_id), (uint8_t*)pnp_id};
            esp_bt_uuid_t uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A50}};
            esp_ble_gatts_add_char(dis_service_handle_, &uuid, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ, &val, nullptr);
            creation_state_ = CreationState::DONE;
            break;
        }
        case CreationState::DONE: ESP_LOGI(TAG, "All services created, starting advertising"); start_advertising_(); break;
        default: break;
    }
}

void SimpleBLEMouse::start_advertising_() {
    esp_ble_adv_data_t adv_data = {};
    adv_data.set_scan_rsp = false;
    adv_data.include_name = true;
    adv_data.include_txpower = true;
    adv_data.appearance = 0x03C2; // Generic Mouse
    adv_data.flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
    uint16_t service_uuid = 0x1812; // HID
    adv_data.service_uuid_len = sizeof(service_uuid);
    adv_data.p_service_uuid = (uint8_t*)&service_uuid;

    esp_ble_adv_params_t adv_params = {};
    adv_params.adv_int_min = 0x20;
    adv_params.adv_int_max = 0x40;
    adv_params.adv_type = ADV_TYPE_IND;
    adv_params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
    adv_params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;

    esp_ble_gap_config_adv_data(&adv_data);
    esp_ble_gap_start_advertising(&adv_params);
}

void SimpleBLEMouse::gap_event_handler_(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
    if (event == ESP_GAP_BLE_SEC_REQ_EVT) {
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
    } else if (event == ESP_GAP_BLE_AUTH_CMPL_EVT) {
        ESP_LOGI(TAG, "Pairing status: %s", param->ble_security.auth_cmpl.success ? "success" : "fail");
    }
}

void SimpleBLEMouse::send_hid_report_(uint8_t* data, size_t length) {
    if (!connected_) return;
    esp_ble_gatts_send_indicate(gatts_if_, conn_id_, hid_report_char_handle_, length, data, false);
}

void SimpleBLEMouse::setBatteryLevel(uint8_t level) {
    battery_level_ = level;
    if (connected_) {
        esp_ble_gatts_set_attr_value(battery_level_char_handle_, sizeof(battery_level_), &battery_level_);
    }
}

void SimpleBLEMouse::move(int8_t x, int8_t y, int8_t wheel) {
    uint8_t report[4] = {0, (uint8_t)x, (uint8_t)y, (uint8_t)wheel};
    send_hid_report_(report, sizeof(report));
}

void SimpleBLEMouse::click(uint8_t b) { press(b); vTaskDelay(10 / portTICK_PERIOD_MS); release(b); }
void SimpleBLEMouse::press(uint8_t b) { uint8_t report[4] = {b, 0, 0, 0}; send_hid_report_(report, sizeof(report)); }
void SimpleBLEMouse::release(uint8_t b) { uint8_t report[4] = {0, 0, 0, 0}; send_hid_report_(report, sizeof(report)); }
bool SimpleBLEMouse::isConnected() { return connected_; }
void SimpleBLEMouse::end() { }
SimpleBLEMouse* SimpleBLEMouse::getMouseById(uint8_t id) { auto it = mice_instances_.find(id); return (it != mice_instances_.end()) ? it->second : nullptr; }

#endif // USE_ESP32

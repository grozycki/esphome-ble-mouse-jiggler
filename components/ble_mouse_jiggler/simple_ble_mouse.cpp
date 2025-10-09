#include "simple_ble_mouse.h"

#ifdef USE_ESP32

#include <string>
#include <algorithm>
#include "esp_log.h"
#include <string.h>
#include "esp_system.h"

static const char* TAG = "SimpleBLEMouse";

// HID Report Descriptor for Mouse
static const uint8_t hid_mouse_report_descriptor[] = {
    0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01, 0xA1, 0x00, 0x05, 0x09, 0x19, 0x01, 0x29, 0x03, 0x15, 0x00, 0x25, 0x01, 0x95, 0x03, 0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x05, 0x81, 0x03, 0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x02, 0x81, 0x06, 0xC0, 0xC0
};

// PnP ID: Vendor ID Source (2 = USB), Vendor ID, Product ID, Product Version
static const uint8_t pnp_id[] = {0x02, 0x58, 0x25, 0x01, 0x00, 0x01, 0x00};

// Static member definitions
std::map<uint8_t, SimpleBLEMouse*> SimpleBLEMouse::mice_instances_;
std::map<uint16_t, SimpleBLEMouse*> SimpleBLEMouse::app_to_mouse_map_;
bool SimpleBLEMouse::bluetooth_initialized_ = false;
uint16_t SimpleBLEMouse::next_app_id_ = 0;

// Advertising parameters
static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// Service UUID for HID
static uint16_t hid_service_uuid = 0x1812;

// Advertising data configuration
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp       = false,
    .include_name       = true,
    .include_txpower    = true,
    .min_interval       = 0x20,
    .max_interval       = 0x40,
    .appearance         = 0x03C2, // Generic Mouse
    .manufacturer_len   = 0,
    .p_manufacturer_data = nullptr,
    .service_data_len   = 0,
    .p_service_data     = nullptr,
    .service_uuid_len   = sizeof(hid_service_uuid),
    .p_service_uuid     = (uint8_t*)&hid_service_uuid,
    .flag               = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

SimpleBLEMouse::SimpleBLEMouse(const std::string& device_name, const std::string& manufacturer, uint8_t battery_level, uint8_t mouse_id)
    : device_name_(device_name), manufacturer_(manufacturer), battery_level_(battery_level), mouse_id_(mouse_id), connected_(false),
      gatts_if_(ESP_GATT_IF_NONE), conn_id_(0), app_id_(next_app_id_++), hid_service_handle_(0), battery_service_handle_(0), dis_service_handle_(0),
      hid_report_char_handle_(0), battery_level_char_handle_(0), creation_state_(CreationState::IDLE) {

    mice_instances_[mouse_id_] = this;
    app_to_mouse_map_[app_id_] = this;
    ESP_LOGI(TAG, "Created mouse instance %d: %s (app_id: %d)", mouse_id_, device_name_.c_str(), app_id_);
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

void SimpleBLEMouse::gap_event_handler_(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT: {
            ESP_LOGI(TAG, "Advertising data set complete");
            esp_ble_gap_start_advertising(&adv_params);
            break;
        }
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT: {
            if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "Advertising started");
            } else {
                ESP_LOGE(TAG, "Failed to start advertising, status=%d", param->adv_start_cmpl.status);
            }
            break;
        }
        default:
            break;
    }
}

void SimpleBLEMouse::gatts_event_handler_(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t* param) {
    SimpleBLEMouse* mouse = nullptr;
    if (event == ESP_GATTS_REG_EVT) {
        auto it = app_to_mouse_map_.find(param->reg.app_id);
        if (it != app_to_mouse_map_.end()) {
            mouse = it->second;
            mouse->gatts_if_ = gatts_if;
        }
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
            esp_ble_gap_set_device_name(mouse->device_name_.c_str());
            mouse->setup_services_();
            break;
        }
        case ESP_GATTS_CREATE_EVT: {
            if (param->create.status != ESP_GATT_OK) {
                ESP_LOGE(TAG, "Service creation failed, status %d", param->create.status);
                break;
            }
            ESP_LOGI(TAG, "Service created, handle %d", param->create.service_handle);
            if (mouse->creation_state_ == CreationState::CREATING_HID_SERVICE) {
                mouse->hid_service_handle_ = param->create.service_handle;
                esp_ble_gatts_start_service(mouse->hid_service_handle_);
            } else if (mouse->creation_state_ == CreationState::CREATING_BATTERY_SERVICE) {
                mouse->battery_service_handle_ = param->create.service_handle;
                esp_ble_gatts_start_service(mouse->battery_service_handle_);
            } else if (mouse->creation_state_ == CreationState::CREATING_DIS_SERVICE) {
                mouse->dis_service_handle_ = param->create.service_handle;
                esp_ble_gatts_start_service(mouse->dis_service_handle_);
            }
            break;
        }
        case ESP_GATTS_START_EVT: {
            if (param->start.status != ESP_GATT_OK) {
                 ESP_LOGE(TAG, "Service start failed, status %d", param->start.status);
                break;
            }
            ESP_LOGI(TAG, "Service started, handle %d", param->start.service_handle);
            mouse->add_next_characteristic_();
            break;
        }
        case ESP_GATTS_ADD_CHAR_EVT: {
            if (param->add_char.status != ESP_GATT_OK) {
                ESP_LOGE(TAG, "Characteristic add failed, status %d, uuid 0x%04x", param->add_char.status, param->add_char.char_uuid.uuid.uuid16);
                break;
            }
            ESP_LOGD(TAG, "Characteristic added, handle %d, uuid 0x%04x", param->add_char.attr_handle, param->add_char.char_uuid.uuid.uuid16);
            if (param->add_char.char_uuid.uuid.uuid16 == 0x2A4D) { // HID Report
                mouse->hid_report_char_handle_ = param->add_char.attr_handle;
            } else if (param->add_char.char_uuid.uuid.uuid16 == 0x2A19) { // Battery Level
                mouse->battery_level_char_handle_ = param->add_char.attr_handle;
            }
            mouse->add_next_characteristic_();
            break;
        }
        case ESP_GATTS_CONNECT_EVT: {
            mouse->connected_ = true;
            mouse->conn_id_ = param->connect.conn_id;
            ESP_LOGI(TAG, "Mouse %d connected, conn_id %d", mouse->mouse_id_, mouse->conn_id_);
            esp_ble_gap_stop_advertising();
            break;
        }
        case ESP_GATTS_DISCONNECT_EVT: {
            mouse->connected_ = false;
            ESP_LOGI(TAG, "Mouse %d disconnected", mouse->mouse_id_);
            mouse->start_advertising_();
            break;
        }
        default:
            break;
    }
}

void SimpleBLEMouse::setup_services_() {
    ESP_LOGI(TAG, "Setting up services for mouse %d", mouse_id_);
    creation_state_ = CreationState::CREATING_HID_SERVICE;
    add_next_characteristic_();
}

void SimpleBLEMouse::add_next_characteristic_() {
    creation_state_ = (CreationState)((int)creation_state_ + 1);
    ESP_LOGI(TAG, "Advancing to state: %d", (int)creation_state_);

    switch (creation_state_) {
        case CreationState::CREATING_HID_SERVICE: {
            esp_gatt_srvc_id_t hid_service_id = {};
            hid_service_id.is_primary = true; hid_service_id.id.inst_id = 0; hid_service_id.id.uuid.len = ESP_UUID_LEN_16; hid_service_id.id.uuid.uuid.uuid16 = 0x1812;
            esp_ble_gatts_create_service(gatts_if_, &hid_service_id, 15);
            break;
        }
        case CreationState::ADDING_HID_INFO_CHAR: {
            uint8_t hid_info[] = {0x11, 0x01, 0x00, 0x02};
            esp_attr_value_t hid_info_val = {sizeof(hid_info), sizeof(hid_info), hid_info};
            esp_bt_uuid_t hid_info_uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A4A}};
            esp_ble_gatts_add_char(hid_service_handle_, &hid_info_uuid, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ, &hid_info_val, nullptr);
            break;
        }
        case CreationState::ADDING_HID_REPORT_MAP_CHAR: {
            esp_attr_value_t report_map_val = {sizeof(hid_mouse_report_descriptor), sizeof(hid_mouse_report_descriptor), (uint8_t*)hid_mouse_report_descriptor};
            esp_bt_uuid_t report_map_uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A4B}};
            esp_ble_gatts_add_char(hid_service_handle_, &report_map_uuid, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ, &report_map_val, nullptr);
            break;
        }
        case CreationState::ADDING_HID_CONTROL_POINT_CHAR: {
            esp_bt_uuid_t control_point_uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A4C}};
            esp_ble_gatts_add_char(hid_service_handle_, &control_point_uuid, ESP_GATT_PERM_WRITE, ESP_GATT_CHAR_PROP_BIT_WRITE_NR, nullptr, nullptr);
            break;
        }
        case CreationState::ADDING_HID_REPORT_CHAR: {
            esp_bt_uuid_t report_uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A4D}};
            esp_ble_gatts_add_char(hid_service_handle_, &report_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY, nullptr, nullptr);
            break;
        }
        case CreationState::CREATING_BATTERY_SERVICE: {
            esp_gatt_srvc_id_t battery_service_id = {};
            battery_service_id.is_primary = true; battery_service_id.id.inst_id = 0; battery_service_id.id.uuid.len = ESP_UUID_LEN_16; battery_service_id.id.uuid.uuid.uuid16 = 0x180F;
            esp_ble_gatts_create_service(gatts_if_, &battery_service_id, 5);
            break;
        }
        case CreationState::ADDING_BATTERY_LEVEL_CHAR: {
            esp_attr_value_t battery_level_val = {1, 1, &this->battery_level_};
            esp_bt_uuid_t battery_level_uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A19}};
            esp_ble_gatts_add_char(battery_service_handle_, &battery_level_uuid, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY, &battery_level_val, nullptr);
            break;
        }
        case CreationState::CREATING_DIS_SERVICE: {
            esp_gatt_srvc_id_t dis_service_id = {};
            dis_service_id.is_primary = true; dis_service_id.id.inst_id = 0; dis_service_id.id.uuid.len = ESP_UUID_LEN_16; dis_service_id.id.uuid.uuid.uuid16 = 0x180A;
            esp_ble_gatts_create_service(gatts_if_, &dis_service_id, 10);
            break;
        }
        case CreationState::ADDING_DIS_MANUFACTURER_CHAR: {
            esp_attr_value_t manufacturer_val = {(uint16_t)manufacturer_.length(), (uint16_t)manufacturer_.length(), (uint8_t*)manufacturer_.c_str()};
            esp_bt_uuid_t manufacturer_uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A29}};
            esp_ble_gatts_add_char(dis_service_handle_, &manufacturer_uuid, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ, &manufacturer_val, nullptr);
            break;
        }
        case CreationState::ADDING_DIS_MODEL_CHAR: {
            std::string model_number = "ESP32 BLE Mouse";
            esp_attr_value_t model_val = {(uint16_t)model_number.length(), (uint16_t)model_number.length(), (uint8_t*)model_number.c_str()};
            esp_bt_uuid_t model_uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A24}};
            esp_ble_gatts_add_char(dis_service_handle_, &model_uuid, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ, &model_val, nullptr);
            break;
        }
        case CreationState::ADDING_DIS_PNP_ID_CHAR: {
            esp_attr_value_t pnp_id_val = {sizeof(pnp_id), sizeof(pnp_id), (uint8_t*)pnp_id};
            esp_bt_uuid_t pnp_id_uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A50}};
            esp_ble_gatts_add_char(dis_service_handle_, &pnp_id_uuid, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ, &pnp_id_val, nullptr);
            break;
        }
        case CreationState::DONE: {
            ESP_LOGI(TAG, "All services and characteristics added.");
            start_advertising_();
            break;
        }
        default: 
            break;
    }
}

void SimpleBLEMouse::start_advertising_() {
    ESP_LOGI(TAG, "Configuring advertising for %s", device_name_.c_str());
    esp_ble_gap_config_adv_data(&adv_data);
}

void SimpleBLEMouse::send_hid_report_(uint8_t* data, size_t length) {
    if (!connected_) return;
    esp_ble_gatts_send_indicate(gatts_if_, conn_id_, hid_report_char_handle_, length, data, false);
}

void SimpleBLEMouse::setBatteryLevel(uint8_t level) {
    battery_level_ = level;
    if (connected_) {
        esp_ble_gatts_set_attr_value(battery_level_char_handle_, sizeof(battery_level_), &battery_level_);
        esp_ble_gatts_send_indicate(gatts_if_, conn_id_, battery_level_char_handle_, sizeof(battery_level_), &battery_level_, false);
    }
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

bool SimpleBLEMouse::isConnected() {
    return connected_;
}

void SimpleBLEMouse::end() {}

SimpleBLEMouse* SimpleBLEMouse::getMouseById(uint8_t mouse_id) {
    auto it = mice_instances_.find(mouse_id);
    return (it != mice_instances_.end()) ? it->second : nullptr;
}

#endif // USE_ESP32
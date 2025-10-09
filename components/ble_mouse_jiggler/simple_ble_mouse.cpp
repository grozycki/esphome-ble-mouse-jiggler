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

// PnP ID: Vendor ID Source, Vendor ID, Product ID, Product Version
static const uint8_t pnp_id[] = {0x02, 0x58, 0x25, 0x01, 0x00, 0x01, 0x00};

// Static member definitions
std::map<uint8_t, SimpleBLEMouse*> SimpleBLEMouse::mice_instances_;
std::map<uint16_t, SimpleBLEMouse*> SimpleBLEMouse::app_to_mouse_map_;
bool SimpleBLEMouse::bluetooth_initialized_ = false;
uint16_t SimpleBLEMouse::next_app_id_ = 0;
SimpleBLEMouse* SimpleBLEMouse::currently_advertising_mouse_ = nullptr;
std::vector<SimpleBLEMouse*> SimpleBLEMouse::advertising_queue_;
bool SimpleBLEMouse::advertising_active_ = false;

SimpleBLEMouse::SimpleBLEMouse(const std::string& device_name, const std::string& manufacturer, uint8_t battery_level, uint8_t mouse_id, const std::string& pin_code)
    : device_name_(device_name), manufacturer_(manufacturer), battery_level_(battery_level), mouse_id_(mouse_id), connected_(false), pin_code_(pin_code), pairing_mode_(false),
      gatts_if_(ESP_GATT_IF_NONE), conn_id_(0), app_id_(next_app_id_++), hid_service_handle_(0), battery_service_handle_(0), dis_service_handle_(0),
      hid_report_char_handle_(0), battery_level_char_handle_(0), service_creation_state_(ServiceCreationState::IDLE) {

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

    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
    uint8_t key_size = 16;
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;

    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

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
            ESP_LOGI(TAG, "Service created, status %d, handle %d", param->create.status, param->create.service_handle);
            if (param->create.status != ESP_GATT_OK) break;

            switch (mouse->service_creation_state_) {
                case ServiceCreationState::CREATING_HID_SERVICE:
                    mouse->hid_service_handle_ = param->create.service_handle;
                    esp_ble_gatts_start_service(mouse->hid_service_handle_);
                    break;
                case ServiceCreationState::CREATING_BATTERY_SERVICE:
                    mouse->battery_service_handle_ = param->create.service_handle;
                    esp_ble_gatts_start_service(mouse->battery_service_handle_);
                    break;
                case ServiceCreationState::CREATING_DIS_SERVICE:
                    mouse->dis_service_handle_ = param->create.service_handle;
                    esp_ble_gatts_start_service(mouse->dis_service_handle_);
                    break;
                default: break;
            }
            break;
        }
        case ESP_GATTS_START_EVT: {
            ESP_LOGI(TAG, "Service started, status %d, handle %d", param->start.status, param->start.service_handle);
            if (param->start.status != ESP_GATT_OK) break;

            switch (mouse->service_creation_state_) {
                case ServiceCreationState::CREATING_HID_SERVICE:
                    mouse->service_creation_state_ = ServiceCreationState::ADDING_HID_CHARS;
                    mouse->add_hid_characteristics_();
                    break;
                case ServiceCreationState::CREATING_BATTERY_SERVICE:
                    mouse->service_creation_state_ = ServiceCreationState::ADDING_BATTERY_CHARS;
                    mouse->add_battery_characteristic_();
                    break;
                case ServiceCreationState::CREATING_DIS_SERVICE:
                    mouse->service_creation_state_ = ServiceCreationState::ADDING_DIS_CHARS;
                    mouse->add_dis_characteristics_();
                    break;
                default: break;
            }
            break;
        }
        case ESP_GATTS_ADD_CHAR_EVT: {
            ESP_LOGD(TAG, "Characteristic added, status %d, handle %d", param->add_char.status, param->add_char.attr_handle);
            if (param->add_char.status != ESP_GATT_OK) break;

            switch (mouse->service_creation_state_) {
                case ServiceCreationState::ADDING_HID_CHARS:
                    if (param->add_char.char_uuid.uuid.uuid16 == 0x2A4D) { // HID Report
                        mouse->hid_report_char_handle_ = param->add_char.attr_handle;
                        ESP_LOGI(TAG, "HID Report Char Handle: %d", mouse->hid_report_char_handle_);
                        mouse->setup_battery_service_(); // Next step
                    }
                    break;
                case ServiceCreationState::ADDING_BATTERY_CHARS:
                    if (param->add_char.char_uuid.uuid.uuid16 == 0x2A19) { // Battery Level
                        mouse->battery_level_char_handle_ = param->add_char.attr_handle;
                        ESP_LOGI(TAG, "Battery Level Char Handle: %d", mouse->battery_level_char_handle_);
                        mouse->setup_dis_service_(); // Next step
                    }
                    break;
                case ServiceCreationState::ADDING_DIS_CHARS:
                    if (param->add_char.char_uuid.uuid.uuid16 == 0x2A50) { // PnP ID
                        ESP_LOGI(TAG, "All services and characteristics added for mouse %d", mouse->mouse_id_);
                        mouse->service_creation_state_ = ServiceCreationState::DONE;
                        mouse->start_advertising_();
                    }
                    break;
                default: break;
            }
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
    setup_hid_service_();
}

void SimpleBLEMouse::setup_hid_service_() {
    ESP_LOGI(TAG, "Creating HID service");
    service_creation_state_ = ServiceCreationState::CREATING_HID_SERVICE;
    esp_gatt_srvc_id_t hid_service_id = {};
    hid_service_id.is_primary = true;
    hid_service_id.id.inst_id = 0;
    hid_service_id.id.uuid.len = ESP_UUID_LEN_16;
    hid_service_id.id.uuid.uuid.uuid16 = 0x1812; // HID Service
    esp_ble_gatts_create_service(gatts_if_, &hid_service_id, 15);
}

void SimpleBLEMouse::add_hid_characteristics_() {
    ESP_LOGI(TAG, "Adding HID characteristics");
    // HID Information
    esp_bt_uuid_t hid_info_uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A4A}};
    uint8_t hid_info[] = {0x11, 0x01, 0x00, 0x02};
    esp_attr_value_t hid_info_val = {sizeof(hid_info), sizeof(hid_info), hid_info};
    esp_ble_gatts_add_char(hid_service_handle_, &hid_info_uuid, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ, &hid_info_val, nullptr);

    // HID Report Map
    esp_bt_uuid_t report_map_uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A4B}};
    esp_attr_value_t report_map_val = {sizeof(hid_mouse_report_descriptor), sizeof(hid_mouse_report_descriptor), (uint8_t*)hid_mouse_report_descriptor};
    esp_ble_gatts_add_char(hid_service_handle_, &report_map_uuid, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ, &report_map_val, nullptr);

    // HID Control Point
    esp_bt_uuid_t control_point_uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A4C}};
    esp_ble_gatts_add_char(hid_service_handle_, &control_point_uuid, ESP_GATT_PERM_WRITE, ESP_GATT_CHAR_PROP_BIT_WRITE_NR, nullptr, nullptr);

    // HID Report (Input)
    esp_bt_uuid_t report_uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A4D}};
    esp_ble_gatts_add_char(hid_service_handle_, &report_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY, nullptr, nullptr);
}

void SimpleBLEMouse::setup_battery_service_() {
    ESP_LOGI(TAG, "Creating Battery service");
    service_creation_state_ = ServiceCreationState::CREATING_BATTERY_SERVICE;
    esp_gatt_srvc_id_t battery_service_id = {};
    battery_service_id.is_primary = true;
    battery_service_id.id.inst_id = 0;
    battery_service_id.id.uuid.len = ESP_UUID_LEN_16;
    battery_service_id.id.uuid.uuid.uuid16 = 0x180F; // Battery Service
    esp_ble_gatts_create_service(gatts_if_, &battery_service_id, 4);
}

void SimpleBLEMouse::add_battery_characteristic_() {
    ESP_LOGI(TAG, "Adding Battery Level characteristic");
    esp_bt_uuid_t battery_level_uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A19}};
    esp_attr_value_t battery_level_val = {1, 1, &this->battery_level_};
    esp_ble_gatts_add_char(battery_service_handle_, &battery_level_uuid, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY, &battery_level_val, nullptr);
}

void SimpleBLEMouse::setup_dis_service_() {
    ESP_LOGI(TAG, "Creating Device Information service");
    service_creation_state_ = ServiceCreationState::CREATING_DIS_SERVICE;
    esp_gatt_srvc_id_t dis_service_id = {};
    dis_service_id.is_primary = true;
    dis_service_id.id.inst_id = 0;
    dis_service_id.id.uuid.len = ESP_UUID_LEN_16;
    dis_service_id.id.uuid.uuid.uuid16 = 0x180A; // Device Information Service
    esp_ble_gatts_create_service(gatts_if_, &dis_service_id, 10);
}

void SimpleBLEMouse::add_dis_characteristics_() {
    ESP_LOGI(TAG, "Adding Device Information characteristics");
    // Manufacturer Name
    esp_bt_uuid_t manufacturer_uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A29}};
    esp_attr_value_t manufacturer_val = {manufacturer_.length(), manufacturer_.length(), (uint8_t*)manufacturer_.c_str()};
    esp_ble_gatts_add_char(dis_service_handle_, &manufacturer_uuid, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ, &manufacturer_val, nullptr);

    // Model Number
    std::string model_number = "ESP32 BLE Mouse";
    esp_bt_uuid_t model_uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A24}};
    esp_attr_value_t model_val = {model_number.length(), model_number.length(), (uint8_t*)model_number.c_str()};
    esp_ble_gatts_add_char(dis_service_handle_, &model_uuid, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ, &model_val, nullptr);

    // PnP ID
    esp_bt_uuid_t pnp_id_uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A50}};
    esp_attr_value_t pnp_id_val = {sizeof(pnp_id), sizeof(pnp_id), (uint8_t*)pnp_id};
    esp_ble_gatts_add_char(dis_service_handle_, &pnp_id_uuid, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ, &pnp_id_val, nullptr);
}

void SimpleBLEMouse::start_advertising_() {
    ESP_LOGI(TAG, "Starting advertising for %s", device_name_.c_str());

    esp_ble_adv_data_t adv_data = {};
    adv_data.set_scan_rsp = false;
    adv_data.include_name = true;
    adv_data.include_txpower = true;
    adv_data.min_interval = 0x20;
    adv_data.max_interval = 0x40;
    adv_data.appearance = 0x03C2; // Generic Mouse
    adv_data.flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);

    esp_ble_adv_params_t adv_params = {};
    adv_params.adv_int_min = 0x20;
    adv_params.adv_int_max = 0x40;
    adv_params.adv_type = ADV_TYPE_IND;
    adv_params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
    adv_params.channel_map = ADV_CHNL_ALL;
    adv_params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;

    esp_ble_gap_config_adv_data(&adv_data);
    esp_ble_gap_start_advertising(&adv_params);
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

// Other methods (move, click, etc.) remain the same

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

// Dummy implementations for unused methods from the original complex code
void SimpleBLEMouse::end() {}
void SimpleBLEMouse::enablePairingMode(uint32_t duration_ms) {}
void SimpleBLEMouse::disablePairingMode() {}
SimpleBLEMouse* SimpleBLEMouse::getMouseById(uint8_t mouse_id) { return nullptr; }
std::vector<SimpleBLEMouse*> SimpleBLEMouse::getAllMice() { return {}; }
void SimpleBLEMouse::deinitBluetooth() {}
void SimpleBLEMouse::gap_event_handler_(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {}

#endif // USE_ESP32

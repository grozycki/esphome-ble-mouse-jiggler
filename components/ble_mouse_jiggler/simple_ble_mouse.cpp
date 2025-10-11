#include "simple_ble_mouse.h"

#ifdef USE_ESP32

#include <string>
#include "esp_log.h"
#include <string.h>
#include "nvs_flash.h"
#include "esp_system.h"

// Replace previous fallback random filler (used esp_random) with LFSR-based filler independent of esp_random
static void _fallback_fill_random(uint8_t *dst, size_t len) {
    uint32_t lfsr = 0xA5A55A5Au; // seed
    for (size_t i = 0; i < len; ++i) {
        // 32-bit Galois LFSR tap mask
        uint32_t lsb = lfsr & 1u;
        lfsr >>= 1;
        if (lsb) lfsr ^= 0xD0000001u;
        dst[i] = static_cast<uint8_t>(lfsr & 0xFFu);
    }
}

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
bool SimpleBLEMouse::adv_data_configured_ = false;
bool SimpleBLEMouse::advertising_active_ = false;
esp_ble_adv_params_t SimpleBLEMouse::adv_params_ = {};
uint16_t SimpleBLEMouse::adv_service_uuid_ = 0x1812;

SimpleBLEMouse::SimpleBLEMouse(const std::string& device_name, const std::string& manufacturer, uint8_t battery_level, uint8_t mouse_id, const std::string& pin_code)
    : device_name_(device_name), manufacturer_(manufacturer), battery_level_(battery_level), mouse_id_(mouse_id), connected_(false), pin_code_(pin_code),
      gatts_if_(ESP_GATT_IF_NONE), conn_id_(0), app_id_(next_app_id_++), hid_service_handle_(0), battery_service_handle_(0), dis_service_handle_(0),
      hid_report_char_handle_(0), battery_level_char_handle_(0), creation_state_(CreationState::IDLE) {
    mice_instances_[mouse_id_] = this;
    app_to_mouse_map_[app_id_] = this;
    ESP_LOGD(TAG, "SimpleBLEMouse instance created for mouse_id: %d, app_id: %d", mouse_id_, app_id_);
}

void SimpleBLEMouse::initBluetooth() {
    if (bluetooth_initialized_) {
        ESP_LOGD(TAG, "Bluetooth already initialized.");
        return;
    }
    ESP_LOGI(TAG, "[INIT] Starting full BLE init sequence");

    // Initialize NVS (required for bonding)
    esp_err_t ret_nvs = nvs_flash_init();
    if (ret_nvs == ESP_ERR_NVS_NO_FREE_PAGES || ret_nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated or new version, erasing...");
        nvs_flash_erase();
        ret_nvs = nvs_flash_init();
    }
    if (ret_nvs != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init NVS: %s", esp_err_to_name(ret_nvs));
    }

    ESP_LOGI(TAG, "Releasing Classic BT memory");
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

    ESP_LOGI(TAG, "Initializing BT controller");
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_bt_controller_init failed: %s", esp_err_to_name(ret));
        return;
    }
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_bt_controller_enable failed: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "Initializing Bluedroid");
    ret = esp_bluedroid_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_bluedroid_init failed: %s", esp_err_to_name(ret));
        return;
    }
    ret = esp_bluedroid_enable();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_bluedroid_enable failed: %s", esp_err_to_name(ret));
        return;
    }

    // Clear existing bonds (helps when device cached as different profile)
    int dev_num = esp_ble_get_bond_device_num();
    if (dev_num > 0) {
        ESP_LOGW(TAG, "Removing %d bonded device(s) to avoid stale pairing", dev_num);
        esp_ble_bond_dev_t *bonded = (esp_ble_bond_dev_t*)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
        if (bonded) {
            if (esp_ble_get_bond_device_list(&dev_num, bonded) == ESP_OK) {
                for (int i = 0; i < dev_num; ++i) {
                    esp_ble_remove_bond_device(bonded[i].bd_addr);
                }
            }
            free(bonded);
        }
    }

    // NOTE: Random static address disabled for Arduino / esp32_ble coexistence issues.
    // If you remove esp32_ble from YAML you can re-enable custom random address generation.
    // (Previous code using _fallback_fill_random and esp_ble_gap_set_rand_addr removed here.)

    ESP_LOGI(TAG, "Registering GATTS & GAP callbacks");
    ret = esp_ble_gatts_register_callback(gatts_event_handler_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_gatts_register_callback failed: %s", esp_err_to_name(ret));
        return;
    }
    ret = esp_ble_gap_register_callback(gap_event_handler_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_gap_register_callback failed: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "Configuring basic security");
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND;
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
    uint8_t key_size = 16;
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key  = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(auth_req));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(iocap));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(key_size));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(init_key));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(rsp_key));

    bluetooth_initialized_ = true;
    ESP_LOGI(TAG, "[INIT] BLE stack ready");
}

void SimpleBLEMouse::begin() {
    if (mouse_id_ > 0) {
        ESP_LOGW(TAG, "Multiple virtual mice not fully supported yet. Instance %d will be inert.", mouse_id_);
        return; // Only first instance active for now
    }
    ESP_LOGI(TAG, "Starting mouse %d: %s", mouse_id_, device_name_.c_str());
    if (!bluetooth_initialized_) {
        initBluetooth();
    }
    esp_err_t ret = esp_ble_gatts_app_register(app_id_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_gatts_app_register failed for app_id %d: %s", app_id_, esp_err_to_name(ret));
    } else {
        ESP_LOGD(TAG, "esp_ble_gatts_app_register called for app_id %d", app_id_);
    }
}

void SimpleBLEMouse::execute_creation_step_() {
    esp_err_t ret;
    ESP_LOGI(TAG, "State progress: %d", creation_state_); // was DEBUG
    switch (creation_state_) {
        case CreationState::CREATING_HID_SERVICE: {
            esp_gatt_srvc_id_t srvc_id = {{ESP_UUID_LEN_16, {0x1812}}, true};
            // Increase attribute capacity for extra descriptors
            ret = esp_ble_gatts_create_service(gatts_if_, &srvc_id, 20);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "esp_ble_gatts_create_service (HID) failed: %s", esp_err_to_name(ret));
            }
            creation_state_ = CreationState::STARTING_HID_SERVICE;
            break;
        }
        case CreationState::STARTING_HID_SERVICE:
            ret = esp_ble_gatts_start_service(hid_service_handle_);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "esp_ble_gatts_start_service (HID) failed: %s", esp_err_to_name(ret));
            }
            creation_state_ = CreationState::ADDING_HID_INFO_CHAR;
            break;
        case CreationState::ADDING_HID_INFO_CHAR: {
            uint8_t hid_info[] = {0x11, 0x01, 0x00, 0x02};
            esp_attr_value_t val = {sizeof(hid_info), sizeof(hid_info), hid_info};
            esp_bt_uuid_t uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A4A}};
            ret = esp_ble_gatts_add_char(hid_service_handle_, &uuid, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ, &val, nullptr);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "esp_ble_gatts_add_char (HID Info) failed: %s", esp_err_to_name(ret));
            }
            creation_state_ = CreationState::ADDING_HID_REPORT_MAP_CHAR;
            break;
        }
        case CreationState::ADDING_HID_REPORT_MAP_CHAR: {
            esp_attr_value_t val = {sizeof(hid_mouse_report_descriptor), sizeof(hid_mouse_report_descriptor), (uint8_t*)hid_mouse_report_descriptor};
            esp_bt_uuid_t uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A4B}};
            ret = esp_ble_gatts_add_char(hid_service_handle_, &uuid, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ, &val, nullptr);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "esp_ble_gatts_add_char (HID Report Map) failed: %s", esp_err_to_name(ret));
            }
            creation_state_ = CreationState::ADDING_HID_CONTROL_POINT_CHAR;
            break;
        }
        case CreationState::ADDING_HID_CONTROL_POINT_CHAR: {
            esp_bt_uuid_t uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A4C}};
            ret = esp_ble_gatts_add_char(hid_service_handle_, &uuid, ESP_GATT_PERM_WRITE, ESP_GATT_CHAR_PROP_BIT_WRITE_NR, nullptr, nullptr);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "esp_ble_gatts_add_char (HID Control Point) failed: %s", esp_err_to_name(ret));
            }
            creation_state_ = CreationState::ADDING_HID_REPORT_CHAR;
            break;
        }
        case CreationState::ADDING_HID_REPORT_CHAR: {
            esp_bt_uuid_t uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A4D}};
            ret = esp_ble_gatts_add_char(hid_service_handle_, &uuid,
                                         ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                         ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                                         nullptr, nullptr);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "esp_ble_gatts_add_char (HID Report) failed: %s", esp_err_to_name(ret));
            }
            creation_state_ = CreationState::ADDING_HID_REPORT_CCCD;
            break;
        }
        case CreationState::ADDING_HID_REPORT_CCCD: {
            // Add Client Characteristic Configuration Descriptor (CCCD) 0x2902
            uint8_t cccd_val[] = {0x00, 0x00}; // notifications disabled by default
            esp_attr_value_t val = {sizeof(cccd_val), sizeof(cccd_val), cccd_val};
            esp_bt_uuid_t uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2902}};
            ret = esp_ble_gatts_add_char_descr(hid_service_handle_, &uuid,
                                               ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                               &val, nullptr);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "esp_ble_gatts_add_char_descr (CCCD) failed: %s", esp_err_to_name(ret));
            }
            creation_state_ = CreationState::ADDING_HID_REPORT_REFERENCE;
            break;
        }
        case CreationState::ADDING_HID_REPORT_REFERENCE: {
            // Report Reference descriptor 0x2908: Report ID + Report Type (1 = Input)
            uint8_t ref_val[] = {0x01, 0x01};
            esp_attr_value_t val = {sizeof(ref_val), sizeof(ref_val), ref_val};
            esp_bt_uuid_t uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2908}};
            ret = esp_ble_gatts_add_char_descr(hid_service_handle_, &uuid, ESP_GATT_PERM_READ, &val, nullptr);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "esp_ble_gatts_add_char_descr (Report Reference) failed: %s", esp_err_to_name(ret));
            }
            creation_state_ = CreationState::ADDING_PROTOCOL_MODE_CHAR;
            break;
        }
        case CreationState::ADDING_PROTOCOL_MODE_CHAR: {
            esp_bt_uuid_t uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A4E}};
            uint8_t proto_mode[] = {0x01}; // Report Protocol
            esp_attr_value_t val = {sizeof(proto_mode), sizeof(proto_mode), proto_mode};
            ret = esp_ble_gatts_add_char(hid_service_handle_, &uuid,
                                         ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                         ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE,
                                         &val, nullptr);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "esp_ble_gatts_add_char (Protocol Mode) failed: %s", esp_err_to_name(ret));
            }
            creation_state_ = CreationState::CREATING_BATTERY_SERVICE;
            break;
        }
        case CreationState::CREATING_BATTERY_SERVICE: {
            esp_gatt_srvc_id_t srvc_id = {{ESP_UUID_LEN_16, {0x180F}}, true};
            ret = esp_ble_gatts_create_service(gatts_if_, &srvc_id, 4);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "esp_ble_gatts_create_service (Battery) failed: %s", esp_err_to_name(ret));
            }
            creation_state_ = CreationState::STARTING_BATTERY_SERVICE;
            break;
        }
        case CreationState::STARTING_BATTERY_SERVICE:
            ret = esp_ble_gatts_start_service(battery_service_handle_);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "esp_ble_gatts_start_service (Battery) failed: %s", esp_err_to_name(ret));
            }
            creation_state_ = CreationState::ADDING_BATTERY_LEVEL_CHAR;
            break;
        case CreationState::ADDING_BATTERY_LEVEL_CHAR: {
            esp_bt_uuid_t uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A19}};
            esp_attr_value_t val = {1, 1, &this->battery_level_};
            ret = esp_ble_gatts_add_char(battery_service_handle_, &uuid, ESP_GATT_PERM_READ,
                                         ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY, &val, nullptr);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "esp_ble_gatts_add_char (Battery Level) failed: %s", esp_err_to_name(ret));
            }
            creation_state_ = CreationState::CREATING_DIS_SERVICE;
            break;
        }
        case CreationState::CREATING_DIS_SERVICE: {
            esp_gatt_srvc_id_t srvc_id = {{ESP_UUID_LEN_16, {0x180A}}, true};
            ret = esp_ble_gatts_create_service(gatts_if_, &srvc_id, 10);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "esp_ble_gatts_create_service (DIS) failed: %s", esp_err_to_name(ret));
            }
            creation_state_ = CreationState::STARTING_DIS_SERVICE;
            break;
        }
        case CreationState::STARTING_DIS_SERVICE:
            ret = esp_ble_gatts_start_service(dis_service_handle_);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "esp_ble_gatts_start_service (DIS) failed: %s", esp_err_to_name(ret));
            }
            creation_state_ = CreationState::ADDING_DIS_MANUFACTURER_CHAR;
            break;
        case CreationState::ADDING_DIS_MANUFACTURER_CHAR: {
            esp_attr_value_t val = {static_cast<uint16_t>(manufacturer_.length()), static_cast<uint16_t>(manufacturer_.length()), (uint8_t*)manufacturer_.c_str()};
            esp_bt_uuid_t uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A29}};
            ret = esp_ble_gatts_add_char(dis_service_handle_, &uuid, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ, &val, nullptr);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "esp_ble_gatts_add_char (DIS Manufacturer) failed: %s", esp_err_to_name(ret));
            }
            creation_state_ = CreationState::ADDING_DIS_MODEL_CHAR;
            break;
        }
        case CreationState::ADDING_DIS_MODEL_CHAR: {
            std::string model = "ESP32 BLE Mouse";
            esp_attr_value_t val = {static_cast<uint16_t>(model.length()), static_cast<uint16_t>(model.length()), (uint8_t*)model.c_str()};
            esp_bt_uuid_t uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A24}};
            ret = esp_ble_gatts_add_char(dis_service_handle_, &uuid, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ, &val, nullptr);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "esp_ble_gatts_add_char (DIS Model) failed: %s", esp_err_to_name(ret));
            }
            creation_state_ = CreationState::ADDING_DIS_PNP_ID_CHAR;
            break;
        }
        case CreationState::ADDING_DIS_PNP_ID_CHAR: {
            esp_attr_value_t val = {sizeof(pnp_id), sizeof(pnp_id), (uint8_t*)pnp_id};
            esp_bt_uuid_t uuid = {ESP_UUID_LEN_16, {.uuid16 = 0x2A50}};
            ret = esp_ble_gatts_add_char(dis_service_handle_, &uuid, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ, &val, nullptr);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "esp_ble_gatts_add_char (DIS PnP ID) failed: %s", esp_err_to_name(ret));
            }
            creation_state_ = CreationState::DONE;
            break;
        }
        case CreationState::DONE:
            ESP_LOGI(TAG, "All services created (with HID descriptors), preparing advertising");
            start_advertising_();
            break;
        default:
            ESP_LOGW(TAG, "Unknown creation state: %d", creation_state_);
            break;
    }
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
    if (!mouse) {
        ESP_LOGE(TAG, "No mouse instance found for gatts_if %d or app_id %d for event %d", gatts_if, param->reg.app_id, event);
        return;
    }

    ESP_LOGD(TAG, "GATTS_EVENT: %d, gatts_if: %d, mouse_id: %d, app_id: %d", event, gatts_if, mouse->mouse_id_, mouse->app_id_);

    switch (event) {
        case ESP_GATTS_REG_EVT: {
            ESP_LOGI(TAG, "GATT app %d registered, status: %d", mouse->app_id_, param->reg.status);
            if (param->reg.status == ESP_GATT_OK) {
                mouse->gatts_if_ = gatts_if;
                esp_ble_gap_set_device_name(mouse->device_name_.c_str());
                mouse->creation_state_ = CreationState::CREATING_HID_SERVICE;
                mouse->execute_creation_step_();
            } else {
                ESP_LOGE(TAG, "GATT app %d registration failed with status %d", mouse->app_id_, param->reg.status);
            }
            break;
        }
        case ESP_GATTS_CREATE_EVT: {
            ESP_LOGD(TAG, "CREATE_EVT for service state %d, status: %d", mouse->creation_state_, param->create.status);
            if (param->create.status == ESP_GATT_OK) {
                if (mouse->creation_state_ == CreationState::CREATING_HID_SERVICE) {
                    mouse->hid_service_handle_ = param->create.service_handle;
                    ESP_LOGD(TAG, "HID Service created, handle: %d", mouse->hid_service_handle_);
                }
                else if (mouse->creation_state_ == CreationState::CREATING_BATTERY_SERVICE) {
                    mouse->battery_service_handle_ = param->create.service_handle;
                    ESP_LOGD(TAG, "Battery Service created, handle: %d", mouse->battery_service_handle_);
                }
                else if (mouse->creation_state_ == CreationState::CREATING_DIS_SERVICE) {
                    mouse->dis_service_handle_ = param->create.service_handle;
                    ESP_LOGD(TAG, "DIS Service created, handle: %d", mouse->dis_service_handle_);
                }
                mouse->execute_creation_step_();
            } else {
                ESP_LOGE(TAG, "Service creation failed for state %d with status %d", mouse->creation_state_, param->create.status);
            }
            break;
        }
        case ESP_GATTS_START_EVT: {
            ESP_LOGD(TAG, "START_EVT for service handle %d, status: %d", param->start.service_handle, param->start.status);
            if (param->start.status == ESP_GATT_OK) {
                mouse->execute_creation_step_();
            } else {
                ESP_LOGE(TAG, "Service start failed for handle %d with status %d", param->start.service_handle, param->start.status);
            }
            break;
        }
        case ESP_GATTS_ADD_CHAR_EVT: {
            ESP_LOGD(TAG, "ADD_CHAR_EVT for char UUID 0x%x, status: %d", param->add_char.char_uuid.uuid.uuid16, param->add_char.status);
            if (param->add_char.status == ESP_GATT_OK) {
                if (mouse->creation_state_ == CreationState::ADDING_HID_REPORT_CHAR) {
                    mouse->hid_report_char_handle_ = param->add_char.attr_handle;
                    ESP_LOGD(TAG, "HID Report Char added, handle: %d", mouse->hid_report_char_handle_);
                }
                else if (mouse->creation_state_ == CreationState::ADDING_BATTERY_LEVEL_CHAR) {
                    mouse->battery_level_char_handle_ = param->add_char.attr_handle;
                    ESP_LOGD(TAG, "Battery Level Char added, handle: %d", mouse->battery_level_char_handle_);
                }
                mouse->execute_creation_step_();
            } else {
                ESP_LOGE(TAG, "Add char failed for state %d with status %d", mouse->creation_state_, param->add_char.status);
            }
            break;
        }
        case ESP_GATTS_ADD_CHAR_DESCR_EVT: {
            ESP_LOGD(TAG, "ADD_CHAR_DESCR_EVT status=%d handle=%d", param->add_char_descr.status, param->add_char_descr.attr_handle);
            if (param->add_char_descr.status == ESP_GATT_OK) {
                if (mouse->creation_state_ == CreationState::ADDING_HID_REPORT_CCCD) {
                    mouse->hid_report_cccd_handle_ = param->add_char_descr.attr_handle; // store CCCD handle
                    mouse->creation_state_ = CreationState::ADDING_HID_REPORT_REFERENCE;
                } else if (mouse->creation_state_ == CreationState::ADDING_HID_REPORT_REFERENCE) {
                    // Report Reference added, proceed to Protocol Mode
                    mouse->creation_state_ = CreationState::ADDING_PROTOCOL_MODE_CHAR;
                }
                mouse->execute_creation_step_();
            } else {
                ESP_LOGE(TAG, "Descriptor add failed state %d status %d", mouse->creation_state_, param->add_char_descr.status);
            }
            break;
        }
        case ESP_GATTS_WRITE_EVT: {
            if (param->write.handle == mouse->hid_report_cccd_handle_) {
                if (param->write.len >= 2) {
                    uint16_t cccd = param->write.value[0] | (param->write.value[1] << 8);
                    ESP_LOGI(TAG, "CCCD updated: 0x%04X (notifications %s)", cccd, (cccd & 0x0001) ? "ENABLED" : "DISABLED");
                }
            }
            break;
        }
        case ESP_GATTS_CONNECT_EVT: {
            mouse->connected_ = true;
            mouse->conn_id_ = param->connect.conn_id;
            ESP_LOGI(TAG, "Mouse %d connected, conn_id: %d", mouse->mouse_id_, mouse->conn_id_);
            if (advertising_active_) {
                esp_ble_gap_stop_advertising();
                advertising_active_ = false;
            }
            break;
        }
        case ESP_GATTS_DISCONNECT_EVT: {
            mouse->connected_ = false;
            ESP_LOGI(TAG, "Mouse %d disconnected, reason: %d", mouse->mouse_id_, param->disconnect.reason);
            mouse->start_advertising_();
            break;
        }
        default:
            ESP_LOGD(TAG, "Unhandled GATTS_EVENT: %d", event);
            break;
    }
}

void SimpleBLEMouse::start_advertising_() {
    if (advertising_active_) {
        ESP_LOGD(TAG, "Advertising already active - skipping start.");
        return;
    }
    esp_err_t ret;

    if (!adv_data_configured_) {
        esp_ble_adv_data_t adv_data = {};
        adv_data.set_scan_rsp = false;
        adv_data.include_name = true;
        adv_data.include_txpower = true;
        adv_data.appearance = 0x03C2; // Mouse appearance
        adv_data.flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
        adv_data.service_uuid_len = sizeof(adv_service_uuid_);
        adv_data.p_service_uuid = (uint8_t*)&adv_service_uuid_;

        ret = esp_ble_gap_config_adv_data(&adv_data);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "esp_ble_gap_config_adv_data failed: %s", esp_err_to_name(ret));
            return;
        }
        ESP_LOGD(TAG, "Requested advertising data configuration.");
    } else {
        ret = esp_ble_gap_start_advertising(&adv_params_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "esp_ble_gap_start_advertising failed: %s", esp_err_to_name(ret));
            return;
        }
        advertising_active_ = true;
        ESP_LOGI(TAG, "Started advertising for mouse %d", mouse_id_);
    }
}

void SimpleBLEMouse::gap_event_handler_(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
    ESP_LOGI(TAG, "GAP event: %d", event); // was DEBUG
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT: {
            ESP_LOGI(TAG, "ADV data set complete status: %d", param->adv_data_cmpl.status); // was DEBUG
            if (param->adv_data_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                adv_data_configured_ = true;
                // Prepare advertising parameters if first time
                if (adv_params_.adv_int_min == 0) {
                    adv_params_ = {};
                    adv_params_.adv_int_min = 0x20;
                    adv_params_.adv_int_max = 0x40;
                    adv_params_.adv_type = ADV_TYPE_IND;
                    adv_params_.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
                    adv_params_.channel_map = ADV_CHNL_ALL; // ensure all channels
                    adv_params_.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
                }
                esp_err_t ret = esp_ble_gap_start_advertising(&adv_params_);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "esp_ble_gap_start_advertising failed after data set: %s", esp_err_to_name(ret));
                } else {
                    advertising_active_ = true;
                    ESP_LOGI(TAG, "BLE advertising started successfully.");
                }
            } else {
                ESP_LOGE(TAG, "Advertising data config failed status: %d", param->adv_data_cmpl.status);
            }
            break;
        }
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            ESP_LOGI(TAG, "ADV start complete status: %d", param->adv_start_cmpl.status); // was DEBUG
            break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            ESP_LOGI(TAG, "ADV stop complete status: %d", param->adv_stop_cmpl.status); // was DEBUG
            advertising_active_ = false;
            break;
        case ESP_GAP_BLE_SEC_REQ_EVT: {
            ESP_LOGI(TAG, "Security request received.");
            esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
            break;
        }
        case ESP_GAP_BLE_AUTH_CMPL_EVT: {
            ESP_LOGI(TAG, "Pairing status: %s", param->ble_security.auth_cmpl.success ? "success" : "fail");
            if (!param->ble_security.auth_cmpl.success) {
                ESP_LOGE(TAG, "Pairing failed, reason: %d", param->ble_security.auth_cmpl.fail_reason);
            }
            break;
        }
        default:
            ESP_LOGD(TAG, "Unhandled GAP_EVENT: %d", event);
            break;
    }
}

void SimpleBLEMouse::send_hid_report_(uint8_t* data, size_t length) {
    if (!connected_) {
        ESP_LOGW(TAG, "Attempted to send HID report while not connected.");
        return;
    }
    esp_err_t ret = esp_ble_gatts_send_indicate(gatts_if_, conn_id_, hid_report_char_handle_, length, data, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_gatts_send_indicate failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGD(TAG, "HID report sent successfully.");
    }
}

void SimpleBLEMouse::setBatteryLevel(uint8_t level) {
    battery_level_ = level;
    if (connected_) {
        esp_err_t ret = esp_ble_gatts_set_attr_value(battery_level_char_handle_, sizeof(battery_level_), &battery_level_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "esp_ble_gatts_set_attr_value failed: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGD(TAG, "Battery level set to %d.", level);
        }
    }
}

void SimpleBLEMouse::move(int8_t x, int8_t y, int8_t wheel) {
    uint8_t report[4] = {0, (uint8_t)x, (uint8_t)y, (uint8_t)wheel};
    send_hid_report_(report, sizeof(report));
    ESP_LOGD(TAG, "Mouse move: x=%d, y=%d, wheel=%d", x, y, wheel);
}

void SimpleBLEMouse::click(uint8_t b) { press(b); vTaskDelay(10 / portTICK_PERIOD_MS); release(b); ESP_LOGD(TAG, "Mouse click: button=%d", b); }
void SimpleBLEMouse::press(uint8_t b) { uint8_t report[4] = {b, 0, 0, 0}; send_hid_report_(report, sizeof(report)); ESP_LOGD(TAG, "Mouse press: button=%d", b); }
void SimpleBLEMouse::release(uint8_t b) { uint8_t report[4] = {0, 0, 0, 0}; send_hid_report_(report, sizeof(report)); ESP_LOGD(TAG, "Mouse release: button=%d", b); }
bool SimpleBLEMouse::isConnected() { return connected_; }
void SimpleBLEMouse::end() { ESP_LOGI(TAG, "Mouse %d ended.", mouse_id_); }
SimpleBLEMouse* SimpleBLEMouse::getMouseById(uint8_t id) { auto it = mice_instances_.find(id); return (it != mice_instances_.end()) ? it->second : nullptr; }

void SimpleBLEMouse::ensureAdvertising() {
    if (!bluetooth_initialized_) return;
    // Only manage first active instance for now
    auto it = mice_instances_.find(0);
    if (it == mice_instances_.end()) return;
    SimpleBLEMouse* m = it->second;
    if (!m) return;
    if (!m->connected_ && adv_data_configured_ && !advertising_active_) {
        ESP_LOGW(TAG, "Advertising was stopped unexpectedly. Restarting...");
        esp_err_t ret = esp_ble_gap_start_advertising(&adv_params_);
        if (ret == ESP_OK) {
            advertising_active_ = true;
            ESP_LOGI(TAG, "Advertising re-started.");
        } else {
            ESP_LOGE(TAG, "Failed to restart advertising: %s", esp_err_to_name(ret));
        }
    }
}

#endif // USE_ESP32

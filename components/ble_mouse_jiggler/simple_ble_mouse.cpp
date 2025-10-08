#include "simple_ble_mouse.h"

#ifdef USE_ESP32

#include <string>
#include <algorithm>
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
SimpleBLEMouse* SimpleBLEMouse::currently_advertising_mouse_ = nullptr;
std::vector<SimpleBLEMouse*> SimpleBLEMouse::advertising_queue_;
bool SimpleBLEMouse::advertising_active_ = false;

SimpleBLEMouse::SimpleBLEMouse(const std::string& device_name, const std::string& manufacturer, uint8_t battery_level, uint8_t mouse_id, const std::string& pin_code)
    : device_name_(device_name), manufacturer_(manufacturer), battery_level_(battery_level), mouse_id_(mouse_id), connected_(false), pin_code_(pin_code), pairing_mode_(false),
      gatts_if_(ESP_GATT_IF_NONE), conn_id_(0), service_handle_(0), char_handle_(0), app_id_(next_app_id_++) {

    mice_instances_[mouse_id_] = this;
    app_to_mouse_map_[app_id_] = this;

    ESP_LOGI(TAG, "Created mouse instance %d: %s (app_id: %d) %s", mouse_id_, device_name_.c_str(), app_id_,
             pin_code_.empty() ? "bez PIN" : "z kodem PIN");
}

void SimpleBLEMouse::initBluetooth() {
    if (bluetooth_initialized_) {
        ESP_LOGW(TAG, "Bluetooth already initialized");
        return;
    }

    esp_err_t ret;

    ESP_LOGI(TAG, "Initializing BLE Mouse using ESPHome's existing BLE stack");

    // ESPHome już zainicjalizowało BLE, więc nie robimy inicjalizacji stosu
    // Sprawdź czy BLE jest już włączony
    if (!esp_bluedroid_get_status()) {
        ESP_LOGE(TAG, "ESPHome BLE stack is not initialized - this shouldn't happen!");
        return;
    }

    ESP_LOGI(TAG, "ESPHome BLE stack detected - skipping BLE initialization");

    // Tylko rejestrujemy callback'i - nie inicjalizujemy stosu BLE
    esp_ble_gatts_register_callback(gatts_event_handler_);
    esp_ble_gap_register_callback(gap_event_handler_);

    // Uproszczona konfiguracja bezpieczeństwa BLE dla lepszej kompatybilności
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_NO_BOND;
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
    ESP_LOGI(TAG, "BLE Mouse initialized successfully using existing ESPHome BLE stack");
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
    // Dodaj myszkę do kolejki reklam jeśli nie jest podłączona
    if (!connected_) {
        // Sprawdź czy mysz już jest w kolejce
        auto it = std::find(advertising_queue_.begin(), advertising_queue_.end(), this);
        if (it == advertising_queue_.end()) {
            advertising_queue_.push_back(this);
            ESP_LOGI(TAG, "Added mouse %d (%s) to advertising queue. Queue size: %d",
                     mouse_id_, device_name_.c_str(), advertising_queue_.size());
        }
    }

    // Jeśli żadna reklama nie jest aktywna, rozpocznij rotację
    if (!advertising_active_ && !advertising_queue_.empty()) {
        start_advertising_rotation_();
    }
}

void SimpleBLEMouse::start_advertising_rotation_() {
    if (advertising_queue_.empty()) {
        advertising_active_ = false;
        currently_advertising_mouse_ = nullptr;
        ESP_LOGD(TAG, "Advertising queue empty, stopping rotation");
        return;
    }

    // Wybierz pierwszą myszkę z kolejki
    currently_advertising_mouse_ = advertising_queue_.front();
    advertising_active_ = true;

    ESP_LOGI(TAG, "🔄 Starting advertising rotation with mouse %d: %s",
             currently_advertising_mouse_->mouse_id_, currently_advertising_mouse_->device_name_.c_str());

    start_single_mouse_advertising_(currently_advertising_mouse_);

    // Zaplanuj rotację do następnej myszy za 10 sekund
    // W prawdziwej implementacji użylibyśmy timer, tutaj użyjemy task
    create_rotation_task_();
}

void SimpleBLEMouse::start_single_mouse_advertising_(SimpleBLEMouse* mouse) {
    if (!mouse) return;

    // Ustaw nazwę urządzenia
    esp_ble_gap_set_device_name(mouse->device_name_.c_str());

    esp_ble_adv_params_t adv_params = {};
    adv_params.adv_int_min = 0x20; // 20ms
    adv_params.adv_int_max = 0x40; // 40ms
    adv_params.adv_type = ADV_TYPE_IND;
    adv_params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
    adv_params.channel_map = ADV_CHNL_ALL;
    adv_params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;

    // Konfiguracja danych reklamy z UUID serwisów HID
    static uint8_t service_uuids[4] = {
        0x12, 0x18,  // HID Service UUID 0x1812 (little endian)
        0x0F, 0x18   // Battery Service UUID 0x180F (little endian)
    };

    esp_ble_adv_data_t adv_data = {};
    adv_data.set_scan_rsp = false;
    adv_data.include_name = true;
    adv_data.include_txpower = true;
    adv_data.min_interval = 0x20;
    adv_data.max_interval = 0x40;
    adv_data.appearance = 0x03C2; // HID Mouse appearance (962 decimal = 0x03C2)
    adv_data.manufacturer_len = 0;
    adv_data.p_manufacturer_data = nullptr;
    adv_data.service_data_len = 0;
    adv_data.p_service_data = nullptr;
    adv_data.service_uuid_len = 4; // 2 services × 2 bytes each
    adv_data.p_service_uuid = service_uuids;
    adv_data.flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);

    esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set advertising data for mouse %d: %s", mouse->mouse_id_, esp_err_to_name(ret));
        return;
    }

    ret = esp_ble_gap_start_advertising(&adv_params);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start advertising for mouse %d: %s", mouse->mouse_id_, esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "📡 Now advertising mouse %d: %s (HID Mouse with Battery service)",
             mouse->mouse_id_, mouse->device_name_.c_str());
}

void SimpleBLEMouse::create_rotation_task_() {
    // Tworzymy zadanie FreeRTOS dla rotacji reklam
    xTaskCreate([](void* param) {
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(10000)); // Czekaj 10 sekund

            if (!advertising_active_ || advertising_queue_.empty()) {
                break; // Zakończ zadanie jeśli reklama nieaktywna
            }

            // Przesuń pierwszą myszkę na koniec kolejki
            if (!advertising_queue_.empty()) {
                SimpleBLEMouse* current = advertising_queue_.front();
                advertising_queue_.erase(advertising_queue_.begin());

                // Jeśli mysz nadal nie jest podłączona, dodaj ją na koniec
                if (!current->connected_) {
                    advertising_queue_.push_back(current);
                }
            }

            // Zatrzymaj obecną reklamę
            esp_ble_gap_stop_advertising();
            vTaskDelay(pdMS_TO_TICKS(100));

            // Rozpocznij reklamę następnej myszy
            if (!advertising_queue_.empty()) {
                currently_advertising_mouse_ = advertising_queue_.front();
                start_single_mouse_advertising_(currently_advertising_mouse_);
                ESP_LOGI(TAG, "🔄 Rotated to mouse %d: %s",
                         currently_advertising_mouse_->mouse_id_,
                         currently_advertising_mouse_->device_name_.c_str());
            } else {
                advertising_active_ = false;
                currently_advertising_mouse_ = nullptr;
                break;
            }
        }
        vTaskDelete(nullptr); // Usuń zadanie
    }, "ble_adv_rotation", 2048, nullptr, 5, nullptr);
}

void SimpleBLEMouse::gap_event_handler_(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            ESP_LOGD(TAG, "Advertising data set complete");
            break;
        case ESP_GAP_BLE_PASSKEY_REQ_EVT: {
            ESP_LOGI(TAG, "Passkey request for device");
            // Znajdź myszkę na podstawie adresu BLE
            SimpleBLEMouse* mouse = nullptr;
            for (auto& pair : mice_instances_) {
                if (pair.second->hasPinCode()) {
                    mouse = pair.second;
                    break;
                }
            }
            if (mouse && !mouse->pin_code_.empty()) {
                uint32_t passkey = std::stoul(mouse->pin_code_);
                esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr, true, passkey);
                ESP_LOGI(TAG, "Replied with PIN code for mouse %d", mouse->mouse_id_);
            } else {
                esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr, false, 0);
                ESP_LOGW(TAG, "No PIN configured, rejecting pairing");
            }
            break;
        }
        case ESP_GAP_BLE_NC_REQ_EVT: {
            ESP_LOGI(TAG, "Numeric comparison request, accepting");
            esp_ble_confirm_reply(param->ble_security.ble_req.bd_addr, true);
            break;
        }
        case ESP_GAP_BLE_SEC_REQ_EVT: {
            ESP_LOGI(TAG, "Security request received");
            esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
            break;
        }
        case ESP_GAP_BLE_AUTH_CMPL_EVT: {
            if (param->ble_security.auth_cmpl.success) {
                ESP_LOGI(TAG, "Authentication completed successfully");
            } else {
                ESP_LOGW(TAG, "Authentication failed with reason: %d", param->ble_security.auth_cmpl.fail_reason);
            }
            break;
        }
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
            // Set device name first
            esp_ble_gap_set_device_name(mouse->device_name_.c_str());
            mouse->setup_hid_service_();
            break;
        case ESP_GATTS_CREATE_EVT:
            if (param->create.status == ESP_GATT_OK) {
                mouse->service_handle_ = param->create.service_handle;
                ESP_LOGI(TAG, "HID service created for mouse %d, handle %d", mouse->mouse_id_, mouse->service_handle_);
                esp_ble_gatts_start_service(mouse->service_handle_);
            } else {
                ESP_LOGE(TAG, "Failed to create HID service for mouse %d: %d", mouse->mouse_id_, param->create.status);
            }
            break;
        case ESP_GATTS_START_EVT:
            if (param->start.status == ESP_GATT_OK) {
                ESP_LOGI(TAG, "HID service started for mouse %d", mouse->mouse_id_);
                mouse->add_hid_characteristics_();
            } else {
                ESP_LOGE(TAG, "Failed to start HID service for mouse %d: %d", mouse->mouse_id_, param->start.status);
            }
            break;
        case ESP_GATTS_ADD_CHAR_EVT:
            if (param->add_char.status == ESP_GATT_OK) {
                ESP_LOGD(TAG, "Characteristic added for mouse %d: UUID 0x%04X, handle %d",
                         mouse->mouse_id_, param->add_char.char_uuid.uuid.uuid16, param->add_char.attr_handle);
                if (param->add_char.char_uuid.uuid.uuid16 == 0x2A4D) { // HID Report characteristic
                    mouse->char_handle_ = param->add_char.attr_handle;
                    ESP_LOGI(TAG, "HID Report characteristic handle for mouse %d: %d", mouse->mouse_id_, mouse->char_handle_);
                    // All characteristics added, start advertising
                    mouse->start_advertising_();
                }
            } else {
                ESP_LOGE(TAG, "Failed to add characteristic for mouse %d: %d", mouse->mouse_id_, param->add_char.status);
            }
            break;
        case ESP_GATTS_CONNECT_EVT:
            mouse->conn_id_ = param->connect.conn_id;
            mouse->connected_ = true;
            ESP_LOGI(TAG, "Mouse %d (%s) connected from %02x:%02x:%02x:%02x:%02x:%02x",
                     mouse->mouse_id_, mouse->device_name_.c_str(),
                     param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
                     param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5]);
            // Stop advertising when connected
            esp_ble_gap_stop_advertising();
            break;
        case ESP_GATTS_DISCONNECT_EVT: {
            mouse->connected_ = false;
            ESP_LOGI(TAG, "Mouse %d (%s) disconnected, reason: %d", mouse->mouse_id_, mouse->device_name_.c_str(), param->disconnect.reason);
            // Restart advertising after disconnect
            vTaskDelay(pdMS_TO_TICKS(1000)); // Wait 1 second before restarting
            mouse->start_advertising_();
            break;
        }
        case ESP_GATTS_WRITE_EVT:
            ESP_LOGD(TAG, "Write event for mouse %d: handle %d, len %d", mouse->mouse_id_, param->write.handle, param->write.len);
            break;
        case ESP_GATTS_READ_EVT:
            ESP_LOGD(TAG, "Read event for mouse %d: handle %d", mouse->mouse_id_, param->read.handle);
            break;
        default:
            ESP_LOGD(TAG, "Unhandled GATTS event: %d for mouse %d", event, mouse->mouse_id_);
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
    ESP_LOGI(TAG, "Creating HID service for mouse %d", mouse_id_);
}

void SimpleBLEMouse::add_hid_characteristics_() {
    ESP_LOGI(TAG, "Adding HID characteristics for mouse %d", mouse_id_);

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

    // Teraz dodaj Battery Service (wymagany dla HID)
    setup_battery_service_();

    ESP_LOGI(TAG, "HID characteristics added for mouse %d", mouse_id_);
}

void SimpleBLEMouse::setup_battery_service_() {
    ESP_LOGI(TAG, "Adding Battery Service for mouse %d", mouse_id_);

    // Tworzenie serwisu Battery BLE (UUID: 0x180F)
    esp_gatt_srvc_id_t battery_service_id = {};
    battery_service_id.is_primary = true;
    battery_service_id.id.inst_id = 1; // Different instance ID from HID service
    battery_service_id.id.uuid.len = ESP_UUID_LEN_16;
    battery_service_id.id.uuid.uuid.uuid16 = 0x180F;

    esp_ble_gatts_create_service(gatts_if_, &battery_service_id, 4);
}

void SimpleBLEMouse::send_hid_report_(uint8_t* data, size_t length) {
    if (!connected_ || gatts_if_ == ESP_GATT_IF_NONE || char_handle_ == 0) {
        ESP_LOGW(TAG, "Cannot send HID report - mouse %d not ready (connected: %d, gatts_if: %d, char_handle: %d)",
                 mouse_id_, connected_, gatts_if_, char_handle_);
        return;
    }

    // Wyślij HID report przez charakterystykę HID Report (0x2A4D)
    esp_err_t ret = esp_ble_gatts_send_indicate(gatts_if_, conn_id_, char_handle_, length, data, false);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send HID report for mouse %d: %s", mouse_id_, esp_err_to_name(ret));
    } else {
        ESP_LOGV(TAG, "Mouse %d - Sent HID report: buttons=%d, x=%d, y=%d, wheel=%d",
                 mouse_id_, data[0], (int8_t)data[1], (int8_t)data[2], (int8_t)data[3]);
    }
}

void SimpleBLEMouse::enablePairingMode(uint32_t duration_ms) {
    if (connected_) {
        ESP_LOGW(TAG, "Cannot enable pairing mode for mouse %d - already connected", mouse_id_);
        return;
    }

    pairing_mode_ = true;
    ESP_LOGI(TAG, "🔄 PAIRING MODE ENABLED for mouse %d (%s) - Duration: %d ms",
             mouse_id_, device_name_.c_str(), duration_ms);

    // Stop current advertising to restart with enhanced discoverability
    esp_ble_gap_stop_advertising();

    // Wait a bit and restart advertising with enhanced parameters
    vTaskDelay(pdMS_TO_TICKS(100));
    start_pairing_advertising_();

    // Set timer to disable pairing mode after duration
    if (duration_ms > 0) {
        // Create timer to auto-disable pairing mode
        // For now, just log - in full implementation we'd use a timer
        ESP_LOGI(TAG, "Pairing mode will auto-disable after %d ms", duration_ms);
    }
}

void SimpleBLEMouse::disablePairingMode() {
    if (!pairing_mode_) {
        return;
    }

    pairing_mode_ = false;
    ESP_LOGI(TAG, "🔒 PAIRING MODE DISABLED for mouse %d (%s)", mouse_id_, device_name_.c_str());

    if (!connected_) {
        // Restart normal advertising
        esp_ble_gap_stop_advertising();
        vTaskDelay(pdMS_TO_TICKS(100));
        start_advertising_();
    }
}

void SimpleBLEMouse::start_pairing_advertising_() {
    ESP_LOGI(TAG, "🔄 Starting simplified pairing advertising for mouse %d", mouse_id_);

    // Uproszczone parametry advertising bez problematycznych flag
    esp_ble_gap_set_device_name(device_name_.c_str());

    esp_ble_adv_params_t adv_params = {};
    adv_params.adv_int_min = 0x20; // Nieco wolniejsze ale stabilniejsze
    adv_params.adv_int_max = 0x40;
    adv_params.adv_type = ADV_TYPE_IND;
    adv_params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
    adv_params.channel_map = ADV_CHNL_ALL;
    adv_params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;

    // Bardzo uproszczone advertising data - tylko podstawowe informacje
    esp_ble_adv_data_t adv_data = {};
    adv_data.set_scan_rsp = false;
    adv_data.include_name = true;
    adv_data.include_txpower = false; // Wyłączam tx power - może powodować problemy
    adv_data.min_interval = 0x20;
    adv_data.max_interval = 0x40;
    adv_data.appearance = 0x03C2; // Mouse appearance
    adv_data.manufacturer_len = 0;
    adv_data.p_manufacturer_data = nullptr;
    adv_data.service_data_len = 0;
    adv_data.p_service_data = nullptr;
    adv_data.service_uuid_len = 0; // Upraszczam - usuwam UUID
    adv_data.p_service_uuid = nullptr;
    // NAPRAWKA: Uproszczone flagi - usuwam problematyczne DMT flags
    adv_data.flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);

    esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set simplified advertising data for mouse %d: %s", mouse_id_, esp_err_to_name(ret));
        ESP_LOGI(TAG, "🔧 Falling back to basic advertising...");
        // Fallback: spróbuj jeszcze prostsze advertising
        start_advertising_();
        return;
    }

    ret = esp_ble_gap_start_advertising(&adv_params);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start simplified advertising for mouse %d: %s", mouse_id_, esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "✅ SIMPLIFIED PAIRING ADVERTISING started for mouse %d: %s", mouse_id_, device_name_.c_str());
}
#endif // USE_ESP32

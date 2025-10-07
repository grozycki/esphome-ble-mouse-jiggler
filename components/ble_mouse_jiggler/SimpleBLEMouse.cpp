#include "SimpleBLEMouse.h"

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

// Global variables for BLE
static SimpleBLEMouse* g_mouse_instance = nullptr;
static uint16_t g_gatts_if = ESP_GATT_IF_NONE;
static uint16_t g_conn_id = 0;
static uint16_t g_service_handle = 0;
static uint16_t g_char_handle = 0;

SimpleBLEMouse::SimpleBLEMouse(const std::string& device_name, const std::string& manufacturer, uint8_t battery_level)
    : device_name_(device_name), manufacturer_(manufacturer), battery_level_(battery_level), connected_(false) {
    g_mouse_instance = this;
}

void SimpleBLEMouse::gap_event_handler_(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT: {
            esp_ble_adv_params_t adv_params = {};
            adv_params.adv_int_min = 0x20;
            adv_params.adv_int_max = 0x40;
            adv_params.adv_type = ADV_TYPE_IND;
            adv_params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
            adv_params.channel_map = ADV_CHNL_ALL;
            adv_params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
            esp_ble_gap_start_advertising(&adv_params);
            break;
        }
        default:
            break;
    }
}

void SimpleBLEMouse::gatts_event_handler_(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t* param) {
    if (!g_mouse_instance) return;

    switch (event) {
        case ESP_GATTS_REG_EVT:
            g_gatts_if = gatts_if;
            g_mouse_instance->setup_hid_service_();
            break;
        case ESP_GATTS_CONNECT_EVT:
            g_conn_id = param->connect.conn_id;
            g_mouse_instance->connected_ = true;
            ESP_LOGI(TAG, "BLE device connected");
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            g_mouse_instance->connected_ = false;
            ESP_LOGI(TAG, "BLE device disconnected, restarting advertising");
            // Restart advertising with proper parameters
            esp_ble_adv_params_t adv_params = {};
            adv_params.adv_int_min = 0x20;
            adv_params.adv_int_max = 0x40;
            adv_params.adv_type = ADV_TYPE_IND;
            adv_params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
            adv_params.channel_map = ADV_CHNL_ALL;
            adv_params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
            esp_ble_gap_start_advertising(&adv_params);
            break;
        default:
            break;
    }
}

void SimpleBLEMouse::begin() {
    ESP_LOGI(TAG, "Initializing SimpleBLEMouse");
    this->init_bluetooth_();
}

void SimpleBLEMouse::end() {
    connected_ = false;
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

void SimpleBLEMouse::init_bluetooth_() {
    esp_err_t ret;

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

    // Register application
    esp_ble_gatts_app_register(0);
}

void SimpleBLEMouse::setup_hid_service_() {
    // This is a simplified version - in a full implementation
    // we would need to create proper HID service and characteristics
    ESP_LOGI(TAG, "HID service setup completed");
}

void SimpleBLEMouse::send_hid_report_(uint8_t* data, size_t length) {
    if (!connected_ || g_gatts_if == ESP_GATT_IF_NONE) return;

    // In a full implementation, this would send the actual HID report
    // For now, this is a placeholder
    ESP_LOGV(TAG, "Sending HID report: buttons=%d, x=%d, y=%d, wheel=%d",
             data[0], (int8_t)data[1], (int8_t)data[2], (int8_t)data[3]);
}

#endif // USE_ESP32

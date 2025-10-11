#include "ble_mouse.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifdef USE_ESP32

namespace esphome {
namespace ble_mouse_jiggler {

static const char *const TAG = "ble_mouse_jiggler";

// HID Report Descriptor for a standard mouse
static const uint8_t HID_MOUSE_REPORT_MAP[] = {
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

static const uint8_t PNP_ID[] = {0x02, 0x58, 0x25, 0x01, 0x00, 0x01, 0x00};

void BleMouseJiggler::setup() {
    ESP_LOGI(TAG, "Setting up BLE Mouse Jiggler '%s'...", this->device_name_.c_str());

    this->hub_->register_callbacks(this);
    this->hub_->set_name(this->device_name_);
    this->hub_->set_manufacturer(this->manufacturer_);

    // HID Service
    this->hid_service_ = this->hub_->create_service(ESP_GATT_UUID_HID_SVC);

    this->hid_service_->create_characteristic(ESP_GATT_UUID_HID_REPORT_MAP_CHR, ESP_GATT_CHAR_PROP_BIT_READ)
        ->set_value((uint8_t *) HID_MOUSE_REPORT_MAP, sizeof(HID_MOUSE_REPORT_MAP));

    this->input_report_char_ = this->hid_service_->create_characteristic(ESP_GATT_UUID_HID_REPORT_CHR, ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY);
    this->input_report_char_->create_descriptor(ESP_GATT_UUID_RPT_REF_DESCR, ESP_GATT_PERM_READ);
    uint8_t report_ref[] = {0x01, 0x01};
    this->input_report_char_->get_descriptor(ESP_GATT_UUID_RPT_REF_DESCR)->set_value(report_ref, sizeof(report_ref));

    this->hid_service_->create_characteristic(ESP_GATT_UUID_HID_INFORMATION_CHR, ESP_GATT_CHAR_PROP_BIT_READ);
    uint8_t hid_info[] = {0x11, 0x01, 0x00, 0x02};
    this->hid_service_->get_characteristic(ESP_GATT_UUID_HID_INFORMATION_CHR)->set_value(hid_info, sizeof(hid_info));

    this->hid_service_->create_characteristic(ESP_GATT_UUID_HID_CONTROL_POINT_CHR, ESP_GATT_CHAR_PROP_BIT_WRITE_NR);

    // Battery Service
    this->battery_service_ = this->hub_->create_service(ESP_GATT_UUID_BATTERY_SERVICE_SVC);
    this->battery_level_char_ = this->battery_service_->create_characteristic(ESP_GATT_UUID_BATTERY_LEVEL_CHR, ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY);
    this->set_battery_level(this->battery_level_);

    // Device Information Service
    this->dis_service_ = this->hub_->create_service(ESP_GATT_UUID_DEVICE_INFO_SVC);
    this->dis_service_->create_characteristic(ESP_GATT_UUID_MANU_NAME_CHR, ESP_GATT_CHAR_PROP_BIT_READ)->set_value(this->manufacturer_);
    this->dis_service_->create_characteristic(ESP_GATT_UUID_PNP_ID_CHR, ESP_GATT_CHAR_PROP_BIT_READ)->set_value((uint8_t *) PNP_ID, sizeof(PNP_ID));

    this->hid_service_->start();
    this->battery_service_->start();
    this->dis_service_->start();

    this->jiggling_enabled_ = true;
    this->last_jiggle_time_ = millis();
}

void BleMouseJiggler::loop() {
    if (this->jiggling_enabled_ && this->client_connected_) {
        if (millis() - this->last_jiggle_time_ >= this->jiggle_interval_) {
            this->jiggle_mouse_();
            this->last_jiggle_time_ = millis();
        }
    }
}

void BleMouseJiggler::dump_config() {
    ESP_LOGCONFIG(TAG, "BLE Mouse Jiggler:");
    ESP_LOGCONFIG(TAG, "  Device Name: %s", this->device_name_.c_str());
    ESP_LOGCONFIG(TAG, "  Manufacturer: %s", this->manufacturer_.c_str());
    ESP_LOGCONFIG(TAG, "  Battery Level: %d%%", this->battery_level_);
    ESP_LOGCONFIG(TAG, "  Jiggle Interval: %ums", this->jiggle_interval_);
    ESP_LOGCONFIG(TAG, "  Jiggle Distance: %d", this->jiggle_distance_);
}

void BleMouseJiggler::on_connect(esp32_ble_server::BLEServer *server) {
    ESP_LOGI(TAG, "Client connected");
    this->client_connected_ = true;
}

void BleMouseJiggler::on_disconnect(esp32_ble_server::BLEServer *server) {
    ESP_LOGI(TAG, "Client disconnected");
    this->client_connected_ = false;
    this->hub_->start_advertising();
}

void BleMouseJiggler::set_battery_level(uint8_t level) {
    this->battery_level_ = level;
    if (this->battery_level_char_ != nullptr) {
        this->battery_level_char_->set_value(&this->battery_level_, 1);
        if (this->client_connected_) {
            this->battery_level_char_->notify();
        }
    }
}

void BleMouseJiggler::jiggle_mouse_() {
    int move_x = (rand() % (2 * this->jiggle_distance_ + 1)) - this->jiggle_distance_;
    int move_y = (rand() % (2 * this->jiggle_distance_ + 1)) - this->jiggle_distance_;
    ESP_LOGD(TAG, "Jiggling mouse: x=%d, y=%d", move_x, move_y);
    this->send_report(0, move_x, move_y, 0);
}

void BleMouseJiggler::send_report(uint8_t buttons, int8_t x, int8_t y, int8_t wheel) {
    uint8_t report[] = {buttons, (uint8_t)x, (uint8_t)y, (uint8_t)wheel};
    this->input_report_char_->set_value(report, sizeof(report));
    this->input_report_char_->notify();
}

void BleMouseJiggler::start_jiggling() {
    ESP_LOGI(TAG, "Jiggling started.");
    this->jiggling_enabled_ = true;
}

void BleMouseJiggler::stop_jiggling() {
    ESP_LOGI(TAG, "Jiggling stopped.");
    this->jiggling_enabled_ = false;
}

void BleMouseJiggler::jiggle_once() {
    ESP_LOGI(TAG, "Jiggling once.");
    this->jiggle_mouse_();
}

}  // namespace ble_mouse_jiggler
}  // namespace esphome

#endif

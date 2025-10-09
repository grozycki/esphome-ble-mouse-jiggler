#include "ble_mouse.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifdef USE_ESP32

namespace esphome {
namespace ble_mouse_jiggler {

static const char *const TAG = "ble_mouse_jiggler";

void BleMouseJiggler::setup() {
    ESP_LOGI(TAG, "Setting up BLE Mouse Jiggler '%s'...", this->device_name_.c_str());

    // Create a new SimpleBLEMouse instance
    this->ble_mouse_ = new SimpleBLEMouse(this->device_name_, this->manufacturer_, this->battery_level_, this->mouse_id_);
    if (!this->pin_code_.empty()) {
        this->ble_mouse_->setPinCode(this->pin_code_);
    }

    // Start the mouse
    this->ble_mouse_->begin();
    this->jiggling_enabled_ = true; // Start jiggling by default
    this->last_jiggle_time_ = millis();

    ESP_LOGI(TAG, "BLE Mouse Jiggler '%s' set up and started.", this->device_name_.c_str());
}

void BleMouseJiggler::loop() {
    if (this->jiggling_enabled_ && this->ble_mouse_ && this->ble_mouse_->isConnected()) {
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
    ESP_LOGCONFIG(TAG, "  Jiggle Interval: %dms", this->jiggle_interval_);
    ESP_LOGCONFIG(TAG, "  Jiggle Distance: %d", this->jiggle_distance_);
    ESP_LOGCONFIG(TAG, "  Mouse ID: %d", this->mouse_id_);
    if (!this->pin_code_.empty()) {
        ESP_LOGCONFIG(TAG, "  PIN Code: [set]");
    }
}


void BleMouseJiggler::jiggle_mouse_() {
    if (this->ble_mouse_ && this->ble_mouse_->isConnected()) {
        int move_x = (rand() % (2 * this->jiggle_distance_ + 1)) - this->jiggle_distance_;
        int move_y = (rand() % (2 * this->jiggle_distance_ + 1)) - this->jiggle_distance_;

        ESP_LOGD(TAG, "Jiggling mouse: x=%d, y=%d", move_x, move_y);
        this->ble_mouse_->move(move_x, move_y);
    }
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

// Custom service for status checking
void BleMouseJiggler::on_jiggle_status(bool connected, bool jiggling, uint32_t interval) {
    ESP_LOGI(TAG, "🔍 STATUS CHECK - Mouse %d: Connected=%s, Jiggling=%s, Interval=%ums",
             this->mouse_id_,
             connected ? "YES" : "NO",
             jiggling ? "ENABLED" : "DISABLED",
             interval);

    if (!connected) {
        ESP_LOGW(TAG, "💡 HELP: To connect, look for '%s' in your computer's Bluetooth settings", this->device_name_.c_str());
    }
}

}  // namespace ble_mouse_jiggler
}  // namespace esphome

#endif
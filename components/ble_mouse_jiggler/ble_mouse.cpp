#include "ble_mouse.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

// Framework-dependent includes
#ifdef USE_ESP_IDF_FRAMEWORK
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "nvs_flash.h"
#endif

namespace esphome {
namespace ble_mouse_jiggler {

static const char *const TAG = "ble_mouse_jiggler";

void BleMouseJiggler::setup() {
  ESP_LOGCONFIG(TAG, "=== BLE MOUSE JIGGLER SETUP START ===");
  ESP_LOGCONFIG(TAG, "Setting up BLE Mouse Jiggler (ID: %d)...", this->mouse_id_);
  ESP_LOGCONFIG(TAG, "Device name: %s", this->device_name_.c_str());

  // Sprawdź czy ESPHome BLE nie koliduje
  ESP_LOGCONFIG(TAG, "Checking for BLE conflicts...");

  // Zawsze używaj SimpleBLEMouse - usuwam warunki frameworku
  ESP_LOGCONFIG(TAG, "Creating SimpleBLEMouse instance...");

  this->ble_mouse_ = new SimpleBLEMouse(this->device_name_, this->manufacturer_, this->battery_level_, this->mouse_id_, this->pin_code_);
  ESP_LOGCONFIG(TAG, "SimpleBLEMouse instance created successfully");

  ESP_LOGCONFIG(TAG, "Calling SimpleBLEMouse::begin()...");
  this->ble_mouse_->begin();
  ESP_LOGCONFIG(TAG, "SimpleBLEMouse::begin() completed");

  ESP_LOGCONFIG(TAG, "BLE Mouse Jiggler %d setup complete", this->mouse_id_);
  ESP_LOGCONFIG(TAG, "=== BLE MOUSE JIGGLER SETUP END ===");
}

void BleMouseJiggler::loop() {
  if (!this->jiggling_enabled_ || !this->is_connected()) {
    return;
  }

  uint32_t now = millis();
  if (now - this->last_jiggle_time_ >= this->jiggle_interval_) {
    this->jiggle_once();
    this->last_jiggle_time_ = now;
  }
}

void BleMouseJiggler::dump_config() {
  ESP_LOGCONFIG(TAG, "BLE Mouse Jiggler %d:", this->mouse_id_);
  ESP_LOGCONFIG(TAG, "  Device Name: %s", this->device_name_.c_str());
  ESP_LOGCONFIG(TAG, "  Manufacturer: %s", this->manufacturer_.c_str());
  ESP_LOGCONFIG(TAG, "  Battery Level: %d%%", this->battery_level_);
  ESP_LOGCONFIG(TAG, "  Jiggle Interval: %d ms", this->jiggle_interval_);
  ESP_LOGCONFIG(TAG, "  Jiggle Distance: %d px", this->jiggle_distance_);
  ESP_LOGCONFIG(TAG, "  Mouse ID: %d", this->mouse_id_);
  ESP_LOGCONFIG(TAG, "  Connected: %s", this->ble_mouse_ && this->ble_mouse_->isConnected() ? "YES" : "NO");
}

void BleMouseJiggler::start_jiggling() {
  ESP_LOGD(TAG, "Starting mouse jiggling for mouse %d", this->mouse_id_);
  this->jiggling_enabled_ = true;
}

void BleMouseJiggler::stop_jiggling() {
  ESP_LOGD(TAG, "Stopping mouse jiggling for mouse %d", this->mouse_id_);
  this->jiggling_enabled_ = false;
}

void BleMouseJiggler::jiggle_once() {
  if (!this->is_connected()) {
    ESP_LOGW(TAG, "Cannot jiggle mouse %d - not connected", this->mouse_id_);
    return;
  }

  // Move mouse in alternating directions to create a subtle jiggle
  int8_t move_x = this->jiggle_distance_ * this->jiggle_direction_;
  int8_t move_y = this->jiggle_distance_ * this->jiggle_direction_;

  ESP_LOGV(TAG, "Jiggling mouse %d: x=%d, y=%d", this->mouse_id_, move_x, move_y);

  // Zawsze używaj SimpleBLEMouse - usuwam warunki frameworku
  this->ble_mouse_->move(move_x, move_y);
  delay(50);
  this->ble_mouse_->move(-move_x, -move_y);

  // Alternate direction for next jiggle
  this->jiggle_direction_ *= -1;
}

bool BleMouseJiggler::is_connected() {
  return this->ble_mouse_ && this->ble_mouse_->isConnected();
}

#ifdef USE_ESP_IDF_FRAMEWORK
void BleMouseJiggler::init_esp_idf_ble_() {
  ESP_LOGI(TAG, "Initializing ESP-IDF BLE for mouse %d", this->mouse_id_);

  // Użyj poprawionej implementacji SimpleBLEMouse także dla ESP-IDF
  this->ble_mouse_ = new SimpleBLEMouse(this->device_name_, this->manufacturer_, this->battery_level_, this->mouse_id_, this->pin_code_);
  this->ble_mouse_->begin();

  ESP_LOGI(TAG, "ESP-IDF BLE initialized with SimpleBLEMouse for mouse %d", this->mouse_id_);
}

void BleMouseJiggler::send_mouse_report_esp_idf_(int8_t x, int8_t y, uint8_t buttons) {
  // Użyj SimpleBLEMouse także dla ESP-IDF
  if (this->ble_mouse_) {
    if (buttons > 0) {
      this->ble_mouse_->press(buttons);
    } else {
      this->ble_mouse_->move(x, y);
    }
    ESP_LOGV(TAG, "ESP-IDF Mouse %d - Sent HID report via SimpleBLEMouse: buttons=%d, x=%d, y=%d",
             this->mouse_id_, buttons, x, y);
  }
}
#endif

}  // namespace ble_mouse_jiggler
}  // namespace esphome

#endif  // USE_ESP32

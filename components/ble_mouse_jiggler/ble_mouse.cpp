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
  ESP_LOGCONFIG(TAG, "Setting up BLE Mouse Jiggler (ID: %d)...", this->mouse_id_);

#ifdef USE_ARDUINO_FRAMEWORK
  // Arduino framework implementation
  ESP_LOGCONFIG(TAG, "Using Arduino framework with ESP32-BLE-Mouse library");
  this->ble_mouse_ = new SimpleBLEMouse(this->device_name_, this->manufacturer_, this->battery_level_, this->mouse_id_, this->pin_code_);
  this->ble_mouse_->begin();
#endif

#ifdef USE_ESP_IDF_FRAMEWORK
  // ESP-IDF framework implementation
  ESP_LOGCONFIG(TAG, "Using ESP-IDF framework with native BLE API");
  this->init_esp_idf_ble_();
#endif

  ESP_LOGCONFIG(TAG, "BLE Mouse Jiggler %d setup complete", this->mouse_id_);
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
#ifdef USE_ARDUINO_FRAMEWORK
  ESP_LOGCONFIG(TAG, "  Framework: Arduino");
  ESP_LOGCONFIG(TAG, "  Connected: %s", this->ble_mouse_ && this->ble_mouse_->isConnected() ? "YES" : "NO");
#endif
#ifdef USE_ESP_IDF_FRAMEWORK
  ESP_LOGCONFIG(TAG, "  Framework: ESP-IDF");
  ESP_LOGCONFIG(TAG, "  Connected: %s", this->connected_ ? "YES" : "NO");
#endif
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

#ifdef USE_ARDUINO_FRAMEWORK
  // Arduino framework - use SimpleBLEMouse
  this->ble_mouse_->move(move_x, move_y);
  delay(50);
  this->ble_mouse_->move(-move_x, -move_y);
#endif

#ifdef USE_ESP_IDF_FRAMEWORK
  // ESP-IDF framework - use native API
  this->send_mouse_report_esp_idf_(move_x, move_y);
  delay(50);
  this->send_mouse_report_esp_idf_(-move_x, -move_y);
#endif

  // Alternate direction for next jiggle
  this->jiggle_direction_ *= -1;
}

bool BleMouseJiggler::is_connected() {
#ifdef USE_ARDUINO_FRAMEWORK
  return this->ble_mouse_ && this->ble_mouse_->isConnected();
#endif
#ifdef USE_ESP_IDF_FRAMEWORK
  return this->connected_;
#endif
  return false;
}

#ifdef USE_ESP_IDF_FRAMEWORK
void BleMouseJiggler::init_esp_idf_ble_() {
  ESP_LOGI(TAG, "Initializing ESP-IDF BLE for mouse %d", this->mouse_id_);

  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // Note: This is a simplified ESP-IDF implementation
  // In production, you would need full HID service implementation
  this->connected_ = true; // Simulate connection for now
  ESP_LOGW(TAG, "ESP-IDF BLE implementation is simplified - mouse will simulate jiggling without actual BLE");
}

void BleMouseJiggler::send_mouse_report_esp_idf_(int8_t x, int8_t y, uint8_t buttons) {
  // Simplified implementation for ESP-IDF
  // In production, this would send actual HID reports via BLE
  ESP_LOGV(TAG, "ESP-IDF Mouse %d - Simulated HID report: buttons=%d, x=%d, y=%d",
           this->mouse_id_, buttons, x, y);
}
#endif

}  // namespace ble_mouse_jiggler
}  // namespace esphome

#endif  // USE_ESP32

#include "ble_mouse.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ble_mouse_jiggler {

static const char *const TAG = "ble_mouse_jiggler";

void BleMouseJiggler::setup() {
  ESP_LOGCONFIG(TAG, "Setting up BLE Mouse Jiggler...");

  this->ble_mouse_ = new BleMouse(this->device_name_, this->manufacturer_, this->battery_level_);
  this->ble_mouse_->begin();

  ESP_LOGCONFIG(TAG, "BLE Mouse Jiggler setup complete");
}

void BleMouseJiggler::loop() {
  if (!this->jiggling_enabled_ || !this->ble_mouse_->isConnected()) {
    return;
  }

  uint32_t now = millis();
  if (now - this->last_jiggle_time_ >= this->jiggle_interval_) {
    this->jiggle_once();
    this->last_jiggle_time_ = now;
  }
}

void BleMouseJiggler::dump_config() {
  ESP_LOGCONFIG(TAG, "BLE Mouse Jiggler:");
  ESP_LOGCONFIG(TAG, "  Device Name: %s", this->device_name_.c_str());
  ESP_LOGCONFIG(TAG, "  Manufacturer: %s", this->manufacturer_.c_str());
  ESP_LOGCONFIG(TAG, "  Battery Level: %d%%", this->battery_level_);
  ESP_LOGCONFIG(TAG, "  Jiggle Interval: %d ms", this->jiggle_interval_);
  ESP_LOGCONFIG(TAG, "  Jiggle Distance: %d px", this->jiggle_distance_);
  ESP_LOGCONFIG(TAG, "  Connected: %s", this->ble_mouse_ && this->ble_mouse_->isConnected() ? "YES" : "NO");
}

void BleMouseJiggler::start_jiggling() {
  ESP_LOGD(TAG, "Starting mouse jiggling");
  this->jiggling_enabled_ = true;
}

void BleMouseJiggler::stop_jiggling() {
  ESP_LOGD(TAG, "Stopping mouse jiggling");
  this->jiggling_enabled_ = false;
}

void BleMouseJiggler::jiggle_once() {
  if (!this->ble_mouse_ || !this->ble_mouse_->isConnected()) {
    ESP_LOGW(TAG, "Cannot jiggle - mouse not connected");
    return;
  }

  // Move mouse in alternating directions to create a subtle jiggle
  int8_t move_x = this->jiggle_distance_ * this->jiggle_direction_;
  int8_t move_y = this->jiggle_distance_ * this->jiggle_direction_;

  ESP_LOGV(TAG, "Jiggling mouse: x=%d, y=%d", move_x, move_y);

  // Move in one direction
  this->ble_mouse_->move(move_x, move_y);
  delay(50); // Small delay

  // Move back to original position
  this->ble_mouse_->move(-move_x, -move_y);

  // Alternate direction for next jiggle
  this->jiggle_direction_ *= -1;
}

bool BleMouseJiggler::is_connected() {
  return this->ble_mouse_ && this->ble_mouse_->isConnected();
}

}  // namespace ble_mouse_jiggler
}  // namespace esphome

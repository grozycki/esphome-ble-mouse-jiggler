#include "BleMouse.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

// Tag for logging, matching the component folder name
static const char *const TAG = "ble_mouse_jiggler";

namespace esphome {
namespace ble_mouse_jiggler {

void BleMouseComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up BLE Mouse Jiggler...");

  // Initialize the mouse with a default device name
  // This is the name you'll see when pairing the device on your computer
  this->bleMouse.begin("ESPHome Mouse Jiggler");
}

void BleMouseComponent::loop() {
  // Check if the BLE mouse is connected and paired
  if (this->bleMouse.isConnected()) {
    uint32_t now = millis();

    // Check if the configured move interval has passed
    if (now - this->last_move_time_ >= MOVE_INTERVAL_MS) {
      this->make_small_move();
      this->last_move_time_ = now;
    }
  }
}

void BleMouseComponent::make_small_move() {
  // Logic to simulate a small, barely noticeable movement.
  // We alternate between (1, 1) and (-1, -1) to keep the cursor in the same spot over time,
  // while still generating mouse activity.

  // Determine if we are in an even or odd interval since startup
  // This creates the alternating move direction
  bool is_even_interval = (this->last_move_time_ / MOVE_INTERVAL_MS) % 2 == 0;

  int8_t move_x = is_even_interval ? 1 : -1;
  int8_t move_y = is_even_interval ? 1 : -1;

  // Send the movement vector: (x, y, vertical_scroll, horizontal_scroll)
  this->bleMouse.move(move_x, move_y);

  ESP_LOGD(TAG, "Cursor moved (%d, %d). Next move in %d seconds.",
           move_x, move_y, MOVE_INTERVAL_MS / 1000);
}

} // namespace ble_mouse_jiggler
} // namespace esphome
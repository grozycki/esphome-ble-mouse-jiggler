#include "esphome/core/component.h"
#include "BleMouse.h"

namespace esphome {
namespace ble_mouse_jiggler { // Custom namespace, matching the component folder name

class BleMouseComponent : public Component {
 public:
  void setup() override;
  void loop() override;

  // Set priority to run after the main Bluetooth stack is initialized
  float get_setup_priority() const override { return esphome::setup_priority::AFTER_BLUETOOTH; }

  // Public method that can be called from ESPHome automations
  void make_small_move();

 protected:
  // Instance of the external BleMouse library
  BleMouse bleMouse;

  // Time tracking for the presence simulation interval
  uint32_t last_move_time_ = 0;
  // Default interval: Move the cursor every 10 seconds (10000 ms)
  const uint32_t MOVE_INTERVAL_MS = 10000;
};

} // namespace ble_mouse_jiggler
} // namespace esphome
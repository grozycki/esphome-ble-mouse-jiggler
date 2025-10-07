#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/automation.h"

#ifdef USE_ESP32

#include "simple_ble_mouse.h"

namespace esphome {
namespace ble_mouse_jiggler {

class BleMouseJiggler : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

  void set_device_name(const std::string &name) { this->device_name_ = name; }
  void set_manufacturer(const std::string &manufacturer) { this->manufacturer_ = manufacturer; }
  void set_battery_level(uint8_t level) { this->battery_level_ = level; }
  void set_jiggle_interval(uint32_t interval) { this->jiggle_interval_ = interval; }
  void set_jiggle_distance(uint8_t distance) { this->jiggle_distance_ = distance; }
  void set_mouse_id(uint8_t mouse_id) { this->mouse_id_ = mouse_id; }

  // Public methods for manual control
  void start_jiggling();
  void stop_jiggling();
  void jiggle_once();
  bool is_connected();

 protected:
  SimpleBLEMouse *ble_mouse_{nullptr};
  std::string device_name_{"ESP32 Mouse Jiggler"};
  std::string manufacturer_{"ESPHome"};
  uint8_t battery_level_{100};
  uint32_t jiggle_interval_{60000}; // 60 seconds in milliseconds
  uint8_t jiggle_distance_{1};
  uint8_t mouse_id_{0}; // Default mouse ID

  uint32_t last_jiggle_time_{0};
  bool jiggling_enabled_{true};
  int8_t jiggle_direction_{1}; // 1 or -1 for alternating direction
};

// Actions for ESPHome automation
template<typename... Ts> class StartJigglingAction : public Action<Ts...> {
 public:
  StartJigglingAction(BleMouseJiggler *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->start_jiggling(); }

 protected:
  BleMouseJiggler *parent_;
};

template<typename... Ts> class StopJigglingAction : public Action<Ts...> {
 public:
  StopJigglingAction(BleMouseJiggler *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->stop_jiggling(); }

 protected:
  BleMouseJiggler *parent_;
};

template<typename... Ts> class JiggleOnceAction : public Action<Ts...> {
 public:
  JiggleOnceAction(BleMouseJiggler *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->jiggle_once(); }

 protected:
  BleMouseJiggler *parent_;
};

}  // namespace ble_mouse_jiggler
}  // namespace esphome

#endif  // USE_ESP32

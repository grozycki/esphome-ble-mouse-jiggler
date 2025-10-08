#pragma once

#include "esphome/core/component.h"
#include "simple_ble_mouse.h"

#ifdef USE_ESP32

namespace esphome {
namespace ble_mouse_jiggler {

class BleMouseJiggler : public Component {
public:
    void setup() override;
    void loop() override;
    void dump_config() override;

    void set_device_name(const std::string &name) { this->device_name_ = name; }
    void set_manufacturer(const std::string &manufacturer) { this->manufacturer_ = manufacturer; }
    void set_battery_level(uint8_t level) { this->battery_level_ = level; }
    void set_jiggle_interval(uint32_t interval) { this->jiggle_interval_ = interval; }
    void set_jiggle_distance(int distance) { this->jiggle_distance_ = distance; }
    void set_mouse_id(uint8_t id) { this->mouse_id_ = id; }
    void set_pin_code(const std::string &pin) { this->pin_code_ = pin; }

    void start_jiggling();
    void stop_jiggling();
    void jiggle_once();

    // Custom service for status checking
    void on_jiggle_status(bool connected, bool jiggling, uint32_t interval);

protected:
    void jiggle_mouse_();

    std::string device_name_{"ESP32 Mouse Jiggler"};
    std::string manufacturer_{"ESPHome"};
    uint8_t battery_level_{100};
    uint32_t jiggle_interval_{60000};
    int jiggle_distance_{1};
    uint8_t mouse_id_{0};
    std::string pin_code_{""};

    SimpleBLEMouse *ble_mouse_{nullptr};
    bool jiggling_enabled_{false};
    uint32_t last_jiggle_time_{0};
};

// Automation actions
template<typename... Ts>
class StartJigglingAction : public Action<Ts...>, public Parented<BleMouseJiggler> {
public:
    void play(Ts... x) override { this->parent_->start_jiggling(); }
};

template<typename... Ts>
class StopJigglingAction : public Action<Ts...>, public Parented<BleMouseJiggler> {
public:
    void play(Ts... x) override { this->parent_->stop_jiggling(); }
};

template<typename... Ts>
class JiggleOnceAction : public Action<Ts...>, public Parented<BleMouseJiggler> {
public:
    void play(Ts... x) override { this->parent_->jiggle_once(); }
};

}  // namespace ble_mouse_jiggler
}  // namespace esphome

#endif

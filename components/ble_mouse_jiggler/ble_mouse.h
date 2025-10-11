#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/esp32_ble_server/ble_server.h"

#ifdef USE_ESP32

namespace esphome {
namespace ble_mouse_jiggler {

class BleMouseJiggler : public Component, public esp32_ble_server::BLEServerCallbacks {
 public:
  BleMouseJiggler(esp32_ble_server::BLEServer *hub) : hub_(hub) {}

  void setup() override;
  void loop() override;
  void dump_config() override;

  void on_connect(esp32_ble_server::BLEServer *server) override;
  void on_disconnect(esp32_ble_server::BLEServer *server) override;

  void set_device_name(const std::string &name) { this->device_name_ = name; }
  void set_manufacturer(const std::string &manufacturer) { this->manufacturer_ = manufacturer; }
  void set_battery_level(uint8_t level);
  void set_jiggle_interval(uint32_t interval) { this->jiggle_interval_ = interval; }
  void set_jiggle_distance(int distance) { this->jiggle_distance_ = distance; }

  void start_jiggling();
  void stop_jiggling();
  void jiggle_once();

 protected:
  void jiggle_mouse_();
  void send_report(uint8_t buttons, int8_t x, int8_t y, int8_t wheel);

  esp32_ble_server::BLEServer *hub_;

  std::string device_name_{"ESP32 Mouse Jiggler"};
  std::string manufacturer_{"ESPHome"};
  uint8_t battery_level_{100};
  uint32_t jiggle_interval_{60000};
  int jiggle_distance_{1};

  bool jiggling_enabled_{false};
  uint32_t last_jiggle_time_{0};
  bool client_connected_{false};

  esp32_ble_server::BLEService *hid_service_{nullptr};
  esp32_ble_server::BLECharacteristic *input_report_char_{nullptr};
  esp32_ble_server::BLEService *battery_service_{nullptr};
  esp32_ble_server::BLECharacteristic *battery_level_char_{nullptr};
  esp32_ble_server::BLEService *dis_service_{nullptr};
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

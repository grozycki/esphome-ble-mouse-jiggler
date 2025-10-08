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
  ESP_LOGE(TAG, "########## SETUP() FUNCTION CALLED ##########");
  ESP_LOGE(TAG, "This should appear in logs if setup() is executed!");

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
  ESP_LOGE(TAG, "########## DUMP_CONFIG() FUNCTION CALLED ##########");
  ESP_LOGE(TAG, "This should appear in logs if dump_config() is executed!");

  // Dodaję szczegółową diagnostykę
  ESP_LOGE(TAG, "🔍 DIAGNOSTIC: ble_mouse_ pointer = %p", this->ble_mouse_);
  ESP_LOGE(TAG, "🔍 DIAGNOSTIC: checking if ble_mouse_ is nullptr...");

  // WORKAROUND: Jeśli setup() nie został wywołany, uruchom BLE tutaj
  if (this->ble_mouse_ == nullptr) {
    ESP_LOGE(TAG, "✅ DIAGNOSTIC: ble_mouse_ is nullptr - proceeding with fallback!");
    ESP_LOGW(TAG, "⚠️  setup() was not called - starting BLE Mouse from dump_config() as fallback!");

    ESP_LOGE(TAG, "=== FALLBACK BLE MOUSE SETUP START ===");
    ESP_LOGE(TAG, "Creating SimpleBLEMouse instance in fallback mode...");

    this->ble_mouse_ = new SimpleBLEMouse(this->device_name_, this->manufacturer_, this->battery_level_, this->mouse_id_, this->pin_code_);
    ESP_LOGE(TAG, "SimpleBLEMouse instance created successfully at %p", this->ble_mouse_);

    ESP_LOGE(TAG, "Calling SimpleBLEMouse::begin() in fallback mode...");
    this->ble_mouse_->begin();
    ESP_LOGE(TAG, "SimpleBLEMouse::begin() completed - BLE Mouse should now be active!");

  } else {
    ESP_LOGE(TAG, "❌ DIAGNOSTIC: ble_mouse_ is NOT nullptr (%p) - but forcing BLE initialization anyway!", this->ble_mouse_);

    // NOWY WORKAROUND: Nawet jeśli instancja istnieje, wymuś uruchomienie BLE
    ESP_LOGE(TAG, "🔧 FORCING BLE INITIALIZATION: Calling begin() on existing instance...");
    this->ble_mouse_->begin();
    ESP_LOGE(TAG, "🔧 FORCING BLE INITIALIZATION: begin() completed!");

    // Sprawdź czy BLE rzeczywiście działa
    ESP_LOGE(TAG, "🔧 TESTING: Checking if SimpleBLEMouse is connected...");
    bool connected = this->ble_mouse_->isConnected();
    ESP_LOGE(TAG, "🔧 TESTING: SimpleBLEMouse connected status = %s", connected ? "YES" : "NO");

    // Jeśli nadal nie działa, spróbuj ponownie utworzyć instancję
    if (!connected) {
      ESP_LOGE(TAG, "🔧 RECREATING: SimpleBLEMouse seems inactive, recreating instance...");
      delete this->ble_mouse_;
      this->ble_mouse_ = new SimpleBLEMouse(this->device_name_, this->manufacturer_, this->battery_level_, this->mouse_id_, this->pin_code_);
      ESP_LOGE(TAG, "🔧 RECREATING: New instance created at %p", this->ble_mouse_);
      ESP_LOGE(TAG, "🔧 RECREATING: Calling begin() on new instance...");
      this->ble_mouse_->begin();
      ESP_LOGE(TAG, "🔧 RECREATING: begin() completed on new instance!");
    }
  }

  ESP_LOGCONFIG(TAG, "BLE Mouse Jiggler %d:", this->mouse_id_);
  ESP_LOGCONFIG(TAG, "  Device Name: %s", this->device_name_.c_str());
  ESP_LOGCONFIG(TAG, "  Manufacturer: %s", this->manufacturer_.c_str());
  ESP_LOGCONFIG(TAG, "  Battery Level: %d%%", this->battery_level_);
  ESP_LOGCONFIG(TAG, "  Jiggle Interval: %d ms", this->jiggle_interval_);
  ESP_LOGCONFIG(TAG, "  Jiggle Distance: %d px", this->jiggle_distance_);
  ESP_LOGCONFIG(TAG, "  Mouse ID: %d", this->mouse_id_);
  ESP_LOGCONFIG(TAG, "  Connected: %s", this->ble_mouse_ && this->ble_mouse_->isConnected() ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Fallback Mode: %s", this->ble_mouse_ ? "ACTIVE" : "INACTIVE");
  ESP_LOGE(TAG, "🔍 FINAL DIAGNOSTIC: ble_mouse_ pointer after dump_config = %p", this->ble_mouse_);
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

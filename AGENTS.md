## Project Goal

The primary goal of this project is to create a BLE Mouse Jiggler component for ESPHome.

### Framework Compatibility

- **Primary Framework: Arduino**
  - The component should be developed and optimized primarily for the Arduino framework.
  - All new features and bug fixes should prioritize compatibility with Arduino.

- **Secondary Framework: ESP-IDF**
  - The component should also be compatible with the ESP-IDF framework.
  - The build system should be able to detect the framework and use the appropriate code.
  - While Arduino is the priority, breaking ESP-IDF compatibility should be avoided if possible.

### Development Principles

- **Prefer Official ESPHome Components:** Whenever possible, the component should integrate with and leverage official, built-in ESPHome components (e.g., `esp32_ble_server`). This ensures better compatibility, stability, and long-term maintainability. Avoid relying on third-party libraries when a native ESPHome component provides the required functionality.

### Minimum ESPHome Version

- **esphome: 2025.9**
  - This component relies on APIs and features available starting from ESPHome version 2025.9. Do not use or test with older versions.

### Technical Notes & Workarounds (ESPHome 2025.9)

- **Codegen Bug for Pointer Access:** This version of ESPHome has a bug in its code generation engine. When accessing a parent object that is a pointer (e.g., via `hub->get_parent()`), the codegen incorrectly uses the `.` operator instead of the required `->` for subsequent method calls. This results in a C++ compilation error.
  - **Solution:** The workaround is to manually generate the correct C++ code in the `__init__.py` file using `cg.RawExpression`. This forces the use of the `->` operator.

- **Component/Header Naming Mismatch:** The BLE server component is located in the `esphome/components/esp32_ble_server/` directory, but its main header file is named `ble_server.h`. The correct include directive is `#include "esp32_ble_server/ble_server.h"`.

- **Advertising API:** Advertising is not configured by calling methods like `advertising_set_name()` from the component's Python code. Instead:
  - The device name is set in the main `esp32_ble_server` configuration in the YAML file.
  - Services are advertised by passing `true` as the second argument when calling `hub->create_service(UUID, true)` in the C++ code.

- **Event Handling:** The correct way to handle connect/disconnect events is to have the component class inherit from `esp32_ble::GATTsEventHandler` and register it as a handler on the main `ESP32BLE` parent object.

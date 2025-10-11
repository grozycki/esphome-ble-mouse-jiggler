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

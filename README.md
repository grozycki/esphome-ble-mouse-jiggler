# ESPHome BLE Mouse Jiggler

An ESPHome component that turns an ESP32 into a Bluetooth Low Energy (BLE) mouse, automatically "jiggling" the cursor to prevent the host computer from going to sleep or idle.

This component integrates with the official ESPHome `esp32_ble_server` to create a standard HID mouse device.

## Features

- 🖱️ **Virtual BLE Mouse** - ESP32 presents itself as a standard Bluetooth HID mouse.
- ⚡ **Automatic Jiggling** - Moves the cursor at regular intervals to simulate user activity.
- 🔧 **Configurable** - Customize the jiggle interval and movement distance.
- 🎯 **ESPHome Integration** - Control jiggling with ESPHome automations (`start`, `stop`, `jiggle_once`).
- 🔋 **Battery Reporting** - Reports a configurable battery level to the host.
- 📱 **Universal Compatibility** - Works with Windows, macOS, Linux, and other BLE-compatible systems.

## Installation & Configuration

This component requires the `esp32_ble_server` to be enabled in your configuration. The jiggler component then attaches to this server.

**Example `your_config.yaml`:**

```yaml
esphome:
  name: test-mouse-jiggler-arduino

# 1. Enable the ESP32 BLE Server
# This creates the main BLE hub for your device.
# The name set here will be the advertised Bluetooth name.
esp32_ble_server:
  id: ble_server_hub
  manufacturer: "ESPHome"
  name: "ESP32 Jiggler"

# 2. Add the external component
external_components:
  - source:
      type: local
      path: /data/components
    # Or using GitHub:
    # source: github://grozycki/esphome-ble-mouse-jiggler@main

# 3. Configure the mouse jiggler
# This links the jiggler logic to the BLE server created above.
ble_mouse_jiggler:
  id: my_mouse # Give it an ID for automations
  ble_server_id: ble_server_hub
  battery_level: 99
  jiggle_interval: 45s
  jiggle_distance: 2
```

## Configuration Parameters

- `id` (optional): An ID for the component so you can control it from automations.
- `ble_server_id` (**Required**): The ID of the `esp32_ble_server` component to attach to.
- `manufacturer` (optional): The manufacturer name reported in the Device Information Service. Defaults to `ESPHome`.
- `battery_level` (optional): The battery level (0-100%) to report to the host. Defaults to `100`.
- `jiggle_interval` (optional): The time between each jiggle movement. Defaults to `60s`.
- `jiggle_distance` (optional): The maximum movement distance in pixels (1-10). Defaults to `1`.

## Automations

The component provides actions to control the jiggling behavior from your ESPHome automations.

### Available Actions

- `ble_mouse_jiggler.start`: Starts the automatic jiggling. Requires the component `id`.
- `ble_mouse_jiggler.stop`: Stops the automatic jiggling. Requires the component `id`.
- `ble_mouse_jiggler.jiggle_once`: Performs a single, immediate jiggle movement. Requires the component `id`.

### Example Automation

This example uses a button to start and stop the jiggling.

```yaml
binary_sensor:
  - platform: gpio
    pin: GPIO0
    name: "Jiggler Toggle Button"
    on_press:
      - ble_mouse_jiggler.start: my_mouse
    on_release:
      - ble_mouse_jiggler.stop: my_mouse
```

## Project Structure

The current project structure is clean and relies on the native ESPHome BLE server.

```
components/ble_mouse_jiggler/
├── CMakeLists.txt      # Build configuration
├── __init__.py         # ESPHome Python integration
├── ble_mouse.cpp       # Main C++ implementation
└── ble_mouse.h         # C++ header file
```

## How It Works

The component integrates with the official `esp32_ble_server` in ESPHome. It creates the necessary BLE services (Human Interface Device, Battery Service, Device Information) and characteristics to present the ESP32 as a standard Bluetooth mouse. When active, it periodically sends small movement reports to the connected host, preventing sleep or idle states.

## Hardware Requirements

- **ESP32** (not ESP8266) - Required for Bluetooth support.
- **ESPHome 2025.9+** - The component relies on APIs available in this version or newer.

## License

MIT License

# ESPHome BLE Mouse Jiggler

An ESPHome component for automatic mouse cursor "jiggling" via Bluetooth Low Energy on ESP32.

## Features

- 🖱️ **Virtual BLE Mouse** - ESP32 presents itself as a Bluetooth mouse
- ⚡ **Automatic Jiggling** - Subtle cursor movement at regular intervals
- 🔧 **Configurable Parameters** - Interval, distance, device name customization
- 🎯 **ESPHome Integration** - Full automation and action support
- 📱 **Universal Compatibility** - Works with Windows, macOS, Linux
- 🚀 **Easy Setup** - Uses proven ESP32-BLE-Mouse library

## Installation

Add the component to your ESPHome configuration:

```yaml
external_components:
  - source: github://grozycki/esphome-ble-mouse-jiggler@main
    components:
      - ble_mouse_jiggler
    refresh: 1s

# Component configuration
ble_mouse_jiggler:
  id: my_mouse_jiggler
  device_name: "ESP32 Mouse Jiggler"
  manufacturer: "ESPHome"
  battery_level: 100
  jiggle_interval: 60s
  jiggle_distance: 1
```

## Configuration Parameters

- `device_name` (optional): BLE device name (default: "ESP32 Mouse Jiggler")
- `manufacturer` (optional): Manufacturer name (default: "ESPHome")
- `battery_level` (optional): Battery level 0-100% (default: 100)
- `jiggle_interval` (optional): Interval between movements (default: 60s)
- `jiggle_distance` (optional): Movement distance in pixels 1-10 (default: 1)

## Automations

The component provides actions for controlling jiggling behavior:

```yaml
# Example automations
automation:
  - alias: "Start jiggling at 9 AM"
    trigger:
      platform: time
      at: "09:00:00"
    action:
      - ble_mouse_jiggler.start:
          id: my_mouse_jiggler

  - alias: "Stop jiggling at 5 PM"
    trigger:
      platform: time
      at: "17:00:00"
    action:
      - ble_mouse_jiggler.stop:
          id: my_mouse_jiggler

  - alias: "Manual jiggle on button press"
    trigger:
      platform: gpio
      pin: GPIO0
      on_press:
    action:
      - ble_mouse_jiggler.jiggle_once:
          id: my_mouse_jiggler
```

## Available Actions

- `ble_mouse_jiggler.start` - Start automatic jiggling
- `ble_mouse_jiggler.stop` - Stop automatic jiggling  
- `ble_mouse_jiggler.jiggle_once` - Perform single mouse movement

## Hardware Requirements

- **ESP32** (not ESP8266) - Required for Bluetooth support
- **ESPHome 2023.5.0+**

## Device Pairing

1. Flash the firmware to your ESP32
2. In Bluetooth settings, find "ESP32 Mouse Jiggler" device
3. Pair the device - it will appear as a mouse in your system
4. The component automatically starts jiggling after connection

## How It Works

The component:
1. Initializes a virtual BLE mouse on ESP32
2. At regular intervals (default every 60s) performs subtle mouse movement
3. Movement is very small (1 pixel) and immediately reversed, so cursor stays in the same place
4. Simulates user activity, preventing screen savers and idle timeouts

## Technical Implementation

- Uses the proven [ESP32-BLE-Mouse](https://github.com/T-vK/ESP32-BLE-Mouse) library
- Automatically downloaded via PlatformIO library management
- Clean, minimal codebase with only essential files
- Full ESPHome component integration

## Project Structure

```
components/ble_mouse_jiggler/
├── CMakeLists.txt      # Build configuration
├── __init__.py         # ESPHome Python integration
├── ble_mouse.cpp       # Main C++ implementation
└── ble_mouse.h         # C++ header file
```

## Safety Notice

⚠️ **Warning**: Use responsibly and in accordance with your company's policies. This component is intended for personal use.

## Credits

This component utilizes the [ESP32-BLE-Mouse](https://github.com/T-vK/ESP32-BLE-Mouse) library by T-vK.

## License

MIT License

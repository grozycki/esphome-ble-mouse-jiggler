# ESPHome BLE Mouse Jiggler

An ESPHome component for automatic mouse cursor "jiggling" via Bluetooth Low Energy on ESP32. **Supports multiple virtual mice on a single ESP32!**

## Features

- 🖱️ **Virtual BLE Mouse** - ESP32 presents itself as a Bluetooth mouse
- ⚡ **Automatic Jiggling** - Subtle cursor movement at regular intervals
- 🔧 **Configurable Parameters** - Interval, distance, device name customization
- 🎯 **ESPHome Integration** - Full automation and action support
- 📱 **Universal Compatibility** - Works with Windows, macOS, Linux
- 🚀 **Easy Setup** - Uses proven ESP32-BLE-Mouse library
- 🖱️🖱️ **Multiple Mice Support** - Create up to multiple virtual mice on one ESP32
- 🔄 **Auto ID Assignment** - No need to manually manage mouse IDs

## Installation

Add the component to your ESPHome configuration:

```yaml
# Required: Enable ESP32 BLE support
esp32_ble:

external_components:
  - source: github://grozycki/esphome-ble-mouse-jiggler@main
    components:
      - ble_mouse_jiggler

# Single mouse configuration
ble_mouse_jiggler:
  id: my_mouse_jiggler
  device_name: "ESP32 Mouse Jiggler"
  manufacturer: "ESPHome"
  battery_level: 100
  jiggle_interval: 60s
  jiggle_distance: 1
```

## Multiple Mice Configuration

Create multiple virtual mice with different names and settings:

```yaml
# Required: Enable ESP32 BLE support
esp32_ble:

external_components:
  - source: github://grozycki/esphome-ble-mouse-jiggler@main
    components:
      - ble_mouse_jiggler

# Multiple mice configuration using list syntax
ble_mouse_jiggler:
  # First mouse - for work
  - id: work_mouse
    device_name: "Work Mouse Jiggler"
    manufacturer: "ESPHome Work"
    jiggle_interval: 30s
    jiggle_distance: 1

  # Second mouse - for gaming
  - id: gaming_mouse
    device_name: "Gaming Mouse Jiggler"
    manufacturer: "ESPHome Gaming"
    jiggle_interval: 120s
    jiggle_distance: 2

  # Third mouse - for presentations
  - id: presentation_mouse
    device_name: "Presentation Helper"
    manufacturer: "ESPHome"
    jiggle_interval: 300s
    jiggle_distance: 1
```

## Configuration Parameters

- `id` (required): ESPHome component ID for automations
- `device_name` (optional): BLE device name (default: "ESP32 Mouse Jiggler")
- `manufacturer` (optional): Manufacturer name (default: "ESPHome")
- `battery_level` (optional): Battery level 0-100% (default: 100)
- `jiggle_interval` (optional): Interval between movements (default: 60s)
- `jiggle_distance` (optional): Movement distance in pixels 1-10 (default: 1)

*Note: `mouse_id` is automatically assigned - no need to specify manually!*

## Automations

The component provides actions for controlling jiggling behavior of each mouse independently:

```yaml
# Example automations for multiple mice
automation:
  # Work mouse schedule
  - alias: "Start work mouse at 9 AM"
    trigger:
      platform: time
      at: "09:00:00"
    action:
      - ble_mouse_jiggler.start:
          id: work_mouse

  - alias: "Stop work mouse at 5 PM"
    trigger:
      platform: time
      at: "17:00:00"
    action:
      - ble_mouse_jiggler.stop:
          id: work_mouse

  # Gaming mouse schedule
  - alias: "Start gaming mouse at 6 PM"
    trigger:
      platform: time
      at: "18:00:00"
    action:
      - ble_mouse_jiggler.start:
          id: gaming_mouse

  - alias: "Stop gaming mouse at midnight"
    trigger:
      platform: time
      at: "00:00:00"
    action:
      - ble_mouse_jiggler.stop:
          id: gaming_mouse

  # Manual control
  - alias: "Emergency jiggle on button press"
    trigger:
      platform: gpio
      pin: GPIO0
      on_press:
    action:
      - ble_mouse_jiggler.jiggle_once:
          id: work_mouse
      - ble_mouse_jiggler.jiggle_once:
          id: gaming_mouse
```

## Available Actions

- `ble_mouse_jiggler.start` - Start automatic jiggling for specified mouse
- `ble_mouse_jiggler.stop` - Stop automatic jiggling for specified mouse
- `ble_mouse_jiggler.jiggle_once` - Perform single mouse movement for specified mouse

## Home Assistant Integration

The component automatically exposes actions as services in Home Assistant through the ESPHome API:

### Available Actions in Home Assistant

ESPHome automatically creates services for component actions:

- `ble_mouse_jiggler.start` - Start automatic jiggling for specified mouse
- `ble_mouse_jiggler.stop` - Stop automatic jiggling for specified mouse
- `ble_mouse_jiggler.jiggle_once` - Perform single mouse movement for specified mouse

### Using Actions in Home Assistant

You can call these actions from Home Assistant automations, scripts, or manually:

```yaml
# Configuration with multiple mice
ble_mouse_jiggler:
  - id: work_mouse
    device_name: "Work Mouse Jiggler"
  - id: gaming_mouse
    device_name: "Gaming Mouse Jiggler"

# Home Assistant automation example - controlling specific mice
automation:
  - alias: "Start work mouse during work hours"
    trigger:
      platform: time
      at: "09:00:00"
    action:
      service: ble_mouse_jiggler.start
      data:
        id: work_mouse
      
  - alias: "Start gaming mouse in evening"
    trigger:
      platform: time
      at: "18:00:00"
    action:
      service: ble_mouse_jiggler.start
      data:
        id: gaming_mouse
      
  - alias: "Stop all mice at night"
    trigger:
      platform: time
      at: "23:00:00"
    action:
      - service: ble_mouse_jiggler.stop
        data:
          id: work_mouse
      - service: ble_mouse_jiggler.stop
        data:
          id: gaming_mouse

# Manual service calls for specific mice
script:
  work_mouse_jiggle:
    sequence:
      - service: ble_mouse_jiggler.jiggle_once
        data:
          id: work_mouse
      
  gaming_mouse_jiggle:
    sequence:
      - service: ble_mouse_jiggler.jiggle_once
        data:
          id: gaming_mouse
```

### Service Discovery

Actions appear automatically in Home Assistant under **Developer Tools > Services** as `ble_mouse_jiggler.*` services. You specify which mouse to control using the `id` parameter in the service data.

## Hardware Requirements

- **ESP32** (not ESP8266) - Required for Bluetooth support
- **ESPHome 2023.5.0+**

## Device Pairing

1. Flash the firmware to your ESP32
2. In Bluetooth settings, you'll see multiple devices (e.g., "Work Mouse Jiggler", "Gaming Mouse Jiggler")
3. Pair each device individually - they will appear as separate mice in your system
4. Each mouse can be connected to different devices or the same device
5. Components automatically start jiggling after connection

## How It Works

The component:
1. Initializes multiple virtual BLE mice on ESP32 (shared Bluetooth stack)
2. Each mouse operates independently with its own:
   - Device name and advertising
   - Connection status
   - Jiggling schedule and parameters
3. At regular intervals, each connected mouse performs subtle movement
4. Movement is very small (1-2 pixels) and immediately reversed, so cursor stays in place
5. Simulates user activity, preventing screen savers and idle timeouts

## Use Cases for Multiple Mice

- **Work + Personal** - Different schedules for work and personal computers
- **Different Applications** - Work mouse (frequent jiggling) vs Gaming mouse (rare jiggling)
- **Multiple Devices** - Connect each mouse to different computers/devices
- **Redundancy** - Backup mice in case one disconnects
- **Specialized Settings** - Different jiggle patterns for different purposes

## Technical Implementation

- Uses enhanced ESP32 BLE stack with multiple GATT applications
- Automatically downloads required dependencies via PlatformIO
- Clean, minimal codebase with multi-instance architecture
- Full ESPHome component integration with independent automation support
- Static Bluetooth management for efficient resource usage

## Project Structure

```
components/ble_mouse_jiggler/
├── CMakeLists.txt              # Build configuration
├── __init__.py                 # ESPHome Python integration (auto mouse_id)
├── ble_mouse.cpp               # Main C++ implementation
├── ble_mouse.h                 # C++ header file
├── simple_ble_mouse.cpp        # Multi-instance BLE mouse implementation
└── simple_ble_mouse.h          # Enhanced BLE mouse API
```

## Limitations

- **ESP32 BLE connections limit**: Typically 3-4 simultaneous connections
- **Memory usage**: Each mouse instance uses additional RAM
- **Advertising conflicts**: All mice advertise simultaneously (managed automatically)

## Safety Notice

⚠️ **Warning**: Use responsibly and in accordance with your company's policies. This component is intended for personal use.

## Troubleshooting

### ESP32 BLE Failed Error

If you see `esp32_ble is marked FAILED: unspecified` in logs, try these solutions:

#### 1. Check BLE Configuration Conflicts

Make sure you don't have conflicting BLE components:

```yaml
# REMOVE these if present - they conflict with ble_mouse_jiggler:
# bluetooth_proxy:
# esp32_ble_tracker:
# esp32_ble_beacon:

# KEEP only this:
esp32_ble:
```

#### 2. Add BLE Debugging

Enable detailed BLE logging to see what's failing:

```yaml
logger:
  level: DEBUG
  logs:
    esp32_ble: DEBUG
    ble_mouse_jiggler: DEBUG
    component: DEBUG
```

#### 3. Check Memory Usage

BLE requires significant RAM. If you have many other components, try:

```yaml
# Reduce memory usage
logger:
  level: WARN  # Less verbose logging
  
# Remove unnecessary components temporarily
# wifi:
#   power_save_mode: light  # Use less WiFi features
```

#### 4. ESP32 Variant Issues

Some ESP32 variants have BLE issues. Try:

```yaml
esp32:
  board: esp32dev
  framework:
    type: arduino
    version: recommended
  
# Force BLE/WiFi coexistence
esphome:
  platformio_options:
    board_build.partitions: "default.csv"
    board_build.arduino.memory_type: "qio_qspi"
```

#### 5. Clean Build

Sometimes a clean build helps:

```bash
# In ESPHome
esphome clean your_config.yaml
esphome compile your_config.yaml
```

#### 6. Minimal Test Configuration

Try this minimal config first to isolate the issue:

```yaml
esphome:
  name: ble-mouse-test
  platform: ESP32
  board: esp32dev

wifi:
  ssid: "your_wifi"
  password: "your_password"

logger:
  level: DEBUG

api:
ota:

# Only enable BLE - no other components
esp32_ble:

external_components:
  - source: github://grozycki/esphome-ble-mouse-jiggler@main
    components:
      - ble_mouse_jiggler

# Single mouse only for testing
ble_mouse_jiggler:
  id: test_mouse
  device_name: "Test Mouse"
```

### Multiple mice not appearing
- Ensure different `device_name` for each mouse
- Check ESP32 memory usage in logs
- Verify Bluetooth is enabled on target devices

### Connection issues
- Try pairing one mouse at a time
- Clear Bluetooth cache on target device
- Check ESP32 logs for connection events

### Jiggling not working
- Verify mouse is connected (check logs)
- Ensure jiggling is enabled for specific mouse ID
- Check automation triggers and actions

## Credits

This component utilizes the [ESP32-BLE-Mouse](https://github.com/T-vK/ESP32-BLE-Mouse) library by T-vK.

## License

MIT License

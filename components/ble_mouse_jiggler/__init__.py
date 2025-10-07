import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

# Define the C++ namespace and class (as defined in ble_mouse.h)
ble_mouse_jiggler_ns = cg.esphome_ns.namespace("ble_mouse_jiggler")
BleMouseComponent = ble_mouse_jiggler_ns.class_("BleMouseComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(BleMouseComponent),
}).extend(cv.COMPONENT_SCHEMA)

# Function to generate the C++ code for the component
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

# IMPORTANT: Define the PlatformIO library dependency here.
# This ensures that the ESP32-BLE-Mouse library is downloaded and available
# for compilation, even when using the ESP-IDF framework.
LIBRARIES = [
    "T-vK/ESP32 BLE Mouse@^0.2.0"
]

BUILD_FLAGS = [
    '-I', '.pioenvs/$ID/libdeps/$ID/ESP32 BLE Mouse/src'
]

PLATFORM = "ESP32"
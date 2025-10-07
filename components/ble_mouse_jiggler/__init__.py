import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

# -----------------
# 1. C++ Component Definition
# -----------------

# Defines the namespace and class (as defined in ble_mouse.h)
ble_mouse_jiggler_ns = cg.esphome_ns.namespace("ble_mouse_jiggler")
BleMouseComponent = ble_mouse_jiggler_ns.class_("BleMouseComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(BleMouseComponent),
}).extend(cv.COMPONENT_SCHEMA)

# -----------------
# 2. Code Generation
# -----------------

async def to_code(config):
    """Generates the C++ code for the component."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

BUILD_FLAGS = [
    '-I', 'components/ble_mouse_jiggler'
]

LIBRARIES = ["T-vK/ESP32 BLE Mouse@^0.2.0"] # Nadal wymagane, by PlatformIO pobrało kod

PLATFORM = "ESP32"

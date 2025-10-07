import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

ble_mouse_jiggler_ns = cg.esphome_ns.namespace("ble_mouse_jiggler")
BleMouseComponent = ble_mouse_jiggler_ns.class_("BleMouseComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(BleMouseComponent),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    """Generates the C++ code for the component."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

LIBRARIES = [
    "T-vK/ESP32 BLE Mouse@^0.2.0"
]

PLATFORM = "ESP32"

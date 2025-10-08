import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32
from esphome import automation
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_BATTERY_LEVEL,
    CONF_INTERVAL,
)

DEPENDENCIES = ["esp32"]  # Usuwam esp32_ble - może powoduje konflikty
AUTO_LOAD = ["esp32"]
CODEOWNERS = ["@esphome/core"]

# Define custom configuration keys not available in esphome.const
CONF_MANUFACTURER = "manufacturer"
CONF_DISTANCE = "distance"
CONF_MOUSE_ID = "mouse_id"
CONF_PIN_CODE = "pin_code"

# Namespace and class
ble_mouse_jiggler_ns = cg.esphome_ns.namespace("ble_mouse_jiggler")
BleMouseJiggler = ble_mouse_jiggler_ns.class_("BleMouseJiggler", cg.Component)

# Actions for automation - using correct base class
StartJigglingAction = ble_mouse_jiggler_ns.class_("StartJigglingAction", automation.Action)
StopJigglingAction = ble_mouse_jiggler_ns.class_("StopJigglingAction", automation.Action)
JiggleOnceAction = ble_mouse_jiggler_ns.class_("JiggleOnceAction", automation.Action)

# Configuration schema
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BleMouseJiggler),
        cv.Optional(CONF_NAME, default="ESP32 Mouse Jiggler"): cv.string,
        cv.Optional(CONF_MANUFACTURER, default="ESPHome"): cv.string,
        cv.Optional(CONF_BATTERY_LEVEL, default=100): cv.int_range(min=0, max=100),
        cv.Optional(CONF_INTERVAL, default="60s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_DISTANCE, default=1): cv.int_range(min=1, max=10),
        cv.Optional(CONF_MOUSE_ID, default=0): cv.int_range(min=0, max=255),
        cv.Optional(CONF_PIN_CODE): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_device_name(config[CONF_NAME]))
    cg.add(var.set_manufacturer(config[CONF_MANUFACTURER]))
    cg.add(var.set_battery_level(config[CONF_BATTERY_LEVEL]))
    cg.add(var.set_jiggle_interval(config[CONF_INTERVAL]))
    cg.add(var.set_jiggle_distance(config[CONF_DISTANCE]))
    cg.add(var.set_mouse_id(config[CONF_MOUSE_ID]))

    # Add PIN code if provided
    if CONF_PIN_CODE in config:
        cg.add(var.set_pin_code(config[CONF_PIN_CODE]))


# Actions for ESPHome automation
@automation.register_action(
    "ble_mouse_jiggler.start_jiggling",
    StartJigglingAction,
    cv.Schema({cv.GenerateID(): cv.use_id(BleMouseJiggler)}),
)
async def start_jiggling_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    parent = await cg.get_variable(config[CONF_ID])
    cg.add(var.set_parent(parent))
    return var


@automation.register_action(
    "ble_mouse_jiggler.stop_jiggling",
    StopJigglingAction,
    cv.Schema({cv.GenerateID(): cv.use_id(BleMouseJiggler)}),
)
async def stop_jiggling_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    parent = await cg.get_variable(config[CONF_ID])
    cg.add(var.set_parent(parent))
    return var


@automation.register_action(
    "ble_mouse_jiggler.jiggle_once",
    JiggleOnceAction,
    cv.Schema({cv.GenerateID(): cv.use_id(BleMouseJiggler)}),
)
async def jiggle_once_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    parent = await cg.get_variable(config[CONF_ID])
    cg.add(var.set_parent(parent))
    return var

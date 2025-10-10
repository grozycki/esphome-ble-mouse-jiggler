import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32_ble
from esphome import automation
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_BATTERY_LEVEL,
    CONF_INTERVAL,
)

DEPENDENCIES = ["esp32_ble"]
CODEOWNERS = ["@grozycki"]

CONF_MANUFACTURER = "manufacturer"
CONF_JIGGLE_DISTANCE = "jiggle_distance"
CONF_PIN_CODE = "pin_code"

ble_mouse_jiggler_ns = cg.esphome_ns.namespace("ble_mouse_jiggler")
BleMouseJiggler = ble_mouse_jiggler_ns.class_("BleMouseJiggler", cg.Component)

StartJigglingAction = ble_mouse_jiggler_ns.class_("StartJigglingAction", automation.Action)
StopJigglingAction = ble_mouse_jiggler_ns.class_("StopJigglingAction", automation.Action)
JiggleOnceAction = ble_mouse_jiggler_ns.class_("JiggleOnceAction", automation.Action)

# Schema for a single mouse configuration
MOUSE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BleMouseJiggler),
        cv.Optional(CONF_NAME, default="ESP32 Mouse Jiggler"): cv.string,
        cv.Optional(CONF_MANUFACTURER, default="ESPHome"): cv.string,
        cv.Optional(CONF_BATTERY_LEVEL, default=100): cv.int_range(min=0, max=100),
        cv.Optional(CONF_INTERVAL, default="60s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_JIGGLE_DISTANCE, default=1): cv.int_range(min=1, max=10),
        cv.Optional(CONF_PIN_CODE): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA)

# Allow a single config or a list of configs
CONFIG_SCHEMA = cv.ensure_list(MOUSE_SCHEMA)


async def to_code(config):
    # This will be called for each item in the list
    for i, conf in enumerate(config):
        var = cg.new_Pvariable(conf[CONF_ID])
        await cg.register_component(var, conf)

        cg.add(var.set_device_name(conf[CONF_NAME]))
        cg.add(var.set_manufacturer(conf[CONF_MANUFACTURER]))
        cg.add(var.set_battery_level(conf[CONF_BATTERY_LEVEL]))
        cg.add(var.set_jiggle_interval(conf[CONF_INTERVAL]))
        cg.add(var.set_jiggle_distance(conf[CONF_JIGGLE_DISTANCE]))
        cg.add(var.set_mouse_id(i)) # Automatically assign mouse_id

        if CONF_PIN_CODE in conf:
            cg.add(var.set_pin_code(conf[CONF_PIN_CODE]))

        # This is the crucial part that was missing
        cg.add_library("T-vK/ESP32-BLE-Mouse", None)
        cg.add(var.setup())


@automation.register_action(
    "ble_mouse_jiggler.start",
    StartJigglingAction,
    cv.Schema({cv.GenerateID(): cv.use_id(BleMouseJiggler)}),
)
@automation.register_action(
    "ble_mouse_jiggler.stop",
    StopJigglingAction,
    cv.Schema({cv.GenerateID(): cv.use_id(BleMouseJiggler)}),
)
@automation.register_action(
    "ble_mouse_jiggler.jiggle_once",
    JiggleOnceAction,
    cv.Schema({cv.GenerateID(): cv.use_id(BleMouseJiggler)}),
)
async def jiggler_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    parent = await cg.get_variable(config[CONF_ID])
    cg.add(var.set_parent(parent))
    return var

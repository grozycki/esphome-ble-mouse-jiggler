import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import esp32_ble_server
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_BATTERY_LEVEL,
    CONF_INTERVAL,
)

DEPENDENCIES = ["esp32_ble_server"]
CODEOWNERS = ["@grozycki"]

CONF_MANUFACTURER = "manufacturer"
CONF_JIGGLE_DISTANCE = "jiggle_distance"
CONF_BLE_SERVER_ID = "ble_server_id"

ble_mouse_jiggler_ns = cg.esphome_ns.namespace("ble_mouse_jiggler")
BleMouseJiggler = ble_mouse_jiggler_ns.class_("BleMouseJiggler", cg.Component)

StartJigglingAction = ble_mouse_jiggler_ns.class_("StartJigglingAction", automation.Action)
StopJigglingAction = ble_mouse_jiggler_ns.class_("StopJigglingAction", automation.Action)
JiggleOnceAction = ble_mouse_jiggler_ns.class_("JiggleOnceAction", automation.Action)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BleMouseJiggler),
            cv.Required(CONF_BLE_SERVER_ID): cv.use_id(esp32_ble_server.BLEServer),
            cv.Optional(CONF_NAME, default="ESP32 Mouse Jiggler"): cv.string,
            cv.Optional(CONF_MANUFACTURER, default="ESPHome"): cv.string,
            cv.Optional(CONF_BATTERY_LEVEL, default=100): cv.int_range(min=0, max=100),
            cv.Optional(CONF_INTERVAL, default="60s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_JIGGLE_DISTANCE, default=1): cv.int_range(min=1, max=10),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BLE_SERVER_ID])
    var = cg.new_Pvariable(config[CONF_ID], hub)
    await cg.register_component(var, config)

    cg.add(var.set_device_name(config[CONF_NAME]))
    cg.add(var.set_manufacturer(config[CONF_MANUFACTURER]))
    cg.add(var.set_battery_level(config[CONF_BATTERY_LEVEL]))
    cg.add(var.set_jiggle_interval(config[CONF_INTERVAL]))
    cg.add(var.set_jiggle_distance(config[CONF_JIGGLE_DISTANCE]))


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

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32
from esphome.const import CONF_ID
from esphome.core import CORE
from esphome import automation

DEPENDENCIES = ["esp32"]
CODEOWNERS = ["@grozycki"]

ble_mouse_jiggler_ns = cg.esphome_ns.namespace("ble_mouse_jiggler")
BleMouseJiggler = ble_mouse_jiggler_ns.class_("BleMouseJiggler", cg.Component)

# Actions
StartJigglingAction = ble_mouse_jiggler_ns.class_("StartJigglingAction", automation.Action)
StopJigglingAction = ble_mouse_jiggler_ns.class_("StopJigglingAction", automation.Action)
JiggleOnceAction = ble_mouse_jiggler_ns.class_("JiggleOnceAction", automation.Action)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BleMouseJiggler),
        cv.Optional("device_name", default="ESP32 Mouse Jiggler"): cv.string,
        cv.Optional("manufacturer", default="ESPHome"): cv.string,
        cv.Optional("battery_level", default=100): cv.int_range(min=0, max=100),
        cv.Optional("jiggle_interval", default="60s"): cv.positive_time_period_milliseconds,
        cv.Optional("jiggle_distance", default=1): cv.int_range(min=1, max=10),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_device_name(config["device_name"]))
    cg.add(var.set_manufacturer(config["manufacturer"]))
    cg.add(var.set_battery_level(config["battery_level"]))
    cg.add(var.set_jiggle_interval(config["jiggle_interval"]))
    cg.add(var.set_jiggle_distance(config["jiggle_distance"]))

    # Add library dependency
    cg.add_platformio_option("lib_deps", "t-vk/ESP32 BLE Mouse@^0.3.1")


# Actions
@automation.register_action("ble_mouse_jiggler.start", StartJigglingAction, cv.Schema({
    cv.GenerateID(): cv.use_id(BleMouseJiggler),
}))
async def start_jiggling_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)


@automation.register_action("ble_mouse_jiggler.stop", StopJigglingAction, cv.Schema({
    cv.GenerateID(): cv.use_id(BleMouseJiggler),
}))
async def stop_jiggling_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)


@automation.register_action("ble_mouse_jiggler.jiggle_once", JiggleOnceAction, cv.Schema({
    cv.GenerateID(): cv.use_id(BleMouseJiggler),
}))
async def jiggle_once_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)

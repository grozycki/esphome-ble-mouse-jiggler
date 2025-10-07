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

# -----------------
# 3. ESP-IDF Dependency Management (Crucial for your fix)
# -----------------

def get_idf_build_components(config):
    """
    Informs ESPHome to include an external folder as an ESP-IDF component.
    This resolves the 'BleMouse.h: No such file or directory' error in IDF mode.
    """
    # The string returned MUST match the folder name containing
    # the BleMouse source files and the CMakeLists.txt file.
    return ["ble_mouse_idf"] # <<< CORRECTED FOLDER NAME

BUILD_FLAGS = [
    # Wymuś dodanie nagłówków z zagnieżdżonego komponentu BleMouse IDF
    '-I', '$PROJECT_COMPONENTS_DIR/ble_mouse_idf/src'
]

LIBRARIES = [
    "T-vK/ESP32 BLE Mouse@^0.2.0"
]

PLATFORM = "ESP32"
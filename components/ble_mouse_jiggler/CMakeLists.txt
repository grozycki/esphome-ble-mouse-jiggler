idf_component_register(
    SRCS "ble_mouse.cpp" "SimpleBLEMouse.cpp"
    INCLUDE_DIRS "."
    REQUIRES "bt" "driver" "nvs_flash"
)
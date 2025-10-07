idf_component_register(
    SRCS "ble_mouse.cpp" "simple_ble_mouse.cpp"
    INCLUDE_DIRS "."
    REQUIRES "bt" "driver" "nvs_flash"
)
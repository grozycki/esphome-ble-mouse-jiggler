idf_component_register(
    SRCS
        "ble_mouse.cpp"
        "BleMouse.cpp"
        "BleConnectionStatus.cpp"
    INCLUDE_DIRS
        # Dodajemy kropkę, aby Bieżący Katalog był przeszukiwany dla nagłówków (BleMouse.h)
        "."
    REQUIRES
        bt
        driver
)
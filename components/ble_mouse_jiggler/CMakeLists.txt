idf_component_register(
    SRCS
        "ble_mouse.cpp"     # Twoja logika
        "BleMouse.cpp"      # Źródło biblioteki
    INCLUDE_DIRS
        "."
    REQUIRES
        bt                  # <<< KLUCZOWA ZMIANA: Dodajemy zależność BLE
)

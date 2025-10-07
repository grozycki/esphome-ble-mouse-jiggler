# CMakeLists.txt
# Rejestruje bibliotekę jako komponent IDF
idf_component_register(
    SRCS "src/BleMouse.cpp"
    INCLUDE_DIRS "src"
    # Wymaga włączenia stosu Bluetooth (bt)
    REQUIRES bt
)

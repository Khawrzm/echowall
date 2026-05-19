idf_component_register(
    SRCS "fl_mesh.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_now freertos heap
)

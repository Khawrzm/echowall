idf_component_register(
    SRCS "echowall_main.c" "csi_extractor.c" "serial_reporter.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi nvs_flash driver esp_timer
)

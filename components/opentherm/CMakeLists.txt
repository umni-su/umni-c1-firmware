idf_component_register(
    SRCS "opentherm_operations.c" "opentherm.c"
    PRIV_REQUIRES driver esp_timer
    )

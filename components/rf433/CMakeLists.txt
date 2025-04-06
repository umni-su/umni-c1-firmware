idf_component_register(
    SRCS "esp32_rf_receiver.c" "rf433.c" 
    PRIV_REQUIRES esp_timer driver json
    )

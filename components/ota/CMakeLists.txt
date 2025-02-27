idf_component_register(
    SRCS "ota.c" 
    PRIV_REQUIRES app_update esp_http_client esp_https_ota mbedtls
    )

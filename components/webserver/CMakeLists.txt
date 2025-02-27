idf_component_register(SRCS "webserver.c" "rest_server.c"
                       PRIV_REQUIRES 
                        esp_netif 
                        esp_http_server 
                        esp_timer
                        vfs 
                        fatfs
                        nvs 
                        json 
                        ntc_driver
                        ota
                       INCLUDE_DIRS ".")
if(CONFIG_EXAMPLE_WEB_DEPLOY_SF)
    set(WEB_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data/")
    if(EXISTS ${WEB_SRC_DIR}/dist)
        spiffs_create_partition_image(www ${WEB_SRC_DIR}/dist FLASH_IN_PROJECT)
    else()
        message(FATAL_ERROR "${WEB_SRC_DIR}/dist doesn't exit. Please run 'npm run build' in ${WEB_SRC_DIR}")
    endif()
endif()

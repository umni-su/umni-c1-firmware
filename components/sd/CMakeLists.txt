idf_component_register(SRCS "sd.c"
                    INCLUDE_DIRS "."
                    REQUIRES 
                      esp_event
                      freertos
                      driver
                      sdmmc
                      vfs 
                      fatfs)
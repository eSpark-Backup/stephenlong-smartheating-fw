idf_component_register(SRCS "ThingsBoard_interface.cpp" 
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi esp_netif esp_http_client json MLX90614_driver LD2410_interface driver Status_led App
                    EMBED_TXTFILES ca_cert.pem
                    )

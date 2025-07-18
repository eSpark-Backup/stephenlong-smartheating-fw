idf_component_register(SRCS "Comm_Interface.c" "provisioning.c" "ble.c" "ble_service_tables.c"
                    INCLUDE_DIRS "." "../../main"
                    REQUIRES bt nvs_flash esp_http_server esp_event esp_wifi esp_system esp_netif esp_http_client json Status_led Storage_interface
                    )

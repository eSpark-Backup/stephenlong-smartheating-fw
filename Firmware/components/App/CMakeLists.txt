idf_component_register(SRCS "app.c" 
                    INCLUDE_DIRS "." "../../main"
                    REQUIRES driver MLX90614_Interface Heater_Interface Status_led Wireless_Interface
                    )

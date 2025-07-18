idf_component_register(SRCS "MLX90614_interface.c" 
                    INCLUDE_DIRS "." "../../main"
                    REQUIRES MLX90614_driver driver Status_led Storage_interface
                    PRIV_REQUIRES Wireless_Interface
                    )

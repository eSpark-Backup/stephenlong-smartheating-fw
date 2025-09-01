# idf_component_register(SRCS "app.c" "app_cpp_wrapper.cpp"
idf_component_register(SRCS "Storage_interface.c" "app_cpp_wrapper.cpp"
                    INCLUDE_DIRS "." "../../main"
                    REQUIRES driver MLX90614_Interface Heater_Interface Status_led Wireless_Interface LD2410_interface
                    )

idf_component_register(SRCS "LD2410_interface.cpp" 
                    INCLUDE_DIRS "." "../../main"
                    REQUIRES LD2410b_driver Storage_interface Wireless_Interface
                    )

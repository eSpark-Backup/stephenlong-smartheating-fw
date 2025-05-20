# set(COMPONENT_ADD_INCLUDEDIRS "include")

idf_component_register(SRCS "LD2410.cpp" 
                         INCLUDE_DIRS include
                       REQUIRES driver esp_timer esp_system)
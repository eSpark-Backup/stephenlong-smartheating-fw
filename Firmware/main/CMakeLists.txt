idf_component_register(SRCS "app_main.cpp"
                       INCLUDE_DIRS "."
                       )

target_compile_options(${COMPONENT_TARGET} PRIVATE 
                        -Wno-missing-field-initializers)

set(COMPONENT_ADD_INCLUDEDIRS "include")

idf_component_register(SRCS "led_strip_encoder.c"
                       INCLUDE_DIRS "include"
                       REQUIRES driver )

# target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu99 -g3 -fno-stack-protector -ffunction-sections -fdata-sections -fstrict-volatile-bitfields -nostdlib -Wpointer-arith -Wno-error=unused-value -Wno-error=unused-label -Wno-error=unused-function -Wno-error=unused-but-set-variable -Wno-error=unused-variable -Wno-error=deprecated-declarations -Wno-error=char-subscripts -Wno-error=maybe-uninitialized -Wno-unused-parameter -Wno-sign-compare -Wno-old-style-declaration -MMD -c -DF_CPU=240000000L -DESP32 -DCORE_DEBUG_LEVEL=0 -DNDEBUG)

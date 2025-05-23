# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PICO_BOARD pico_w CACHE STRING "Board type")
include(pico_sdk_import.cmake)
set(FREERTOS_KERNEL_PATH "c:/FreeRTOS-Kernel")
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

project(Atividade_semaforo.c C CXX ASM)
pico_sdk_init()


include_directories(${CMAKE_SOURCE_DIR}/lib)


add_executable(${PROJECT_NAME}
    Atividade_semaforo.c
    lib/buzzer.c
    lib/ssd1306.c
    lib/leds.c
    extras/bitmaps.c
    extras/Desenho.c
    lib/leds.c
    lib/matrizRGB.c # Biblioteca para o display OLED
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR})
pico_set_program_name(Atividade_semaforo.c "Atividade_semaforo.c")
pico_set_program_version(Atividade_semaforo.c "0.1")
pico_generate_pio_header(Atividade_semaforo.c ${CMAKE_CURRENT_LIST_DIR}/ws2818b.pio)

target_link_libraries(${PROJECT_NAME}
    pico_bootrom
    pico_stdlib
    hardware_adc
    hardware_pwm
    hardware_clocks
    hardware_pio
    pico_stdlib
    hardware_timer
    hardware_gpio
    hardware_i2c
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap4
)

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 1)

pico_add_extra_outputs(${PROJECT_NAME})





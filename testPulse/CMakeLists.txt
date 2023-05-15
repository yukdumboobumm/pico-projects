cmake_minimum_required(VERSION 3.13)

# Add pico_sdk_import.cmake file
include(pico_sdk_import.cmake)

# Set name of project (as PROJECT_NAME) and C/C++ standards
project(testPulse C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

# Your main project file
add_executable(${PROJECT_NAME}  testPulse.c testPulse.pio)
# Create C header file with the name <pio program>.pio.h
pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/testPulse.pio)

# Link against the Raspberry Pi Pico SDK and the dc_motor library
target_link_libraries(${PROJECT_NAME}  pico_stdlib hardware_pio)

# Specify where the dc_motor library is located
#add_subdirectory(${PICO_SDK_PATH}/../customLibraries/dc_motor ${CMAKE_CURRENT_BINARY_DIR}/dc_motor)

# Enable USB output, disable UART output
pico_enable_stdio_usb(${PROJECT_NAME}  1)
pico_enable_stdio_uart(${PROJECT_NAME}  0)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(${PROJECT_NAME})



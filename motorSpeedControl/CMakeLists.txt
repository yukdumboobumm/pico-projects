# == DO NEVER EDIT THE NEXT LINES for Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.0.0)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.0.0)
include(${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
# ====================================================================================
cmake_minimum_required(VERSION 3.13)

# Add pico_sdk_import.cmake file
include(pico_sdk_import.cmake)

# Set name of project (as PROJECT_NAME) and C/C++ standards
project(motorSpeedControl C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

# Your main project file
add_executable(${PROJECT_NAME}  motorSpeedControl.c)

# Specify where the dc_motor library is located
add_subdirectory(${CMAKE_SOURCE_DIR}/../../pico-libraries/dc_motor ${CMAKE_CURRENT_BINARY_DIR}/dc_motor)
add_subdirectory(${CMAKE_SOURCE_DIR}/../../pico-libraries/pulseCounter ${CMAKE_CURRENT_BINARY_DIR}/pulseCounter)

# Link against the Raspberry Pi Pico SDK and the dc_motor library
target_link_libraries(${PROJECT_NAME} dc_motor pulseCounter)

# Enable USB output, disable UART output
pico_enable_stdio_usb(${PROJECT_NAME}  1)
pico_enable_stdio_uart(${PROJECT_NAME}  0)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(${PROJECT_NAME})
cmake_minimum_required(VERSION 3.10)

# Project name and version
project(Doip-Mqtt VERSION 1.0)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include directories
include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/settings  # Ensure config.h is included
)

# Set Paho MQTT C library paths
set(PAHO_SRC_DIR "${CMAKE_SOURCE_DIR}/../paho.mqtt.c")
set(PAHO_INCLUDE_DIR "${PAHO_SRC_DIR}/src")
set(PAHO_LIB_DIR "${PAHO_SRC_DIR}/build/src")

# Include directories for MQTT
include_directories(${PAHO_INCLUDE_DIR})
link_directories(${PAHO_LIB_DIR})

# Print the Paho MQTT library directory for verification
message(STATUS "Paho MQTT library directory: ${PAHO_LIB_DIR}")

# Create a library for DoIP Client
add_library(doip-lib
    src/doip-client.cpp
    settings/config.cpp  # Ensure config.cpp is part of the library
)

# DoIP Client Executable
add_executable(doip-client
    main.cpp
    src/doip-client.cpp
    src/mqtt-subscriber.cpp
)

# MQTT Subscriber Executable
set(MQTT_SOURCE_FILES
    src/mqtt-subscriber.cpp
    main.cpp
)
add_executable(itrams-mqtt-subscriber ${MQTT_SOURCE_FILES})

# Link against the DoIP Library and Paho MQTT
target_link_libraries(itrams-mqtt-subscriber
    doip-lib
    ${PAHO_LIB_DIR}/libpaho-mqtt3c.so
    pthread
)

# Link against pthread for threading and Paho MQTT
target_link_libraries(doip-client
    doip-lib
    ${PAHO_LIB_DIR}/libpaho-mqtt3c.so
    pthread
)

# Ensure runtime can find the shared library
set_target_properties(itrams-mqtt-subscriber PROPERTIES
    INSTALL_RPATH "${PAHO_LIB_DIR}"
)

set_target_properties(doip-client PROPERTIES
    INSTALL_RPATH "${PAHO_LIB_DIR}"
)

# Install the config.ini file so it can be accessed
install(FILES settings/config.ini DESTINATION ${CMAKE_BINARY_DIR}/settings)


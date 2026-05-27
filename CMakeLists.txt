cmake_minimum_required(VERSION 3.15)
project(TrackerServer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Boost REQUIRED SYSTEM THREAD)
find_package(CURL REQUIRED)
find_package(MySQL REQUIRED)

# Source files
set(TRACKER_SOURCES
    src/main.cpp
    src/tracker/tracker_server.cpp
    src/tracker/bep_handler.cpp
    src/database/db_manager.cpp
    src/database/connection_pool.cpp
    src/api/rest_api.cpp
    src/utils/logger.cpp
    src/utils/config.cpp
)

# Create executable
add_executable(tracker-server ${TRACKER_SOURCES})

# Link libraries
target_link_libraries(tracker-server
    PRIVATE
    ${Boost_LIBRARIES}
    ${CURL_LIBRARIES}
    ${MySQL_LIBRARIES}
    pthread
)

# Include directories
target_include_directories(tracker-server
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${Boost_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${MySQL_INCLUDE_DIRS}
)

# Compiler flags
if(MSVC)
    target_compile_options(tracker-server PRIVATE /W4)
else()
    target_compile_options(tracker-server PRIVATE -Wall -Wextra -pedantic)
endif()

# Installation
install(TARGETS tracker-server RUNTIME DESTINATION bin)
install(FILES config/tracker.conf DESTINATION etc/tracker-server)
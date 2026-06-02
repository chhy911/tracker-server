cmake_minimum_required(VERSION 3.15)
project(TrackerServer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use legacy FindBoost module (Boost 1.89+ / CMake 4.x)
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()
set(Boost_NO_BOOST_CMAKE ON)

# Boost.System is header-only since 1.69; we only need headers for Asio
find_package(Boost 1.70 REQUIRED)
find_package(CURL REQUIRED)

find_path(MYSQL_INCLUDE_DIR mysql/mysql.h
    PATHS /usr/include /usr/local/include)
find_library(MYSQL_LIBRARY NAMES mysqlclient mariadb
    PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib)
if(NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_LIBRARY)
    message(FATAL_ERROR "MySQL client library not found. Install libmysqlclient-dev.")
endif()

# Source files
set(TRACKER_SOURCES
    src/main.cpp
    src/tracker/tracker_server.cpp
    src/tracker/bep_handler.cpp
    src/database/db_manager.cpp
    src/database/connection_pool.cpp
    src/api/rest_api.cpp
    src/api/http_server.cpp
    src/utils/logger.cpp
    src/utils/config.cpp
)

# Create executable
add_executable(tracker-server ${TRACKER_SOURCES})

# Link libraries
target_link_libraries(tracker-server
    PRIVATE
    ${CURL_LIBRARIES}
    ${MYSQL_LIBRARY}
    pthread
)

# Include directories
target_include_directories(tracker-server
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${Boost_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${MYSQL_INCLUDE_DIR}
)

# Compiler flags
if(MSVC)
    target_compile_options(tracker-server PRIVATE /W4)
else()
    target_compile_options(tracker-server PRIVATE -Wall -Wextra -pedantic -fPIC)
endif()

# Installation
install(TARGETS tracker-server RUNTIME DESTINATION bin)
install(FILES config/tracker.conf DESTINATION etc/tracker-server)

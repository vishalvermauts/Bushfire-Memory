cmake_minimum_required(VERSION 3.20)
project(bushfire_memory VERSION 3.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags for safety, performance, and standards compliance
if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -O3)
endif()

# Find external dependencies
find_package(Threads REQUIRED)
find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)

# Include paths
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CURL_INCLUDE_DIRS}
    ${SQLite3_INCLUDE_DIRS}
)

# Core Static Library
set(CORE_SOURCES
    src/core/GeoUtils.cpp
    src/core/CacheStore.cpp
    src/core/HttpClient.cpp
    src/data/VerifiedSnapshots.cpp
    src/providers/RfsIncidentAdapter.cpp
    src/providers/HotspotAdapter.cpp
    src/providers/HistoricalFireAdapter.cpp
    src/providers/StacImageryAdapter.cpp
    src/engine/SimilarityEngine.cpp
    src/engine/ExportService.cpp
    src/server/HttpServer.cpp
)

add_library(bushfire_core STATIC ${CORE_SOURCES})
target_link_libraries(bushfire_core PUBLIC
    CURL::libcurl
    SQLite::SQLite3
    Threads::Threads
)

# 1. Main Server Executable
add_executable(bushfire_memory_server src/main_server.cpp)
target_link_libraries(bushfire_memory_server PRIVATE bushfire_core)

# 2. Main CLI Executable
add_executable(bushfire_memory_cli src/main_cli.cpp)
target_link_libraries(bushfire_memory_cli PRIVATE bushfire_core)

# 3. Unit Test Suite
add_executable(bushfire_memory_tests tests/test_main.cpp)
target_link_libraries(bushfire_memory_tests PRIVATE bushfire_core)

enable_testing()
add_test(NAME BushfireCoreTests COMMAND bushfire_memory_tests)

# Installation targets
install(TARGETS bushfire_memory_server bushfire_memory_cli
    RUNTIME DESTINATION bin
)
install(DIRECTORY web/ DESTINATION share/bushfire_memory/web)

cmake_minimum_required(VERSION 3.10)
project(KrakenWebSocketClient)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include FetchContent module for downloading dependencies
include(FetchContent)

# Find OpenSSL (system package required)
find_package(OpenSSL REQUIRED)

# Try to find Boost
find_package(Boost COMPONENTS system)

if(NOT Boost_FOUND)
    message(WARNING
        "\n"
        "Boost not found! Attempting to install...\n"
        "If this fails, manually install with:\n"
        "  sudo yum install boost-devel\n"
    )

    # Try to install boost-devel using system package manager
    execute_process(
        COMMAND sudo yum install -y boost-devel
        RESULT_VARIABLE BOOST_INSTALL_RESULT
        OUTPUT_QUIET
        ERROR_QUIET
    )

    if(BOOST_INSTALL_RESULT EQUAL 0)
        message(STATUS "Boost-devel installed successfully")
        # Try to find Boost again
        find_package(Boost REQUIRED COMPONENTS system)
    else()
        message(WARNING
            "\n"
            "Failed to install boost-devel automatically.\n"
            "Full WebSocket version will be skipped.\n"
            "To build it, run: sudo yum install boost-devel\n"
        )
        set(BUILD_FULL_VERSION OFF)
    endif()
else()
    set(BUILD_FULL_VERSION ON)
endif()

# Fetch websocketpp (header-only library) - only if building full version
if(BUILD_FULL_VERSION)
    message(STATUS "Checking for websocketpp...")
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/websocketpp")
        message(STATUS "Downloading websocketpp...")
        FetchContent_Declare(
            websocketpp
            GIT_REPOSITORY https://github.com/zaphoyd/websocketpp.git
            GIT_TAG master
            SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/websocketpp
        )
        FetchContent_MakeAvailable(websocketpp)
        message(STATUS "websocketpp downloaded successfully")
    else()
        message(STATUS "websocketpp already exists")
    endif()

    # Fetch nlohmann/json (header-only library)
    message(STATUS "Checking for nlohmann/json...")
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/nlohmann/json.hpp")
        message(STATUS "Downloading nlohmann/json...")
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/nlohmann)
        file(DOWNLOAD
            https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/include/nlohmann/json.hpp
            SHOW_PROGRESS
            STATUS DOWNLOAD_STATUS
        )

        list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
        if(STATUS_CODE EQUAL 0)
            message(STATUS "nlohmann/json downloaded successfully")
        else()
            message(WARNING "Failed to download nlohmann/json - full version will be skipped")
            set(BUILD_FULL_VERSION OFF)
        endif()
    else()
        message(STATUS "nlohmann/json already exists")
    endif()

    # Fetch simdjson library
    message(STATUS "Checking for simdjson...")
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/simdjson")
        message(STATUS "Downloading simdjson...")
        FetchContent_Declare(
            simdjson
            GIT_REPOSITORY https://github.com/simdjson/simdjson.git
            GIT_TAG v3.10.1
            SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/simdjson
        )
        FetchContent_MakeAvailable(simdjson)
        message(STATUS "simdjson downloaded successfully")
    else()
        message(STATUS "simdjson already exists")
        # Add simdjson as subdirectory to build it
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/simdjson EXCLUDE_FROM_ALL)
    endif()
endif()

# Include directories - updated for new structure
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${OPENSSL_INCLUDE_DIR})

if(BUILD_FULL_VERSION)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/websocketpp)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/simdjson/include)
    include_directories(${Boost_INCLUDE_DIRS})
endif()

# Build common library
add_library(kraken_common STATIC
    lib/kraken_common.cpp
)

# Build CLI utilities library
add_library(cli_utils STATIC
    lib/cli_utils.cpp
)

# Build order book common library
add_library(orderbook_common STATIC
    lib/orderbook_common.cpp
)

# Build JSON Lines writer library
add_library(jsonl_writer STATIC
    lib/jsonl_writer.cpp
)

# Build order book state library
add_library(orderbook_state STATIC
    lib/orderbook_state.cpp
)
target_link_libraries(orderbook_state
    orderbook_common
)

# Build snapshot CSV writer library
add_library(snapshot_csv_writer STATIC
    lib/snapshot_csv_writer.cpp
)

# Build Level 3 common library
add_library(level3_common STATIC
    lib/level3_common.cpp
)

# Build Level 3 JSON Lines writer library
add_library(level3_jsonl_writer STATIC
    lib/level3_jsonl_writer.cpp
)

# Build Level 3 state library
add_library(level3_state STATIC
    lib/level3_state.cpp
)

# Build Level 3 CSV writer library
add_library(level3_csv_writer STATIC
    lib/level3_csv_writer.cpp
)

# Build full WebSocket versions (with dependencies)
if(BUILD_FULL_VERSION)
    # WebSocket client library (non-blocking, nlohmann version)
    add_library(kraken_websocket_client STATIC
        lib/kraken_websocket_client.cpp
    )
    target_link_libraries(kraken_websocket_client
        kraken_common
        ${OPENSSL_LIBRARIES}
        ${Boost_LIBRARIES}
        pthread
    )

    # WebSocket client library (non-blocking, simdjson version)
    add_library(kraken_websocket_client_simdjson STATIC
        lib/kraken_websocket_client_simdjson.cpp
    )
    target_link_libraries(kraken_websocket_client_simdjson
        kraken_common
        simdjson
        ${OPENSSL_LIBRARIES}
        ${Boost_LIBRARIES}
        pthread
    )

    # Level 3 WebSocket client library
    add_library(kraken_level3_client STATIC
        lib/kraken_level3_client.cpp
    )
    target_link_libraries(kraken_level3_client
        level3_common
        kraken_common
        simdjson
        ${OPENSSL_LIBRARIES}
        ${Boost_LIBRARIES}
        pthread
    )

    # Example 1: Simple polling (using template version with simdjson)
    add_executable(example_simple_polling examples/example_simple_polling.cpp)
    target_link_libraries(example_simple_polling
        kraken_common
        simdjson
        ${OPENSSL_LIBRARIES}
        ${Boost_LIBRARIES}
        pthread
    )
    install(TARGETS example_simple_polling DESTINATION bin)
    message(STATUS "Building example: example_simple_polling")

    # Example 2: Callback-driven (using template version with simdjson)
    add_executable(example_callback_driven examples/example_callback_driven.cpp)
    target_link_libraries(example_callback_driven
        kraken_common
        simdjson
        ${OPENSSL_LIBRARIES}
        ${Boost_LIBRARIES}
        pthread
    )
    install(TARGETS example_callback_driven DESTINATION bin)
    message(STATUS "Building example: example_callback_driven")

    # Example 3: System integration (using template version with simdjson)
    add_executable(example_integration examples/example_integration.cpp)
    target_link_libraries(example_integration
        kraken_common
        simdjson
        ${OPENSSL_LIBRARIES}
        ${Boost_LIBRARIES}
        pthread
    )
    install(TARGETS example_integration DESTINATION bin)
    message(STATUS "Building example: example_integration")

    # Example 3b: System integration with condition variables (responsive version)
    add_executable(example_integration_cond examples/example_integration_cond.cpp)
    target_link_libraries(example_integration_cond
        kraken_common
        simdjson
        ${OPENSSL_LIBRARIES}
        ${Boost_LIBRARIES}
        pthread
    )
    install(TARGETS example_integration_cond DESTINATION bin)
    message(STATUS "Building example: example_integration_cond")

    # Example 4: simdjson comparison (using template version)
    add_executable(example_simdjson_comparison examples/example_simdjson_comparison.cpp)
    target_link_libraries(example_simdjson_comparison
        kraken_common
        simdjson
        ${OPENSSL_LIBRARIES}
        ${Boost_LIBRARIES}
        pthread
    )
    install(TARGETS example_simdjson_comparison DESTINATION bin)
    message(STATUS "Building example: example_simdjson_comparison")

    # Example 5: Template-based version (demonstrates refactoring)
    add_executable(example_template_version examples/example_template_version.cpp)
    target_link_libraries(example_template_version
        kraken_common
        simdjson
        ${OPENSSL_LIBRARIES}
        ${Boost_LIBRARIES}
        pthread
    )
    install(TARGETS example_template_version DESTINATION bin)
    message(STATUS "Building example: example_template_version")

    # Example 6: Threading patterns (educational)
    add_executable(threading_examples examples/threading_examples.cpp)
    target_link_libraries(threading_examples
        pthread
    )
    install(TARGETS threading_examples DESTINATION bin)
    message(STATUS "Building example: threading_examples")

    # Example 7: sleep_for vs condition variables (educational)
    add_executable(sleep_vs_cv examples/sleep_vs_condition_variable.cpp)
    target_link_libraries(sleep_vs_cv
        pthread
    )
    install(TARGETS sleep_vs_cv DESTINATION bin)
    message(STATUS "Building example: sleep_vs_cv")

    # Example 8: Hybrid callbacks (performance comparison)
    add_executable(example_hybrid_callbacks examples/example_hybrid_callbacks.cpp)
    target_link_libraries(example_hybrid_callbacks
        kraken_common
        simdjson
        ${OPENSSL_LIBRARIES}
        ${Boost_LIBRARIES}
        pthread
    )
    install(TARGETS example_hybrid_callbacks DESTINATION bin)
    message(STATUS "Building example: example_hybrid_callbacks")

    # Production Tool: Kraken Live Data Retriever Level 1
    add_executable(retrieve_kraken_live_data_level1 examples/retrieve_kraken_live_data_level1.cpp)
    target_link_libraries(retrieve_kraken_live_data_level1
        kraken_common
        cli_utils
        simdjson
        ${OPENSSL_LIBRARIES}
        ${Boost_LIBRARIES}
        pthread
    )
    install(TARGETS retrieve_kraken_live_data_level1 DESTINATION bin)
    message(STATUS "Building production tool: retrieve_kraken_live_data_level1")

    # Production Tool: Kraken Live Data Retriever Level 2
    add_executable(retrieve_kraken_live_data_level2 examples/retrieve_kraken_live_data_level2.cpp)
    target_link_libraries(retrieve_kraken_live_data_level2
        kraken_common
        cli_utils
        orderbook_common
        jsonl_writer
        simdjson
        ${OPENSSL_LIBRARIES}
        ${Boost_LIBRARIES}
        pthread
    )
    install(TARGETS retrieve_kraken_live_data_level2 DESTINATION bin)
    message(STATUS "Building production tool: retrieve_kraken_live_data_level2")

    # Production Tool: Process Order Book Snapshots
    add_executable(process_orderbook_snapshots examples/process_orderbook_snapshots.cpp)
    target_link_libraries(process_orderbook_snapshots
        cli_utils
        orderbook_common
        orderbook_state
        snapshot_csv_writer
        simdjson
    )
    install(TARGETS process_orderbook_snapshots DESTINATION bin)
    message(STATUS "Building production tool: process_orderbook_snapshots")

    # Production Tool: Kraken Live Data Retriever Level 3
    add_executable(retrieve_kraken_live_data_level3 examples/retrieve_kraken_live_data_level3.cpp)
    target_link_libraries(retrieve_kraken_live_data_level3
        kraken_level3_client
        level3_jsonl_writer
        level3_common
        kraken_common
        cli_utils
        simdjson
        ${OPENSSL_LIBRARIES}
        ${Boost_LIBRARIES}
        pthread
    )
    install(TARGETS retrieve_kraken_live_data_level3 DESTINATION bin)
    message(STATUS "Building production tool: retrieve_kraken_live_data_level3")

    # Production Tool: Process Level 3 Snapshots
    add_executable(process_level3_snapshots examples/process_level3_snapshots.cpp)
    target_link_libraries(process_level3_snapshots
        cli_utils
        level3_common
        level3_state
        level3_csv_writer
        simdjson
    )
    install(TARGETS process_level3_snapshots DESTINATION bin)
    message(STATUS "Building production tool: process_level3_snapshots")

    # Legacy: Blocking version
    add_executable(query_live_data_v2 legacy/query_live_data_v2_refactored.cpp)
    target_link_libraries(query_live_data_v2
        kraken_common
        ${OPENSSL_LIBRARIES}
        ${Boost_LIBRARIES}
        pthread
    )
    install(TARGETS query_live_data_v2 DESTINATION bin)
    message(STATUS "Building legacy: query_live_data_v2")

    # Legacy: Standalone demo
    add_executable(query_live_data_v2_standalone legacy/query_live_data_v2_standalone_refactored.cpp)
    target_link_libraries(query_live_data_v2_standalone
        kraken_common
        pthread
    )
    install(TARGETS query_live_data_v2_standalone DESTINATION bin)
    message(STATUS "Building legacy: query_live_data_v2_standalone")

else()
    message(STATUS "Skipping full WebSocket versions (missing dependencies)")
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "=================================================")
message(STATUS "Configuration Summary:")
message(STATUS "  C++ Standard:        ${CMAKE_CXX_STANDARD}")
message(STATUS "  OpenSSL version:     ${OPENSSL_VERSION}")
if(BUILD_FULL_VERSION)
    message(STATUS "  Boost version:       ${Boost_VERSION}")
    message(STATUS "  Full version:        ENABLED")
else()
    message(STATUS "  Full version:        DISABLED (missing dependencies)")
endif()
message(STATUS "  Build type:          ${CMAKE_BUILD_TYPE}")
message(STATUS "")
message(STATUS "Project structure:")
message(STATUS "  lib/                 Libraries (common, clients)")
message(STATUS "  examples/            Example applications")
message(STATUS "  legacy/              Legacy implementations")
message(STATUS "  docs/                Documentation")
message(STATUS "=================================================")
message(STATUS "")

if(NOT BUILD_FULL_VERSION)
    message(STATUS "")
    message(STATUS "To build the full WebSocket version, install:")
    message(STATUS "  sudo yum install boost-devel")
    message(STATUS "Then re-run: cmake -B build -S .")
    message(STATUS "")
endif()

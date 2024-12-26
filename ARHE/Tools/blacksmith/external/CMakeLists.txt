include(FetchContent)

function(message)
    if (NOT MESSAGE_QUIET)
        _message(${ARGN})
    endif()
endfunction()

# === ARGAGG ===================================================================
# Using argagg @ GIT_TAG 79e4adfa2c6e2bfbe63da05cc668eb9ad5596748
set(ARGAGG_LOCAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/local/argagg")

if (EXISTS ${ARGAGG_LOCAL_PATH})
    message(STATUS "Using local version of argagg")
    add_subdirectory(${ARGAGG_LOCAL_PATH} ${CMAKE_BINARY_DIR}/argagg EXCLUDE_FROM_ALL)
else()
    message(FATAL_ERROR "argagg dependency not found at ${ARGAGG_LOCAL_PATH}")
endif()

# Header-only library
add_library(
        argagg
        INTERFACE
)

target_include_directories(
        argagg
        INTERFACE
        ${ARGAGG_LOCAL_PATH}/include
)

unset(CMAKE_PROJECT_INCLUDE_BEFORE)

message(STATUS "External dependency argagg -- done!")
set(MESSAGE_QUIET OFF)

# === ASMJIT ===================================================================
if (BLACKSMITH_ENABLE_JITTING)
    # Using asmjit @ GIT_TAG 78de7d9c81a6ad1b0f732b52666960d9be1c6461
    set(ASMJIT_LOCAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/local/asmjit")

    if (EXISTS ${ASMJIT_LOCAL_PATH})
        message(STATUS "Using local version of asmjit")
        add_subdirectory(${ASMJIT_LOCAL_PATH} ${CMAKE_BINARY_DIR}/asmjit EXCLUDE_FROM_ALL)
    else()
        message(FATAL_ERROR "asmjit dependency not found at ${ASMJIT_LOCAL_PATH}")
    endif()

    message(STATUS "External dependency asmjit -- done!")
endif()

# === NLOHMANN/JSON ============================================================
if (BLACKSMITH_ENABLE_JSON)
    # Using nlohmann/json @ GIT_TAG v3.9.1
    set(JSON_LOCAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/local/json")

    if (EXISTS ${JSON_LOCAL_PATH})
        message(STATUS "Using local version of nlohmann/json")
        add_subdirectory(${JSON_LOCAL_PATH} ${CMAKE_BINARY_DIR}/json EXCLUDE_FROM_ALL)
    else()
        message(FATAL_ERROR "nlohmann/json dependency not found at ${JSON_LOCAL_PATH}")
    endif()

    message(STATUS "External dependency nlohmann/json -- done!")
endif()

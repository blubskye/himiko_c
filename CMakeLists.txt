# Himiko Discord Bot (C Edition) - CMake Build
# Copyright (C) 2025 Himiko Contributors
# SPDX-License-Identifier: AGPL-3.0-or-later

cmake_minimum_required(VERSION 3.15)
project(himiko VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# =============================================================================
# Build Options
# =============================================================================
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_MSAN "Enable MemorySanitizer (incompatible with ASan)" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer (incompatible with ASan/MSan)" OFF)
option(ENABLE_FUZZING "Enable AFL++ fuzzing build" OFF)
option(ENABLE_LTO "Enable Link-Time Optimization" OFF)
option(ENABLE_PGO_GEN "Enable PGO instrumentation (generate profile)" OFF)
option(ENABLE_PGO_USE "Enable PGO optimization (use profile)" OFF)
option(ENABLE_OPTIMIZED "Enable aggressive optimizations (O3, Graphite, etc.)" OFF)

# Profile data directory for PGO
set(PGO_PROFILE_DIR "${CMAKE_BINARY_DIR}/pgo-data" CACHE PATH "Directory for PGO profile data")

# AFL++ path (default to system, can override with -DAFL_PATH=...)
set(AFL_PATH "" CACHE PATH "Path to AFL++ installation")

# Sanitizer flags
if(ENABLE_ASAN)
    message(STATUS "AddressSanitizer enabled")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g -O1)
    add_link_options(-fsanitize=address)
endif()

if(ENABLE_UBSAN)
    message(STATUS "UndefinedBehaviorSanitizer enabled")
    add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=undefined)
endif()

if(ENABLE_MSAN)
    if(ENABLE_ASAN)
        message(FATAL_ERROR "MSan and ASan cannot be used together")
    endif()
    message(STATUS "MemorySanitizer enabled")
    add_compile_options(-fsanitize=memory -fno-omit-frame-pointer -g -O1)
    add_link_options(-fsanitize=memory)
endif()

if(ENABLE_TSAN)
    if(ENABLE_ASAN OR ENABLE_MSAN)
        message(FATAL_ERROR "TSan cannot be used with ASan or MSan")
    endif()
    message(STATUS "ThreadSanitizer enabled")
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=thread)
endif()

# =============================================================================
# Optimization Options (O3, LTO, Graphite, PGO)
# =============================================================================
if(ENABLE_OPTIMIZED)
    message(STATUS "Aggressive optimizations enabled (O3, Graphite)")
    add_compile_options(
        -O3
        -march=native
        -mtune=native
        -ffast-math
        -funroll-loops
        -fomit-frame-pointer
        # Graphite optimizations (requires GCC with Graphite support)
        -fgraphite-identity
        -floop-nest-optimize
        -floop-parallelize-all
        -ftree-loop-distribution
        -ftree-loop-im
        -ftree-loop-ivcanon
        -fivopts
        # Additional optimizations
        -fpredictive-commoning
        -fgcse-after-reload
        -ftree-vectorize
        -fvect-cost-model=dynamic
        -fsimd-cost-model=dynamic
        -fipa-cp-clone
        -ftree-partial-pre
        -fdevirtualize-speculatively
    )
endif()

if(ENABLE_LTO)
    message(STATUS "Link-Time Optimization enabled")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    add_compile_options(-flto=auto -fuse-linker-plugin)
    add_link_options(-flto=auto -fuse-linker-plugin)
endif()

if(ENABLE_PGO_GEN)
    message(STATUS "PGO instrumentation enabled (generating profile)")
    file(MAKE_DIRECTORY ${PGO_PROFILE_DIR})
    add_compile_options(-fprofile-generate=${PGO_PROFILE_DIR})
    add_link_options(-fprofile-generate=${PGO_PROFILE_DIR})
endif()

if(ENABLE_PGO_USE)
    message(STATUS "PGO optimization enabled (using profile from ${PGO_PROFILE_DIR})")
    add_compile_options(-fprofile-use=${PGO_PROFILE_DIR} -fprofile-correction)
    add_link_options(-fprofile-use=${PGO_PROFILE_DIR})
endif()

# =============================================================================
# Find required packages
find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)

# Concord library (assumes installed via system or manually)
find_library(CONCORD_LIBRARY NAMES discord concord PATHS /usr/local/lib /usr/lib)
find_path(CONCORD_INCLUDE_DIR NAMES discord.h PATHS /usr/local/include/concord /usr/include/concord)

if(NOT CONCORD_LIBRARY)
    message(WARNING "Concord library not found. You may need to install it from https://github.com/Cogmasters/concord")
endif()

# JSON-C for config parsing
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSON_C REQUIRED json-c)

# Opus for audio encoding (optional for music)
pkg_check_modules(OPUS opus)
if(OPUS_FOUND)
    add_definitions(-DHAVE_OPUS)
endif()

# Libsodium for voice encryption
pkg_check_modules(SODIUM REQUIRED libsodium)
if(SODIUM_FOUND)
    add_definitions(-DHAVE_SODIUM)
endif()

# Enable voice support (Concord must be compiled with voice support)
add_definitions(-DCCORD_VOICE)

# Source files
set(SOURCES
    src/main.c
    src/bot.c
    src/database.c
    src/config.c
    src/debug.c
    src/updater.c
    src/commands/admin.c
    src/commands/fun.c
    src/commands/text.c
    src/commands/images.c
    src/commands/utility.c
    src/commands/info.c
    src/commands/lookup.c
    src/commands/random.c
    src/commands/tools.c
    src/commands/settings.c
    src/commands/xp.c
    src/commands/ai.c
    src/commands/music.c
    src/modules/spam_filter.c
    src/modules/antiraid.c
    src/modules/antispam.c
    src/modules/auto_cleaner.c
    src/modules/logging.c
    src/modules/terminal.c
    src/modules/ticket.c
    src/modules/mention_response.c
    src/audio/voice_udp.c
    src/audio/audio_stream.c
)

# Header files
set(HEADERS
    include/bot.h
    include/database.h
    include/config.h
    include/debug.h
    include/updater.h
    include/commands/admin.h
    include/commands/fun.h
    include/commands/text.h
    include/commands/images.h
    include/commands/utility.h
    include/commands/info.h
    include/commands/lookup.h
    include/commands/random.h
    include/commands/tools.h
    include/commands/settings.h
    include/commands/xp.h
    include/commands/ai.h
    include/commands/music.h
    include/modules/spam_filter.h
    include/modules/antiraid.h
    include/modules/antispam.h
    include/modules/auto_cleaner.h
    include/modules/logging.h
    include/modules/terminal.h
    include/modules/ticket.h
    include/modules/mention_response.h
    include/audio/voice_udp.h
    include/audio/audio_stream.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CONCORD_INCLUDE_DIR}
    ${JSON_C_INCLUDE_DIRS}
    ${OPUS_INCLUDE_DIRS}
    ${SODIUM_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${CONCORD_LIBRARY}
    CURL::libcurl
    SQLite::SQLite3
    ${JSON_C_LIBRARIES}
    ${OPUS_LIBRARIES}
    ${SODIUM_LIBRARIES}
    pthread
    m
)

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Install target
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# =============================================================================
# AFL++ Fuzzing Targets
# =============================================================================
if(ENABLE_FUZZING)
    message(STATUS "AFL++ fuzzing build enabled")
    add_definitions(-DFUZZING_BUILD)

    # Common include directories for fuzz targets
    set(FUZZ_INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/include
        ${CONCORD_INCLUDE_DIR}
        ${JSON_C_INCLUDE_DIRS}
    )

    # Common libraries for fuzz targets
    set(FUZZ_LIBRARIES
        ${JSON_C_LIBRARIES}
        pthread
        m
    )

    # Fuzz target: Config JSON parser
    add_executable(fuzz_config
        fuzz/fuzz_config.c
        src/config.c
        src/debug.c
    )
    target_include_directories(fuzz_config PRIVATE ${FUZZ_INCLUDE_DIRS})
    target_link_libraries(fuzz_config PRIVATE ${FUZZ_LIBRARIES})
    set_target_properties(fuzz_config PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/fuzz)

    # Fuzz target: Duration parser
    add_executable(fuzz_duration
        fuzz/fuzz_duration.c
    )
    target_include_directories(fuzz_duration PRIVATE ${FUZZ_INCLUDE_DIRS})
    target_link_libraries(fuzz_duration PRIVATE ${FUZZ_LIBRARIES})
    set_target_properties(fuzz_duration PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/fuzz)

    # Fuzz target: Math expression evaluator
    add_executable(fuzz_math
        fuzz/fuzz_math.c
    )
    target_include_directories(fuzz_math PRIVATE ${FUZZ_INCLUDE_DIRS})
    target_link_libraries(fuzz_math PRIVATE ${FUZZ_LIBRARIES})
    set_target_properties(fuzz_math PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/fuzz)

    # Fuzz target: Mention parsers
    add_executable(fuzz_mentions
        fuzz/fuzz_mentions.c
    )
    target_include_directories(fuzz_mentions PRIVATE ${FUZZ_INCLUDE_DIRS})
    target_link_libraries(fuzz_mentions PRIVATE ${FUZZ_LIBRARIES})
    set_target_properties(fuzz_mentions PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/fuzz)

    # Fuzz target: Text transformations
    add_executable(fuzz_text
        fuzz/fuzz_text.c
    )
    target_include_directories(fuzz_text PRIVATE ${FUZZ_INCLUDE_DIRS})
    target_link_libraries(fuzz_text PRIVATE ${FUZZ_LIBRARIES})
    set_target_properties(fuzz_text PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/fuzz)

    message(STATUS "Fuzz targets: fuzz_config, fuzz_duration, fuzz_math, fuzz_mentions, fuzz_text")
endif()

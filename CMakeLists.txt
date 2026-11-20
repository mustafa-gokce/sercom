# restrict outdated cmake versions
cmake_minimum_required(VERSION 3.16)

# set version numbers
set(APP_VERSION_MAJOR 1)
set(APP_VERSION_MINOR 0)
set(APP_VERSION_BUILD 0)

# set project name
set(PROJECT_NAME sercom)

# options for protocol limits
set(MAXIMUM_PAYLOAD_SIZE 255)
set(MAXIMUM_PENDING_FRAMES 4)
set(DEFAULT_DUPLICATE_HISTORY_SIZE 16)

#####################
# DO NOT EDIT BELOW #
#####################

# check if this is the main project
set(SERCOM_IS_MAIN_PROJECT OFF)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(SERCOM_IS_MAIN_PROJECT ON)
endif()

# build options
option(SERCOM_BUILD_BINARIES  "Build the binaries in the project"    ${SERCOM_IS_MAIN_PROJECT})
option(SERCOM_BUILD_TESTS     "Build the tests in the project"       ${SERCOM_IS_MAIN_PROJECT})

# set project name
project(${PROJECT_NAME} LANGUAGES CXX)

# get build number
if (DEFINED ENV{GITHUB_RUN_NUMBER})
    set(APP_VERSION_BUILD "$ENV{GITHUB_RUN_NUMBER}")
endif()

# get git hash
if (DEFINED ENV{GITHUB_SHA} AND SERCOM_IS_MAIN_PROJECT)
    set(APP_GIT_HASH "$ENV{GITHUB_SHA}")
else()
    execute_process(
        COMMAND git rev-parse HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_HASH_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    set(APP_GIT_HASH ${GIT_HASH_OUTPUT})
endif()
if (NOT APP_GIT_HASH)
    set(APP_GIT_HASH "unknown")
endif()

# set version numbers
set(APP_VERSION_STRING "${APP_VERSION_MAJOR}.${APP_VERSION_MINOR}.${APP_VERSION_BUILD}")

# use C++ 2017
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# let the options of the dependencies honor the set() lines
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

# disable file prefix check
set(CATCH_ENABLE_REPRODUCIBLE_BUILD OFF)

# disable Catch2 extra build targets
set(CATCH_DEVELOPMENT_BUILD OFF)
set(CATCH_INSTALL_DOCS OFF)
set(CATCH_INSTALL_EXTRAS OFF)

# include external library dependencies
if (SERCOM_BUILD_TESTS)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dep/Catch2)
endif()

# get payload limit of the small limit test build
if (DEFINED ENV{SERCOM_TEST_MAXIMUM_PAYLOAD_SIZE})
    set(MAXIMUM_PAYLOAD_SIZE "$ENV{SERCOM_TEST_MAXIMUM_PAYLOAD_SIZE}")
endif()

# privately reusable settings
add_library(${PROJECT_NAME}_settings INTERFACE)
target_compile_definitions(${PROJECT_NAME}_settings INTERFACE
    APP_VERSION_MAJOR=${APP_VERSION_MAJOR}
    APP_VERSION_MINOR=${APP_VERSION_MINOR}
    APP_VERSION_BUILD=${APP_VERSION_BUILD}
    APP_GIT_HASH="${APP_GIT_HASH}"
    APP_VERSION_STRING="${APP_VERSION_STRING}"
    MAXIMUM_PAYLOAD_SIZE=${MAXIMUM_PAYLOAD_SIZE}
    MAXIMUM_PENDING_FRAMES=${MAXIMUM_PENDING_FRAMES}
    DEFAULT_DUPLICATE_HISTORY_SIZE=${DEFAULT_DUPLICATE_HISTORY_SIZE}
)

# create core library
add_library(${PROJECT_NAME}_core_lib
    core/include/Settings.h core/src/Settings.cpp
    core/include/Definitions.h core/src/Definitions.cpp

    core/include/ByteCodec.h core/src/ByteCodec.cpp
    core/include/CRC32.h core/src/CRC32.cpp
    core/include/Frame.h core/src/Frame.cpp
    core/include/FrameDecoder.h core/src/FrameDecoder.cpp

    core/include/Clock.h core/src/Clock.cpp
    core/include/Transport.h core/src/Transport.cpp
    core/include/Link.h core/src/Link.cpp
)
target_link_libraries(${PROJECT_NAME}_core_lib PRIVATE
    ${PROJECT_NAME}_settings
)

# create application library
add_library(${PROJECT_NAME}_app_lib
    app/include/AppSettings.h app/src/AppSettings.cpp

    app/include/Messages.h app/src/Messages.cpp
    app/include/DeviceApplication.h app/src/DeviceApplication.cpp
    app/include/DeviceClient.h app/src/DeviceClient.cpp
)
target_link_libraries(${PROJECT_NAME}_app_lib PRIVATE
    ${PROJECT_NAME}_settings
    ${PROJECT_NAME}_core_lib
)

# create host library
add_library(${PROJECT_NAME}_host_lib
    host/include/HostSettings.h host/src/HostSettings.cpp

    host/include/HostUtils.h host/src/HostUtils.cpp
    host/include/SerialPort.h host/src/SerialPort.cpp

    host/include/CliApplication.h host/src/CliApplication.cpp
    host/include/SimulatedPlatform.h host/src/SimulatedPlatform.cpp
    host/include/DeviceSimulator.h host/src/DeviceSimulator.cpp
)
target_link_libraries(${PROJECT_NAME}_host_lib PRIVATE
    ${PROJECT_NAME}_settings
    ${PROJECT_NAME}_core_lib
    ${PROJECT_NAME}_app_lib
)

# create tool executables
if (SERCOM_BUILD_BINARIES)
    # create command line executable
    add_executable(${PROJECT_NAME}_cli
        host/apps/sercom_cli.cpp
    )
    target_link_libraries(${PROJECT_NAME}_cli PRIVATE
        ${PROJECT_NAME}_settings
        ${PROJECT_NAME}_core_lib
        ${PROJECT_NAME}_app_lib
        ${PROJECT_NAME}_host_lib
    )

    # create device simulator executable
    add_executable(${PROJECT_NAME}_device_sim
        host/apps/sercom_device_sim.cpp
    )
    target_link_libraries(${PROJECT_NAME}_device_sim PRIVATE
        ${PROJECT_NAME}_settings
        ${PROJECT_NAME}_core_lib
        ${PROJECT_NAME}_app_lib
        ${PROJECT_NAME}_host_lib
    )
endif()

# create example executables
if (SERCOM_BUILD_BINARIES)
    # create raw serial example executable
    add_executable(${PROJECT_NAME}_example_01_raw_serial
        examples/01_raw_serial.cpp
    )
    target_link_libraries(${PROJECT_NAME}_example_01_raw_serial PRIVATE
        ${PROJECT_NAME}_settings
        ${PROJECT_NAME}_core_lib
        ${PROJECT_NAME}_host_lib
    )

    # create frame building example executable
    add_executable(${PROJECT_NAME}_example_02_build_frame
        examples/02_build_frame.cpp
    )
    target_link_libraries(${PROJECT_NAME}_example_02_build_frame PRIVATE
        ${PROJECT_NAME}_settings
        ${PROJECT_NAME}_core_lib
        ${PROJECT_NAME}_app_lib
        ${PROJECT_NAME}_host_lib
    )

    # create noisy stream decoding example executable
    add_executable(${PROJECT_NAME}_example_03_decode_noisy_stream
        examples/03_decode_noisy_stream.cpp
    )
    target_link_libraries(${PROJECT_NAME}_example_03_decode_noisy_stream PRIVATE
        ${PROJECT_NAME}_settings
        ${PROJECT_NAME}_core_lib
        ${PROJECT_NAME}_app_lib
        ${PROJECT_NAME}_host_lib
    )

    # create byte order example executable
    add_executable(${PROJECT_NAME}_example_04_byte_order
        examples/04_byte_order.cpp
    )
    target_link_libraries(${PROJECT_NAME}_example_04_byte_order PRIVATE
        ${PROJECT_NAME}_settings
        ${PROJECT_NAME}_core_lib
        ${PROJECT_NAME}_app_lib
        ${PROJECT_NAME}_host_lib
    )

    # create field writing and reading example executable
    add_executable(${PROJECT_NAME}_example_05_write_and_read_fields
        examples/05_write_and_read_fields.cpp
    )
    target_link_libraries(${PROJECT_NAME}_example_05_write_and_read_fields PRIVATE
        ${PROJECT_NAME}_settings
        ${PROJECT_NAME}_core_lib
        ${PROJECT_NAME}_host_lib
    )

    # create text CRC32 example executable
    add_executable(${PROJECT_NAME}_example_06_crc32_of_text
        examples/06_crc32_of_text.cpp
    )
    target_link_libraries(${PROJECT_NAME}_example_06_crc32_of_text PRIVATE
        ${PROJECT_NAME}_settings
        ${PROJECT_NAME}_core_lib
        ${PROJECT_NAME}_host_lib
    )

    # create piecewise CRC32 example executable
    add_executable(${PROJECT_NAME}_example_07_crc32_in_pieces
        examples/07_crc32_in_pieces.cpp
    )
    target_link_libraries(${PROJECT_NAME}_example_07_crc32_in_pieces PRIVATE
        ${PROJECT_NAME}_settings
        ${PROJECT_NAME}_core_lib
        ${PROJECT_NAME}_app_lib
        ${PROJECT_NAME}_host_lib
    )

    # create frame reading example executable
    add_executable(${PROJECT_NAME}_example_08_read_a_frame
        examples/08_read_a_frame.cpp
    )
    target_link_libraries(${PROJECT_NAME}_example_08_read_a_frame PRIVATE
        ${PROJECT_NAME}_settings
        ${PROJECT_NAME}_core_lib
        ${PROJECT_NAME}_app_lib
        ${PROJECT_NAME}_host_lib
    )

    # create frame flags example executable
    add_executable(${PROJECT_NAME}_example_09_frame_flags
        examples/09_frame_flags.cpp
    )
    target_link_libraries(${PROJECT_NAME}_example_09_frame_flags PRIVATE
        ${PROJECT_NAME}_settings
        ${PROJECT_NAME}_core_lib
        ${PROJECT_NAME}_app_lib
        ${PROJECT_NAME}_host_lib
    )

    # create byte by byte decoding example executable
    add_executable(${PROJECT_NAME}_example_10_decode_byte_by_byte
        examples/10_decode_byte_by_byte.cpp
    )
    target_link_libraries(${PROJECT_NAME}_example_10_decode_byte_by_byte PRIVATE
        ${PROJECT_NAME}_settings
        ${PROJECT_NAME}_core_lib
        ${PROJECT_NAME}_app_lib
        ${PROJECT_NAME}_host_lib
    )

    # create resynchronization after garbage example executable
    add_executable(${PROJECT_NAME}_example_11_resync_after_garbage
        examples/11_resync_after_garbage.cpp
    )
    target_link_libraries(${PROJECT_NAME}_example_11_resync_after_garbage PRIVATE
        ${PROJECT_NAME}_settings
        ${PROJECT_NAME}_core_lib
        ${PROJECT_NAME}_app_lib
        ${PROJECT_NAME}_host_lib
    )

    # create CRC error detection example executable
    add_executable(${PROJECT_NAME}_example_12_crc_error_detected
        examples/12_crc_error_detected.cpp
    )
    target_link_libraries(${PROJECT_NAME}_example_12_crc_error_detected PRIVATE
        ${PROJECT_NAME}_settings
        ${PROJECT_NAME}_core_lib
        ${PROJECT_NAME}_app_lib
        ${PROJECT_NAME}_host_lib
    )

    # create inter-byte timeout example executable
    add_executable(${PROJECT_NAME}_example_13_inter_byte_timeout
        examples/13_inter_byte_timeout.cpp
    )
    target_link_libraries(${PROJECT_NAME}_example_13_inter_byte_timeout PRIVATE
        ${PROJECT_NAME}_settings
        ${PROJECT_NAME}_core_lib
        ${PROJECT_NAME}_app_lib
        ${PROJECT_NAME}_host_lib
    )

    # create two frames in one read example executable
    add_executable(${PROJECT_NAME}_example_14_two_frames_one_read
        examples/14_two_frames_one_read.cpp
    )
    target_link_libraries(${PROJECT_NAME}_example_14_two_frames_one_read PRIVATE
        ${PROJECT_NAME}_settings
        ${PROJECT_NAME}_core_lib
        ${PROJECT_NAME}_app_lib
        ${PROJECT_NAME}_host_lib
    )

    # create ping messages example executable
    add_executable(${PROJECT_NAME}_example_15_ping_messages
        examples/15_ping_messages.cpp
    )
    target_link_libraries(${PROJECT_NAME}_example_15_ping_messages PRIVATE
        ${PROJECT_NAME}_settings
        ${PROJECT_NAME}_core_lib
        ${PROJECT_NAME}_app_lib
        ${PROJECT_NAME}_host_lib
    )

    # create device info message example executable
    add_executable(${PROJECT_NAME}_example_16_device_info_message
        examples/16_device_info_message.cpp
    )
    target_link_libraries(${PROJECT_NAME}_example_16_device_info_message PRIVATE
        ${PROJECT_NAME}_settings
        ${PROJECT_NAME}_core_lib
        ${PROJECT_NAME}_app_lib
        ${PROJECT_NAME}_host_lib
    )

    # create telemetry message example executable
    add_executable(${PROJECT_NAME}_example_17_telemetry_message
        examples/17_telemetry_message.cpp
    )
    target_link_libraries(${PROJECT_NAME}_example_17_telemetry_message PRIVATE
        ${PROJECT_NAME}_settings
        ${PROJECT_NAME}_core_lib
        ${PROJECT_NAME}_app_lib
        ${PROJECT_NAME}_host_lib
    )

    # create link loopback example executable
    add_executable(${PROJECT_NAME}_example_18_link_loopback
        examples/18_link_loopback.cpp
    )
    target_link_libraries(${PROJECT_NAME}_example_18_link_loopback PRIVATE
        ${PROJECT_NAME}_settings
        ${PROJECT_NAME}_core_lib
        ${PROJECT_NAME}_app_lib
        ${PROJECT_NAME}_host_lib
    )

    # create retransmission example executable
    add_executable(${PROJECT_NAME}_example_19_retransmission
        examples/19_retransmission.cpp
    )
    target_link_libraries(${PROJECT_NAME}_example_19_retransmission PRIVATE
        ${PROJECT_NAME}_settings
        ${PROJECT_NAME}_core_lib
        ${PROJECT_NAME}_app_lib
        ${PROJECT_NAME}_host_lib
    )

    # create duplicate detection example executable
    add_executable(${PROJECT_NAME}_example_20_duplicate_detection
        examples/20_duplicate_detection.cpp
    )
    target_link_libraries(${PROJECT_NAME}_example_20_duplicate_detection PRIVATE
        ${PROJECT_NAME}_settings
        ${PROJECT_NAME}_core_lib
        ${PROJECT_NAME}_app_lib
        ${PROJECT_NAME}_host_lib
    )

    # create custom message example executable
    add_executable(${PROJECT_NAME}_example_23_custom_message
        examples/23_custom_message.cpp
    )
    target_link_libraries(${PROJECT_NAME}_example_23_custom_message PRIVATE
        ${PROJECT_NAME}_settings
        ${PROJECT_NAME}_core_lib
        ${PROJECT_NAME}_app_lib
        ${PROJECT_NAME}_host_lib
    )

    # create message dispatching example executable
    add_executable(${PROJECT_NAME}_example_25_dispatch_messages
        examples/25_dispatch_messages.cpp
    )
    target_link_libraries(${PROJECT_NAME}_example_25_dispatch_messages PRIVATE
        ${PROJECT_NAME}_settings
        ${PROJECT_NAME}_core_lib
        ${PROJECT_NAME}_app_lib
        ${PROJECT_NAME}_host_lib
    )
endif()

# create fuzz executables (only with the fuzzer flags passed from outside)
if (CMAKE_CXX_FLAGS MATCHES "fuzzer-no-link")
    # create frame decoder fuzz executable
    add_executable(${PROJECT_NAME}_fuzz_frame_decoder
        fuzz/fuzz_FrameDecoder.cpp
    )
    target_link_options(${PROJECT_NAME}_fuzz_frame_decoder PRIVATE -fsanitize=fuzzer)
    target_link_libraries(${PROJECT_NAME}_fuzz_frame_decoder PRIVATE
        ${PROJECT_NAME}_settings
        ${PROJECT_NAME}_core_lib
    )

    # create link fuzz executable
    add_executable(${PROJECT_NAME}_fuzz_link
        fuzz/fuzz_Link.cpp
    )
    target_link_options(${PROJECT_NAME}_fuzz_link PRIVATE -fsanitize=fuzzer)
    target_link_libraries(${PROJECT_NAME}_fuzz_link PRIVATE
        ${PROJECT_NAME}_settings
        ${PROJECT_NAME}_core_lib
    )
endif()

# create test executable for auto tests
if (SERCOM_BUILD_TESTS)
    add_executable(${PROJECT_NAME}_test
        tests/test_main.cpp
        tests/test_CRC32.cpp
        tests/test_ByteCodec.cpp
        tests/test_Frame.cpp
        tests/test_FrameDecoder.cpp
        tests/test_Link.cpp
        tests/test_Messages.cpp
        tests/test_NoiseChecklist.cpp
        tests/test_Definitions.cpp
        tests/test_SerialPort.cpp
        tests/test_CliApplication.cpp
    )
    target_link_libraries(${PROJECT_NAME}_test PRIVATE
        ${PROJECT_NAME}_settings
        ${PROJECT_NAME}_core_lib
        ${PROJECT_NAME}_app_lib
        ${PROJECT_NAME}_host_lib
        Catch2::Catch2
    )
endif()

# introduce test
if (SERCOM_BUILD_TESTS)
    enable_testing()
    add_test(
        NAME ${PROJECT_NAME}_test
        COMMAND ${PROJECT_NAME}_test
        --reporter JUnit::out=test_results_junit.xml
        --reporter xml::out=test_results.xml
        --reporter console
    )
endif()

# compiler flag add macro
macro(add_compiler_flags)
    foreach (flag ${ARGV})
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
    endforeach()
endmacro()

# enable additional warnings
add_compiler_flags(-Wall)
add_compiler_flags(-Wextra)
add_compiler_flags(-Wpedantic)
add_compiler_flags(-Wconversion)
add_compiler_flags(-Werror)
add_compiler_flags(-pedantic-errors)
add_compiler_flags(-fstrict-aliasing)
add_compiler_flags(-fvisibility=hidden)
add_compiler_flags(-fdiagnostics-show-option)
add_compiler_flags(-Wconversion)
add_compiler_flags(-Wold-style-cast)
add_compiler_flags(-Wfloat-equal)
add_compiler_flags(-Wlogical-op)
add_compiler_flags(-Wundef)
add_compiler_flags(-Wredundant-decls)
add_compiler_flags(-Wwrite-strings)
add_compiler_flags(-Wpointer-arith)
add_compiler_flags(-Wcast-qual)
add_compiler_flags(-Wformat=2)
add_compiler_flags(-Wswitch-default)
add_compiler_flags(-Wmissing-include-dirs)
add_compiler_flags(-Wcast-align)
add_compiler_flags(-Wnon-virtual-dtor)
add_compiler_flags(-Wsign-conversion)
add_compiler_flags(-Wdisabled-optimization)
add_compiler_flags(-Winvalid-pch)
add_compiler_flags(-Woverloaded-virtual)
add_compiler_flags(-Wunused-but-set-variable)
add_compiler_flags(-Wunused-result)
add_compiler_flags(-Wsuggest-attribute=format)
add_compiler_flags(-Wsuggest-attribute=noreturn)
add_compiler_flags(-Wnoexcept)
add_compiler_flags(-Wno-missing-field-initializers)
add_compiler_flags(-Wdouble-promotion)
add_compiler_flags(-Wtrampolines)
add_compiler_flags(-Wzero-as-null-pointer-constant)
add_compiler_flags(-Wvector-operation-performance)
add_compiler_flags(-Wshift-overflow=2)
add_compiler_flags(-Wduplicated-cond)
add_compiler_flags(-Walloc-zero)
add_compiler_flags(-Walloca)
add_compiler_flags(-Wduplicated-branches)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compiler_flags(-Wstrict-overflow=2)
    add_compiler_flags(-Wsuggest-attribute=const)
    add_compiler_flags(-Wsuggest-attribute=pure)
    add_compiler_flags(-Wsuggest-final-methods)
    add_compiler_flags(-Wsuggest-final-types)
    add_compiler_flags(-Wno-c++98-compat)
    add_compiler_flags(-Wno-c++98-compat-pedantic)
    add_compiler_flags(-Wno-c++98-compat-bind-to-temporary-copy)
    add_compiler_flags(-Wno-c++98-compat-local-type-template-args)
endif()

# add optimizers on builds
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
endif()

# check the compiler of the fuzz executables
if (CMAKE_CXX_FLAGS MATCHES "fuzzer-no-link" AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "The fuzz executables need Clang")
endif()

# debug the project
string(SUBSTRING ${APP_GIT_HASH} 0 10 APP_GIT_HASH_SHORT)
message(STATUS "###################################################")
if (SERCOM_IS_MAIN_PROJECT)
    message(STATUS ">>> PROJECT_NAME        : ${PROJECT_NAME}      ")
else()
    message(STATUS ">>> PROJECT_NAME        : ${PROJECT_NAME} (sub)")
endif()
message(STATUS ">>> APP_VERSION_STRING  : ${APP_VERSION_STRING}    ")
message(STATUS ">>> APP_GIT_HASH        : ${APP_GIT_HASH_SHORT}    ")
message(STATUS "###################################################")

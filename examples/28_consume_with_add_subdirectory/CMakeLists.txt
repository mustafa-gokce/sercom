# restrict outdated cmake versions
cmake_minimum_required(VERSION 3.16)

# set project name
set(PROJECT_NAME consume_with_add_subdirectory)

# set the sercom checkout, a git submodule in dep/sercom of a project of its own
set(SERCOM_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)

#####################
# DO NOT EDIT BELOW #
#####################

# set project name
project(${PROJECT_NAME} LANGUAGES CXX)

# use C++ 2017
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# add sercom without its tests, binaries and host library
add_subdirectory(${SERCOM_DIRECTORY} ${CMAKE_CURRENT_BINARY_DIR}/sercom EXCLUDE_FROM_ALL)

# create consumer executable
add_executable(${PROJECT_NAME}
    28_consume_with_add_subdirectory.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE
    sercom::settings
    sercom::core
    sercom::app
)

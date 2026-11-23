# restrict outdated cmake versions
cmake_minimum_required(VERSION 3.16)

# set project name
set(PROJECT_NAME consume_with_fetch_content)

# set the repository and the tag of sercom
set(SERCOM_GIT_REPOSITORY https://github.com/mustafa-gokce/sercom.git)
set(SERCOM_GIT_TAG L115)

#####################
# DO NOT EDIT BELOW #
#####################

# set project name
project(${PROJECT_NAME} LANGUAGES CXX)

# use C++ 2017
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# fetch sercom at its tag with its host library
include(FetchContent)
set(SERCOM_BUILD_HOST ON)
FetchContent_Declare(sercom
    GIT_REPOSITORY ${SERCOM_GIT_REPOSITORY}
    GIT_TAG ${SERCOM_GIT_TAG}
    GIT_SHALLOW ON
    GIT_SUBMODULES ""
)
FetchContent_MakeAvailable(sercom)

# find Qt packages of the consumer
find_package(Qt5 REQUIRED COMPONENTS Core SerialPort)

# create consumer executable
add_executable(${PROJECT_NAME}
    29_consume_with_fetch_content.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE
    sercom::settings
    sercom::core
    sercom::app
    sercom::host
    Qt5::Core
    Qt5::SerialPort
)

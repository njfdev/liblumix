cmake_minimum_required(VERSION 3.20)
project(liblumix C CXX)

include(GNUInstallDirs)

# remove lib prefix
set(CMAKE_SHARED_LIBRARY_PREFIX "")

# set install prefix to /usr if on linux (default is /usr/local)
if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(CMAKE_INSTALL_PREFIX "/usr")
endif()

set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 1)
set(PROJECT_VERSION_PATCH 0)
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

add_library(liblumix SHARED liblumix.cpp liblumix.h)

set_property(TARGET liblumix PROPERTY CXX_STANDARD 23)

# add cpr (Curl for People) library
include(FetchContent)
FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git
                         GIT_TAG 871ed52d350214a034f6ef8a3b8f51c5ce1bd400) # The commit hash for 1.9.0 -> https://github.com/libcpr/cpr/releases
FetchContent_MakeAvailable(cpr)
target_link_libraries(liblumix PRIVATE cpr::cpr)

# add cppcodec library
FetchContent_Declare(cppcodec GIT_REPOSITORY https://github.com/tplgy/cppcodec.git
                              GIT_TAG 302dc28f8fd5c8bf2ea8d7212aed3be884d5d166) # The commit hash for 0.2 -> https://github.com/tplgy/cppcodec/releases
FetchContent_MakeAvailable(cppcodec)
target_link_libraries(liblumix PRIVATE cppcodec)

# get header correctly setup
set_target_properties(liblumix PROPERTIES PUBLIC_HEADER liblumix.h)

# correct set the build macro
target_compile_definitions(liblumix PRIVATE LIBLUMIX_BUILD)

# install the library
install(TARGETS liblumix
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
cmake_minimum_required(VERSION 3.15)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(BDParser CXX)

set(CMAKE_CONFIGURATION_TYPES
    "Debug"
    "Release"
    CACHE STRING "" FORCE
)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING
        "Choose the type of build, options are: Debug Release."
        FORCE)
endif()

if(MSVC)
    option(STATIC_MSVC_CRT "Static MSVC runtimes" ON)
    if(STATIC_MSVC_CRT)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(${PROJECT_NAME}
    "${PROJECT_SOURCE_DIR}/src/BDParser.cpp"
    "${PROJECT_SOURCE_DIR}/src/BDParser.hpp"
)
target_include_directories(${PROJECT_NAME} PUBLIC "${PROJECT_SOURCE_DIR}/src")

if(MSVC)
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        "NOMINMAX"
        "UNICODE"
        "_UNICODE"
        "WIN32_LEAN_AND_MEAN"
    )
endif()

if(BDPARSER_BUILD_SAMPLE)
    add_executable(Sample example/main.cpp)
    target_link_libraries(Sample PRIVATE ${PROJECT_NAME})

    if(MSVC)
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Sample)
        set_target_properties(Sample PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_BINARY_DIR}/$<0:>)
    endif()
endif()

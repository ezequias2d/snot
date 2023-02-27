cmake_minimum_required(VERSION 3.0.0)
project(snot VERSION 0.1.0)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(SNOT_STANDALONE TRUE)
endif()

option(SNOT_BUILD_EXAMPLES "Build the GLFW example programs" ${SNOT_STANDALONE})

include(CTest)
enable_testing()

set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED True)

set(SOURCE_DIR src)
set(INCLUDE_DIR include)
set(SOURCES
    ${SOURCE_DIR}/snot.c
)

add_library(snot SHARED ${SOURCES})
target_include_directories(snot PRIVATE ${SOURCE_DIR})
target_include_directories(snot PUBLIC ${INCLUDE_DIR})

if(MSVC)
    target_compile_options(snot PRIVATE
        /MP
        $<$<CONFIG:DEBUG>:/W4>
        $<$<CONFIG:DEBUG>:/WX>)
else()
    target_compile_options(snot PRIVATE

        $<$<CONFIG:DEBUG>:-Wall>
        $<$<CONFIG:DEBUG>:-Wextra>
        $<$<CONFIG:DEBUG>:-pedantic>
        $<$<CONFIG:DEBUG>:-Werror>
    )
endif()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

if(SNOT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

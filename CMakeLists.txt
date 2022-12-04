cmake_minimum_required(VERSION 3.16)

project(libbutteraugli LANGUAGES CXX)

set(sources src/Butteraugli.cpp)

if (MSVC)
    set(sources ${sources} ${CMAKE_CURRENT_BINARY_DIR}/Butteraugli.rc)
endif ()

add_library(butteraugli SHARED ${sources})

if (UNIX)
    target_include_directories(butteraugli PRIVATE /usr/local/include/avisynth)
else ()
    target_include_directories(butteraugli PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../AviSynthPlus/avs_core/include")
endif ()

target_include_directories(butteraugli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/libjxl
    ${CMAKE_CURRENT_SOURCE_DIR}/libjxl/jxl_install/include
)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

string(TOLOWER ${CMAKE_BUILD_TYPE} build_type)
if (build_type STREQUAL debug)
    target_compile_definitions(butteraugli PRIVATE DEBUG_BUILD)
else (build_type STREQUAL release)
    target_compile_definitions(butteraugli PRIVATE RELEASE_BUILD)
endif ()

message(STATUS "Build type - ${CMAKE_BUILD_TYPE}")

target_compile_features(butteraugli PRIVATE cxx_std_17)

if (UNIX)
    target_compile_options(butteraugli PRIVATE $<$<CONFIG:RELEASE>:-ffunction-sections -s>)
#else ()
#    target_compile_options(butteraugli PRIVATE $<$<CONFIG:RELEASE>:/fp:precise /Gy /GS /Ot /Oi /Oy /GL>)
endif ()

if (NOT MSVC)
    find_library(libjxl NAMES jxl PATHS ${CMAKE_CURRENT_SOURCE_DIR}/libjxl/jxl_install/lib)
else ()
    find_library(libjxl NAMES jxl-static PATHS ${CMAKE_CURRENT_SOURCE_DIR}/libjxl/jxl_install/lib)
endif ()
message(STATUS "find_library returned ${libjxl}")

find_library(libhwy NAMES hwy PATHS ${CMAKE_CURRENT_SOURCE_DIR}/libjxl/jxl_install/lib)
message(STATUS "find_library returned ${libhwy}")

target_link_libraries(butteraugli PRIVATE
    ${libjxl}
    ${libhwy}
)

#if (MSVC)
#    target_link_options(butteraugli PRIVATE $<$<CONFIG:RELEASE>:/NOLOGO /OPT:REF /OPT:ICF /LTCG>)
#endif ()

find_package (Git)

if (GIT_FOUND)
    execute_process (COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
        OUTPUT_VARIABLE ver
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else ()
    message (STATUS "GIT not found")
endif ()

if (MSVC)
    configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/Butteraugli.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Butteraugli.rc"
    )
endif ()

if (MSVC)
    set_target_properties(butteraugli PROPERTIES
        PREFIX ""
        OUTPUT_NAME "Butteraugli"
    )
else ()
    set_target_properties(butteraugli PROPERTIES OUTPUT_NAME "butteraugli.${ver}")
endif ()

if (UNIX)
    include(GNUInstallDirs)

    INSTALL(TARGETS butteraugli LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/avisynth")

    # uninstall target
    if(NOT TARGET uninstall)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY)

    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
    endif ()
endif ()

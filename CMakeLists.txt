cmake_minimum_required(VERSION 3.16)

project(libbutteraugli LANGUAGES CXX)

set(sources ${CMAKE_CURRENT_SOURCE_DIR}/src/Butteraugli.cpp)

if (WIN32)
    set(sources ${sources} ${CMAKE_CURRENT_SOURCE_DIR}/src/Butteraugli.rc)
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

if (NOT CMAKE_GENERATOR MATCHES "Visual Studio")
    if (NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
    endif()

    string(TOLOWER ${CMAKE_BUILD_TYPE} build_type)
    if (build_type STREQUAL debug)
        target_compile_definitions(butteraugli PRIVATE DEBUG_BUILD)
    else (build_type STREQUAL release)
        target_compile_definitions(butteraugli PRIVATE RELEASE_BUILD)
    endif()

    message(STATUS "Build type - ${CMAKE_BUILD_TYPE}")
endif()

target_compile_features(butteraugli PRIVATE cxx_std_17)

find_library(libjxl
    NAMES jxl jxl-static
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/libjxl/jxl_install/lib
)
find_library(libhwy NAMES hwy PATHS ${CMAKE_CURRENT_SOURCE_DIR}/libjxl/jxl_install/lib NO_DEFAULT_PATH)

message(STATUS "Butteraugli: find_library returned ${libjxl}")
message(STATUS "Butteraugli: find_library returned ${libhwy}")

target_link_libraries(butteraugli PRIVATE
    ${libjxl}
    ${libhwy}
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM")
    target_link_libraries(butteraugli PRIVATE
        libmmds
        svml_disp
    )
endif ()

if (MINGW)
    target_link_libraries(butteraugli PRIVATE -static-libstdc++ -static-libgcc -s)
endif()

if (WIN32)
    set_target_properties(butteraugli PROPERTIES
        PREFIX ""
        OUTPUT_NAME "Butteraugli"
    )
else ()
    find_package (Git)    
    if (GIT_FOUND)
        execute_process (COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
            OUTPUT_VARIABLE ver
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    else ()
        message (STATUS "GIT not found")
    endif ()

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

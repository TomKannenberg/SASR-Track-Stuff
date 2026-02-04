cmake_minimum_required(VERSION 3.22)
project(CppSLib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Root directories for source and include layouts.
set(SLLIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/SlLib")
set(SEEDITOR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/SeEditor")
set(SLLIB_MARIOKART_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/SlLib.MarioKart")
set(MAPPING_GC_PATH "${CMAKE_CURRENT_SOURCE_DIR}/SeEditor/Resources/MAPPING.GC")
set(MAPPING_GS3_PATH "${CMAKE_CURRENT_SOURCE_DIR}/SeEditor/Resources/MAPPING.GS3")

# Collect sources for each logical target.
file(GLOB_RECURSE SLLIB_SOURCES
    "${SLLIB_ROOT}/*.cpp"
)

file(GLOB_RECURSE SEEDITOR_SOURCES
    "${SEEDITOR_ROOT}/*.cpp"
)
list(FILTER SEEDITOR_SOURCES EXCLUDE REGEX ".*/SifParser\\.cpp$")
if (WIN32)
    list(FILTER SEEDITOR_SOURCES EXCLUDE REGEX ".*/Dialogs/TinyFileDialog_stub\\.cpp$")
endif()
if (NOT WIN32)
    list(FILTER SEEDITOR_SOURCES EXCLUDE REGEX ".*/Dialogs/TinyFileDialog\\.cpp$")
    list(APPEND SEEDITOR_SOURCES "${SEEDITOR_ROOT}/Dialogs/TinyFileDialog_stub.cpp")
endif()

add_library(SeEditorCore STATIC "${SEEDITOR_ROOT}/SifParser.cpp")
target_include_directories(SeEditorCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(SeEditorCore PUBLIC ZLIB::ZLIB)

file(GLOB_RECURSE SLLIB_MARIOKART_SOURCES
    "${SLLIB_MARIOKART_ROOT}/*.cpp"
)

add_library(SlLib STATIC ${SLLIB_SOURCES})
target_include_directories(SlLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(SlLibMarioKart STATIC ${SLLIB_MARIOKART_SOURCES})
target_link_libraries(SlLibMarioKart PUBLIC SlLib)
target_include_directories(SlLibMarioKart PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(SeEditorForest STATIC ${SEEDITOR_ROOT}/Forest/ForestTypes.cpp)
target_link_libraries(SeEditorForest PUBLIC SlLib)
target_include_directories(SeEditorForest PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(UnityTooling)

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG QUIET)
if (NOT TARGET glad::glad)
    add_subdirectory(linux_libs/glad EXCLUDE_FROM_ALL)
endif()
set(ZLIB_USE_STATIC_LIBS ON)
if (DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
    set(_vcpkg_zlib_lib "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib/libz.a")
    set(_vcpkg_zlib_inc "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include")
    if (EXISTS "${_vcpkg_zlib_lib}")
        set(ZLIB_LIBRARY "${_vcpkg_zlib_lib}")
        set(ZLIB_LIBRARY_RELEASE "${_vcpkg_zlib_lib}")
        set(ZLIB_INCLUDE_DIR "${_vcpkg_zlib_inc}")
    endif()
endif()
find_package(ZLIB REQUIRED)

add_library(SeEditorLib STATIC ${SEEDITOR_SOURCES})
target_link_libraries(SeEditorLib PUBLIC SeEditorCore SlLib imgui::imgui glfw glad::glad OpenGL::GL)
target_include_directories(SeEditorLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
if (EXISTS "${MAPPING_GC_PATH}")
    set(EMBEDDED_MAPPING_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/EmbeddedMapping.hpp")
    set(EMBEDDED_MAPPING_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/SeEditor/Resources/GenerateEmbeddedMapping.cmake")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
    set(EMBEDDED_MAPPING_DEPENDS "${MAPPING_GC_PATH}" "${EMBEDDED_MAPPING_SCRIPT}")
    if (EXISTS "${MAPPING_GS3_PATH}")
        list(APPEND EMBEDDED_MAPPING_DEPENDS "${MAPPING_GS3_PATH}")
    endif()
    add_custom_command(
        OUTPUT "${EMBEDDED_MAPPING_HEADER}"
        COMMAND ${CMAKE_COMMAND}
            -DMAPPING_GC_PATH="${MAPPING_GC_PATH}"
            -DMAPPING_GS3_PATH="${MAPPING_GS3_PATH}"
            -DEMBEDDED_MAPPING_HEADER="${EMBEDDED_MAPPING_HEADER}"
            -P "${EMBEDDED_MAPPING_SCRIPT}"
        DEPENDS ${EMBEDDED_MAPPING_DEPENDS}
        COMMENT "Generating EmbeddedMapping.hpp from MAPPING.GC/MAPPING.GS3"
        VERBATIM
    )
    add_custom_target(EmbeddedMapping DEPENDS "${EMBEDDED_MAPPING_HEADER}")
    add_dependencies(SeEditorLib EmbeddedMapping)
    target_include_directories(SeEditorLib PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_compile_definitions(SeEditorLib PRIVATE SEEDITOR_EMBED_MAPPING=1)
endif()

add_executable(CppSLib main.cpp)
target_link_libraries(CppSLib PRIVATE SeEditorLib SlLibMarioKart)

# Convenience: copy the built CppSLib.exe two directories up after a successful build.
if (WIN32)
    add_custom_command(TARGET CppSLib POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:CppSLib>"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../$<TARGET_FILE_NAME:CppSLib>"
        COMMENT "Copying CppSLib to ../../"
        VERBATIM
    )
endif()

add_executable(forest_extractor SeEditor/ForestExtractor.cpp)
target_link_libraries(forest_extractor PRIVATE SeEditorCore)
target_include_directories(forest_extractor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(forest_to_obj SeEditor/ForestToObj.cpp)
target_link_libraries(forest_to_obj PRIVATE SeEditorCore SeEditorForest SlLib)
target_include_directories(forest_to_obj PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(forest_unpacker SeEditor/ForestUnpacker.cpp)
target_link_libraries(forest_unpacker PRIVATE SeEditorCore)
target_include_directories(forest_unpacker PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(forest_unpacker PRIVATE ZLIB::ZLIB)

add_executable(sif_to_unity SeEditor/SifToUnity.cpp)
target_link_libraries(sif_to_unity PRIVATE SeEditorLib SlLib SeEditorCore)
target_include_directories(sif_to_unity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(sif_unpacker SeEditor/SifUnpacker.cpp)
target_link_libraries(sif_unpacker PRIVATE SeEditorCore SeEditorForest)
target_include_directories(sif_unpacker PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(forest_tree_dump SeEditor/ForestTreeDump.cpp)
target_link_libraries(forest_tree_dump PRIVATE SeEditorCore SeEditorForest)
target_include_directories(forest_tree_dump PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(anim_debug SeEditor/AnimDebug.cpp)
target_link_libraries(anim_debug PRIVATE SeEditorCore SeEditorForest)
target_include_directories(anim_debug PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(SlLib PRIVATE ZLIB::ZLIB)

# CLI unit tests.
add_executable(type6_decode_tests tests/type6_decode_tests.cpp)
target_link_libraries(type6_decode_tests PRIVATE SeEditorLib SlLib)
target_include_directories(type6_decode_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(type6_decode_tests PRIVATE
    SSR_WII_ELF_C_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../SSR_Wii.elf.c"
    SSR_WII_ELF_H_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../SSR_Wii.elf.h"
)

# Statically link libgcc/libstdc++ for MinGW builds.
if (MINGW)
    foreach(_tgt CppSLib forest_extractor forest_to_obj forest_unpacker sif_to_unity)
        target_link_options(${_tgt} PRIVATE -static -static-libgcc -static-libstdc++)
    endforeach()
endif()

find_package(imgui CONFIG QUIET)
if (NOT TARGET imgui::imgui)
    add_subdirectory(linux_libs/imgui EXCLUDE_FROM_ALL)
endif()

include(GNUInstallDirs)

install(TARGETS CppSLib
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

cmake_minimum_required(VERSION 3.22)
project(CppSLib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Root directories for source and include layouts.
set(SLLIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/SlLib")
set(SEEDITOR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/SeEditor")
set(SLLIB_MARIOKART_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/SlLib.MarioKart")

# Collect sources for each logical target.
file(GLOB_RECURSE SLLIB_SOURCES
    "${SLLIB_ROOT}/*.cpp"
)

file(GLOB_RECURSE SEEDITOR_SOURCES
    "${SEEDITOR_ROOT}/*.cpp"
)
list(FILTER SEEDITOR_SOURCES EXCLUDE REGEX ".*/SifParser\\.cpp$")

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

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

add_library(SeEditorLib STATIC ${SEEDITOR_SOURCES})
target_link_libraries(SeEditorLib PUBLIC SeEditorCore SlLib imgui::imgui glfw glad::glad OpenGL::GL)
target_include_directories(SeEditorLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(CppSLib main.cpp)
target_link_libraries(CppSLib PRIVATE SeEditorLib SlLibMarioKart)

add_executable(forest_extractor SeEditor/ForestExtractor.cpp)
target_link_libraries(forest_extractor PRIVATE SeEditorCore)
target_include_directories(forest_extractor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(forest_to_obj SeEditor/ForestToObj.cpp)
target_link_libraries(forest_to_obj PRIVATE SeEditorCore SlLib SeEditorLib)
target_include_directories(forest_to_obj PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(forest_unpacker SeEditor/ForestUnpacker.cpp)
target_link_libraries(forest_unpacker PRIVATE SeEditorCore)
target_include_directories(forest_unpacker PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(forest_unpacker PRIVATE ZLIB::ZLIB)

target_link_libraries(SlLib PRIVATE ZLIB::ZLIB)

# Statically link libgcc/libstdc++ for MinGW builds.
if (MINGW)
    foreach(_tgt CppSLib forest_extractor forest_to_obj forest_unpacker)
        target_link_options(${_tgt} PRIVATE -static -static-libgcc -static-libstdc++)
    endforeach()
endif()

find_package(imgui CONFIG REQUIRED)

include(GNUInstallDirs)

install(TARGETS CppSLib
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

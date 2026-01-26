add_library(UnityTooling STATIC
    src/SceneBuilder.cpp
)

target_include_directories(UnityTooling PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(UnitySceneExample
    examples/simple_scene.cpp
)
target_link_libraries(UnitySceneExample PRIVATE UnityTooling)
target_include_directories(UnitySceneExample PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

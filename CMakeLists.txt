cmake_minimum_required(VERSION 3.16.3)

project(retriever)

# ${PROJECT_NAME} used to reference name

include_directories(glad)

add_executable(${PROJECT_NAME} main.cpp glad/glad.c 
)

add_subdirectory(Adder)
add_subdirectory(external/glfw)

target_include_directories(${PROJECT_NAME}
    PUBLIC Adder
    PUBLIC external/glfw/include
    PUBLIC external/glfw/deps
)

target_link_directories(${PROJECT_NAME}
    PRIVATE Adder
    PRIVATE external/glfw/include
)

target_link_libraries(${PROJECT_NAME} adder glfw)

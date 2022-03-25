cmake_minimum_required(VERSION 3.16.3)

project(retriever)

# ${PROJECT_NAME} used to reference name

add_executable(${PROJECT_NAME} main.cpp)

add_subdirectory(Adder)

target_link_libraries(${PROJECT_NAME} adder)

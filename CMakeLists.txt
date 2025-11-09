cmake_minimum_required(VERSION 3.14)
project(TrabalhoFinalOpenGL LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_SHARED_LIBS "Build libraries shared" OFF)

include(FetchContent)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.3.8
)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
  glad
  GIT_REPOSITORY https://github.com/Dav1dde/glad.git
  GIT_TAG v0.1.34
)
FetchContent_MakeAvailable(glad)

FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 0.9.9.8
)
FetchContent_MakeAvailable(glm)

add_executable(app
  src/main.cpp
)

target_include_directories(app PRIVATE ${PROJECT_SOURCE_DIR}/include)

target_link_libraries(app PRIVATE glfw glad glm)

if (WIN32)
  # nothing extra required for typical MSVC/MinGW setups
endif()

configure_file(${CMAKE_SOURCE_DIR}/shaders/simple.vert ${CMAKE_BINARY_DIR}/shaders/simple.vert COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/shaders/simple.frag ${CMAKE_BINARY_DIR}/shaders/simple.frag COPYONLY)

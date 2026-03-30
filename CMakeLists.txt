cmake_minimum_required(VERSION 3.31)
project(CG2026 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
include(FetchContent)
FetchContent_Declare(tinyobjloader GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git GIT_TAG v2.0.0rc13)
FetchContent_MakeAvailable(tinyobjloader)
FetchContent_Declare(stb GIT_REPOSITORY https://github.com/nothings/stb.git GIT_TAG master)
FetchContent_MakeAvailable(stb)
file(GLOB_RECURSE PROJECT_SRC ${PROJECT_SOURCE_DIR}/src/*)
file(COPY ${PROJECT_SOURCE_DIR}/assets DESTINATION ${PROJECT_BINARY_DIR})
add_executable(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/main.cpp ${PROJECT_SRC})
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include/)
target_include_directories(${PROJECT_NAME} PRIVATE ${stb_SOURCE_DIR})
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
target_include_directories(${PROJECT_NAME} PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan glfw glm::glm tinyobjloader)
if(UNIX AND NOT APPLE)
  target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_WAYLAND_KHR)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
  target_include_directories(${PROJECT_NAME} PRIVATE ${WAYLAND_CLIENT_INCLUDE_DIRS})
  target_link_libraries(${PROJECT_NAME} PRIVATE ${WAYLAND_CLIENT_LIBRARIES})
endif()
find_program(GLSLC glslc)
if(GLSLC)
  set(SHADER_SOURCE_DIR ${PROJECT_SOURCE_DIR}/shaders)
  set(SHADER_BINARY_DIR ${PROJECT_BINARY_DIR}/shaders)
  file(MAKE_DIRECTORY ${SHADER_BINARY_DIR})
  add_custom_command(OUTPUT ${SHADER_BINARY_DIR}/shadow.spv COMMAND ${GLSLC} -fshader-stage=vertex ${SHADER_SOURCE_DIR}/shadow.vert -o ${SHADER_BINARY_DIR}/shadow.spv DEPENDS ${SHADER_SOURCE_DIR}/shadow.vert)

  # Геометрия + Тесселяция
  add_custom_command(OUTPUT ${SHADER_BINARY_DIR}/geom.vert.spv COMMAND ${GLSLC} -fshader-stage=vertex ${SHADER_SOURCE_DIR}/geom.vert -o ${SHADER_BINARY_DIR}/geom.vert.spv DEPENDS ${SHADER_SOURCE_DIR}/geom.vert)
  add_custom_command(OUTPUT ${SHADER_BINARY_DIR}/geom.tesc.spv COMMAND ${GLSLC} -fshader-stage=tesscontrol ${SHADER_SOURCE_DIR}/geom.tesc -o ${SHADER_BINARY_DIR}/geom.tesc.spv DEPENDS ${SHADER_SOURCE_DIR}/geom.tesc)
  add_custom_command(OUTPUT ${SHADER_BINARY_DIR}/geom.tese.spv COMMAND ${GLSLC} -fshader-stage=tesseval ${SHADER_SOURCE_DIR}/geom.tese -o ${SHADER_BINARY_DIR}/geom.tese.spv DEPENDS ${SHADER_SOURCE_DIR}/geom.tese)
  add_custom_command(OUTPUT ${SHADER_BINARY_DIR}/geom.frag.spv COMMAND ${GLSLC} -fshader-stage=fragment ${SHADER_SOURCE_DIR}/geom.frag -o ${SHADER_BINARY_DIR}/geom.frag.spv DEPENDS ${SHADER_SOURCE_DIR}/geom.frag)

  add_custom_command(OUTPUT ${SHADER_BINARY_DIR}/light.vert.spv COMMAND ${GLSLC} -fshader-stage=vertex ${SHADER_SOURCE_DIR}/light.vert -o ${SHADER_BINARY_DIR}/light.vert.spv DEPENDS ${SHADER_SOURCE_DIR}/light.vert)
  add_custom_command(OUTPUT ${SHADER_BINARY_DIR}/light.frag.spv COMMAND ${GLSLC} -fshader-stage=fragment ${SHADER_SOURCE_DIR}/light.frag -o ${SHADER_BINARY_DIR}/light.frag.spv DEPENDS ${SHADER_SOURCE_DIR}/light.frag)

  add_custom_target(Shaders DEPENDS
      ${SHADER_BINARY_DIR}/shadow.spv
      ${SHADER_BINARY_DIR}/geom.vert.spv
      ${SHADER_BINARY_DIR}/geom.tesc.spv
      ${SHADER_BINARY_DIR}/geom.tese.spv
      ${SHADER_BINARY_DIR}/geom.frag.spv
      ${SHADER_BINARY_DIR}/light.vert.spv
      ${SHADER_BINARY_DIR}/light.frag.spv)

  add_dependencies(${PROJECT_NAME} Shaders)
  add_custom_command(TARGET Shaders POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${SHADER_BINARY_DIR} ${PROJECT_BINARY_DIR}/shaders)
endif()

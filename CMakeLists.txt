cmake_minimum_required(VERSION 3.16)
project(vrml_viewer CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── FetchContent for dependencies ───────────────────────────────
include(FetchContent)

# ── GLFW ─────────────────────────────────────────────────────────
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://gitee.com/mirrors/glfw.git
    GIT_TAG        3.3.8
)
FetchContent_MakeAvailable(glfw)

# ── GLM (header-only math library) ───────────────────────────────
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://gitee.com/mirrors/glm.git
    GIT_TAG        0.9.9.8
)
FetchContent_MakeAvailable(glm)

# ── glad (bundled minimal loader) ────────────────────────────────
add_library(glad STATIC deps/glad/src/glad.c)
target_include_directories(glad PUBLIC deps/glad/include)

# ── vrml_parser static library ──────────────────────────────────
add_library(vrml_parser STATIC
    vrml_parser/src/vrml_parser.cpp
)
target_include_directories(vrml_parser PUBLIC
    vrml_parser/include
    vrml_parser/src
)
if(NOT MSVC)
    target_compile_options(vrml_parser PRIVATE
        -Wall -Wextra -Wno-unused-parameter
    )
endif()

# ── Viewer executable ────────────────────────────────────────────
add_executable(vrml_viewer viewer/src/main.cpp)

target_include_directories(vrml_viewer PRIVATE
    vrml_parser/include
)
target_link_libraries(vrml_viewer PRIVATE
    vrml_parser
    glad
    glfw
    glm::glm
)
if(NOT WIN32)
    target_link_libraries(vrml_viewer PRIVATE dl m)
endif()
if(NOT MSVC)
    target_compile_options(vrml_viewer PRIVATE
        -Wall -Wextra -Wno-unused-parameter
    )
endif()

# ── Copy sample files alongside the binary ───────────────────────
add_custom_command(TARGET vrml_viewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/samples"
        "$<TARGET_FILE_DIR:vrml_viewer>/samples"
    COMMENT "Copying sample VRML files..."
)

# ── Parser unit test (no OpenGL) ─────────────────────────────────
add_executable(vrml_parser_test
    vrml_parser/src/vrml_parser_test.cpp
)
target_include_directories(vrml_parser_test PRIVATE vrml_parser/include)
target_link_libraries(vrml_parser_test PRIVATE vrml_parser)
add_custom_command(TARGET vrml_parser_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/samples"
        "$<TARGET_FILE_DIR:vrml_parser_test>/samples"
    COMMENT "Copying sample VRML files for test..."
)

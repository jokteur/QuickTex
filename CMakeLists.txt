# Set C++ 17 compiler flags
if(MSVC)
    cmake_minimum_required(VERSION 3.21)
else()
    cmake_minimum_required(VERSION 3.17)
endif()

set(CMAKE_CXX_STANDARD 17)

# Set project name
project(quicktex)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(INSTALL_DIR "0.1")

# ---- MicroTex ----
add_subdirectory(external/microtex)
include_directories(external/microtex)

# ---- Cairo ----
find_package(Cairo)
include_directories(${CAIRO_INCLUDE_DIRS})

# ---- Tempo app ----
# ImGui config
add_subdirectory(external/TempoApp)
include_directories(external/TempoApp/include)
include_directories(external/TempoApp/external/imgui/imgui)

# -----
# Fonts
# -----
include_directories(external/IconFontCppHeaders)

# ---------------
# STB image write
# ---------------
include_directories(external/stb_image)

# ----
# Clip
# ----
set(CLIP_EXAMPLES OFF CACHE BOOL "Compile clip examples")
set(CLIP_TESTS OFF CACHE BOOL "Compile clip tests")
add_subdirectory(external/clip)

# Sources of program
include_directories(src)
file(GLOB source_list RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    src/system/*.cpp
    src/chars/drawable_char.cpp
    src/chars/*.cpp
    src/latex/*.cpp
    src/fonts/*.cpp
    src/core/*.cpp
    src/window/*.cpp
)

# Making a library without the main, for the tests and the program
add_library(${PROJECT_NAME}_lib ${source_list})

# Main program executable
add_executable(${PROJECT_NAME} src/main.cpp ${APP_ICON_RESOURCE_WINDOWS})

set(LIB_LINK microtex-imgui ${CAIRO_LIBRARIES} clip)
target_link_libraries(${PROJECT_NAME}_lib ${LIB_LINK})
target_include_directories(${PROJECT_NAME}_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/external/clip)
target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}_lib)

# Copy the data (such as fonts) in build directory
add_custom_command(TARGET quicktex PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/data/ $<TARGET_FILE_DIR:quicktex>/data)

if(MSVC)
    # CMake 3.21 is for this functionnality / Copy the dlls
    add_custom_command(TARGET quicktex POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:quicktex> $<TARGET_FILE_DIR:quicktex>
        COMMAND_EXPAND_LISTS
    )
endif()

# Set compiler options
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W0)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)
endif()
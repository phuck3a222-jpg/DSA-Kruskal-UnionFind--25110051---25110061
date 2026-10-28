cmake_minimum_required(VERSION 3.16)
project(DSA LANGUAGES CXX)
if(MSVC)
    add_compile_options(/utf-8)
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(dsa_core
    src/Edge.h
    src/Graph.h
    src/MSTResult.h
    src/UnionFind.h
    src/UnionFind.cpp
    src/Kruskal.h
    src/Kruskal.cpp
    src/GraphInput.h
    src/GraphInput.cpp
    src/GraphGenerator.h
    src/GraphGenerator.cpp
)
target_include_directories(dsa_core PUBLIC src)

add_executable(DSA src/main.cpp)
target_link_libraries(DSA PRIVATE dsa_core)

add_executable(DSA_tests tests/test.cpp)
target_link_libraries(DSA_tests PRIVATE dsa_core)

enable_testing()
add_test(NAME Week1Core COMMAND DSA_tests)

add_executable(DSA_week2_tests tests/week2_test.cpp)
target_link_libraries(DSA_week2_tests PRIVATE dsa_core)
add_test(NAME Week2Validation COMMAND DSA_week2_tests)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/week3_test.cpp")
    add_executable(DSA_week3_tests tests/week3_test.cpp)
    target_link_libraries(DSA_week3_tests PRIVATE dsa_core)
    add_test(NAME Week3Generator COMMAND DSA_week3_tests)
endif()
add_executable(DSA_generate src/generate_main.cpp)
target_link_libraries(DSA_generate PRIVATE dsa_core)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/benchmark.cpp")
    add_executable(DSA_benchmark src/benchmark.cpp)
    target_link_libraries(DSA_benchmark PRIVATE dsa_core)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/find_benchmark.cpp")
    add_executable(DSA_find_benchmark src/find_benchmark.cpp)
    target_link_libraries(DSA_find_benchmark PRIVATE dsa_core)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/brute_force_test.cpp")
    add_executable(DSA_brute_force_tests tests/brute_force_test.cpp)
    target_link_libraries(DSA_brute_force_tests PRIVATE dsa_core)
    add_test(NAME BruteForceMST COMMAND DSA_brute_force_tests "${CMAKE_CURRENT_SOURCE_DIR}/examples")
endif()

option(DSA_BUILD_GUI "Build the basic Windows GUI" ON)
if(WIN32 AND DSA_BUILD_GUI)
    add_library(graph_view src/gui/GraphView.cpp src/gui/VertexLabels.cpp)
    target_link_libraries(graph_view PUBLIC dsa_core user32 gdi32 gdiplus)
    add_executable(DSA_gui WIN32 src/gui/main.cpp)
    target_link_libraries(DSA_gui PRIVATE graph_view comdlg32)
    add_executable(DSA_graph_view_tests tests/graph_view_test.cpp)
    target_link_libraries(DSA_graph_view_tests PRIVATE graph_view)
    add_test(NAME Week3DpiRendering COMMAND DSA_graph_view_tests)
    add_executable(DSA_gui_tests tests/gui_test.cpp)
    target_link_libraries(DSA_gui_tests PRIVATE graph_view comdlg32)
    add_dependencies(DSA_gui_tests DSA_gui)
    add_test(NAME GuiIntegration COMMAND DSA_gui_tests)
    add_custom_command(TARGET DSA_gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/examples" "$<TARGET_FILE_DIR:DSA_gui>/examples")
endif()

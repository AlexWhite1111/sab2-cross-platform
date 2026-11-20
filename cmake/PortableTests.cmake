find_package(Threads REQUIRED)
find_package(Python3 COMPONENTS Interpreter QUIET)

if(Python3_Interpreter_FOUND)
    add_test(NAME workflow_mcp_protocol_regressions
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/workflow_mcp_tests.py")
endif()

add_executable(overlay_window_geometry_tests tests/overlay_window_geometry_tests.cpp)
target_include_directories(overlay_window_geometry_tests PRIVATE "${SRC_DIR}")
add_test(NAME overlay_window_geometry_regressions COMMAND overlay_window_geometry_tests)

add_executable(node_graph_tests tests/node_graph_tests.cpp "${SRC_DIR}/nodes/node_graph.cpp")
target_include_directories(node_graph_tests PRIVATE "${SRC_DIR}")
target_link_libraries(node_graph_tests PRIVATE Threads::Threads)
add_test(NAME node_graph_regressions COMMAND node_graph_tests)

add_executable(node_config_bridge_tests
    tests/node_config_bridge_tests.cpp
    "${SRC_DIR}/nodes/node_graph.cpp"
    "${SRC_DIR}/nodes/node_config_bridge.cpp"
    "${SRC_DIR}/keyboard/keycodes.cpp")
target_include_directories(node_config_bridge_tests PRIVATE "${SRC_DIR}" "${SRC_DIR}/config")
target_link_libraries(node_config_bridge_tests PRIVATE Threads::Threads)
add_test(NAME node_config_migration COMMAND node_config_bridge_tests)

if(WIN32)
    target_link_libraries(node_graph_tests PRIVATE dxgi user32)
    target_link_libraries(node_config_bridge_tests PRIVATE dxgi user32)
endif()

if(MSVC)
    foreach(target overlay_window_geometry_tests node_graph_tests node_config_bridge_tests)
        set_target_properties(${target} PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        target_compile_options(${target} PRIVATE /W3 /permissive- /EHsc)
    endforeach()
endif()

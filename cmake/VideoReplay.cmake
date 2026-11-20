find_package(OpenCV 4 REQUIRED COMPONENTS core imgproc dnn videoio highgui)
find_package(Threads REQUIRED)

set(SAB2_ONNXRUNTIME_ROOT "" CACHE PATH "ONNX Runtime SDK or Windows NuGet package root")
find_path(SAB2_ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h
    HINTS "${SAB2_ONNXRUNTIME_ROOT}"
    PATH_SUFFIXES include include/onnxruntime build/native/include
    REQUIRED)
find_library(SAB2_ONNXRUNTIME_LIBRARY NAMES onnxruntime
    HINTS "${SAB2_ONNXRUNTIME_ROOT}"
    PATH_SUFFIXES lib runtimes/win-x64/native
    REQUIRED)

add_executable(sab2_video_replay
    "${SRC_DIR}/portable/video_replay.cpp"
    "${SRC_DIR}/detector/postProcess.cpp"
    "${SRC_DIR}/mouse/AimbotTarget.cpp"
    "${SRC_DIR}/nodes/node_graph.cpp")
target_include_directories(sab2_video_replay PRIVATE "${SRC_DIR}")
target_include_directories(sab2_video_replay SYSTEM PRIVATE
    "${SAB2_ONNXRUNTIME_INCLUDE_DIR}" ${OpenCV_INCLUDE_DIRS})
target_link_libraries(sab2_video_replay PRIVATE
    "${SAB2_ONNXRUNTIME_LIBRARY}" ${OpenCV_LIBS} Threads::Threads)

if(APPLE)
    enable_language(OBJCXX)
    set_target_properties(sab2_video_replay PROPERTIES OBJCXX_STANDARD 17 OBJCXX_STANDARD_REQUIRED ON)
    target_sources(sab2_video_replay PRIVATE "${SRC_DIR}/portable/screen_capture_kit.mm")
    target_compile_options(sab2_video_replay PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
    foreach(framework Foundation CoreFoundation CoreGraphics CoreMedia CoreVideo ScreenCaptureKit)
        find_library(SAB2_${framework}_FRAMEWORK "${framework}" REQUIRED)
        target_link_libraries(sab2_video_replay PRIVATE "${SAB2_${framework}_FRAMEWORK}")
    endforeach()
endif()

if(WIN32)
    target_link_libraries(sab2_video_replay PRIVATE dxgi user32)
    find_file(SAB2_ONNXRUNTIME_DLL onnxruntime.dll
        HINTS "${SAB2_ONNXRUNTIME_ROOT}"
        PATH_SUFFIXES lib bin runtimes/win-x64/native
        REQUIRED)
    add_custom_command(TARGET sab2_video_replay POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${SAB2_ONNXRUNTIME_DLL}" $<TARGET_RUNTIME_DLLS:sab2_video_replay>
            "$<TARGET_FILE_DIR:sab2_video_replay>"
        COMMAND_EXPAND_LISTS)
endif()

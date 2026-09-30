cmake_minimum_required(VERSION 3.16)
project(HailMary)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0>=1.20 gstreamer-app-1.0 gstreamer-video-1.0)

set(DEEPSTREAM_ROOT "/opt/nvidia/deepstream/deepstream" CACHE PATH "DeepStream SDK root")
if(NOT EXISTS "${DEEPSTREAM_ROOT}/sources/includes/gstnvdsmeta.h")
    message(FATAL_ERROR "DeepStream SDK not found: ${DEEPSTREAM_ROOT}")
endif()

set(CUDA_ROOT "/usr/local/cuda" CACHE PATH "CUDA toolkit root")

include_directories(
        .
        Gstreamer
        ${GST_INCLUDE_DIRS}
        ${DEEPSTREAM_ROOT}/sources/includes
        ${CUDA_ROOT}/include
)

link_directories(
        ${GST_LIBRARY_DIRS}
        ${DEEPSTREAM_ROOT}/lib
)

add_executable(
        HailMary
        main.cpp
        main.h
        Gstreamer/main_pipeline.cpp
        Gstreamer/main_pipeline.h
        Gstreamer/Elements/Camera/Gst_elements.cpp
        Gstreamer/Elements/Camera/Gst_elements.h
        Gstreamer/Elements/Caps/Gst_caps.cpp
        Gstreamer/Elements/Caps/Gst_caps.h
        Gstreamer/Elements/Sink/Gst_sink.cpp
        Gstreamer/Elements/Sink/Gst_sink.h
        Gstreamer/Elements/AI/AI_element.cpp
        Gstreamer/Elements/AI/AI_element.h
        Gstreamer/Elements/AI/AI_types.h
        Gstreamer/Elements/Judge/Gst_judge.cpp
        Gstreamer/Elements/Judge/Gst_judge.h
        Gstreamer/Elements/Judge/Judge_meta.cpp
        Gstreamer/Elements/Judge/Judge_types.h
)

target_link_libraries(HailMary ${GST_LIBRARIES} nvdsgst_meta nvds_meta)
target_compile_definitions(HailMary PRIVATE HAILMARY_MODEL_DIR="${CMAKE_SOURCE_DIR}/models")
set_target_properties(HailMary PROPERTIES BUILD_RPATH "${DEEPSTREAM_ROOT}/lib")

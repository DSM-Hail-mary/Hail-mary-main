cmake_minimum_required(VERSION 4.3)
project(HailMary)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
        .
        Gstreamer
)

if(WIN32)
    if(DEFINED ENV{GSTREAMER_1_0_ROOT_X86_64})
        set(GST_ROOT "$ENV{GSTREAMER_1_0_ROOT_X86_64}")
    else()
        set(GST_ROOT "D:/mingw_x86_64")
    endif()

    include_directories(
            ${GST_ROOT}/include/gstreamer-1.0
            ${GST_ROOT}/include/glib-2.0
            ${GST_ROOT}/lib/glib-2.0/include
    )
    link_directories(${GST_ROOT}/lib)
    set(GST_LIBS gstreamer-1.0 gobject-2.0 glib-2.0 gstapp-1.0 gstvideo-1.0)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0)
    include_directories(${GST_INCLUDE_DIRS})
    link_directories(${GST_LIBRARY_DIRS})
    set(GST_LIBS ${GST_LIBRARIES})

    set(DEEPSTREAM_ROOT "/opt/nvidia/deepstream/deepstream" CACHE PATH "DeepStream SDK root")
    if(EXISTS "${DEEPSTREAM_ROOT}/sources/includes/gstnvdsmeta.h")
        set(HAILMARY_DEEPSTREAM ON)
        include_directories(${DEEPSTREAM_ROOT}/sources/includes)
        if(EXISTS "/usr/local/cuda/include")
            include_directories(/usr/local/cuda/include)
        endif()
        link_directories(${DEEPSTREAM_ROOT}/lib)
        list(APPEND GST_LIBS nvdsgst_meta nvds_meta)
        set(CMAKE_BUILD_RPATH "${DEEPSTREAM_ROOT}/lib")
        message(STATUS "DeepStream found: ${DEEPSTREAM_ROOT}")
    else()
        message(STATUS "DeepStream not found: AI element runs as pass-through")
    endif()
endif()

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

target_link_libraries(HailMary ${GST_LIBS})
target_compile_definitions(HailMary PRIVATE HAILMARY_MODEL_DIR="${CMAKE_SOURCE_DIR}/models")
if(HAILMARY_DEEPSTREAM)
    target_compile_definitions(HailMary PRIVATE HAILMARY_DEEPSTREAM)
endif()

if(WIN32)
    set(GST_COPY_DLLS_SCRIPT "${CMAKE_BINARY_DIR}/copy_gst_dlls.cmake")
    file(WRITE "${GST_COPY_DLLS_SCRIPT}" [=[
file(GLOB dlls "${GST_BIN}/*.dll")
file(COPY ${dlls} DESTINATION "${DEST}")
]=])
    file(TO_NATIVE_PATH "${GST_ROOT}/lib/gstreamer-1.0" GST_PLUGIN_DIR_NATIVE)
    add_custom_command(TARGET HailMary POST_BUILD
            COMMAND ${CMAKE_COMMAND} -DGST_BIN=${GST_ROOT}/bin -DDEST=$<TARGET_FILE_DIR:HailMary> -P ${GST_COPY_DLLS_SCRIPT}
            COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:HailMary>/lib
            COMMAND if not exist "$<SHELL_PATH:$<TARGET_FILE_DIR:HailMary>/lib/gstreamer-1.0>"
                    mklink /J "$<SHELL_PATH:$<TARGET_FILE_DIR:HailMary>/lib/gstreamer-1.0>" "${GST_PLUGIN_DIR_NATIVE}"
            COMMENT "Copying GStreamer runtime DLLs / linking plugin dir"
            VERBATIM)
endif()

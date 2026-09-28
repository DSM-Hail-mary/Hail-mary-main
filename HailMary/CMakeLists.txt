cmake_minimum_required(VERSION 4.3)
project(HailMary)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(DEFINED ENV{GSTREAMER_1_0_ROOT_X86_64})
    set(GST_ROOT "$ENV{GSTREAMER_1_0_ROOT_X86_64}")
else()
    set(GST_ROOT "D:/mingw_x86_64")
endif()

include_directories(
        .
        ${GST_ROOT}/include/gstreamer-1.0
        ${GST_ROOT}/include/glib-2.0
        ${GST_ROOT}/lib/glib-2.0/include
)

link_directories(${GST_ROOT}/lib)

add_executable(
        HailMary
        main.cpp
        main.h
        Gstreamer/main_pipeline.cpp
        Gstreamer/main_pipeline.h
)

target_link_libraries(HailMary gstreamer-1.0 gobject-2.0 glib-2.0)

# Windows: GStreamer가 PATH에 없으면 실행 시 0xC0000135(DLL not found)로 종료되고,
# 플러그인은 libgstreamer DLL 위치 기준 <exe dir>/lib/gstreamer-1.0 에서 찾는다.
# -> 빌드 후 GStreamer DLL(플러그인 의존성 포함)을 exe 옆에 복사하고, 플러그인 폴더는 junction으로 연결.
if(WIN32)
    # DLL이 100개 넘어 커맨드라인 길이 제한에 걸리므로 스크립트로 복사 (file(COPY)는 변경된 것만 복사)
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

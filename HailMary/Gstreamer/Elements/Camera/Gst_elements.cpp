#include "Gst_elements.h"
#include <iostream>
using namespace std;

CameraElement::CameraElement() {
    src = nullptr;
    parse = nullptr;
    addedToBin = false;
}

CameraElement::~CameraElement() {
    if (!addedToBin) {
        if (src != nullptr) gst_object_unref(src);
        if (parse != nullptr) gst_object_unref(parse);
    }
    src = nullptr;
    parse = nullptr;
}

bool CameraElement::cameraInit() {
    if (src != nullptr) {
        cerr << "Camera element already created" << endl;
        return false;
    }

    src = gst_element_factory_make("tcpserversrc", "camera");
    parse = gst_element_factory_make("rawvideoparse", "camera_parse");
    if (src == nullptr || parse == nullptr) {
        cerr << "Failed to create camera elements" << endl;
        return false;
    }

    g_object_set(src,
                 "host", OPENCV_HOST,
                 "port", OPENCV_PORT,
                 nullptr);

    g_object_set(parse,
                 "width", OPENCV_WIDTH,
                 "height", OPENCV_HEIGHT,
                 "format", GST_VIDEO_FORMAT_BGR,
                 "framerate", OPENCV_FPS, 1,
                 nullptr);

    return true;
}

bool CameraElement::cameraConnect() {
    if (pipeline == nullptr || src == nullptr || parse == nullptr) {
        cerr << "Cannot connect to camera elements" << endl;
        return false;
    }

    gst_bin_add_many(GST_BIN(pipeline), src, parse, nullptr);
    addedToBin = true;

    if (!gst_element_link(src, parse)) {
        cerr << "Failed to link camera elements" << endl;
        return false;
    }

    cout << "Waiting for OpenCV frames on " << OPENCV_HOST << ":" << OPENCV_PORT << endl;
    return true;
}

GstElement *CameraElement::getOutput() {
    return parse;
}

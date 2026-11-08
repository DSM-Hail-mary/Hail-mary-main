#include "Gst_elements.h"
#include <cstdlib>
#include <iostream>
using namespace std;

CameraElement::CameraElement() {
    src = nullptr;
    jpegCaps = nullptr;
    decode = nullptr;
    convert = nullptr;
    output = nullptr;
    hwDecode = false;
    addedToBin = false;
}

CameraElement::~CameraElement() {
    if (!addedToBin) {
        if (src != nullptr) gst_object_unref(src);
        if (jpegCaps != nullptr) gst_object_unref(jpegCaps);
        if (decode != nullptr) gst_object_unref(decode);
        if (convert != nullptr) gst_object_unref(convert);
    }
    src = nullptr;
    jpegCaps = nullptr;
    decode = nullptr;
    convert = nullptr;
    output = nullptr;
}

bool CameraElement::cameraInit() {
    if (src != nullptr) {
        cerr << "Camera element already created" << endl;
        return false;
    }

    src = gst_element_factory_make("v4l2src", "camera");
    jpegCaps = gst_element_factory_make("capsfilter", "camera_jpeg_caps");
    decode = gst_element_factory_make("nvv4l2decoder", "camera_decode");
    if (decode != nullptr) {
        hwDecode = true;
        convert = gst_element_factory_make("nvvidconv", "camera_convert");
    } else {
        decode = gst_element_factory_make("jpegdec", "camera_decode");
    }
    if (src == nullptr || jpegCaps == nullptr || decode == nullptr || (hwDecode && convert == nullptr)) {
        cerr << "Failed to create camera elements" << endl;
        return false;
    }

    const char *device = getenv("HAILMARY_CAMERA");
    g_object_set(src,
                 "device", device != nullptr ? device : CAMERA_DEVICE,
                 "io-mode", 2,
                 nullptr);

    GstCaps *caps = gst_caps_new_simple("image/jpeg",
                                        "width", G_TYPE_INT, CAMERA_WIDTH,
                                        "height", G_TYPE_INT, CAMERA_HEIGHT,
                                        "framerate", GST_TYPE_FRACTION, CAMERA_FPS, 1,
                                        nullptr);
    g_object_set(jpegCaps, "caps", caps, nullptr);
    gst_caps_unref(caps);

    if (hwDecode) {
        g_object_set(decode, "mjpeg", TRUE, nullptr);
    }
    output = hwDecode ? convert : decode;

    return true;
}

bool CameraElement::cameraConnect(GstElement *bin) {
    if (bin == nullptr || src == nullptr || output == nullptr) {
        cerr << "Cannot connect to camera elements" << endl;
        return false;
    }

    gst_bin_add_many(GST_BIN(bin), src, jpegCaps, decode, nullptr);
    if (hwDecode) {
        gst_bin_add(GST_BIN(bin), convert);
    }
    addedToBin = true;

    bool linked = hwDecode
                      ? gst_element_link_many(src, jpegCaps, decode, convert, nullptr)
                      : gst_element_link_many(src, jpegCaps, decode, nullptr);
    if (!linked) {
        cerr << "Failed to link camera elements" << endl;
        return false;
    }

    const char *device = getenv("HAILMARY_CAMERA");
    cout << "Camera " << (device != nullptr ? device : CAMERA_DEVICE) << " " << CAMERA_WIDTH << "x"
         << CAMERA_HEIGHT << "@" << CAMERA_FPS << " MJPG (" << (hwDecode ? "NVDEC" : "jpegdec") << ")" << endl;
    return true;
}

GstElement *CameraElement::getOutput() {
    return output;
}

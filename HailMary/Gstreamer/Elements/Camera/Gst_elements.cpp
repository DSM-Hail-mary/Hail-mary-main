#include "Gst_elements.h"
using namespace std;

CameraElement::CameraElement() {
    src = nullptr;
}
CameraElement::~CameraElement() {
    if (src != nullptr) {
        gst_object_unref(src);
        src = nullptr;
    }
}

bool CameraElement::cameraInit() {
    GstElement *src = gst_element_factory_make("v412src", "camera");
    if (src == nullptr) {
        cout << "Failed to create v412src element" << endl;
        return false;
    }

    return true;
}

bool CameraElement::cameraConnect() {
    gst_bin_add(GST_BIN(src), src);
    gst_element_link(pipeline, src);

    gst_element_set_state(src, GST_STATE_PLAYING);

    return true;
}

GstElement *CameraElement::getElement() {
    return src;
}
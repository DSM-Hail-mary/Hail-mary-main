#include "Gst_elements.h"
#include <iostream>
using namespace std;

CameraElement::CameraElement() {
    src = nullptr;
    addedToBin = false;
}

CameraElement::~CameraElement() {
    if (src != nullptr && !addedToBin) {
        gst_object_unref(src);
    }
    src = nullptr;
}

bool CameraElement::cameraInit() {
    if (src != nullptr) {
        cerr << "Camera element already created" << endl;
        return false;
    }

    src = gst_element_factory_make(CAMERA_SRC, "camera");
    if (src == nullptr) {
        cerr << "Failed to create " << CAMERA_SRC << " element" << endl;
        return false;
    }

    return true;
}

bool CameraElement::cameraConnect() {
    if (pipeline == nullptr || src == nullptr) {
        cerr << "Cannot connect to " << CAMERA_SRC << " element" << endl;
        return false;
    }

    if (!gst_bin_add(GST_BIN(pipeline), src)) {
        cerr << "Failed to add " << CAMERA_SRC << " to pipeline" << endl;
        return false;
    }
    addedToBin = true;

    if (!gst_element_sync_state_with_parent(src)) {
        cerr << "Failed to sync the state with pipeline" << endl;
        return false;
    }

    return true;
}

GstElement *CameraElement::getElement() {
    return src;
}

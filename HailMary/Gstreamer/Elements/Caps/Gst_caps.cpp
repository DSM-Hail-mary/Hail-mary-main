#include "Gst_caps.h"
#include <iostream>
using namespace std;

CapsElement::CapsElement() {
    convert = nullptr;
    filter = nullptr;
    addedToBin = false;
}

CapsElement::~CapsElement() {
    if (!addedToBin) {
        if (convert != nullptr) gst_object_unref(convert);
        if (filter != nullptr) gst_object_unref(filter);
    }
    convert = nullptr;
    filter = nullptr;
}

bool CapsElement::capsInit() {
    if (filter != nullptr) {
        cerr << "Caps elements already created" << endl;
        return false;
    }

    convert = gst_element_factory_make("videoconvert", "caps_convert");
    filter = gst_element_factory_make("capsfilter", "caps_filter");
    if (!convert || !filter) {
        cerr << "Failed to create caps elements" << endl;
        return false;
    }

    GstCaps *caps = gst_caps_new_simple(
        "video/x-raw",
        "format", G_TYPE_STRING, FRAME_FORMAT,
        nullptr);
    g_object_set(filter, "caps", caps, nullptr);
    gst_caps_unref(caps);

    return true;
}

bool CapsElement::capsConnect(GstElement *input) {
    if (pipeline == nullptr || input == nullptr || filter == nullptr) {
        cerr << "Cannot connect to caps elements" << endl;
        return false;
    }

    gst_bin_add_many(GST_BIN(pipeline), convert, filter, nullptr);
    addedToBin = true;

    if (!gst_element_link_many(input, convert, filter, nullptr)) {
        cerr << "Failed to link caps elements" << endl;
        return false;
    }

    return true;
}

GstElement *CapsElement::getOutput() {
    return filter;
}

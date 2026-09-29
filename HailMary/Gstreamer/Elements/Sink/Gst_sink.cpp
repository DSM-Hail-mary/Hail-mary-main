#include "Gst_sink.h"
#include <iostream>
using namespace std;

SinkElement::SinkElement() {
    sink = nullptr;
    addedToBin = false;
}

SinkElement::~SinkElement() {
    if (sink != nullptr && !addedToBin) {
        gst_object_unref(sink);
    }
    sink = nullptr;
}

bool SinkElement::sinkInit() {
    if (sink != nullptr) {
        cerr << "Sink element already created" << endl;
        return false;
    }

    sink = gst_element_factory_make("appsink", "frame_sink");
    if (sink == nullptr) {
        cerr << "Failed to create appsink element" << endl;
        return false;
    }

    g_object_set(sink,
                 "max-buffers", 1,
                 "drop", TRUE,
                 "sync", FALSE,
                 nullptr);

    return true;
}

bool SinkElement::sinkConnect(GstElement *input) {
    if (pipeline == nullptr || input == nullptr || sink == nullptr) {
        cerr << "Cannot connect to appsink element" << endl;
        return false;
    }

    if (!gst_bin_add(GST_BIN(pipeline), sink)) {
        cerr << "Failed to add appsink to pipeline" << endl;
        return false;
    }
    addedToBin = true;

    if (!gst_element_link(input, sink)) {
        cerr << "Failed to link appsink" << endl;
        return false;
    }

    return true;
}

GstSample *SinkElement::pullFrame(GstClockTime timeout) {
    if (sink == nullptr) {
        return nullptr;
    }
    return gst_app_sink_try_pull_sample(GST_APP_SINK(sink), timeout);
}

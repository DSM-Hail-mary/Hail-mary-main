#include "Gst_sink.h"
#include <iostream>
using namespace std;

SinkElement::SinkElement() {
    sink = nullptr;
    callback = nullptr;
    userData = nullptr;
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

void SinkElement::sinkSetCallback(SinkCallback callback, void *userData) {
    this->callback = callback;
    this->userData = userData;

    GstAppSinkCallbacks callbacks = {};
    callbacks.new_sample = sinkNewSample;
    gst_app_sink_set_callbacks(GST_APP_SINK(sink), &callbacks, this, nullptr);
}

GstFlowReturn SinkElement::sinkNewSample(GstAppSink *appsink, gpointer data) {
    SinkElement *self = static_cast<SinkElement *>(data);

    GstSample *sample = gst_app_sink_pull_sample(appsink);
    if (sample == nullptr) {
        return GST_FLOW_ERROR;
    }

    if (self->callback != nullptr) {
        GstBuffer *buffer = gst_sample_get_buffer(sample);
        const JudgeResult *result = buffer != nullptr ? judgeMetaGet(buffer) : nullptr;
        self->callback(sample, result, self->userData);
    }

    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

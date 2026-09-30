#include "main_pipeline.h"

using namespace std;
GstElement *pipeline;

int runPipeline() {
    cout << "Create new Pipeline" << endl;
    gst_init(nullptr, nullptr);

    pipeline = gst_pipeline_new("pipeline");

    CameraElement camera;
    CapsElement caps;
    AIElement ai;
    JudgeElement judge;
    SinkElement sink;

    if (!camera.cameraInit() || !camera.cameraConnect()) {
        cout << "Cannot connect to camera" << endl;
        return -1;
    }
    if (!caps.capsInit() || !caps.capsConnect(camera.getOutput())) {
        cout << "Cannot connect to caps" << endl;
        return -1;
    }
    if (!ai.aiInit() || !ai.aiConnect(caps.getOutput())) {
        cout << "Cannot connect to AI" << endl;
        return -1;
    }
    if (!judge.judgeInit() || !judge.judgeConnect(ai.getOutput())) {
        cout << "Cannot connect to judge" << endl;
        return -1;
    }
    if (!sink.sinkInit() || !sink.sinkConnect(judge.getOutput())) {
        cout << "Cannot connect to sink" << endl;
        return -1;
    }

    numberOfPipelines();
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    for (int i = 0; i < 30; i++) {
        GstSample *sample = sink.pullFrame(GST_SECOND);
        if (sample == nullptr) {
            cerr << "No frame" << endl;
            break;
        }

        GstBuffer *buffer = gst_sample_get_buffer(sample);
        GstMapInfo map;
        if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
            cout << "Frame " << i << " size: " << map.size << endl;
            gst_buffer_unmap(buffer, &map);
        }
        gst_sample_unref(sample);
    }

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    pipeline = nullptr;
    return 0;
}

void messageBusTest() {
    GstBus *bus = gst_element_get_bus(pipeline);
    GstMessage *msg = gst_bus_timed_pop_filtered(
        bus, 3 * GST_SECOND,
        static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    if (msg) {
        gst_message_unref(msg);
    }

    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    pipeline = nullptr;
}

void numberOfPipelines() {
    cout << "Check the element" << endl;
    GValue item = G_VALUE_INIT;
    int num = 0;

    GstIterator *iter = gst_bin_iterate_elements(GST_BIN(pipeline));
    while (gst_iterator_next(iter, &item) == GST_ITERATOR_OK) {
        GstElement *element = GST_ELEMENT(g_value_get_object(&item));

        g_print("Element: %s\n", GST_ELEMENT_NAME(element));
        num++;

        g_value_reset(&item);
    }

    cout << "Element number: " << num << endl;
    gst_iterator_free(iter);
}
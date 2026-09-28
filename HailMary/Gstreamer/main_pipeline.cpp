#include "main_pipeline.h"

using namespace std;
GstElement *pipeline;

int runPipeline() {
    cout << "Create new Pipeline" << endl;
    gst_init(nullptr, nullptr);

    pipeline = gst_pipeline_new("pipeline");
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    messageBusTest();
    numberOfPipelines();
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
}

void numberOfPipelines() {
    cout << "Check the element" << endl;
    GValue item = G_VALUE_INIT;
    static uint8_t num = 0;

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
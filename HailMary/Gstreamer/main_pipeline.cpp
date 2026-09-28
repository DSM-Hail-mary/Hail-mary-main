#include "main_pipeline.h"
#include <gst/gst.h>

int runPipeline() {
    gst_init(nullptr, nullptr);

    GstElement *pipeline = gst_pipeline_new("pipeline");
    GstElement *element = gst_element_factory_make("fakesrc", "src");

    gst_bin_add(GST_BIN(pipeline), element);

    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    GstBus *bus = gst_element_get_bus(pipeline);
    GstMessage *msg = gst_bus_timed_pop_filtered(
        bus, 3 * GST_SECOND,
        static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    if (msg) gst_message_unref(msg);

    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    return 0;
}

#include "main_pipeline.h"

using namespace std;
GstElement *pipeline;

static void onFrame(GstSample *sample, const JudgeResult *result, void *userData) {
    static int frames = 0;
    frames++;
    if (frames % 30 == 0) {
        cout << "Sink frame " << frames << " judge="
             << (result == nullptr ? "none" : (result->problem ? "problem" : "normal")) << endl;
    }
}

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

    sink.sinkSetCallback(onFrame, nullptr);

    numberOfPipelines();
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    GstBus *bus = gst_element_get_bus(pipeline);
    bool running = true;
    while (running) {
        GstMessage *msg = gst_bus_timed_pop_filtered(
            bus, GST_CLOCK_TIME_NONE,
            static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS | GST_MESSAGE_ELEMENT));

        switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_ERROR: {
                GError *error = nullptr;
                gst_message_parse_error(msg, &error, nullptr);
                cerr << "Pipeline error: " << error->message << endl;
                g_error_free(error);
                running = false;
                break;
            }
            case GST_MESSAGE_EOS:
                cout << "End of stream" << endl;
                running = false;
                break;
            case GST_MESSAGE_ELEMENT: {
                const GstStructure *structure = gst_message_get_structure(msg);
                gboolean problem = FALSE;
                if (gst_structure_has_name(structure, "judge") &&
                    gst_structure_get_boolean(structure, "problem", &problem)) {
                    cout << "Bus: judge problem=" << (problem ? "true" : "false") << endl;
                }
                break;
            }
            default:
                break;
        }
        gst_message_unref(msg);
    }
    gst_object_unref(bus);

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
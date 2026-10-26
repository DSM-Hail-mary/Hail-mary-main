#include "main_pipeline.h"
#include <chrono>
#include <csignal>
#include <thread>

using namespace std;

static volatile sig_atomic_t stopRequested = 0;

static void onSignal(int sig) {
    stopRequested = 1;
    // A second Ctrl+C kills the process if shutdown hangs
    signal(sig, SIG_DFL);
}

// Sleep that wakes up early when a stop signal arrives
static bool waitOrStop(int seconds) {
    for (int i = 0; i < seconds * 10; i++) {
        if (stopRequested) {
            return false;
        }
        this_thread::sleep_for(chrono::milliseconds(100));
    }
    return !stopRequested;
}

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

    signal(SIGINT, onSignal);
    signal(SIGTERM, onSignal);

    GstElement *pipeline = gst_pipeline_new("pipeline");

    CameraElement camera;
    CapsElement caps;
    AIElement ai;
    JudgeElement judge;
    SinkElement sink;

    const char *failed = nullptr;
    if (!camera.cameraInit() || !camera.cameraConnect(pipeline)) {
        failed = "camera";
    } else if (!caps.capsInit() || !caps.capsConnect(pipeline, camera.getOutput())) {
        failed = "caps";
    } else if (!ai.aiInit() || !ai.aiConnect(pipeline, caps.getOutput())) {
        failed = "AI";
    } else if (!judge.judgeInit() || !judge.judgeConnect(pipeline, ai.getOutput())) {
        failed = "judge";
    } else if (!sink.sinkInit() || !sink.sinkConnect(pipeline, judge.getOutput())) {
        failed = "sink";
    }
    if (failed != nullptr) {
        cout << "Cannot connect to " << failed << endl;
        gst_object_unref(pipeline);
        return -1;
    }

    sink.sinkSetCallback(onFrame, nullptr);

    numberOfPipelines(pipeline);
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    GstBus *bus = gst_element_get_bus(pipeline);
    bool running = true;
    int reconnectAttempts = 0;
    int exitCode = 0;
    while (running) {
        if (stopRequested) {
            cout << "Stop requested, shutting down" << endl;
            break;
        }

        GstMessage *msg = gst_bus_timed_pop_filtered(
            bus, 100 * GST_MSECOND,
            static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS |
                                        GST_MESSAGE_ELEMENT | GST_MESSAGE_ASYNC_DONE));
        if (msg == nullptr) {
            continue;
        }

        switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_ERROR: {
                GError *error = nullptr;
                gst_message_parse_error(msg, &error, nullptr);
                cerr << "Pipeline error: " << error->message << endl;
                g_error_free(error);
                running = false;
                exitCode = -1;
                break;
            }
            case GST_MESSAGE_EOS:
                if (reconnectAttempts >= RECONNECT_MAX_ATTEMPTS) {
                    cerr << "End of stream: reconnect failed " << RECONNECT_MAX_ATTEMPTS << " times" << endl;
                    running = false;
                    exitCode = -1;
                    break;
                }
                reconnectAttempts++;
                cout << "Stream disconnected, reconnecting (" << reconnectAttempts << "/"
                     << RECONNECT_MAX_ATTEMPTS << ")" << endl;

                // tcpserversrc listens again once the pipeline is restarted
                gst_element_set_state(pipeline, GST_STATE_NULL);
                if (!waitOrStop(RECONNECT_INTERVAL_SEC)) {
                    running = false;
                    break;
                }
                if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
                    cerr << "Failed to restart pipeline" << endl;
                }
                break;
            case GST_MESSAGE_ASYNC_DONE:
                // Preroll finished: a client is connected and frames are flowing
                if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline) && reconnectAttempts > 0) {
                    cout << "Stream reconnected" << endl;
                    reconnectAttempts = 0;
                }
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
    cout << "Pipeline stopped" << endl;
    return exitCode;
}

void messageBusTest(GstElement *pipeline) {
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

void numberOfPipelines(GstElement *pipeline) {
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

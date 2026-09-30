#include "AI_element.h"
#include <fstream>
#include <string>
#include <algorithm>
using namespace std;

static GstPadProbeReturn aiProbe(GstPad *pad, GstPadProbeInfo *info, gpointer data) {
    AIElement *self = static_cast<AIElement *>(data);
    self->aiProcess(pad, GST_PAD_PROBE_INFO_BUFFER(info));
    return GST_PAD_PROBE_OK;
}

AIElement::AIElement() {
    queue = nullptr;
    identity = nullptr;
    addedToBin = false;
    skipCount = 0;
    lastSnapshot = 0;
    snapshotIndex = 0;
}

AIElement::~AIElement() {
    if (!addedToBin) {
        if (queue != nullptr) gst_object_unref(queue);
        if (identity != nullptr) gst_object_unref(identity);
    }
    queue = nullptr;
    identity = nullptr;
}

bool AIElement::aiInit() {
    if (identity != nullptr) {
        cerr << "AI elements already created" << endl;
        return false;
    }

    queue = gst_element_factory_make("queue", "ai_queue");
    identity = gst_element_factory_make("identity", "ai_identity");
    if (queue == nullptr || identity == nullptr) {
        cerr << "Failed to create AI elements" << endl;
        return false;
    }

    g_object_set(queue,
                 "leaky", 2,
                 "max-size-buffers", 1,
                 "max-size-bytes", 0,
                 "max-size-time", (guint64) 0,
                 nullptr);

    return true;
}

bool AIElement::aiConnect(GstElement *input) {
    if (pipeline == nullptr || input == nullptr || identity == nullptr) {
        cerr << "Cannot connect to AI elements" << endl;
        return false;
    }

    gst_bin_add_many(GST_BIN(pipeline), queue, identity, nullptr);
    addedToBin = true;

    if (!gst_element_link_many(input, queue, identity, nullptr)) {
        cerr << "Failed to link AI elements" << endl;
        return false;
    }

    GstPad *pad = gst_element_get_static_pad(identity, "src");
    gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, aiProbe, this, nullptr);
    gst_object_unref(pad);

    cout << "Probe to each Elements" << endl;

    return true;
}

void AIElement::aiProcess(GstPad *pad, GstBuffer *buffer) {
    if (skipCount > 0) {
        skipCount--;
        return;
    }

    GstCaps *caps = gst_pad_get_current_caps(pad);
    if (caps == nullptr) {
        return;
    }

    GstVideoInfo info;
    if (!gst_video_info_from_caps(&info, caps)) {
        gst_caps_unref(caps);
        return;
    }

    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        gst_caps_unref(caps);
        return;
    }

    gint64 start = g_get_monotonic_time();
    vector<BBox> boxes = aiInference(info, map.data);
    gst_buffer_unmap(buffer, &map);

    gint64 now = g_get_monotonic_time();
    if (!boxes.empty() && now - lastSnapshot >= SNAPSHOT_INTERVAL_US) {
        aiSnapshot(buffer, caps, info, boxes);
        lastSnapshot = now;
    }

    aiTimer(g_get_monotonic_time() - start);
    gst_caps_unref(caps);
}

void AIElement::aiTimer(gint64 elapsed) {
    if (elapsed > AI_BUDGET_US) {
        skipCount = static_cast<int>(elapsed / AI_BUDGET_US);
    }
}

vector<BBox> AIElement::aiInference(const GstVideoInfo &info, const uint8_t *data) {
    return vector<BBox>();
}

void AIElement::aiSnapshot(GstBuffer *buffer, GstCaps *caps, const GstVideoInfo &info, const vector<BBox> &boxes) {
    GstBuffer *copy = gst_buffer_copy_deep(buffer);

    GstMapInfo map;
    if (!gst_buffer_map(copy, &map, GST_MAP_WRITE)) {
        gst_buffer_unref(copy);
        return;
    }

    int width = GST_VIDEO_INFO_WIDTH(&info);
    int height = GST_VIDEO_INFO_HEIGHT(&info);
    int stride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);
    int thickness = 2;

    for (const BBox &box : boxes) {
        int x0 = max(0, box.x);
        int y0 = max(0, box.y);
        int x1 = min(width - 1, box.x + box.width);
        int y1 = min(height - 1, box.y + box.height);

        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                bool edge = x < x0 + thickness || x > x1 - thickness ||
                            y < y0 + thickness || y > y1 - thickness;
                if (!edge) {
                    continue;
                }
                uint8_t *pixel = map.data + y * stride + x * 3;
                pixel[0] = 255;
                pixel[1] = 0;
                pixel[2] = 0;
            }
        }
    }
    gst_buffer_unmap(copy, &map);

    GstSample *sample = gst_sample_new(copy, caps, nullptr, nullptr);
    gst_buffer_unref(copy);

    GstCaps *jpegCaps = gst_caps_new_empty_simple("image/jpeg");
    GError *error = nullptr;
    GstSample *jpeg = gst_video_convert_sample(sample, jpegCaps, GST_SECOND, &error);
    gst_caps_unref(jpegCaps);
    gst_sample_unref(sample);

    if (jpeg == nullptr) {
        cerr << "Failed to encode snapshot: " << (error ? error->message : "unknown") << endl;
        if (error) g_error_free(error);
        return;
    }

    GstBuffer *jpegBuffer = gst_sample_get_buffer(jpeg);
    if (gst_buffer_map(jpegBuffer, &map, GST_MAP_READ)) {
        string path = "snapshot_" + to_string(snapshotIndex++) + ".jpg";
        ofstream file(path, ios::binary);
        file.write(reinterpret_cast<const char *>(map.data), map.size);
        cout << "Snapshot saved: " << path << endl;
        gst_buffer_unmap(jpegBuffer, &map);
    }
    gst_sample_unref(jpeg);
}

GstElement *AIElement::getOutput() {
    return identity;
}

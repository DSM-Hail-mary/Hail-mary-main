#include "AI_element.h"
#include <string>
using namespace std;

static GstPadProbeReturn aiProbe(GstPad *pad, GstPadProbeInfo *info, gpointer data) {
    AIElement *self = static_cast<AIElement *>(data);

    GstBuffer *buffer = gst_buffer_make_writable(GST_PAD_PROBE_INFO_BUFFER(info));
    GST_PAD_PROBE_INFO_DATA(info) = buffer;

    self->aiProcess(pad, buffer);
    return GST_PAD_PROBE_OK;
}

AIElement::AIElement() {
    queue = nullptr;
    identity = nullptr;
    addedToBin = false;
    skipCount = 0;
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

    if (gst_meta_get_info(AI_META_NAME) == nullptr) {
        gst_meta_register_custom_simple(AI_META_NAME);
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

    aiAttachMeta(buffer, boxes);

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

void AIElement::aiAttachMeta(GstBuffer *buffer, const vector<BBox> &boxes) {
    GstCustomMeta *meta = gst_buffer_add_custom_meta(buffer, AI_META_NAME);
    if (meta == nullptr) {
        cerr << "Failed to attach AI meta" << endl;
        return;
    }

    GstStructure *structure = gst_custom_meta_get_structure(meta);
    gst_structure_set(structure, "count", G_TYPE_INT, static_cast<int>(boxes.size()), nullptr);

    for (size_t i = 0; i < boxes.size(); i++) {
        string n = to_string(i);
        gst_structure_set(structure,
                          ("x" + n).c_str(), G_TYPE_INT, boxes[i].x,
                          ("y" + n).c_str(), G_TYPE_INT, boxes[i].y,
                          ("w" + n).c_str(), G_TYPE_INT, boxes[i].width,
                          ("h" + n).c_str(), G_TYPE_INT, boxes[i].height,
                          ("s" + n).c_str(), G_TYPE_DOUBLE, static_cast<double>(boxes[i].score),
                          nullptr);
    }
}

GstElement *AIElement::getOutput() {
    return identity;
}

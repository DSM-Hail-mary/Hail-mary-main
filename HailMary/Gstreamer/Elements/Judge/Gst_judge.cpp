#include "Gst_judge.h"
#include <fstream>
#include <string>
#include <algorithm>
using namespace std;

static GstPadProbeReturn judgeProbe(GstPad *pad, GstPadProbeInfo *info, gpointer data) {
    JudgeElement *self = static_cast<JudgeElement *>(data);

    GstBuffer *buffer = gst_buffer_make_writable(GST_PAD_PROBE_INFO_BUFFER(info));
    GST_PAD_PROBE_INFO_DATA(info) = buffer;

    self->judgeProcess(pad, buffer);
    return GST_PAD_PROBE_OK;
}

JudgeElement::JudgeElement() {
    identity = nullptr;
    addedToBin = false;
    problem = false;
    hitCount = 0;
    missCount = 0;
    snapshotIndex = 0;
}

JudgeElement::~JudgeElement() {
    if (identity != nullptr && !addedToBin) {
        gst_object_unref(identity);
    }
    identity = nullptr;
}

bool JudgeElement::judgeInit() {
    if (identity != nullptr) {
        cerr << "Judge element already created" << endl;
        return false;
    }

    identity = gst_element_factory_make("identity", "judge_identity");
    if (identity == nullptr) {
        cerr << "Failed to create judge element" << endl;
        return false;
    }

    return true;
}

bool JudgeElement::judgeConnect(GstElement *input) {
    if (pipeline == nullptr || input == nullptr || identity == nullptr) {
        cerr << "Cannot connect to judge element" << endl;
        return false;
    }

    if (!gst_bin_add(GST_BIN(pipeline), identity)) {
        cerr << "Failed to add judge to pipeline" << endl;
        return false;
    }
    addedToBin = true;

    if (!gst_element_link(input, identity)) {
        cerr << "Failed to link judge element" << endl;
        return false;
    }

    GstPad *pad = gst_element_get_static_pad(identity, "src");
    gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, judgeProbe, this, nullptr);
    gst_object_unref(pad);

    return true;
}

void JudgeElement::judgeProcess(GstPad *pad, GstBuffer *buffer) {
    GstCustomMeta *meta = gst_buffer_get_custom_meta(buffer, AI_META_NAME);
    if (meta == nullptr) {
        return;
    }

    GstStructure *structure = gst_custom_meta_get_structure(meta);
    int count = 0;
    gst_structure_get_int(structure, "count", &count);

    vector<BBox> hits;
    for (int i = 0; i < count; i++) {
        string n = to_string(i);
        BBox box = {0, 0, 0, 0, 0.0f};
        double score = 0.0;
        gst_structure_get_int(structure, ("x" + n).c_str(), &box.x);
        gst_structure_get_int(structure, ("y" + n).c_str(), &box.y);
        gst_structure_get_int(structure, ("w" + n).c_str(), &box.width);
        gst_structure_get_int(structure, ("h" + n).c_str(), &box.height);
        gst_structure_get_double(structure, ("s" + n).c_str(), &score);
        box.score = static_cast<float>(score);

        if (score >= JUDGE_SCORE) {
            hits.push_back(box);
        }
    }

    bool next = judgeDecide(hits);
    bool changed = next != problem;
    problem = next;

    JudgeResult *judgeResult = judgeMetaAdd(buffer);
    if (judgeResult != nullptr) {
        judgeResult->problem = problem;
    }

    if (!changed) {
        return;
    }

    GstStructure *result = gst_structure_new("judge", "problem", G_TYPE_BOOLEAN, problem, nullptr);
    gst_element_post_message(identity, gst_message_new_element(GST_OBJECT(identity), result));

    if (problem) {
        cout << "Judge: problem detected" << endl;
        GstCaps *caps = gst_pad_get_current_caps(pad);
        if (caps != nullptr) {
            judgeSnapshot(buffer, caps, hits);
            gst_caps_unref(caps);
        }
    } else {
        cout << "Judge: back to normal" << endl;
    }
}

bool JudgeElement::judgeDecide(const vector<BBox> &boxes) {
    if (!boxes.empty()) {
        hitCount++;
        missCount = 0;
    } else {
        missCount++;
        hitCount = 0;
    }

    if (!problem && hitCount >= JUDGE_HIT_FRAMES) {
        return true;
    }
    if (problem && missCount >= JUDGE_CLEAR_FRAMES) {
        return false;
    }
    return problem;
}

void JudgeElement::judgeSnapshot(GstBuffer *buffer, GstCaps *caps, const vector<BBox> &boxes) {
    GstVideoInfo info;
    if (!gst_video_info_from_caps(&info, caps)) {
        return;
    }

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

GstElement *JudgeElement::getOutput() {
    return identity;
}

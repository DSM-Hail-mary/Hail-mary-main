#include "Gst_judge.h"
#include <fstream>
#include <iostream>
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
    snapshotStop = false;
    snapshotThread = thread(&JudgeElement::judgeSnapshotWorker, this);
}

JudgeElement::~JudgeElement() {
    {
        lock_guard<mutex> lock(snapshotMutex);
        snapshotStop = true;
    }
    snapshotCond.notify_one();
    snapshotThread.join();

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

bool JudgeElement::judgeConnect(GstElement *bin, GstElement *input) {
    if (bin == nullptr || input == nullptr || identity == nullptr) {
        cerr << "Cannot connect to judge element" << endl;
        return false;
    }

    if (!gst_bin_add(GST_BIN(bin), identity)) {
        cerr << "Failed to add judge to bin" << endl;
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
        BBox box = {0, 0, 0, 0, 0.0f, 0};
        double score = 0.0;
        gst_structure_get_int(structure, ("x" + n).c_str(), &box.x);
        gst_structure_get_int(structure, ("y" + n).c_str(), &box.y);
        gst_structure_get_int(structure, ("w" + n).c_str(), &box.width);
        gst_structure_get_int(structure, ("h" + n).c_str(), &box.height);
        gst_structure_get_double(structure, ("s" + n).c_str(), &score);
        gst_structure_get_int(structure, ("c" + n).c_str(), &box.classId);
        box.score = static_cast<float>(score);

        if (box.classId == JUDGE_PROBLEM_CLASS && score >= JUDGE_SCORE) {
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
            judgeSnapshotQueue(buffer, caps, hits);
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

// Runs on the streaming thread: only copy the frame, encoding happens in the worker
void JudgeElement::judgeSnapshotQueue(GstBuffer *buffer, GstCaps *caps, const vector<BBox> &boxes) {
    lock_guard<mutex> lock(snapshotMutex);
    if (snapshotJobs.size() >= JUDGE_SNAPSHOT_QUEUE_MAX) {
        cerr << "Snapshot queue full, dropping snapshot" << endl;
        return;
    }

    SnapshotJob job;
    job.buffer = gst_buffer_copy_deep(buffer);
    job.caps = gst_caps_ref(caps);
    job.boxes = boxes;
    job.time = time(nullptr);
    job.index = snapshotIndex++;
    snapshotJobs.push_back(job);
    snapshotCond.notify_one();
}

void JudgeElement::judgeSnapshotWorker() {
    while (true) {
        unique_lock<mutex> lock(snapshotMutex);
        snapshotCond.wait(lock, [this] { return snapshotStop || !snapshotJobs.empty(); });
        if (snapshotJobs.empty()) {
            return;
        }

        SnapshotJob job = snapshotJobs.front();
        snapshotJobs.pop_front();
        lock.unlock();

        judgeSnapshot(job);
        gst_buffer_unref(job.buffer);
        gst_caps_unref(job.caps);
    }
}

void JudgeElement::judgeSnapshot(SnapshotJob &job) {
    GstVideoInfo info;
    if (!gst_video_info_from_caps(&info, job.caps)) {
        return;
    }

    if (!GST_VIDEO_INFO_IS_RGB(&info) || GST_VIDEO_INFO_N_PLANES(&info) != 1) {
        cerr << "Snapshot needs packed RGB frame" << endl;
        return;
    }

    GstMapInfo map;
    if (!gst_buffer_map(job.buffer, &map, GST_MAP_WRITE)) {
        return;
    }

    int width = GST_VIDEO_INFO_WIDTH(&info);
    int height = GST_VIDEO_INFO_HEIGHT(&info);
    int stride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);
    int pixelStride = GST_VIDEO_INFO_COMP_PSTRIDE(&info, 0);
    int red = GST_VIDEO_INFO_COMP_POFFSET(&info, GST_VIDEO_COMP_R);
    int green = GST_VIDEO_INFO_COMP_POFFSET(&info, GST_VIDEO_COMP_G);
    int blue = GST_VIDEO_INFO_COMP_POFFSET(&info, GST_VIDEO_COMP_B);
    int thickness = 2;

    for (const BBox &box : job.boxes) {
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
                uint8_t *pixel = map.data + y * stride + x * pixelStride;
                pixel[red] = 255;
                pixel[green] = 0;
                pixel[blue] = 0;
            }
        }
    }
    gst_buffer_unmap(job.buffer, &map);

    GstSample *sample = gst_sample_new(job.buffer, job.caps, nullptr, nullptr);

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
        tm local;
        localtime_r(&job.time, &local);
        char stamp[32];
        strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &local);

        string path = "snapshot_" + string(stamp) + "_" + to_string(job.index) + ".jpg";
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

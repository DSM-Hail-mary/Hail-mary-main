#include "Gst_judge.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

static const char *judgeKind(int classId) {
    if (classId == JUDGE_PROBLEM_CLASS) return "nest";
    if (classId == JUDGE_POLE_CLASS) return "pole";
    return nullptr;
}

static string judgeTimestamp(time_t t) {
    tm local;
    localtime_r(&t, &local);
    char stamp[32];
    strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%S", &local);
    long offset = local.tm_gmtoff / 60;
    char zone[8];
    snprintf(zone, sizeof(zone), "%c%02ld:%02ld", offset < 0 ? '-' : '+', labs(offset) / 60, labs(offset) % 60);
    return string(stamp) + zone;
}

JudgeElement::JudgeElement() {
    identity = nullptr;
    addedToBin = false;
    problem = false;
    hitCount = 0;
    missCount = 0;
    snapshotIndex = 0;
    const char *dir = getenv("HAILMARY_SNAPSHOT_DIR");
    snapshotDir = dir != nullptr ? dir : JUDGE_SNAPSHOT_DIR;
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

    if (g_mkdir_with_parents(snapshotDir.c_str(), 0755) != 0) {
        cerr << "Failed to create snapshot dir: " << snapshotDir << endl;
        return false;
    }
    cout << "Snapshots: " << snapshotDir << endl;

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

    vector<BBox> boxes;
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

        if (judgeKind(box.classId) != nullptr) {
            boxes.push_back(box);
        }
        if (box.classId == JUDGE_PROBLEM_CLASS && score >= JUDGE_CAUTION_SCORE) {
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
        BBox target = *max_element(hits.begin(), hits.end(),
                                   [](const BBox &a, const BBox &b) { return a.score < b.score; });
        string grade = target.score >= JUDGE_DANGER_SCORE ? "danger" : "caution";
        cout << "Judge: problem detected (" << grade << " " << target.score << ")" << endl;
        GstCaps *caps = gst_pad_get_current_caps(pad);
        if (caps != nullptr) {
            judgeSnapshotQueue(buffer, caps, boxes, target, grade);
            gst_caps_unref(caps);
        }
    } else {
        cout << "Judge: back to normal" << endl;
    }
}

bool JudgeElement::judgeDecide(const vector<BBox> &hits) {
    if (!hits.empty()) {
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
void JudgeElement::judgeSnapshotQueue(GstBuffer *buffer, GstCaps *caps, const vector<BBox> &boxes,
                                      const BBox &target, const string &grade) {
    lock_guard<mutex> lock(snapshotMutex);
    if (snapshotJobs.size() >= JUDGE_SNAPSHOT_QUEUE_MAX) {
        cerr << "Snapshot queue full, dropping snapshot" << endl;
        return;
    }

    SnapshotJob job;
    job.buffer = gst_buffer_copy_deep(buffer);
    job.caps = gst_caps_ref(caps);
    job.boxes = boxes;
    job.target = target;
    job.grade = grade;
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

    int width = GST_VIDEO_INFO_WIDTH(&info);
    int height = GST_VIDEO_INFO_HEIGHT(&info);
    int stride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);
    int pixelStride = GST_VIDEO_INFO_COMP_PSTRIDE(&info, 0);

    const BBox &t = job.target;
    int padX = static_cast<int>(t.width * JUDGE_CROP_PAD);
    int padY = static_cast<int>(t.height * JUDGE_CROP_PAD);
    int x0 = max(0, t.x - padX);
    int y0 = max(0, t.y - padY);
    int x1 = min(width, t.x + t.width + padX);
    int y1 = min(height, t.y + t.height + padY);
    if (x1 <= x0 || y1 <= y0) {
        x0 = 0;
        y0 = 0;
        x1 = width;
        y1 = height;
    }
    int cropWidth = x1 - x0;
    int cropHeight = y1 - y0;

    GstVideoInfo cropInfo;
    gst_video_info_set_format(&cropInfo, GST_VIDEO_INFO_FORMAT(&info), cropWidth, cropHeight);
    int cropStride = GST_VIDEO_INFO_PLANE_STRIDE(&cropInfo, 0);
    GstBuffer *cropBuffer = gst_buffer_new_allocate(nullptr, GST_VIDEO_INFO_SIZE(&cropInfo), nullptr);

    GstMapInfo src;
    GstMapInfo dst;
    if (!gst_buffer_map(job.buffer, &src, GST_MAP_READ)) {
        gst_buffer_unref(cropBuffer);
        return;
    }
    if (!gst_buffer_map(cropBuffer, &dst, GST_MAP_WRITE)) {
        gst_buffer_unmap(job.buffer, &src);
        gst_buffer_unref(cropBuffer);
        return;
    }
    for (int y = 0; y < cropHeight; y++) {
        memcpy(dst.data + y * cropStride, src.data + (y0 + y) * stride + x0 * pixelStride,
               static_cast<size_t>(cropWidth) * pixelStride);
    }
    gst_buffer_unmap(cropBuffer, &dst);
    gst_buffer_unmap(job.buffer, &src);

    GstCaps *cropCaps = gst_video_info_to_caps(&cropInfo);
    GstSample *sample = gst_sample_new(cropBuffer, cropCaps, nullptr, nullptr);
    gst_caps_unref(cropCaps);
    gst_buffer_unref(cropBuffer);

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
    GstMapInfo map;
    if (gst_buffer_map(jpegBuffer, &map, GST_MAP_READ)) {
        tm local;
        localtime_r(&job.time, &local);
        char stamp[32];
        strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &local);

        string name = "snapshot_" + string(stamp) + "_" + to_string(job.index);
        string image = name + ".jpg";
        ofstream file(snapshotDir + "/" + image, ios::binary);
        file.write(reinterpret_cast<const char *>(map.data), map.size);
        file.close();
        gst_buffer_unmap(jpegBuffer, &map);

        judgeWriteMeta(job, image, x0, y0, cropWidth, cropHeight);
        cout << "Snapshot saved: " << snapshotDir << "/" << image << endl;
    }
    gst_sample_unref(jpeg);
}

void JudgeElement::judgeWriteMeta(const SnapshotJob &job, const string &image, int x0, int y0, int cropWidth,
                                  int cropHeight) {
    string detections;
    for (const BBox &box : job.boxes) {
        const char *kind = judgeKind(box.classId);
        double bx0 = max(box.x, x0);
        double by0 = max(box.y, y0);
        double bx1 = min(box.x + box.width, x0 + cropWidth);
        double by1 = min(box.y + box.height, y0 + cropHeight);
        if (kind == nullptr || bx1 <= bx0 || by1 <= by0) {
            continue;
        }
        char item[256];
        snprintf(item, sizeof(item),
                 "%s{\"kind\":\"%s\",\"score\":%.3f,\"box\":{\"x\":%.4f,\"y\":%.4f,\"w\":%.4f,\"h\":%.4f}}",
                 detections.empty() ? "" : ",", kind, box.score,
                 (bx0 - x0) / cropWidth, (by0 - y0) / cropHeight,
                 (bx1 - bx0) / cropWidth, (by1 - by0) / cropHeight);
        detections += item;
    }

    char head[256];
    snprintf(head, sizeof(head),
             "{\"grade\":\"%s\",\"hazard_type\":\"nest\",\"conf\":%.3f,\"recorded_at\":\"%s\",\"image\":\"%s\",",
             job.grade.c_str(), job.target.score, judgeTimestamp(job.time).c_str(), image.c_str());

    string base = snapshotDir + "/" + image.substr(0, image.size() - 4);
    string tmp = base + ".json.tmp";
    ofstream file(tmp);
    file << head << "\"detections\":[" << detections << "]}\n";
    file.close();
    if (rename(tmp.c_str(), (base + ".json").c_str()) != 0) {
        cerr << "Failed to write snapshot meta: " << base << ".json" << endl;
    }
}

GstElement *JudgeElement::getOutput() {
    return identity;
}

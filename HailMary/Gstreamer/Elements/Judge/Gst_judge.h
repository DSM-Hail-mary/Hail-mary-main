#ifndef HAILMARY_GST_JUDGE_H
#define HAILMARY_GST_JUDGE_H

#include <gst/gst.h>
#include <gst/video/video.h>
#include <condition_variable>
#include <ctime>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>
#include "Elements/AI/AI_types.h"
#include "Judge_types.h"

static const double JUDGE_SCORE = 0.6;
static const int JUDGE_PROBLEM_CLASS = 0;
static const int JUDGE_HIT_FRAMES = 3;
static const int JUDGE_CLEAR_FRAMES = 5;
static const size_t JUDGE_SNAPSHOT_QUEUE_MAX = 4;

struct SnapshotJob {
    GstBuffer *buffer;
    GstCaps *caps;
    std::vector<BBox> boxes;
    time_t time;
    int index;
};

class JudgeElement {
    public:
        JudgeElement();
        ~JudgeElement();

        JudgeElement(const JudgeElement &) = delete;
        JudgeElement &operator=(const JudgeElement &) = delete;

        bool judgeInit();
        bool judgeConnect(GstElement *bin, GstElement *input);
        void judgeProcess(GstPad *pad, GstBuffer *buffer);

        GstElement *getOutput();

    private:
        bool judgeDecide(const std::vector<BBox> &boxes);
        void judgeSnapshotQueue(GstBuffer *buffer, GstCaps *caps, const std::vector<BBox> &boxes);
        void judgeSnapshotWorker();
        void judgeSnapshot(SnapshotJob &job);

        GstElement *identity;
        bool addedToBin;
        bool problem;
        int hitCount;
        int missCount;
        int snapshotIndex;

        std::thread snapshotThread;
        std::mutex snapshotMutex;
        std::condition_variable snapshotCond;
        std::deque<SnapshotJob> snapshotJobs;
        bool snapshotStop;
};

#endif //HAILMARY_GST_JUDGE_H

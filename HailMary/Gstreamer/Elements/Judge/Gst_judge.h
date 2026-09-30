#ifndef HAILMARY_GST_JUDGE_H
#define HAILMARY_GST_JUDGE_H

#include <gst/gst.h>
#include <gst/video/video.h>
#include <vector>
#include "Elements/AI/AI_types.h"
#include "main_pipeline.h"

static const double JUDGE_SCORE = 0.6;
static const int JUDGE_HIT_FRAMES = 3;
static const int JUDGE_CLEAR_FRAMES = 5;

class JudgeElement {
    public:
        JudgeElement();
        ~JudgeElement();

        JudgeElement(const JudgeElement &) = delete;
        JudgeElement &operator=(const JudgeElement &) = delete;

        bool judgeInit();
        bool judgeConnect(GstElement *input);
        void judgeProcess(GstPad *pad, GstBuffer *buffer);

        GstElement *getOutput();

    private:
        bool judgeDecide(const std::vector<BBox> &boxes);
        void judgeSnapshot(GstBuffer *buffer, GstCaps *caps, const std::vector<BBox> &boxes);

        GstElement *identity;
        bool addedToBin;
        bool problem;
        int hitCount;
        int missCount;
        int snapshotIndex;
};

#endif //HAILMARY_GST_JUDGE_H

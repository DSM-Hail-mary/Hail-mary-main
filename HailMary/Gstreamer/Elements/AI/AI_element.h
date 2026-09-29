#ifndef HAILMARY_AI_ELEMENT_H
#define HAILMARY_AI_ELEMENT_H

#include <gst/gst.h>
#include <gst/video/video.h>
#include <vector>
#include "main_pipeline.h"

static const gint64 AI_BUDGET_US = 33000;
static const gint64 SNAPSHOT_INTERVAL_US = 1000000;

typedef struct {
    int x;
    int y;
    int width;
    int height;
    float score;
} BBox;

class AIElement {
    public:
        AIElement();
        ~AIElement();

        AIElement(const AIElement &) = delete;
        AIElement &operator=(const AIElement &) = delete;

        bool aiInit();
        bool aiConnect(GstElement *input);
        void aiProcess(GstPad *pad, GstBuffer *buffer);

        GstElement *getOutput();

    private:
        void aiTimer(gint64 elapsed);
        std::vector<BBox> aiInference(const GstVideoInfo &info, const uint8_t *data);
        void aiSnapshot(GstBuffer *buffer, GstCaps *caps, const GstVideoInfo &info, const std::vector<BBox> &boxes);

        GstElement *queue;
        GstElement *identity;
        bool addedToBin;
        int skipCount;
        gint64 lastSnapshot;
        int snapshotIndex;
};

#endif //HAILMARY_AI_ELEMENT_H

#ifndef HAILMARY_AI_ELEMENT_H
#define HAILMARY_AI_ELEMENT_H

#include <gst/gst.h>
#include <vector>
#include "AI_types.h"
#include "main_pipeline.h"

static const char *AI_CONFIG_PATH = HAILMARY_MODEL_DIR "/config_infer_primary_yoloV8.txt";

class AIElement {
    public:
        AIElement();
        ~AIElement();

        AIElement(const AIElement &) = delete;
        AIElement &operator=(const AIElement &) = delete;

        bool aiInit();
        bool aiConnect(GstElement *input);
        void aiProcess(GstBuffer *buffer);

        GstElement *getOutput();

    private:
        void aiAttachMeta(GstBuffer *buffer, const std::vector<BBox> &boxes);

        GstElement *queue;
        GstElement *upload;
        GstElement *uploadCaps;
        GstElement *mux;
        GstElement *infer;
        GstElement *download;
        GstElement *downloadCaps;
        GstElement *identity;
        bool addedToBin;
};

#endif //HAILMARY_AI_ELEMENT_H

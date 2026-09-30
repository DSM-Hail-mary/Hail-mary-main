#ifndef HAILMARY_GST_CAPS_H
#define HAILMARY_GST_CAPS_H

#include <gst/gst.h>
#include "main_pipeline.h"

static const int FRAME_WIDTH = 640;
static const int FRAME_HEIGHT = 480;
static const int FRAME_FPS = 30;

class CapsElement {
    public:
        CapsElement();
        ~CapsElement();

        CapsElement(const CapsElement &) = delete;
        CapsElement &operator=(const CapsElement &) = delete;

        bool capsInit();
        bool capsConnect(GstElement *input);

        GstElement *getOutput();

    private:
        GstElement *convert;
        GstElement *scale;
        GstElement *rate;
        GstElement *filter;
        bool addedToBin;
};

#endif //HAILMARY_GST_CAPS_H

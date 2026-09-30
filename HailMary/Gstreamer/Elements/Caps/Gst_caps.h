#ifndef HAILMARY_GST_CAPS_H
#define HAILMARY_GST_CAPS_H

#include <gst/gst.h>
#include "main_pipeline.h"

static const char *FRAME_FORMAT = "RGBA";
static const int FRAME_WIDTH = 1280;
static const int FRAME_HEIGHT = 720;

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
        GstElement *filter;
        bool addedToBin;
};

#endif //HAILMARY_GST_CAPS_H

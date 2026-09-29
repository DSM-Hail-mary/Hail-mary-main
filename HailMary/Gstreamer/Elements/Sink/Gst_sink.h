#ifndef HAILMARY_GST_SINK_H
#define HAILMARY_GST_SINK_H

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include "main_pipeline.h"

class SinkElement {
    public:
        SinkElement();
        ~SinkElement();

        SinkElement(const SinkElement &) = delete;
        SinkElement &operator=(const SinkElement &) = delete;

        bool sinkInit();
        bool sinkConnect(GstElement *input);

        GstSample *pullFrame(GstClockTime timeout);

    private:
        GstElement *sink;
        bool addedToBin;
};

#endif //HAILMARY_GST_SINK_H

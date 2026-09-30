#ifndef HAILMARY_GST_SINK_H
#define HAILMARY_GST_SINK_H

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include "Elements/Judge/Judge_types.h"
#include "main_pipeline.h"

typedef void (*SinkCallback)(GstSample *sample, const JudgeResult *result, void *userData);

class SinkElement {
    public:
        SinkElement();
        ~SinkElement();

        SinkElement(const SinkElement &) = delete;
        SinkElement &operator=(const SinkElement &) = delete;

        bool sinkInit();
        bool sinkConnect(GstElement *input);
        void sinkSetCallback(SinkCallback callback, void *userData);

    private:
        static GstFlowReturn sinkNewSample(GstAppSink *appsink, gpointer data);

        GstElement *sink;
        SinkCallback callback;
        void *userData;
        bool addedToBin;
};

#endif //HAILMARY_GST_SINK_H

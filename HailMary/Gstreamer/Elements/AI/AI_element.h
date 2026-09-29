#ifndef HAILMARY_AI_ELEMENT_H
#define HAILMARY_AI_ELEMENT_H

#include <gst/gst.h>
#include "Elements/Sink/Gst_sink.h"

struct AIResult {
};

class AIElement {
    public:
        AIElement();
        ~AIElement();

        AIElement(const AIElement &) = delete;
        AIElement &operator=(const AIElement &) = delete;

        bool aiInit();
        bool aiConnect(SinkElement *input);
        bool aiGet(GstClockTime timeout);
        bool aiProcess();
        const AIResult &aiOutput();

    private:
        SinkElement *input;
        GstSample *sample;
        AIResult result;
};

#endif //HAILMARY_AI_ELEMENT_H

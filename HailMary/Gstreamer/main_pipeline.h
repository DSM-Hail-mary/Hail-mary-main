#ifndef HAILMARY_MAIN_PIPELINE_H
#define HAILMARY_MAIN_PIPELINE_H

#include <gst/gst.h>
#include <iostream>
#include <stdint.h>
#include "Elements/Camera/Gst_elements.h"
#include "Elements/Caps/Gst_caps.h"
#include "Elements/Sink/Gst_sink.h"

extern GstElement *pipeline;

int runPipeline();

void messageBusTest();
void numberOfPipelines();

#endif

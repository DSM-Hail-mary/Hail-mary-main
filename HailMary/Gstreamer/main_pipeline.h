#ifndef HAILMARY_MAIN_PIPELINE_H
#define HAILMARY_MAIN_PIPELINE_H

#include <gst/gst.h>
#include <iostream>
#include <stdint.h>
#include "Elements/Camera/Gst_elements.h"
#include "Elements/Caps/Gst_caps.h"
#include "Elements/Sink/Gst_sink.h"
#include "Elements/AI/AI_element.h"
#include "Elements/Judge/Gst_judge.h"

static const int RECONNECT_MAX_ATTEMPTS = 5;
static const int RECONNECT_INTERVAL_SEC = 5;

int runPipeline();

void messageBusTest(GstElement *pipeline);
void numberOfPipelines(GstElement *pipeline);

#endif

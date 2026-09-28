#ifndef HAILMARY_MAIN_PIPELINE_H
#define HAILMARY_MAIN_PIPELINE_H

#include <gst/gst.h>
#include <iostream>
#include <stdint.h>

extern GstElement *pipeline;

int runPipeline();

void messageBusTest();
void numberOfPipelines();

#endif

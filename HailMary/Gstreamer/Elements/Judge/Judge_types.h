#ifndef HAILMARY_JUDGE_TYPES_H
#define HAILMARY_JUDGE_TYPES_H

#include <gst/gst.h>

struct JudgeResult {
    bool problem;
};

JudgeResult *judgeMetaAdd(GstBuffer *buffer);
JudgeResult *judgeMetaGet(GstBuffer *buffer);

#endif //HAILMARY_JUDGE_TYPES_H

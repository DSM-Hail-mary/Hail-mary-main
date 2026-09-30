#ifndef HAILMARY_AI_TYPES_H
#define HAILMARY_AI_TYPES_H

static const char *AI_META_NAME = "HailMaryAIMeta";

typedef struct {
    int x;
    int y;
    int width;
    int height;
    float score;
} BBox;

#endif //HAILMARY_AI_TYPES_H

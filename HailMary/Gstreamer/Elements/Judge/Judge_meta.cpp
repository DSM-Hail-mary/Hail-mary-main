#include "Judge_types.h"
#include <new>

struct JudgeMeta {
    GstMeta meta;
    JudgeResult result;
};

static GType judgeMetaApiType() {
    static const gchar *tags[] = {nullptr};
    static GType type = gst_meta_api_type_register("JudgeMetaAPI", tags);
    return type;
}

static gboolean judgeMetaInit(GstMeta *meta, gpointer params, GstBuffer *buffer) {
    JudgeMeta *judgeMeta = reinterpret_cast<JudgeMeta *>(meta);
    new (&judgeMeta->result) JudgeResult();
    return TRUE;
}

static void judgeMetaFree(GstMeta *meta, GstBuffer *buffer) {
    JudgeMeta *judgeMeta = reinterpret_cast<JudgeMeta *>(meta);
    judgeMeta->result.~JudgeResult();
}

static gboolean judgeMetaTransform(GstBuffer *dest, GstMeta *meta, GstBuffer *buffer, GQuark type, gpointer data) {
    if (!GST_META_TRANSFORM_IS_COPY(type)) {
        return FALSE;
    }

    JudgeResult *result = judgeMetaAdd(dest);
    if (result == nullptr) {
        return FALSE;
    }
    *result = reinterpret_cast<JudgeMeta *>(meta)->result;
    return TRUE;
}

static const GstMetaInfo *judgeMetaInfo() {
    static const GstMetaInfo *info = gst_meta_register(
        judgeMetaApiType(), "JudgeMeta", sizeof(JudgeMeta),
        judgeMetaInit, judgeMetaFree, judgeMetaTransform);
    return info;
}

JudgeResult *judgeMetaAdd(GstBuffer *buffer) {
    JudgeMeta *meta = reinterpret_cast<JudgeMeta *>(gst_buffer_add_meta(buffer, judgeMetaInfo(), nullptr));
    return meta != nullptr ? &meta->result : nullptr;
}

JudgeResult *judgeMetaGet(GstBuffer *buffer) {
    JudgeMeta *meta = reinterpret_cast<JudgeMeta *>(gst_buffer_get_meta(buffer, judgeMetaApiType()));
    return meta != nullptr ? &meta->result : nullptr;
}

#include "AI_element.h"
#include <cstdlib>
#include <string>
#include <iostream>
#include "gstnvdsmeta.h"
#include "Elements/Caps/Gst_caps.h"
using namespace std;

static GstPadProbeReturn aiProbe(GstPad *pad, GstPadProbeInfo *info, gpointer data) {
    AIElement *self = static_cast<AIElement *>(data);

    GstBuffer *buffer = gst_buffer_make_writable(GST_PAD_PROBE_INFO_BUFFER(info));
    GST_PAD_PROBE_INFO_DATA(info) = buffer;

    self->aiProcess(buffer);
    return GST_PAD_PROBE_OK;
}

AIElement::AIElement() {
    queue = nullptr;
    upload = nullptr;
    uploadCaps = nullptr;
    mux = nullptr;
    infer = nullptr;
    download = nullptr;
    downloadCaps = nullptr;
    identity = nullptr;
    tee = nullptr;
    mainQueue = nullptr;
    displayQueue = nullptr;
    displayConvert = nullptr;
    displayCaps = nullptr;
    osd = nullptr;
    display = nullptr;
    showDisplay = false;
    addedToBin = false;
}

AIElement::~AIElement() {
    if (!addedToBin) {
        if (queue != nullptr) gst_object_unref(queue);
        if (upload != nullptr) gst_object_unref(upload);
        if (uploadCaps != nullptr) gst_object_unref(uploadCaps);
        if (mux != nullptr) gst_object_unref(mux);
        if (infer != nullptr) gst_object_unref(infer);
        if (download != nullptr) gst_object_unref(download);
        if (downloadCaps != nullptr) gst_object_unref(downloadCaps);
        if (identity != nullptr) gst_object_unref(identity);
        if (tee != nullptr) gst_object_unref(tee);
        if (mainQueue != nullptr) gst_object_unref(mainQueue);
        if (displayQueue != nullptr) gst_object_unref(displayQueue);
        if (displayConvert != nullptr) gst_object_unref(displayConvert);
        if (displayCaps != nullptr) gst_object_unref(displayCaps);
        if (osd != nullptr) gst_object_unref(osd);
        if (display != nullptr) gst_object_unref(display);
    }
    queue = nullptr;
    identity = nullptr;
}

bool AIElement::aiInit() {
    if (identity != nullptr) {
        cerr << "AI elements already created" << endl;
        return false;
    }

    queue = gst_element_factory_make("queue", "ai_queue");
    upload = gst_element_factory_make("nvvideoconvert", "ai_upload");
    uploadCaps = gst_element_factory_make("capsfilter", "ai_upload_caps");
    mux = gst_element_factory_make("nvstreammux", "ai_mux");
    infer = gst_element_factory_make("nvinfer", "ai_infer");
    download = gst_element_factory_make("nvvideoconvert", "ai_download");
    downloadCaps = gst_element_factory_make("capsfilter", "ai_download_caps");
    identity = gst_element_factory_make("identity", "ai_identity");
    if (!queue || !upload || !uploadCaps || !mux || !infer || !download || !downloadCaps || !identity) {
        cerr << "Failed to create AI elements" << endl;
        return false;
    }

    if (gst_meta_get_info(AI_META_NAME) == nullptr) {
        gst_meta_register_custom_simple(AI_META_NAME);
    }

    g_object_set(queue,
                 "leaky", 2,
                 "max-size-buffers", 1,
                 "max-size-bytes", 0,
                 "max-size-time", (guint64) 0,
                 nullptr);

    GstCaps *nvmmCaps = gst_caps_from_string("video/x-raw(memory:NVMM),format=NV12");
    g_object_set(uploadCaps, "caps", nvmmCaps, nullptr);
    gst_caps_unref(nvmmCaps);

    g_object_set(mux,
                 "batch-size", 1,
                 "width", FRAME_WIDTH,
                 "height", FRAME_HEIGHT,
                 "enable-padding", TRUE,
                 "live-source", TRUE,
                 "batched-push-timeout", 40000,
                 nullptr);

    g_object_set(infer, "config-file-path", AI_CONFIG_PATH, nullptr);

    GstCaps *rawCaps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, FRAME_FORMAT, nullptr);
    g_object_set(downloadCaps, "caps", rawCaps, nullptr);
    gst_caps_unref(rawCaps);

    const char *show = getenv("HAILMARY_SHOW");
    showDisplay = show != nullptr && string(show) == "1";
    if (showDisplay && !aiInitDisplay()) {
        return false;
    }

    return true;
}

bool AIElement::aiInitDisplay() {
    tee = gst_element_factory_make("tee", "ai_tee");
    mainQueue = gst_element_factory_make("queue", "ai_main_queue");
    displayQueue = gst_element_factory_make("queue", "ai_display_queue");
    displayConvert = gst_element_factory_make("nvvideoconvert", "ai_display_convert");
    displayCaps = gst_element_factory_make("capsfilter", "ai_display_caps");
    osd = gst_element_factory_make("nvdsosd", "ai_osd");
    display = gst_element_factory_make("nv3dsink", "ai_display");
    if (!tee || !mainQueue || !displayQueue || !displayConvert || !displayCaps || !osd || !display) {
        cerr << "Failed to create display elements" << endl;
        return false;
    }

    g_object_set(displayQueue,
                 "leaky", 2,
                 "max-size-buffers", 1,
                 "max-size-bytes", 0,
                 "max-size-time", (guint64) 0,
                 nullptr);

    GstCaps *osdCaps = gst_caps_from_string("video/x-raw(memory:NVMM),format=RGBA");
    g_object_set(displayCaps, "caps", osdCaps, nullptr);
    gst_caps_unref(osdCaps);

    g_object_set(display, "sync", FALSE, nullptr);

    return true;
}

bool AIElement::aiConnect(GstElement *bin, GstElement *input) {
    if (bin == nullptr || input == nullptr || identity == nullptr) {
        cerr << "Cannot connect to AI elements" << endl;
        return false;
    }

    gst_bin_add_many(GST_BIN(bin), queue, upload, uploadCaps, mux, infer,
                     download, downloadCaps, identity, nullptr);
    addedToBin = true;

    if (!gst_element_link_many(input, queue, upload, uploadCaps, nullptr)) {
        cerr << "Failed to link AI upload elements" << endl;
        return false;
    }

    GstPad *muxPad = gst_element_request_pad_simple(mux, "sink_0");
    GstPad *uploadPad = gst_element_get_static_pad(uploadCaps, "src");
    GstPadLinkReturn linked = gst_pad_link(uploadPad, muxPad);
    gst_object_unref(uploadPad);
    gst_object_unref(muxPad);
    if (linked != GST_PAD_LINK_OK) {
        cerr << "Failed to link nvstreammux" << endl;
        return false;
    }

    if (showDisplay) {
        gst_bin_add_many(GST_BIN(bin), tee, mainQueue, displayQueue, displayConvert, displayCaps,
                         osd, display, nullptr);
        if (!gst_element_link_many(mux, infer, tee, mainQueue, download, downloadCaps, identity, nullptr) ||
            !gst_element_link_many(tee, displayQueue, displayConvert, displayCaps, osd, display, nullptr)) {
            cerr << "Failed to link AI display elements" << endl;
            return false;
        }
        cout << "Display on (nvdsosd)" << endl;
    } else if (!gst_element_link_many(mux, infer, download, downloadCaps, identity, nullptr)) {
        cerr << "Failed to link AI inference elements" << endl;
        return false;
    }

    GstPad *pad = gst_element_get_static_pad(identity, "src");
    gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, aiProbe, this, nullptr);
    gst_object_unref(pad);

    cout << "Probe to each Elements" << endl;

    return true;
}

void AIElement::aiProcess(GstBuffer *buffer) {
    NvDsBatchMeta *batchMeta = gst_buffer_get_nvds_batch_meta(buffer);
    if (batchMeta == nullptr) {
        return;
    }

    vector<BBox> boxes;
    for (NvDsMetaList *frameList = batchMeta->frame_meta_list; frameList != nullptr; frameList = frameList->next) {
        NvDsFrameMeta *frameMeta = static_cast<NvDsFrameMeta *>(frameList->data);
        for (NvDsMetaList *objList = frameMeta->obj_meta_list; objList != nullptr; objList = objList->next) {
            NvDsObjectMeta *objMeta = static_cast<NvDsObjectMeta *>(objList->data);
            BBox box;
            box.x = static_cast<int>(objMeta->rect_params.left);
            box.y = static_cast<int>(objMeta->rect_params.top);
            box.width = static_cast<int>(objMeta->rect_params.width);
            box.height = static_cast<int>(objMeta->rect_params.height);
            box.score = objMeta->confidence;
            box.classId = objMeta->class_id;
            boxes.push_back(box);
        }
    }

    aiAttachMeta(buffer, boxes);
}

void AIElement::aiAttachMeta(GstBuffer *buffer, const vector<BBox> &boxes) {
    GstCustomMeta *meta = gst_buffer_add_custom_meta(buffer, AI_META_NAME);
    if (meta == nullptr) {
        cerr << "Failed to attach AI meta" << endl;
        return;
    }

    GstStructure *structure = gst_custom_meta_get_structure(meta);
    gst_structure_set(structure, "count", G_TYPE_INT, static_cast<int>(boxes.size()), nullptr);

    for (size_t i = 0; i < boxes.size(); i++) {
        string n = to_string(i);
        gst_structure_set(structure,
                          ("x" + n).c_str(), G_TYPE_INT, boxes[i].x,
                          ("y" + n).c_str(), G_TYPE_INT, boxes[i].y,
                          ("w" + n).c_str(), G_TYPE_INT, boxes[i].width,
                          ("h" + n).c_str(), G_TYPE_INT, boxes[i].height,
                          ("s" + n).c_str(), G_TYPE_DOUBLE, static_cast<double>(boxes[i].score),
                          ("c" + n).c_str(), G_TYPE_INT, boxes[i].classId,
                          nullptr);
    }
}

GstElement *AIElement::getOutput() {
    return identity;
}

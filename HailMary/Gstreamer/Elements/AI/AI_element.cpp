#include "AI_element.h"
#include <iostream>
using namespace std;

AIElement::AIElement() {
    input = nullptr;
    sample = nullptr;
}

AIElement::~AIElement() {
    if (sample != nullptr) {
        gst_sample_unref(sample);
    }
    sample = nullptr;
    input = nullptr;
}

bool AIElement::aiInit() {
    return true;
}

bool AIElement::aiConnect(SinkElement *input) {
    if (input == nullptr) {
        cerr << "Cannot connect to AI element" << endl;
        return false;
    }

    this->input = input;
    return true;
}

bool AIElement::aiGet(GstClockTime timeout) {
    if (input == nullptr) {
        cerr << "AI element is not connected" << endl;
        return false;
    }

    if (sample != nullptr) {
        gst_sample_unref(sample);
    }

    sample = input->pullFrame(timeout);
    return sample != nullptr;
}

bool AIElement::aiProcess() {
    if (sample == nullptr) {
        return false;
    }

    return true;
}

const AIResult &AIElement::aiOutput() {
    return result;
}

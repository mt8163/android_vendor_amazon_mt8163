#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#include "vsync_hint/VsyncHint.h"

#include <cutils/log.h>
#include <utils/Trace.h>

#include "ged/ged.h"

#undef LOG_TAG
#define LOG_TAG "VsyncHint"

namespace android {

VsyncHint::VsyncHint()
{
    mGed = reinterpret_cast<void*>(ged_create());
    if (mGed == NULL) {
        ALOGW("Failed to create ged handle for VsyncHine");
    }
}

VsyncHint::~VsyncHint()
{
    ged_destroy(reinterpret_cast<GED_HANDLE>(mGed));
}

void VsyncHint::notifyVsync(int type, nsecs_t period)
{
    if (type == VSYNC_TYPE_SF) {
        ATRACE_NAME("calculateDelayTime");
        ged_vsync_calibration(reinterpret_cast<GED_HANDLE>(mGed), 0, period);
    }
}

VsyncHintApi* createVsyncHint()
{
    return new VsyncHint();
}

}

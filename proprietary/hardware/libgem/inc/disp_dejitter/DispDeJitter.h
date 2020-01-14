#ifndef __ANDROID_SURFACE_FLINGER_DISP_DEJITTER_H__
#define __ANDROID_SURFACE_FLINGER_DISP_DEJITTER_H__

#include <stdint.h>

#include <utils/RefBase.h>
#include <utils/Timers.h>

namespace android {

class GraphicBuffer;

class DispDeJitter {
public:
    DispDeJitter();
    ~DispDeJitter();

    // calculate buffer present time with de-jitter
    // and judge it over expected time or not.
    bool shouldDelayPresent(const sp<GraphicBuffer>& gb, const nsecs_t& expectedPresent);

protected:
    int64_t kalmanFilter(int64_t measuredProcessTime);
    void resetFilter();

    bool mEnable;
    bool mEnableLog;
    int64_t mLastEstimateProcessTime;
    float mLastCovariance;
    uint64_t mLastSourceTimestamp;
};

extern "C" {
    DispDeJitter* createDispDeJitter();
    void destroyDispDeJitter(DispDeJitter* dispDeJitter);
    bool shouldDelayPresent(DispDeJitter* dispDeJitter, const sp<GraphicBuffer>& gb, const nsecs_t& expectedPresent);
    void markTimestamp(const sp<GraphicBuffer>& gb);
}

}   // namespace android

#endif  // __ANDROID_SURFACE_FLINGER_DISP_DEJITTER_H__
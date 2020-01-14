#include "vsync_enhance/DispSyncEnhancement.h"

#include <dlfcn.h>
#include <inttypes.h>
#include <math.h>

#include <log/log.h>
#include <utils/String8.h>

namespace android {

DispSyncEnhancement::DispSyncEnhancement()
    : mVSyncMode(VSYNC_MODE_CALIBRATED_SW_VSYNC)
    , mFps(DS_DEFAULT_FPS)
    , mAppPhase(0)
    , mSfPhase(0)
    , mSetVSyncMode(NULL)
    , mHasAnyEventListeners(NULL)
    , mAddResyncSample(NULL)
    , mAddEventListener(NULL)
    , mRemoveEventListener(NULL)
    , mOnSwVsyncChange(NULL)
#ifdef MTK_DYNAMIC_FPS_FW_SUPPORT
    , mFpsPolicyHandle(NULL)
    , mFpsVsync(NULL)
#endif
{
#ifdef MTK_DYNAMIC_FPS_FW_SUPPORT
    typedef FpsVsyncApi* (*createFpsVsyncPrototype)();
    mFpsPolicyHandle = dlopen("libfpspolicy_fw.so", RTLD_LAZY);
    if (mFpsPolicyHandle) {
        createFpsVsyncPrototype creatPtr = reinterpret_cast<createFpsVsyncPrototype>(dlsym(mFpsPolicyHandle, "createFpsVsyncApi"));
        if (creatPtr) {
            mFpsVsync = creatPtr();
        } else {
            ALOGW("Failed to get function: createFpsVsyncApi");
        }
    } else {
        ALOGW("Failed to load libfpspolicy_fw.so");
    }
#endif
}

DispSyncEnhancement::~DispSyncEnhancement() {
#ifdef MTK_DYNAMIC_FPS_FW_SUPPORT
    if (mFpsPolicyHandle) {
        dlclose(mFpsPolicyHandle);
    }
#endif
}

void DispSyncEnhancement::registerFunction(struct DispSyncEnhancementFunctionList* list) {
    mSetVSyncMode = list->setVSyncMode;
    mHasAnyEventListeners = list->hasAnyEventListeners;
    mAddResyncSample = list->addResyncSample;
    mAddEventListener = list->addEventListener;
    mEnableHardwareVsync = list->enableHardwareVsync;
    mRemoveEventListener = list->removeEventListener;
    mOnSwVsyncChange = list->onSwVsyncChange;
#ifdef MTK_DYNAMIC_FPS_FW_SUPPORT
    if (mFpsVsync != NULL) {
        mFpsVsync->registerSwVsyncChangeCallback(mOnSwVsyncChange);
    }
#endif
}

status_t DispSyncEnhancement::setVSyncMode(int32_t mode, int32_t fps, nsecs_t *period, nsecs_t *phase, nsecs_t *referenceTime) {
    status_t res = NO_ERROR;
    if (mVSyncMode != mode || mFps != fps) {
        if (fps <= 0) {
            fps = DS_DEFAULT_FPS;
        }
        calculateModel(mode, fps, period, phase, referenceTime);
        if (mSetVSyncMode) {
            res = mSetVSyncMode(mode, fps, *period, *phase, *referenceTime);
        }
        mVSyncMode = mode;
        mFps = fps;
    }
    return res;
}

bool DispSyncEnhancement::addPresentFence(bool* res) {
    if (mHasAnyEventListeners == NULL) {
        return false;
    }

    if (mVSyncMode == VSYNC_MODE_PASS_HW_VSYNC) {
        *res =  mHasAnyEventListeners();
        return true;
    } else if  (mVSyncMode == VSYNC_MODE_INTERNAL_SW_VSYNC) {
        *res =  false;
        return true;
    }
    return false;
}

bool DispSyncEnhancement::addResyncSample(bool* res, nsecs_t timestamp, nsecs_t* period, nsecs_t* phase, nsecs_t* referenceTime) {
    if (mAddResyncSample == NULL || mHasAnyEventListeners == NULL) {
        return false;
    }

    if (mVSyncMode == VSYNC_MODE_PASS_HW_VSYNC) {
        calculateHwModel(timestamp, mFps, period, phase, referenceTime);
        mAddResyncSample(*period, *phase, *referenceTime);
        *res =  mHasAnyEventListeners();
        return true;
    } else if (mVSyncMode == VSYNC_MODE_INTERNAL_SW_VSYNC) {
        *res =  false;
        return true;
    }
    return false;
}

bool DispSyncEnhancement::addEventListener(status_t* res, Mutex* mutex, const char* name, nsecs_t phase, DispSync::Callback* callback) {
    if (mHasAnyEventListeners == NULL || mAddEventListener == NULL || mEnableHardwareVsync == NULL) {
        return false;
    }

    bool firstListener = false;
    {
        Mutex::Autolock lock(mutex);
        if (!mHasAnyEventListeners()) {
            firstListener = true;
        }
        *res = mAddEventListener(name, phase, callback);
        storeVsyncSourcePhase(name, phase);
    }
    if (firstListener) {
#ifdef MTK_DYNAMIC_FPS_FW_SUPPORT
        if (mFpsVsync != NULL) {
            mFpsVsync->enableTracker(true);
        }
#endif
        if (mVSyncMode == VSYNC_MODE_PASS_HW_VSYNC) {
            mEnableHardwareVsync();
        }
    }
    return true;
}

bool DispSyncEnhancement::removeEventListener(status_t* res, Mutex* mutex, DispSync::Callback* callback) {
    if (mRemoveEventListener == NULL || mHasAnyEventListeners == NULL) {
        return false;
    }

    {
        Mutex::Autolock lock(mutex);
        *res = mRemoveEventListener(callback);
    }
#ifdef MTK_DYNAMIC_FPS_FW_SUPPORT
    if (!mHasAnyEventListeners() && mFpsVsync != NULL) {
        mFpsVsync->enableTracker(false);
    }
#endif
    return true;
}

bool DispSyncEnhancement::obeyResync() {
    bool res = false;
    if (mVSyncMode == VSYNC_MODE_CALIBRATED_SW_VSYNC || mVSyncMode == VSYNC_MODE_PASS_HW_VSYNC) {
        res = true;
    }
    return res;
}

void DispSyncEnhancement::dump(String8& result) {
    result.appendFormat("DispSync information:\n");
    result.appendFormat("  mAppPhase: %" PRId64 "\n", mAppPhase);
    result.appendFormat("  mSfPhase: %" PRId64 "\n", mSfPhase);
    result.appendFormat("  mVSyncMode: %d\n", mVSyncMode);
    if (mVSyncMode != VSYNC_MODE_CALIBRATED_SW_VSYNC) {
        result.appendFormat("  mFps: %d\n", mFps);
    } else {
        result.appendFormat("\n");
    }
#ifdef MTK_DYNAMIC_FPS_FW_SUPPORT
    result.appendFormat("FpsVsync(%p)\n", mFpsVsync.get());
    if (mFpsVsync != NULL) {
        mFpsVsync->dumpInfo(result);
    }
#endif
}

void DispSyncEnhancement::calculateModel(const int32_t mode, const int32_t fps, nsecs_t* period, nsecs_t* phase, nsecs_t* referenceTime) {
    if (mode == VSYNC_MODE_INTERNAL_SW_VSYNC) {
        nsecs_t newPeriod = 1000000000 / fps;
        nsecs_t prevVsync = computePrevVsync(*period, *phase, *referenceTime);
        double sampleX = cos(0);
        double sampleY = sin(0);
        double scale = 2.0 * M_PI / double(newPeriod);
        nsecs_t newPhase = nsecs_t(atan2(sampleY, sampleX) / scale);
        *period = newPeriod;
        *phase = newPhase;
        *referenceTime = prevVsync;
    }
}

void DispSyncEnhancement::calculateHwModel(nsecs_t timestamp, int32_t fps, nsecs_t* period, nsecs_t* phase, nsecs_t* referenceTime) {
    nsecs_t newPeriod = 1000000000 / fps;
    *referenceTime = timestamp;
    *period = newPeriod;
    *phase = 0;
}

void DispSyncEnhancement::storeVsyncSourcePhase(const char* name, nsecs_t phase) {
    if (!strcmp(name, "app")) {
        mAppPhase = phase;
    } else if (!strcmp(name, "sf")) {
        mSfPhase = phase;
    }
}

nsecs_t DispSyncEnhancement::getAppPhase() const {
    return mAppPhase;
}

nsecs_t DispSyncEnhancement::getSfPhase() const {
    return mSfPhase;
}

nsecs_t DispSyncEnhancement::computePrevVsync(nsecs_t period, nsecs_t phase, nsecs_t referenceTime) {
    nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
    return (((now - referenceTime - phase) / period)) * period + referenceTime + phase;
}

DispSyncEnhancementApi* createDispSyncEnhancement() {
    return new DispSyncEnhancement();
}

}

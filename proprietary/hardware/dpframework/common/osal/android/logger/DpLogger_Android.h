#ifndef __DP_LOGGER_ANDROID_H__
#define __DP_LOGGER_ANDROID_H__

#include <cutils/log.h>

#undef LOG_TAG
#define LOG_TAG "MDP"

#define DPLOGI(...) // (ALOGI(__VA_ARGS__))

#define DPLOGW(...) (ALOGW(__VA_ARGS__))

#define DPLOGD(...) (ALOGD(__VA_ARGS__))

#define DPLOGE(...) (ALOGE(__VA_ARGS__))

#endif  // __DP_LOGGER_ANDROID_H__

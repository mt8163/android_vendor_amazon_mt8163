#define LOG_TAG "android.hardware.power@1.3-service.mt8163"

#include <android-base/logging.h>
#include <hidl/HidlTransportSupport.h>

#include "Power.h"

using android::OK;
using android::sp;
using android::status_t;
using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;

using android::hardware::power::V1_3::IPower;

using android::hardware::power::V1_3::implementation::Power;

int main() {
    configureRpcThreadpool(1, true /*callerWillJoin*/);

    sp<IPower> power = new Power();

    if (power->registerAsService() != android::OK) {
        LOG(ERROR) << "Can't register Power HAL service";
        return 1;
    }

    joinRpcThreadpool();

    return 0;  // should never get here
}

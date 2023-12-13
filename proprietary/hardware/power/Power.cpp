/*
 * Copyright (C) 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <android-base/file.h>

#include "Power.h"
#include "amazon_power.h"

#include <fstream>

namespace android {
namespace hardware {
namespace power {
namespace V1_3 {
namespace implementation {

template <typename T>
static void set(const std::string& path, const T& value) {
    std::ofstream file(path);
    file << value << std::endl;
}

static void setDynamicBoost(const DynamicBoostMode mode, const int32_t durationMs) {
    set(kDynamicBoostPath, std::to_string(durationMs) + " " + std::to_string(static_cast<int32_t>(mode)));
}

// Methods from ::android::hardware::power::V1_0::IPower follow.
Return<void> Power::setInteractive(bool interactive) {
    set(kIoBusyPath, interactive ? "1" : "0");
    return Void();
}

Return<void> Power::powerHint(PowerHint_1_0 hint, int32_t data) {
    switch (hint) {
        case PowerHint_1_0::LOW_POWER:
            if (data) {
                set(kRushBoostPath,  "0");
                set(kFpsUpperBoundPath, "30");
            } else {
                set(kRushBoostPath, "1");
                set(kFpsUpperBoundPath, "60");
            }
            break;
        case PowerHint_1_0::INTERACTION:
            if (data < 1)
                return Void();
            setDynamicBoost(DynamicBoostMode::PRIO_TWO_LITTLES_MAX_FREQ, data);
            break;
        case PowerHint_1_0::LAUNCH:
            if (data > 0)
                setDynamicBoost(DynamicBoostMode::PRIO_MAX_CORES_MAX_FREQ, 5000);
            else
                setDynamicBoost(DynamicBoostMode::PRIO_RESET, 0);
            break;
        default:
            break;
    }
    return Void();
}

Return<void> Power::setFeature(Feature /* feature */, bool /* activate */) {
    return Void();
}

Return<void> Power::getPlatformLowPowerStats(getPlatformLowPowerStats_cb _hidl_cb) {
    _hidl_cb({}, Status::SUCCESS);
    return Void();
}


// Methods from ::android::hardware::power::V1_1::IPower follow.
Return<void> Power::getSubsystemLowPowerStats(getSubsystemLowPowerStats_cb _hidl_cb) {
    _hidl_cb({}, Status::SUCCESS);
    return Void();
}

Return<void> Power::powerHintAsync(PowerHint_1_0 hint, int32_t data) {
    return powerHint(hint, data);
}


// Methods from ::android::hardware::power::V1_2::IPower follow.
Return<void> Power::powerHintAsync_1_2(PowerHint_1_2 hint, int32_t data) {
    return powerHint(static_cast<PowerHint_1_0>(hint), data);
}


// Methods from ::android::hardware::power::V1_3::IPower follow.
Return<void> Power::powerHintAsync_1_3(PowerHint_1_3 hint, int32_t data) {
    return powerHint(static_cast<PowerHint_1_0>(hint), data);
}

}  // namespace implementation
}  // namespace V1_3
}  // namespace power
}  // namespace hardware
}  // namespace android

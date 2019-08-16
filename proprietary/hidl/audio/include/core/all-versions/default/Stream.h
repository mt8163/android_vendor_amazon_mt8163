/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <common/all-versions/IncludeGuard.h>

#include <vector>

#include "../../../audio.h"
#include <hidl/Status.h>

#include <hidl/MQDescriptor.h>

#include <VersionUtils.h>

namespace android {
namespace hardware {
namespace audio {
namespace AUDIO_HAL_VERSION {
namespace implementation {

using ::android::hardware::audio::common::AUDIO_HAL_VERSION::AudioChannelMask;
using ::android::hardware::audio::common::AUDIO_HAL_VERSION::AudioDevice;
using ::android::hardware::audio::common::AUDIO_HAL_VERSION::AudioFormat;
using ::android::hardware::audio::common::AUDIO_HAL_VERSION::implementation::AudioChannelBitfield;
using ::android::hardware::audio::AUDIO_HAL_VERSION::DeviceAddress;
using ::android::hardware::audio::AUDIO_HAL_VERSION::IStream;
using ::android::hardware::audio::AUDIO_HAL_VERSION::ParameterValue;
using ::android::hardware::audio::AUDIO_HAL_VERSION::Result;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::hidl_vec;
using ::android::hardware::hidl_string;
using ::android::sp;

struct Stream : public IStream, public ParametersUtil {
    explicit Stream(audio_stream_t* stream);

    /** 1GiB is the maximum buffer size the HAL client is allowed to request.
     * This value has been chosen to be under SIZE_MAX and still big enough
     * for all audio use case.
     * Keep private for 2.0, put in .hal in 2.1
     */
    static constexpr uint32_t MAX_BUFFER_SIZE = 2 << 30 /* == 1GiB */;

    // Methods from ::android::hardware::audio::AUDIO_HAL_VERSION::IStream follow.
    Return<uint64_t> getFrameSize() override;
    Return<uint64_t> getFrameCount() override;
    Return<uint64_t> getBufferSize() override;
    Return<uint32_t> getSampleRate() override;
#ifdef AUDIO_HAL_VERSION_2_0
    Return<void> getSupportedSampleRates(getSupportedSampleRates_cb _hidl_cb) override;
    Return<void> getSupportedChannelMasks(getSupportedChannelMasks_cb _hidl_cb) override;
#endif
    Return<void> getSupportedSampleRates(AudioFormat format, getSupportedSampleRates_cb _hidl_cb);
    Return<void> getSupportedChannelMasks(AudioFormat format, getSupportedChannelMasks_cb _hidl_cb);
    Return<Result> setSampleRate(uint32_t sampleRateHz) override;
    Return<AudioChannelBitfield> getChannelMask() override;
    Return<Result> setChannelMask(AudioChannelBitfield mask) override;
    Return<AudioFormat> getFormat() override;
    Return<void> getSupportedFormats(getSupportedFormats_cb _hidl_cb) override;
    Return<Result> setFormat(AudioFormat format) override;
    Return<void> getAudioProperties(getAudioProperties_cb _hidl_cb) override;
    Return<Result> addEffect(uint64_t effectId) override;
    Return<Result> removeEffect(uint64_t effectId) override;
    Return<Result> standby() override;
#ifdef AUDIO_HAL_VERSION_2_0
    Return<AudioDevice> getDevice() override;
    Return<Result> setDevice(const DeviceAddress& address) override;
    Return<void> getParameters(const hidl_vec<hidl_string>& keys,
                               getParameters_cb _hidl_cb) override;
    Return<Result> setParameters(const hidl_vec<ParameterValue>& parameters) override;
    Return<Result> setConnectedState(const DeviceAddress& address, bool connected) override;
#elif defined(AUDIO_HAL_VERSION_4_0)
    Return<void> getDevices(getDevices_cb _hidl_cb) override;
    Return<Result> setDevices(const hidl_vec<DeviceAddress>& devices) override;
    Return<void> getParameters(const hidl_vec<ParameterValue>& context,
                               const hidl_vec<hidl_string>& keys,
                               getParameters_cb _hidl_cb) override;
    Return<Result> setParameters(const hidl_vec<ParameterValue>& context,
                                 const hidl_vec<ParameterValue>& parameters) override;
#endif
    Return<Result> setHwAvSync(uint32_t hwAvSync) override;
    Return<Result> start() override;
    Return<Result> stop() override;
    Return<void> createMmapBuffer(int32_t minSizeFrames, createMmapBuffer_cb _hidl_cb) override;
    Return<void> getMmapPosition(getMmapPosition_cb _hidl_cb) override;
    Return<Result> close() override;

    Return<void> debug(const hidl_handle& fd, const hidl_vec<hidl_string>& options) override;
#ifdef AUDIO_HAL_VERSION_2_0
    Return<void> debugDump(const hidl_handle& fd) override;
#endif

    // Utility methods for extending interfaces.
    static Result analyzeStatus(const char* funcName, int status);
    static Result analyzeStatus(const char* funcName, int status,
                                const std::vector<int>& ignoreErrors);

   private:
    audio_stream_t* mStream;

    virtual ~Stream();

    // Methods from ParametersUtil.
    char* halGetParameters(const char* keys) override;
    int halSetParameters(const char* keysAndValues) override;
};

template <typename T>
struct StreamMmap : public RefBase {
    explicit StreamMmap(T* stream) : mStream(stream) {}

    Return<Result> start();
    Return<Result> stop();
    Return<void> createMmapBuffer(int32_t minSizeFrames, size_t frameSize,
                                  IStream::createMmapBuffer_cb _hidl_cb);
    Return<void> getMmapPosition(IStream::getMmapPosition_cb _hidl_cb);

   private:
    StreamMmap() {}

    T* mStream;
};

template <typename T>
Return<Result> StreamMmap<T>::start() {
    return Result::NOT_SUPPORTED;
}

template <typename T>
Return<Result> StreamMmap<T>::stop() {
    return Result::NOT_SUPPORTED;
}

template <typename T>
Return<void> StreamMmap<T>::createMmapBuffer(int32_t minSizeFrames, size_t frameSize,
                                             IStream::createMmapBuffer_cb _hidl_cb) {
    (void)minSizeFrames;
    (void)frameSize;
    Result retval(Result::NOT_SUPPORTED);
    MmapBufferInfo info;
    native_handle_t* hidlHandle = nullptr;
    _hidl_cb(retval, info);
    if (hidlHandle != nullptr) {
        native_handle_delete(hidlHandle);
    }
    return Void();
}

template <typename T>
Return<void> StreamMmap<T>::getMmapPosition(IStream::getMmapPosition_cb _hidl_cb) {
    Result retval(Result::NOT_SUPPORTED);
    MmapPosition position;
    _hidl_cb(retval, position);
    return Void();
}

}  // namespace implementation
}  // namespace AUDIO_HAL_VERSION
}  // namespace audio
}  // namespace hardware
}  // namespace android

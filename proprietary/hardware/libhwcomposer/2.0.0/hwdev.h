#ifndef HWC_HWDEV_H_
#define HWC_HWDEV_H_

#include <ui/Rect.h>
#include <utils/Singleton.h>
#include <utils/threads.h>

#include <linux/disp_session.h>

#ifdef USES_GRALLOC1
#include <hardware/gralloc1.h>
#endif // USES_GRALLOC1

#include "color.h"
#include "display.h"
#include "hwc2_api.h"

#define DISP_NO_PRESENT_FENCE  -1

using namespace android;

struct OverlayPrepareParam;
struct OverlayPortParam;
class MTKM4UDrv;

// ---------------------------------------------------------------------------

class OvlInSizeArbitrator : public Singleton<OvlInSizeArbitrator>
{
public:
    OvlInSizeArbitrator();

    enum CONFIG
    {
        CONFIG_FIXED_HI,
        CONFIG_FIXED_LO,
        CONFIG_ADAPTIVE,
        CONFIG_ADAPTIVE_HILO,
        CONFIG_COUNT,
    };

    struct InputSize
    {
        InputSize()
            : w(0), h(0)
        { }
        int32_t w;
        int32_t h;
    };

    void dump(String8* dump_str);
    void config(int32_t width, int32_t height);
    bool isConfigurationDirty();
    void adjustMdpDstRoi(DispatcherJob* job, sp<HWCDisplay> disp);

private:
    void switchBackHi();
    void switchToLo(int32_t width, int32_t height);
    void getMediumRoi(hwc_frect_t* medium_roi, const InputSize& medium_base, const hwc_rect_t& frame);
    void getFixedMediumRoi(hwc_rect_t* medium_size, hwc_frect_t& f_medium_size);

    int32_t m_disp_w;

    int32_t m_disp_h;

    uint32_t m_present_config;

    // for checking config dirty
    uint32_t m_prior_config;

    // is resolution set to lo
    bool m_is_lo;

    // for each configuration, there a resolution list used to do arbitration
    KeyedVector<int, InputSize> m_input_size[CONFIG_COUNT];
};

// ---------------------------------------------------------------------------

DISP_FORMAT mapDispInFormat(unsigned int format, int mode = HWC2_BLEND_MODE_NONE);

// MAX_DIRTY_RECT_CNT hwc supports
enum
{
    MAX_DIRTY_RECT_CNT = 10,
};

enum
{
    OVL_DEVICE_TYPE_INVALID,
    OVL_DEVICE_TYPE_OVL,
    OVL_DEVICE_TYPE_BLITDEV
};

class IOverlayDevice: public RefBase
{
public:
    virtual ~IOverlayDevice() {}

    virtual int32_t getType() = 0;

    // initOverlay() initializes overlay related hw setting
    virtual void initOverlay() = 0;

    // getDisplayRotation gets LCM's degree
    virtual uint32_t getDisplayRotation(uint32_t dpy) = 0;

    // get supported resolutions
    virtual void getSupportedResolution(KeyedVector<int, OvlInSizeArbitrator::InputSize>& config) = 0;

    // isDispRszSupported() is used to query if display rsz is supported
    virtual bool isDispRszSupported() = 0;

    // isDispRszSupported() is used to query if display rsz is supported
    virtual bool isDispRpoSupported() = 0;

    // isPartialUpdateSupported() is used to query if PartialUpdate is supported
    virtual bool isPartialUpdateSupported() = 0;

    // isFenceWaitSupported() is used to query if FenceWait is supported
    virtual bool isFenceWaitSupported() = 0;

    // isConstantAlphaForRGBASupported() is used to query if PRGBA is supported
    virtual bool isConstantAlphaForRGBASupported() = 0;

    // isDispSelfRefreshSupported is used to query if hardware support ioctl of self-refresh
    virtual bool isDispSelfRefreshSupported() = 0;

    // getMaxOverlayInputNum() gets overlay supported input amount
    virtual int getMaxOverlayInputNum() = 0;

    // getMaxOverlayHeight() gets overlay supported height amount
    virtual uint32_t getMaxOverlayHeight() = 0;

    // getMaxOverlayWidth() gets overlay supported width amount
    virtual uint32_t getMaxOverlayWidth() = 0;

    // createOverlaySession() creates overlay composition session
    virtual status_t createOverlaySession(
        int dpy, DISP_MODE mode = DISP_SESSION_DIRECT_LINK_MODE) = 0;

    // destroyOverlaySession() destroys overlay composition session
    virtual void destroyOverlaySession(int dpy) = 0;

    // truggerOverlaySession() used to trigger overlay engine to do composition
    virtual status_t triggerOverlaySession(int dpy, int present_fence_idx, int ovlp_layer_num,
                                   int prev_present_fence_fd,
                                   EXTD_TRIGGER_MODE trigger_mode = TRIGGER_NORMAL) = 0;

    // disableOverlaySession() usd to disable overlay session to do composition
    virtual void disableOverlaySession(int dpy,  OverlayPortParam* const* params, int num) = 0;

    // setOverlaySessionMode() sets the overlay session mode
    virtual status_t setOverlaySessionMode(int dpy, DISP_MODE mode) = 0;

    // getOverlaySessionMode() gets the overlay session mode
    virtual DISP_MODE getOverlaySessionMode(int dpy) = 0;

    // getOverlaySessionInfo() gets specific display device information
    virtual status_t getOverlaySessionInfo(int dpy, disp_session_info* info) = 0;

    // getAvailableOverlayInput gets available amount of overlay input
    // for different session
    virtual int getAvailableOverlayInput(int dpy) = 0;

    // prepareOverlayInput() gets timeline index and fence fd of overlay input layer
    virtual void prepareOverlayInput(int dpy, OverlayPrepareParam* param) = 0;

    // updateOverlayInputs() updates multiple overlay input layers
    virtual void updateOverlayInputs(int dpy, OverlayPortParam* const* params, int num, sp<ColorTransform> color_transform) = 0;

    // prepareOverlayOutput() gets timeline index and fence fd for overlay output buffer
    virtual void prepareOverlayOutput(int dpy, OverlayPrepareParam* param) = 0;

    // disableOverlayOutput() disables overlay output buffer
    virtual void disableOverlayOutput(int dpy) = 0;

    // enableOverlayOutput() enables overlay output buffer
    virtual void enableOverlayOutput(int dpy, OverlayPortParam* param) = 0;

    // prepareOverlayPresentFence() gets present timeline index and fence
    virtual void prepareOverlayPresentFence(int dpy, OverlayPrepareParam* param) = 0;

    // waitVSync() is used to wait vsync signal for specific display device
    virtual status_t waitVSync(int dpy, nsecs_t *ts) = 0;

    // setPowerMode() is used to switch power setting for display
    virtual void setPowerMode(int dpy, int mode) = 0;

    virtual disp_caps_info* getCapsInfo() = 0;

    // to query valid layers which can handled by OVL
    virtual bool queryValidLayer(disp_layer_info* disp_layer) = 0;

    // waitAllJobDone() use to wait driver for processing all job
    virtual status_t waitAllJobDone(const int dpy) = 0;

    // setLastValidColorTransform is used to save last valid color matrix
    virtual void setLastValidColorTransform(const int32_t& dpy, sp<ColorTransform> color_transform) = 0;
};

class DispDevice : public IOverlayDevice, public Singleton<DispDevice>
{
public:
    DispDevice();
    ~DispDevice();

    int32_t getType() { return OVL_DEVICE_TYPE_OVL; }

    // initOverlay() initializes overlay related hw setting
    void initOverlay();

    // getDisplayRotation gets LCM's degree
    uint32_t getDisplayRotation(uint32_t dpy);

    // get supported resolutions
    void getSupportedResolution(KeyedVector<int, OvlInSizeArbitrator::InputSize>& config);

    // isDispRszSupported() is used to query if display rsz is supported
    bool isDispRszSupported();

    // isDispRszSupported() is used to query if display rsz is supported
    bool isDispRpoSupported();

    // isPartialUpdateSupported() is used to query if PartialUpdate is supported
    bool isPartialUpdateSupported();

    // isFenceWaitSupported() is used to query if FenceWait is supported
    bool isFenceWaitSupported();

    // isConstantAlphaForRGBASupported is used to query if hardware support constant alpha for RGBA
    bool isConstantAlphaForRGBASupported();

    // isDispSelfRefreshSupported is used to query if hardware support ioctl of self-refresh
    bool isDispSelfRefreshSupported();

    // getMaxOverlayInputNum() gets overlay supported input amount
    int getMaxOverlayInputNum();

    // getMaxOverlayHeight() gets overlay supported height amount
    uint32_t getMaxOverlayHeight();

    // getMaxOverlayWidth() gets overlay supported width amount
    uint32_t getMaxOverlayWidth();

    // createOverlaySession() creates overlay composition session
    status_t createOverlaySession(
        int dpy, DISP_MODE mode = DISP_SESSION_DIRECT_LINK_MODE);

    // destroyOverlaySession() destroys overlay composition session
    void destroyOverlaySession(int dpy);

    // truggerOverlaySession() used to trigger overlay engine to do composition
    status_t triggerOverlaySession(int dpy, int present_fence_idx, int ovlp_layer_num,
                                   int prev_present_fence_fd,
                                   EXTD_TRIGGER_MODE trigger_mode = TRIGGER_NORMAL);

    // disableOverlaySession() usd to disable overlay session to do composition
    void disableOverlaySession(int dpy,  OverlayPortParam* const* params, int num);

    // setOverlaySessionMode() sets the overlay session mode
    status_t setOverlaySessionMode(int dpy, DISP_MODE mode);

    // getOverlaySessionMode() gets the overlay session mode
    DISP_MODE getOverlaySessionMode(int dpy);

    // getOverlaySessionInfo() gets specific display device information
    status_t getOverlaySessionInfo(int dpy, disp_session_info* info);

    // getAvailableOverlayInput gets available amount of overlay input
    // for different session
    int getAvailableOverlayInput(int dpy);

    // prepareOverlayInput() gets timeline index and fence fd of overlay input layer
    void prepareOverlayInput(int dpy, OverlayPrepareParam* param);

    // updateOverlayInputs() updates multiple overlay input layers
    void updateOverlayInputs(int dpy, OverlayPortParam* const* params, int num, sp<ColorTransform> color_transform);

    // prepareOverlayOutput() gets timeline index and fence fd for overlay output buffer
    void prepareOverlayOutput(int dpy, OverlayPrepareParam* param);

    // disableOverlayOutput() disables overlay output buffer
    void disableOverlayOutput(int dpy);

    // enableOverlayOutput() enables overlay output buffer
    void enableOverlayOutput(int dpy, OverlayPortParam* param);

    // prepareOverlayPresentFence() gets present timeline index and fence
    void prepareOverlayPresentFence(int dpy, OverlayPrepareParam* param);

    // waitVSync() is used to wait vsync signal for specific display device
    status_t waitVSync(int dpy, nsecs_t *ts);

    // setPowerMode() is used to switch power setting for display
    void setPowerMode(int dpy, int mode);

    inline disp_caps_info* getCapsInfo() { return &m_caps_info; }

    // to query valid layers which can handled by OVL
    bool queryValidLayer(disp_layer_info* disp_layer);

    // waitAllJobDone() use to wait driver for processing all job
    status_t waitAllJobDone(const int dpy);

    // waitRefreshRequest() is used to wait for refresh request from driver
    status_t waitRefreshRequest(unsigned int* type);

    // setLastValidColorTransform is used to save last valid color matrix
    void setLastValidColorTransform(const int32_t& dpy, sp<ColorTransform> color_transform);
private:

    // for lagacy driver API
    status_t legacySetInputBuffer(int dpy);
    status_t legacySetOutputBuffer(int dpy);
    status_t legacyTriggerSession(int dpy, int present_fence_idx);

    // for new driver API from MT6755
    status_t frameConfig(int dpy, int present_fence_idx, int ovlp_layer_num,
                         int prev_present_fence_fd,
                         EXTD_TRIGGER_MODE trigger_mode);

    // query hw capabilities through ioctl and store in m_caps_info
    status_t queryCapsInfo();

    // get the correct device id for extension display when enable dual display
    unsigned int getDeviceId(int dpy);

    enum
    {
        DISP_INVALID_SESSION = -1,
    };

    int m_dev_fd;

    int m_ovl_input_num;

    disp_frame_cfg_t m_frame_cfg[DisplayManager::MAX_DISPLAYS];

    disp_session_input_config m_input_config[DisplayManager::MAX_DISPLAYS];

    disp_session_output_config m_output_config[DisplayManager::MAX_DISPLAYS];

    disp_caps_info m_caps_info;

    layer_config* m_layer_config_list[DisplayManager::MAX_DISPLAYS];

    int m_layer_config_len[DisplayManager::MAX_DISPLAYS];

    layer_dirty_roi** m_hwdev_dirty_rect[DisplayManager::MAX_DISPLAYS];

    struct DispColorTransformInfo
    {
        sp<ColorTransform> last_valid_color_transform;
        bool prev_enable_ccorr;
        bool resend_color_transform;
    };

    DispColorTransformInfo m_color_transform_info[DisplayManager::MAX_DISPLAYS];
};

// --------------------------------------------------------------------------

#ifdef USES_GRALLOC1
class GrallocDevice : public Singleton<GrallocDevice>
{
public:
    GrallocDevice();
    ~GrallocDevice();

    template<typename T>
    void initDispatch(gralloc1_function_descriptor_t desc, T* outPfn);
    void initDispatch();

    struct AllocParam
    {
        AllocParam()
            : width(0), height(0), format(0)
            , usage(0), handle(NULL), stride(0)
        { }

        unsigned int width;
        unsigned int height;
        int format;
        int usage;

        buffer_handle_t handle;
        int stride;
    };

    // allocate memory by gralloc driver
    status_t alloc(AllocParam& param);

    // free a previously allocated buffer
    status_t free(buffer_handle_t handle);

    status_t createDescriptor(const AllocParam& param,
    gralloc1_buffer_descriptor_t* outDescriptor);

    // dump information of allocated buffers
    void dump() const;

private:
    gralloc1_device_t* m_dev;
    struct GrallocPfn{
        GRALLOC1_PFN_DUMP dump;
        GRALLOC1_PFN_CREATE_DESCRIPTOR createDescriptor;
        GRALLOC1_PFN_DESTROY_DESCRIPTOR destroyDescriptor;
        GRALLOC1_PFN_SET_DIMENSIONS setDimensions;
        GRALLOC1_PFN_SET_FORMAT setFormat;
        GRALLOC1_PFN_SET_LAYER_COUNT setLayerCount;
        GRALLOC1_PFN_SET_CONSUMER_USAGE setConsumerUsage;
        GRALLOC1_PFN_SET_PRODUCER_USAGE setProducerUsage;
        GRALLOC1_PFN_ALLOCATE allocate;
        GRALLOC1_PFN_RELEASE release;
        GRALLOC1_PFN_LOCK lock;
        GRALLOC1_PFN_UNLOCK unlock;

        GrallocPfn()
            : dump(nullptr)
            , createDescriptor(nullptr)
            , destroyDescriptor(nullptr)
            , setDimensions(nullptr)
            , setFormat(nullptr)
            , setLayerCount(nullptr)
            , setConsumerUsage(nullptr)
            , setProducerUsage(nullptr)
            , allocate(nullptr)
            , release(nullptr)
            , lock(nullptr)
            , unlock(nullptr)
        {}
    } m_dispatch;
};

#else // USES_GRALLOC1

class GrallocDevice : public Singleton<GrallocDevice>
{
public:
    GrallocDevice();
    ~GrallocDevice();

    struct AllocParam
    {
        AllocParam()
            : width(0), height(0), format(0)
            , usage(0), handle(NULL), stride(0)
        { }

        unsigned int width;
        unsigned int height;
        int format;
        int usage;

        buffer_handle_t handle;
        int stride;
    };

    // allocate memory by gralloc driver
    status_t alloc(AllocParam& param);

    // free a previously allocated buffer
    status_t free(buffer_handle_t handle);

    // dump information of allocated buffers
    void dump() const;

private:
    alloc_device_t* m_dev;
};
#endif // USES_GRALLOC1

#endif // HWC_HWDEV_H_

#define DEBUG_LOG_TAG "DEV"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS

#include <cutils/properties.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <sys/ioctl.h>

#include <linux/fb.h>
#include <linux/mtkfb.h>

#include "m4u_lib.h"

#include "ddp_ovl.h"

#include "hwc_priv.h"

#include <hardware/gralloc.h>

#include "utils/debug.h"
#include "utils/tools.h"
#include <utils/Trace.h>
#include "hwdev.h"
#include "display.h"
#include "overlay.h"
#include "platform.h"
#include "hwc2.h"
#include "dispatcher.h"
#include "sync.h"
#ifdef USE_SWWATCHDOG
#include "ui_ext/SWWatchDog.h"

#define WDT_IOCTL(fd, CMD, ...)                                                             \
({                                                                                          \
    int err = 0;                                                                            \
    if (Platform::getInstance().m_config.wdt_ioctl)                                         \
    {                                                                                       \
        ATRACE_NAME(#CMD);                                                                  \
        SWWatchDog::AutoWDT _wdt(String8::format("[DEV] ioctl(" #CMD "):%d", __LINE__), 500);   \
        err = ioctl(fd, CMD, ##__VA_ARGS__);                                                \
    }                                                                                       \
    else                                                                                    \
    {                                                                                       \
        SWWatchDog::AutoWDT _wdt(String8::format("[DEV] ioctl(" #CMD "):%d", __LINE__), 500);   \
        err = ioctl(fd, CMD, ##__VA_ARGS__);                                                \
    }                                                                                       \
    err;                                                                                    \
})
#else // USE_SWWATCHDOG
#define WDT_IOCTL(fd, CMD, ...)                                                             \
({                                                                                          \
    int err = 0;                                                                            \
    if (Platform::getInstance().m_config.wdt_ioctl)                                         \
    {                                                                                       \
        ATRACE_NAME(#CMD);                                                                  \
        err = ioctl(fd, CMD, ##__VA_ARGS__);                                                \
    }                                                                                       \
    else                                                                                    \
    {                                                                                       \
        err = ioctl(fd, CMD, ##__VA_ARGS__);                                                \
    }                                                                                       \
    err;                                                                                    \
})
#endif // USE_SWWATCHDOG

#define HWC_ATRACE_BUFFER_INFO(string, n1, n2, n3, n4)                       \
    if (ATRACE_ENABLED()) {                                                   \
        char ___traceBuf[1024];                                               \
        snprintf(___traceBuf, 1024, "%s(%d:%d): %u %d", (string),             \
            (n1), (n2), (n3), (n4));                                          \
        android::ScopedTrace ___bufTracer(ATRACE_TAG, ___traceBuf);           \
    }

// ---------------------------------------------------------------------------

#define DLOGD(i, x, ...) HWC_LOGD("(%d) " x " id:%x", i, ##__VA_ARGS__, m_frame_cfg[i].session_id)
#define DLOGI(i, x, ...) HWC_LOGI("(%d) " x " id:%x", i, ##__VA_ARGS__, m_frame_cfg[i].session_id)
#define DLOGW(i, x, ...) HWC_LOGW("(%d) " x " id:%x", i, ##__VA_ARGS__, m_frame_cfg[i].session_id)
#define DLOGE(i, x, ...) HWC_LOGE("(%d) " x " id:%x", i, ##__VA_ARGS__, m_frame_cfg[i].session_id)

#define IOLOGE(i, err, x, ...) HWC_LOGE("(%d) " x " id:%x err:%d, %s", i, ##__VA_ARGS__, m_frame_cfg[i].session_id, err, strerror(err))

DISP_SESSION_TYPE g_session_type[DisplayManager::MAX_DISPLAYS] = {
    DISP_SESSION_PRIMARY,
    DISP_SESSION_EXTERNAL,
    DISP_SESSION_MEMORY
};

enum {
    EXT_DEVICE_MHL = 1,
    EXT_DEVICE_EPD = 2,
    EXT_DEVICE_LCM = 3,
};

static int g_ext_device_id[] = {
    EXT_DEVICE_MHL,
    EXT_DEVICE_EPD,
    EXT_DEVICE_LCM,
};

DISP_FORMAT mapDispOutFormat(unsigned int format)
{
    switch (format)
    {
        case HAL_PIXEL_FORMAT_RGBA_8888:
            return DISP_FORMAT_RGBA8888;

        case HAL_PIXEL_FORMAT_YV12:
            return DISP_FORMAT_YV12;

        case HAL_PIXEL_FORMAT_RGBX_8888:
            return DISP_FORMAT_RGBX8888;

        case HAL_PIXEL_FORMAT_BGRA_8888:
            return DISP_FORMAT_BGRA8888;

        case HAL_PIXEL_FORMAT_BGRX_8888:
        case HAL_PIXEL_FORMAT_IMG1_BGRX_8888:
            return DISP_FORMAT_BGRX8888;

        case HAL_PIXEL_FORMAT_RGB_888:
            return DISP_FORMAT_RGB888;

        case HAL_PIXEL_FORMAT_RGB_565:
            return DISP_FORMAT_RGB565;

        case HAL_PIXEL_FORMAT_I420:
        case HAL_PIXEL_FORMAT_NV12_BLK:
        case HAL_PIXEL_FORMAT_NV12_BLK_FCM:
        case HAL_PIXEL_FORMAT_YUV_PRIVATE:
        case HAL_PIXEL_FORMAT_YUYV:
        case HAL_PIXEL_FORMAT_YCbCr_420_888:
            return DISP_FORMAT_YUV422;

        case HAL_PIXEL_FORMAT_RGBA_1010102:
            return DISP_FORMAT_RGBA1010102;

        case HAL_PIXEL_FORMAT_RGBA_FP16:
            return DISP_FORMAT_RGBA_FP16;
    }
    HWC_LOGW("Not support output format(%d), use default RGBA8888", format);
    return DISP_FORMAT_ABGR8888;
}

DISP_YUV_RANGE_ENUM mapDispColorRange(uint32_t range, int format)
{
    switch (format)
    {
        case HAL_PIXEL_FORMAT_I420:
        case HAL_PIXEL_FORMAT_YV12:
        case HAL_PIXEL_FORMAT_NV12_BLK:
        case HAL_PIXEL_FORMAT_NV12_BLK_FCM:
        case HAL_PIXEL_FORMAT_YUV_PRIVATE:
        case HAL_PIXEL_FORMAT_YUYV:
        case HAL_PIXEL_FORMAT_UFO:
        case HAL_PIXEL_FORMAT_YCbCr_422_I:
            switch (range)
            {
                case GRALLOC_EXTRA_BIT_YUV_BT601_NARROW:
                    return DISP_YUV_BT601;

                case GRALLOC_EXTRA_BIT_YUV_BT601_FULL:
                    return DISP_YUV_BT601_FULL;

                case GRALLOC_EXTRA_BIT_YUV_BT709_NARROW:
                case GRALLOC_EXTRA_BIT_YUV_BT2020_NARROW:
                case GRALLOC_EXTRA_BIT_YUV_BT2020_FULL:
                    return DISP_YUV_BT709;
            }
            HWC_LOGW("Not support range(%#x), use default BT601", range);
            break;
    }
    return DISP_YUV_BT601;
}

// ---------------------------------------------------------------------------
ANDROID_SINGLETON_STATIC_INSTANCE(OvlInSizeArbitrator);

OvlInSizeArbitrator::OvlInSizeArbitrator()
    : m_present_config(CONFIG_FIXED_HI)
    , m_prior_config(CONFIG_FIXED_HI)
    , m_is_lo(false)
{
    m_disp_w = DisplayManager::getInstance().m_data[0].width;
    m_disp_h = DisplayManager::getInstance().m_data[0].height;

    DispDevice::getInstance().getSupportedResolution(m_input_size[CONFIG_ADAPTIVE]);

    switchBackHi();
}

void OvlInSizeArbitrator::dump(String8* dump_str)
{
    for (int i = 0; i < CONFIG_COUNT; i++)
    {
        if (0 != m_input_size[i].size())
        {
            dump_str->appendFormat("\n  %d", i);
            for (size_t j = 0; j < m_input_size[i].size(); j++)
            {
                InputSize size = m_input_size[i].valueAt(j);
                dump_str->appendFormat(" (%d, %d)", size.w, size.h);
            }
        }
    }
    dump_str->appendFormat("\n\n");
}

void OvlInSizeArbitrator::switchBackHi()
{
    m_input_size[CONFIG_FIXED_LO].clear();
    m_input_size[CONFIG_ADAPTIVE_HILO].clear();
    m_is_lo = false;
}

void OvlInSizeArbitrator::switchToLo(int32_t width, int32_t height)
{
    DisplayData& data = DisplayManager::getInstance().m_data[0];
    InputSize display_size;
    display_size.w = data.width;
    display_size.h = data.height;

    InputSize low_resolution;
    low_resolution.w = width;
    low_resolution.h = height;

    m_input_size[CONFIG_FIXED_LO].clear();
    m_input_size[CONFIG_FIXED_LO].add(-low_resolution.w, low_resolution);

    m_input_size[CONFIG_ADAPTIVE_HILO].clear();
    m_input_size[CONFIG_ADAPTIVE_HILO].add(-display_size.w, display_size);
    m_input_size[CONFIG_ADAPTIVE_HILO].add(-low_resolution.w, low_resolution);

    m_is_lo = true;
}

void OvlInSizeArbitrator::config(int32_t width, int32_t height)
{
    if (width == DisplayManager::getInstance().m_data[0].width &&
        height == DisplayManager::getInstance().m_data[0].height)
    {
        switchBackHi();
    }
    else
    {
        switchToLo(width, height);
    }
}

bool OvlInSizeArbitrator::isConfigurationDirty()
{
    return (m_prior_config != m_present_config);
}

void OvlInSizeArbitrator::getMediumRoi(
    hwc_frect_t* f_medium_roi, const InputSize& medium_base, const hwc_rect_t& frame)
{
    float disp_w = (float)m_disp_w;
    float disp_h = (float)m_disp_h;
    float dst_base_w = medium_base.w;
    float dst_base_h = medium_base.h;

    f_medium_roi->left   = (float)frame.left / disp_w * dst_base_w;
    f_medium_roi->top    = (float)frame.top / disp_h * dst_base_h;
    f_medium_roi->right  = dst_base_w - ((disp_w - (float)frame.right) / disp_w * dst_base_w);
    f_medium_roi->bottom = dst_base_h - ((disp_h - (float)frame.bottom) / disp_h * dst_base_h);
}

void OvlInSizeArbitrator::getFixedMediumRoi(hwc_rect_t* medium_roi, hwc_frect_t& f_medium_roi)
{
    medium_roi->left = f_medium_roi.left;
    medium_roi->top  = f_medium_roi.top;
    medium_roi->right = medium_roi->left + floorf(f_medium_roi.right - f_medium_roi.left + 0.5);
    medium_roi->bottom = medium_roi->top + floorf(f_medium_roi.bottom - f_medium_roi.top + 0.5);
}

void OvlInSizeArbitrator::adjustMdpDstRoi(DispatcherJob* job, sp<HWCDisplay> disp)
{
    // skip secondary display
    if (HWC_DISPLAY_PRIMARY != job->disp_ori_id)
        return;

    const int layers_num = disp->getVisibleLayersSortedByZ().size();

    m_prior_config = m_present_config;

    if (m_is_lo)
    {
        // avoid MM layer to use RSZ, RSZ should be used on UI layers.
        if (1 != layers_num)
            m_present_config = CONFIG_ADAPTIVE_HILO;
    }
    else
    {
        // at most 2 UI layers can be composed with adjusted MM layer
        if (layers_num > 3)
            m_present_config = CONFIG_FIXED_HI;
    }

    DbgLogger logger(DbgLogger::TYPE_DUMPSYS, 'I');
    logger.printf("MDPTGT cfg:%d lo:%d", m_present_config, m_is_lo);

    KeyedVector<int, InputSize>& configs = m_input_size[m_present_config];

    // default behavior, return directly, no need to adjust
    if (0 == configs.size())
        return;

    for (int i = 0; i < layers_num; ++i)
    {
        sp<HWCLayer> layer = disp->getVisibleLayersSortedByZ()[i];
        if ((HWC_LAYER_TYPE_MM != layer->getHwlayerType()) &&
            (HWC_LAYER_TYPE_MM_HIGH != layer->getHwlayerType()))
            continue;

        // skip secure layer, avoid additional risk
        if (isSecure(&layer->getPrivateHandle()))
            continue;

        hwc_frect_t medium_roi;
        uint32_t selected_base_id = 0;
        if ((configs.valueAt(0).w != m_disp_w) || (configs.valueAt(0).h != m_disp_h))
        {
            getMediumRoi(&medium_roi, configs.valueAt(0), layer->getDisplayFrame());
            getFixedMediumRoi(&layer->editMdpDstRoi(), medium_roi);
        }
        // else, because mdp_dst_roi is set with displayFrame defaultly, no need to set again

        for (size_t j = 1; j < configs.size(); j++)
        {
            getMediumRoi(&medium_roi, configs.valueAt(j), layer->getDisplayFrame());

            uint32_t transform = layer->getTransform();
            rectifyXformWithPrexform(&transform, layer->getPrivateHandle().prexform);

            float src_width = 0.0f;
            float src_height = 0.0f;

            if (0 == (transform & HAL_TRANSFORM_ROT_90))
            {
                src_width = layer->getSourceCrop().right - layer->getSourceCrop().left;
                src_height = layer->getSourceCrop().bottom - layer->getSourceCrop().top;
            }
            else
            {
                src_width = layer->getSourceCrop().bottom - layer->getSourceCrop().top;
                src_height = layer->getSourceCrop().right - layer->getSourceCrop().left;
            }

            const float dst_width = medium_roi.right - medium_roi.left;
            const float dst_height = medium_roi.bottom - medium_roi.top;

            const float tolerance = 0.95;
            if (dst_width <= (src_width * tolerance) || dst_height <= (src_height * tolerance))
                break;

            getFixedMediumRoi(&layer->editMdpDstRoi(), medium_roi);
            selected_base_id = j;
        }

        logger.printf(" (L%d)[%d, %d, %d, %d]/[%d, %d]",
                        i, layer->getMdpDstRoi().left, layer->getMdpDstRoi().top,
                        layer->getMdpDstRoi().right, layer->getMdpDstRoi().bottom,
                        configs.valueAt(selected_base_id).w, configs.valueAt(selected_base_id).h);
        return;
    }
}

// ---------------------------------------------------------------------------

DISP_FORMAT mapDispInFormat(unsigned int format, int mode)
{
    if (!DispDevice::getInstance().isConstantAlphaForRGBASupported())
    {
        mode = HWC2_BLEND_MODE_NONE;
    }

    switch (format)
    {
        case HAL_PIXEL_FORMAT_RGBA_8888:
            return (HWC2_BLEND_MODE_PREMULTIPLIED == mode) ? DISP_FORMAT_PRGBA8888 : DISP_FORMAT_RGBA8888;

        case HAL_PIXEL_FORMAT_RGBX_8888:
            return DISP_FORMAT_RGBX8888;

        case HAL_PIXEL_FORMAT_BGRA_8888:
            return (HWC2_BLEND_MODE_PREMULTIPLIED == mode) ? DISP_FORMAT_PBGRA8888 : DISP_FORMAT_BGRA8888;

        case HAL_PIXEL_FORMAT_BGRX_8888:
        case HAL_PIXEL_FORMAT_IMG1_BGRX_8888:
            return DISP_FORMAT_BGRX8888;

        case HAL_PIXEL_FORMAT_RGB_888:
            return DISP_FORMAT_RGB888;

        case HAL_PIXEL_FORMAT_RGB_565:
            return DISP_FORMAT_RGB565;

        case HAL_PIXEL_FORMAT_I420:
        case HAL_PIXEL_FORMAT_YV12:
        case HAL_PIXEL_FORMAT_NV12_BLK:
        case HAL_PIXEL_FORMAT_NV12_BLK_FCM:
        case HAL_PIXEL_FORMAT_YUV_PRIVATE:
        case HAL_PIXEL_FORMAT_YCbCr_422_I:
        case HAL_PIXEL_FORMAT_YUYV:
        case HAL_PIXEL_FORMAT_UFO:
        case HAL_PIXEL_FORMAT_YCbCr_420_888:
        case HAL_PIXEL_FORMAT_YUV_PRIVATE_10BIT:
        case HAL_PIXEL_FORMAT_NV12_BLK_10BIT_H:
        case HAL_PIXEL_FORMAT_NV12_BLK_10BIT_V:
        case HAL_PIXEL_FORMAT_UFO_10BIT_H:
        case HAL_PIXEL_FORMAT_UFO_10BIT_V:
        case HAL_PIXEL_FORMAT_YCRCB_420_SP:
            return DISP_FORMAT_YUV422;

        case HAL_PIXEL_FORMAT_RGBA_1010102:
            return DISP_FORMAT_RGBA1010102;

        case HAL_PIXEL_FORMAT_RGBA_FP16:
            return DISP_FORMAT_RGBA_FP16;

        case HAL_PIXEL_FORMAT_DIM:
            return DISP_FORMAT_DIM;
    }
    HWC_LOGW("Not support input format(%d), use default RGBA8888", format);
    return (HWC2_BLEND_MODE_PREMULTIPLIED == mode) ? DISP_FORMAT_PABGR8888 : DISP_FORMAT_ABGR8888;
}

int32_t mapBlending(const int32_t blending)
{
    switch (blending)
    {
        case HWC2_BLEND_MODE_INVALID:
        case HWC2_BLEND_MODE_NONE:
            return HWC_BLENDING_NONE;

        case HWC2_BLEND_MODE_PREMULTIPLIED:
            return HWC_BLENDING_NONE;

        case HWC2_BLEND_MODE_COVERAGE:
            return HWC_BLENDING_NONE;

        default:
            HWC_LOGE("%s: Unknown blending mode(%d)", __func__, blending);
            return HWC_BLENDING_PREMULT;
    }
    return HWC_BLENDING_NONE;
}

ANDROID_SINGLETON_STATIC_INSTANCE(DispDevice);

DispDevice::DispDevice()
{
    char filename[256];
    sprintf(filename, "/dev/%s", DISP_SESSION_DEVICE);
    m_dev_fd = open(filename, O_RDONLY);
    if (m_dev_fd <= 0)
    {
        HWC_LOGE("Failed to open display device: %s ", strerror(errno));
        abort();
    }

    // query hw capibilities and set to m_caps_info
    if (NO_ERROR != queryCapsInfo())
    {
        abort();
    }

    m_ovl_input_num = getMaxOverlayInputNum();

    memset(m_frame_cfg, 0, sizeof(disp_frame_cfg_t) * DisplayManager::MAX_DISPLAYS);
    memset(m_color_transform_info, 0, sizeof(DispColorTransformInfo) * DisplayManager::MAX_DISPLAYS);

    for (int i = 0; i < DisplayManager::MAX_DISPLAYS; i++)
    {
        m_frame_cfg[i].session_id = DISP_INVALID_SESSION;
        m_frame_cfg[i].mode = DISP_INVALID_SESSION_MODE;

        // partial update - allocate dirty rect structures for ioctl to disp driver
        m_hwdev_dirty_rect[i] = (layer_dirty_roi**)malloc(sizeof(layer_dirty_roi*) * m_ovl_input_num);
        LOG_ALWAYS_FATAL_IF(m_hwdev_dirty_rect[i] == nullptr, "DispDevice() malloc(%zu) fail",
            sizeof(layer_dirty_roi*) * m_ovl_input_num);
        for (int j = 0; j < m_ovl_input_num; j++)
        {
            m_hwdev_dirty_rect[i][j] = (layer_dirty_roi*)malloc(sizeof(layer_dirty_roi) * MAX_DIRTY_RECT_CNT);
            LOG_ALWAYS_FATAL_IF(m_hwdev_dirty_rect[i][j] == nullptr, "DispDevice() malloc(%zu) fail",
                sizeof(layer_dirty_roi) * MAX_DIRTY_RECT_CNT);
        }
    }

    memset(m_layer_config_list, 0, sizeof(layer_config*) * DisplayManager::MAX_DISPLAYS);
    memset(m_layer_config_len, 0, sizeof(int) * DisplayManager::MAX_DISPLAYS);
    memset(m_input_config, 0, sizeof(disp_session_input_config) * DisplayManager::MAX_DISPLAYS);
    memset(m_output_config, 0, sizeof(disp_session_output_config) * DisplayManager::MAX_DISPLAYS);
}

DispDevice::~DispDevice()
{
    // partial update - free dirty rect structures
    for (int i = 0; i < DisplayManager::MAX_DISPLAYS; i++)
    {
        for (int j = 0; j < m_ovl_input_num; j++)
        {
            free(m_hwdev_dirty_rect[i][j]);
        }
        free(m_hwdev_dirty_rect[i]);
        if (NULL != m_layer_config_list[i])
        {
            free(m_layer_config_list[i]);
        }
    }

    protectedClose(m_dev_fd);
}

void DispDevice::initOverlay()
{
    Platform::getInstance().initOverlay();
}

status_t DispDevice::queryCapsInfo()
{
    memset(&m_caps_info, 0, sizeof(disp_caps_info));

    // query device Capabilities
    int err = WDT_IOCTL(m_dev_fd, DISP_IOCTL_GET_DISPLAY_CAPS, &m_caps_info);
    if (err < 0)
    {
        IOLOGE(0, err, "DISP_IOCTL_GET_DISPLAY_CAPS");
        return err;
    }

    HWC_LOGD("CapsInfo [%d] lcm_degree",                 m_caps_info.lcm_degree);
    HWC_LOGD("CapsInfo [%d] output_rotated",             m_caps_info.is_output_rotated);
    HWC_LOGD("CapsInfo [%d] disp_feature",               m_caps_info.disp_feature);
    HWC_LOGD("CapsInfo [%d] support_frame_cfg_ioctl",    m_caps_info.is_support_frame_cfg_ioctl);
    HWC_LOGD("CapsInfo [%d,%d,%d] rpo,rsz_in_max(w,h)",
        m_caps_info.disp_feature & DISP_FEATURE_RPO,
        m_caps_info.rsz_in_max[0], m_caps_info.rsz_in_max[1]);
    HWC_LOGD("CapsInfo [%d]dispRszSupported",           isDispRszSupported());
    // print supported resolutions
    if (isDispRszSupported())
    {
        for (uint32_t i = 0; i < m_caps_info.rsz_list_length; ++i)
        {
            HWC_LOGD(" (%d, %d, %d)",
                m_caps_info.rsz_in_res_list[i][0],
                m_caps_info.rsz_in_res_list[i][1],
                m_caps_info.rsz_in_res_list[i][2]);
        }
    }

    const bool isSelfRefreshSupported = isDispSelfRefreshSupported();
    HWC_LOGD("CapsInfo [%d]isDispSelfRefreshSupported", isSelfRefreshSupported);

    return NO_ERROR;
}

uint32_t DispDevice::getDisplayRotation(uint32_t dpy)
{
    uint32_t rotation = 0;
    if (HWC_DISPLAY_PRIMARY == dpy)
    {
        rotation = (uint32_t)m_caps_info.lcm_degree & 0x7;
        if (1 == m_caps_info.is_output_rotated)
        {
            rotation ^= HAL_TRANSFORM_ROT_180;
        }
    }
    return rotation;
}


bool DispDevice::isDispRszSupported()
{
    return (0 != (m_caps_info.disp_feature & DISP_FEATURE_RSZ));
}

bool DispDevice::isDispRpoSupported()
{
    return m_caps_info.disp_feature & DISP_FEATURE_RPO;
}

void DispDevice::getSupportedResolution(KeyedVector<int, OvlInSizeArbitrator::InputSize>& config)
{
    config.clear();

    // init resoluton for all configuration
    for (int i = 0; i < RSZ_RES_LIST_NUM; i++)
    {
        if (0 == m_caps_info.rsz_in_res_list[i][0] ||
            0 == m_caps_info.rsz_in_res_list[i][1])
            break;

        OvlInSizeArbitrator::InputSize tmp;
        tmp.w = m_caps_info.rsz_in_res_list[i][0];
        tmp.h = m_caps_info.rsz_in_res_list[i][1];
        config.add(-tmp.w, tmp);
    }
}

bool DispDevice::isPartialUpdateSupported()
{
    if (Platform::getInstance().m_config.force_full_invalidate)
    {
        HWC_LOGW("!!!! force full invalidate !!!!");
        return false;
    }

    return (0 != (m_caps_info.disp_feature & DISP_FEATURE_PARTIAL));
}

bool DispDevice::isFenceWaitSupported()
{
    if (Platform::getInstance().m_config.wait_fence_for_display)
    {
        HWC_LOGW("!!!! force hwc wait fence for display !!!!");
        return false;
    }

    return m_caps_info.disp_feature & DISP_FEATURE_FENCE_WAIT;
}

bool DispDevice::isConstantAlphaForRGBASupported()
{
    return  (0 == (m_caps_info.disp_feature & DISP_FEATURE_NO_PARGB));
}

bool DispDevice::isDispSelfRefreshSupported()
{
    return m_caps_info.disp_feature & DISP_FEATURE_DISP_SELF_REFRESH;
}

int DispDevice::getMaxOverlayInputNum()
{
    // TODO: deinfe in ddp_ovl.h now.
    // This's header file for kernel, not for userspace. Need to refine later
    return OVL_LAYER_NUM;
}

uint32_t DispDevice::getMaxOverlayHeight()
{
    // deinfe in ddp_ovl.h now
    return OVL_MAX_HEIGHT;
}

uint32_t DispDevice::getMaxOverlayWidth()
{
    // deinfe in ddp_ovl.h.
    return OVL_MAX_WIDTH;
}

status_t DispDevice::createOverlaySession(int dpy, DISP_MODE mode)
{
    int session_id = m_frame_cfg[dpy].session_id;
    if (DISP_INVALID_SESSION != session_id)
    {
        HWC_LOGW("(%d) Failed to create existed DispSession (id=0x%x)", dpy, session_id);
        return INVALID_OPERATION;
    }

    disp_session_config config;
    memset(&config, 0, sizeof(disp_session_config));

    config.type       = g_session_type[dpy];
    config.device_id  = getDeviceId(dpy);
    config.mode       = mode;
    config.session_id = DISP_INVALID_SESSION;

    int err = WDT_IOCTL(m_dev_fd, DISP_IOCTL_CREATE_SESSION, &config);
    if (err < 0)
    {
        m_frame_cfg[dpy].session_id = DISP_INVALID_SESSION;
        m_frame_cfg[dpy].mode = DISP_INVALID_SESSION_MODE;

        IOLOGE(dpy, err, "DISP_IOCTL_CREATE_SESSION (%s)", getSessionModeString(mode).string());
        return BAD_VALUE;
    }

    m_frame_cfg[dpy].session_id = config.session_id;
    m_frame_cfg[dpy].mode = mode;

    DLOGD(dpy, "Create Session (%s)", getSessionModeString(mode).string());

    return NO_ERROR;
}

void DispDevice::destroyOverlaySession(int dpy)
{
    int session_id = m_frame_cfg[dpy].session_id;
    if (DISP_INVALID_SESSION == session_id)
    {
        HWC_LOGW("(%d) Failed to destroy invalid DispSession", dpy);
        return;
    }

    disp_session_config config;
    memset(&config, 0, sizeof(disp_session_config));

    config.type       = g_session_type[dpy];
    config.device_id  = getDeviceId(dpy);
    config.session_id = session_id;

    int err = WDT_IOCTL(m_dev_fd, DISP_IOCTL_DESTROY_SESSION, &config);
    if (err < 0)
    {
        IOLOGE(dpy, err, "DISP_IOCTL_DESTROY_SESSION");
    }

    m_frame_cfg[dpy].session_id = DISP_INVALID_SESSION;
    m_frame_cfg[dpy].mode = DISP_INVALID_SESSION_MODE;

    DLOGD(dpy, "Destroy DispSession");
}

status_t DispDevice::legacySetInputBuffer(int dpy)
{
    if (m_caps_info.is_support_frame_cfg_ioctl)
        return NO_ERROR;

    memset(&m_input_config[dpy], 0, sizeof(disp_session_input_config));

    m_input_config[dpy].session_id = m_frame_cfg[dpy].session_id;
    m_input_config[dpy].config_layer_num = m_frame_cfg[dpy].input_layer_num;
    int size = m_input_config[dpy].config_layer_num * sizeof(disp_input_config);
    memcpy(m_input_config[dpy].config, m_frame_cfg[dpy].input_cfg, size);
    memcpy(&m_input_config[dpy].ccorr_config, &m_frame_cfg[dpy].ccorr_config, sizeof(m_frame_cfg[dpy].ccorr_config));

    int err = WDT_IOCTL(m_dev_fd, DISP_IOCTL_SET_INPUT_BUFFER, &m_input_config[dpy]);
    if (err < 0)
    {
        IOLOGE(dpy, err, "DISP_IOCTL_SET_INPUT_BUFFER");
    }

    return err;
}

status_t DispDevice::legacySetOutputBuffer(int dpy)
{
    if (m_caps_info.is_support_frame_cfg_ioctl)
        return NO_ERROR;

    memset(&m_output_config[dpy], 0, sizeof(disp_session_output_config));

    m_output_config[dpy].session_id = m_frame_cfg[dpy].session_id;
    memcpy(&m_output_config[dpy].config, &m_frame_cfg[dpy].output_cfg, sizeof(disp_output_config));

    int err = WDT_IOCTL(m_dev_fd, DISP_IOCTL_SET_OUTPUT_BUFFER, &m_output_config[dpy]);
    if (err < 0)
    {
        IOLOGE(dpy, err, "DISP_IOCTL_SET_OUTPUT_BUFFER");
    }

    return err;
}

status_t DispDevice::legacyTriggerSession(int dpy, int pf_idx)
{
    if (m_caps_info.is_support_frame_cfg_ioctl)
        return NO_ERROR;

    disp_session_config config;
    memset(&config, 0, sizeof(disp_session_config));

    config.type       = g_session_type[dpy];
    config.device_id  = getDeviceId(dpy);
    config.session_id = m_frame_cfg[dpy].session_id;
    config.present_fence_idx = pf_idx;

    int err = WDT_IOCTL(m_dev_fd, DISP_IOCTL_TRIGGER_SESSION, &config);
    if (err < 0)
    {
        IOLOGE(dpy, err, "DISP_IOCTL_TRIGGER_SESSION pf_idx=%d", pf_idx);
    }
    else
    {
        DLOGD(dpy, "DISP_IOCTL_TRIGGER_SESSION pf_idx=%d", pf_idx);
    }

    return err;
}

status_t DispDevice::frameConfig(int dpy, int pf_idx, int ovlp_layer_num,
                                 int prev_present_fence_fd,
                                 EXTD_TRIGGER_MODE trigger_mode)
{
    if (-1 == ovlp_layer_num)
    {
        HWC_LOGW("ovlp_layer_num is not available, calculate roughly ...");
        ovlp_layer_num = 0;
        for (int i = 0; i < m_ovl_input_num; i++)
        {
            disp_input_config* input = &m_frame_cfg[dpy].input_cfg[i];
            if (1 == input->layer_enable)
                ovlp_layer_num++;
        }
    }

    m_frame_cfg[dpy].present_fence_idx = pf_idx;
    m_frame_cfg[dpy].overlap_layer_num = ovlp_layer_num;
    m_frame_cfg[dpy].tigger_mode = trigger_mode;
    m_frame_cfg[dpy].prev_present_fence_fd = prev_present_fence_fd;

    int err = WDT_IOCTL(m_dev_fd, DISP_IOCTL_FRAME_CONFIG, &m_frame_cfg[dpy]);
    if (err < 0)
    {
        IOLOGE(dpy, err, "DISP_IOCTL_FRAME_CONFIG ovlp:%d pf_idx=%d", ovlp_layer_num, pf_idx);
    }
    else
    {
        DbgLogger logger(DbgLogger::TYPE_HWC_LOG, 'D');
        logger.printf("(%d) DISP_IOCTL_FRAME_CONFIG ovlp:%d pf_idx=%d id:%x",
                                    dpy, ovlp_layer_num, pf_idx, m_frame_cfg[dpy].session_id);
    }

    return err;
}

bool DispDevice::queryValidLayer(disp_layer_info* disp_layer)
{
    int err = WDT_IOCTL(m_dev_fd, DISP_IOCTL_QUERY_VALID_LAYER, disp_layer);

    if (err < 0)
    {
        IOLOGE(HWC_DISPLAY_PRIMARY, err, "DISP_IOCTL_QUERY_VALID_LAYER: %d", err);
        return false;
    }

    return (disp_layer->hrt_num != -1) ? true : false;
}

status_t DispDevice::triggerOverlaySession(int dpy, int present_fence_idx, int ovlp_layer_num,
                                           int prev_present_fence_fd,
                                           EXTD_TRIGGER_MODE trigger_mode)
{
    if (DISP_INVALID_SESSION == static_cast<int32_t>(m_frame_cfg[dpy].session_id))
    {
        HWC_LOGW("(%d) Failed to trigger invalid DispSession", dpy);
        return INVALID_OPERATION;
    }

    HWC_ATRACE_FORMAT_NAME("TrigerOVL:%d", present_fence_idx);
    if (!m_caps_info.is_support_frame_cfg_ioctl)
        return legacyTriggerSession(dpy, present_fence_idx);
    else
       return frameConfig(dpy, present_fence_idx, ovlp_layer_num, prev_present_fence_fd, trigger_mode);
}

void DispDevice::disableOverlaySession(
    int dpy,  OverlayPortParam* const* params, int num)
{
    int session_id = m_frame_cfg[dpy].session_id;
    if (DISP_INVALID_SESSION == session_id)
    {
        HWC_LOGW("(%d) Failed to disable invalid DispSession", dpy);
        return;
    }

    {
        DbgLogger logger(DbgLogger::TYPE_HWC_LOG, 'D', "(%d) disableOverlaySession ", dpy);
        for (int i = 0; i < m_ovl_input_num; i++)
        {
            disp_input_config* input = &m_frame_cfg[dpy].input_cfg[i];

            if (i >= num)
            {
                input->layer_id = m_ovl_input_num + 1;
                continue;
            }

            input->layer_id     = i;
            input->layer_enable = 0;
            input->next_buff_idx = params[i]->fence_index;

            logger.printf("-%d,idx=%d/ ", i, input->next_buff_idx);
        }
        m_frame_cfg[dpy].input_layer_num = (num < m_ovl_input_num) ? num : m_ovl_input_num;
    }

    if (!m_caps_info.is_support_frame_cfg_ioctl)
        legacySetInputBuffer(dpy);

    disableOverlayOutput(dpy);
    triggerOverlaySession(dpy, -1, 0, -1);

    DLOGD(dpy, "Disable DispSession");
}

status_t DispDevice::setOverlaySessionMode(int dpy, DISP_MODE mode)
{
    int session_id = m_frame_cfg[dpy].session_id;
    if (DISP_INVALID_SESSION == session_id)
    {
        HWC_LOGW("(%d) Failed to set invalid DispSession (mode)", dpy);
        return INVALID_OPERATION;
    }

    disp_session_config config;
    memset(&config, 0, sizeof(disp_session_config));

    config.device_id  = getDeviceId(dpy);
    config.session_id = session_id;
    config.mode       = mode;

    int err = WDT_IOCTL(m_dev_fd, DISP_IOCTL_SET_SESSION_MODE, &config);
    if (err < 0)
    {
        IOLOGE(dpy, err, "DISP_IOCTL_SET_SESSION_MODE %s", getSessionModeString(mode).string());
        return BAD_VALUE;
    }

    m_frame_cfg[dpy].mode = mode;

    DLOGD(dpy, "DispSessionMode (%s)", getSessionModeString(mode).string());
    return NO_ERROR;
}

DISP_MODE DispDevice::getOverlaySessionMode(int dpy)
{
    int session_id = m_frame_cfg[dpy].session_id;
    if (DISP_INVALID_SESSION == session_id)
    {
        HWC_LOGW("(%d) Failed to get invalid DispSession", dpy);
        return DISP_INVALID_SESSION_MODE;
    }

    return m_frame_cfg[dpy].mode;
}

status_t DispDevice::getOverlaySessionInfo(int dpy, disp_session_info* info)
{
    int session_id = m_frame_cfg[dpy].session_id;
    if (DISP_INVALID_SESSION == session_id)
    {
        HWC_LOGW("(%d) Failed to get info for invalid DispSession", dpy);
        return INVALID_OPERATION;
    }

    info->session_id = session_id;

    int err = WDT_IOCTL(m_dev_fd, DISP_IOCTL_GET_SESSION_INFO, info);
    if (err < 0)
    {
        IOLOGE(dpy, err, "DISP_IOCTL_GET_SESSION_INFO");
    }

    return NO_ERROR;
}

int DispDevice::getAvailableOverlayInput(int dpy)
{
    disp_session_info info;
    memset(&info, 0, sizeof(disp_session_info));

    getOverlaySessionInfo(dpy, &info);

    return info.maxLayerNum;
}

void DispDevice::prepareOverlayInput(
    int dpy, OverlayPrepareParam* param)
{
    int session_id = m_frame_cfg[dpy].session_id;
    if (DISP_INVALID_SESSION == session_id)
    {
        HWC_LOGW("(%d) Failed to preapre invalid DispSession (input)", dpy);
        return;
    }

    disp_buffer_info buffer;
    memset(&buffer, 0, sizeof(disp_buffer_info));

    buffer.session_id = session_id;
    buffer.layer_id   = param->id;
    buffer.layer_en   = 1;
    buffer.ion_fd     = param->ion_fd;
    buffer.cache_sync = param->is_need_flush;
    buffer.index      = -1;
    buffer.fence_fd   = -1;

    int err = WDT_IOCTL(m_dev_fd, DISP_IOCTL_PREPARE_INPUT_BUFFER, &buffer);
    if (err < 0)
    {
        IOLOGE(dpy, err, "DISP_IOCTL_PREPARE_INPUT_BUFFER");
    }
    param->fence_index = buffer.index;
    param->fence_fd    = dupCloseAndSetFd(&buffer.fence_fd);

    HWC_ATRACE_BUFFER_INFO("pre_input",
        dpy, param->id, param->fence_index, param->fence_fd);

    HWC_LOGV("DispDevice::prepareOverlayInput() dpy:%d id:%d ion:%d fence_idx:%d fence_fd:%d",
        dpy, param->id, param->ion_fd, param->fence_index, param->fence_fd);
}

void DispDevice::updateOverlayInputs(
    int dpy, OverlayPortParam* const* params, int num, sp<ColorTransform> color_transform)
{
    HWC_LOGV("+ updateOverlayInputs num:%d", num);
    int session_id = m_frame_cfg[dpy].session_id;
    if (DISP_INVALID_SESSION == session_id)
    {
        HWC_LOGW("(%d) Failed to update invalid DispSession (input)", dpy);
        return;
    }

    DbgLogger logger(DbgLogger::TYPE_HWC_LOG, 'D', "(%d) Input: ", dpy);
    DbgLogger logger_dirty(DbgLogger::TYPE_HWC_LOG, 'V', "[DIRTY] dev(xywh)");

    bool is_fbt_exist = false;
    size_t non_fbt_layer_num = 0;
    int i = 0;
    for (; i < m_ovl_input_num; i++)
    {
        if (i >= num) break;

        HWC_LOGV("i:%d params[i]->state:%d", i, params[i]->state);

        disp_input_config* input = &m_frame_cfg[dpy].input_cfg[i];

        if (OVL_IN_PARAM_DISABLE == params[i]->state)
        {
            input->layer_id      = i;
            input->layer_enable  = 0;
            input->next_buff_idx = params[i]->fence_index;
            input->src_fence_fd = -1;

            logger.printf("-%d,idx:%d/ ", i, params[i]->fence_index);
            continue;
        }

        //some crop may be empty(w/h = 0), don't enable layer if crop is empty
        if (params[i]->src_crop.getWidth() < 1 ||
            params[i]->src_crop.getHeight() < 1 ||
            params[i]->dst_crop.getWidth() < 1 ||
            params[i]->dst_crop.getHeight() < 1)
        {
            input->layer_id      = i;
            input->layer_enable  = 0;
            input->next_buff_idx = params[i]->fence_index;
            input->src_fence_fd = -1;

            HWC_LOGW("(%d:%d) Layer- (idx=%d,w/h=0) ", dpy, i, input->next_buff_idx);
            continue;
        }

        input->layer_id       = i;
        input->layer_enable   = 1;

        if (params[i]->dim)
        {
            input->buffer_source = DISP_BUFFER_ALPHA;
            input->dim_color = params[i]->layer_color;
        }
        else if (params[i]->ion_fd == DISP_NO_ION_FD)
        {
            input->buffer_source = DISP_BUFFER_MVA;
        }
        else
        {
            input->buffer_source = DISP_BUFFER_ION;
        }

        // setup layer type for S3D info to driver
        if ((params[i]->is_s3d_layer) && (!HWCMediator::getInstance().m_features.hdmi_s3d_debug))
        {
            if (params[i]->s3d_type == HWC_IS_S3D_LAYER_TAB)
            {
                input->layer_type = DISP_LAYER_3D_TAB_0;
            }
            else if(params[i]->s3d_type == HWC_IS_S3D_LAYER_SBS)
            {
                input->layer_type = DISP_LAYER_3D_SBS_0;
            }
        }
        else
        {
            input->layer_type = DISP_LAYER_2D;
        }

        input->layer_rotation = DISP_ORIENTATION_0;
        input->src_base_addr  = params[i]->va;
        input->src_phy_addr   = params[i]->mva;
        input->src_pitch      = params[i]->pitch;
        input->src_fmt        = mapDispInFormat(params[i]->format, params[i]->blending);
        input->src_offset_x   = params[i]->src_crop.left;
        input->src_offset_y   = params[i]->src_crop.top;
        input->src_width      = params[i]->src_crop.getWidth();
        input->src_height     = params[i]->src_crop.getHeight();
        input->tgt_offset_x   = params[i]->dst_crop.left;
        input->tgt_offset_y   = params[i]->dst_crop.top;
        input->tgt_width      = params[i]->dst_crop.getWidth();
        input->tgt_height     = params[i]->dst_crop.getHeight();
        input->isTdshp        = params[i]->is_sharpen;
        input->next_buff_idx  = params[i]->fence_index;
        input->identity       = params[i]->identity;
        if (params[i]->identity == HWC_LAYER_TYPE_FBT)
        {
            is_fbt_exist = true;
        }
        else
        {
            ++non_fbt_layer_num;
        }

        input->connected_type = params[i]->connected_type;
        input->alpha_enable   = params[i]->alpha_enable;
        input->alpha          = params[i]->alpha;
        input->frm_sequence   = params[i]->sequence;
        input->ext_sel_layer  = params[i]->ext_sel_layer;
        input->yuv_range      = mapDispColorRange(params[i]->color_range, params[i]->format);
        input->src_fence_fd   = params[i]->fence;

        // partial update - fill dirty rects info
        logger_dirty.printf(" (%d,n:%d)", i, params[i]->ovl_dirty_rect_cnt);
        if (0 == params[i]->ovl_dirty_rect_cnt)
        {
            input->dirty_roi_num = 0;
            logger_dirty.printf("NULL", i);
        }
        else
        {
            layer_dirty_roi* hwdev_dirty_rect = m_hwdev_dirty_rect[dpy][i];
            hwc_rect_t* ovl_dirty_rect = params[i]->ovl_dirty_rect;
            for (int j = 0; j < params[i]->ovl_dirty_rect_cnt; j++)
            {
                hwdev_dirty_rect[j].dirty_x = ovl_dirty_rect[j].left;
                hwdev_dirty_rect[j].dirty_y = ovl_dirty_rect[j].top;
                hwdev_dirty_rect[j].dirty_w = ovl_dirty_rect[j].right - ovl_dirty_rect[j].left;
                hwdev_dirty_rect[j].dirty_h = ovl_dirty_rect[j].bottom - ovl_dirty_rect[j].top;
                logger_dirty.printf("[%d,%d,%d,%d]", hwdev_dirty_rect[j].dirty_x,
                                                     hwdev_dirty_rect[j].dirty_y,
                                                     hwdev_dirty_rect[j].dirty_w,
                                                     hwdev_dirty_rect[j].dirty_h);
            }
            input->dirty_roi_addr = hwdev_dirty_rect;
            input->dirty_roi_num = params[i]->ovl_dirty_rect_cnt;
        }

        if (!isConstantAlphaForRGBASupported())
        {
            if (params[i]->blending == HWC2_BLEND_MODE_PREMULTIPLIED)
            {
                input->sur_aen = 1;
                input->src_alpha = DISP_ALPHA_ONE;
                input->dst_alpha = DISP_ALPHA_SRC_INVERT;
            }
            else
            {
                input->sur_aen = 0;
            }
        }

        if (params[i]->secure)
            input->security = DISP_SECURE_BUFFER;
        else if (params[i]->protect)
            input->security = DISP_PROTECT_BUFFER;
        else
            input->security = DISP_NORMAL_BUFFER;

        logger.printf("+%d,ion:%d,idx:%d,ext:%d/ ", i,params[i]->ion_fd, params[i]->fence_index, params[i]->ext_sel_layer);

        DbgLogger* ovl_logger = Debugger::getInstance().m_logger->ovlInput[dpy][i];
        ovl_logger->printf("(%d:%d) Layer+ ("
                 "mva=%p/sec=%d/prot=%d/"
                 "alpha=%d:0x%02x/blend=%04x/dim=%d/fmt=%d/range=%x(%x)/"
                 "x=%d y=%d w=%d h=%d s=%d -> x=%d y=%d w=%d h=%d/"
                 "layer_type=%d s3d=%d ext_layer=%d )",
                 dpy, i, params[i]->mva, params[i]->secure, params[i]->protect,
                 params[i]->alpha_enable, params[i]->alpha, params[i]->blending,
                 params[i]->dim, input->src_fmt, input->yuv_range, params[i]->color_range,
                 params[i]->src_crop.left, params[i]->src_crop.top,
                 params[i]->src_crop.getWidth(), params[i]->src_crop.getHeight(), params[i]->pitch,
                 params[i]->dst_crop.left, params[i]->dst_crop.top,
                 params[i]->dst_crop.getWidth(), params[i]->dst_crop.getHeight(),
                 input->layer_type ,params[i]->s3d_type, params[i]->ext_sel_layer);

        ovl_logger->tryFlush();

        HWC_ATRACE_BUFFER_INFO("set_input",
            dpy, i, params[i]->fence_index, params[i]->if_fence_index);
    }

    m_frame_cfg[dpy].input_layer_num = i;

    if (HWCMediator::getInstance().m_features.is_support_pq &&
        Platform::getInstance().m_config.support_color_transform)
    {
        if (color_transform != nullptr && color_transform->dirty)
        {
            m_frame_cfg[dpy].ccorr_config.is_dirty = true;
            m_frame_cfg[dpy].ccorr_config.mode = color_transform->hint;

            for (int32_t i = 0; i < COLOR_MATRIX_DIM * COLOR_MATRIX_DIM ; ++i)
                m_frame_cfg[dpy].ccorr_config.color_matrix[i] =
                    static_cast<int32_t>(color_transform->matrix[i / 4][i % 4] * 1024.f + 0.5f);

            if (m_frame_cfg[dpy].ccorr_config.mode == HAL_COLOR_TRANSFORM_IDENTITY)
            {
                HWC_LOGI("[DEV] (Send identity matrix)");
            }
            else
            {
                for (int32_t i = 0; i < COLOR_MATRIX_DIM; ++i)
                {
                    DbgLogger logger(DbgLogger::TYPE_HWC_LOG, 'I', "[DEV] ");
                    logger.printf("%4d %4d %4d %4d",
                        m_frame_cfg[dpy].ccorr_config.color_matrix[i * COLOR_MATRIX_DIM],
                        m_frame_cfg[dpy].ccorr_config.color_matrix[i * COLOR_MATRIX_DIM + 1],
                        m_frame_cfg[dpy].ccorr_config.color_matrix[i * COLOR_MATRIX_DIM + 2],
                        m_frame_cfg[dpy].ccorr_config.color_matrix[i * COLOR_MATRIX_DIM + 3]);
                }
            }
        }
    }
    else
    {
        m_frame_cfg[dpy].ccorr_config.is_dirty = false;
    }

    HWC_LOGV("- updateOverlayInputs");

    if (!m_caps_info.is_support_frame_cfg_ioctl)
        legacySetInputBuffer(dpy);
}

void DispDevice::prepareOverlayOutput(int dpy, OverlayPrepareParam* param)
{
    int session_id = m_frame_cfg[dpy].session_id;
    if (DISP_INVALID_SESSION == session_id)
    {
        HWC_LOGW("(%d) Failed to prepare invalid DispSession (output)", dpy);
        return;
    }

    disp_buffer_info buffer;
    memset(&buffer, 0, sizeof(disp_buffer_info));

    buffer.session_id = session_id;
    buffer.layer_id   = param->id;
    buffer.layer_en   = 1;
    buffer.ion_fd     = param->ion_fd;
    buffer.cache_sync = param->is_need_flush;
    buffer.index      = -1;
    buffer.fence_fd   = -1;

    int err = WDT_IOCTL(m_dev_fd, DISP_IOCTL_PREPARE_OUTPUT_BUFFER, &buffer);
    if (err < 0)
    {
        IOLOGE(dpy, err, "DISP_IOCTL_PREPARE_OUTPUT_BUFFER");
    }

    param->fence_index = buffer.index;
    param->fence_fd    = dupCloseAndSetFd(&buffer.fence_fd);

    param->if_fence_index = buffer.interface_index;
    param->if_fence_fd    = dupCloseAndSetFd(&buffer.interface_fence_fd);

    HWC_ATRACE_BUFFER_INFO("pre_output",
        dpy, param->id, param->fence_index, param->fence_fd);
}

void DispDevice::disableOverlayOutput(int dpy)
{
    m_frame_cfg[dpy].output_en = false;
    m_frame_cfg[dpy].output_cfg.src_fence_fd = -1;
}

void DispDevice::enableOverlayOutput(int dpy, OverlayPortParam* param)
{
    int session_id = m_frame_cfg[dpy].session_id;
    if (DISP_INVALID_SESSION == session_id)
    {
        HWC_LOGW("(%d) Failed to update invalid DispSession (output)", dpy);
        return;
    }

    disp_output_config* out_cfg = &m_frame_cfg[dpy].output_cfg;

    out_cfg->va     = param->va;
    out_cfg->pa     = param->mva;
    out_cfg->fmt    = mapDispOutFormat(param->format);
    out_cfg->x      = param->dst_crop.left;
    out_cfg->y      = param->dst_crop.top;
    out_cfg->width  = param->dst_crop.getWidth();
    out_cfg->height = param->dst_crop.getHeight();
    out_cfg->pitch  = param->pitch;
    out_cfg->src_fence_fd = param->fence;

    if (param->secure)
        out_cfg->security = DISP_SECURE_BUFFER;
    else
        out_cfg->security = DISP_NORMAL_BUFFER;

    out_cfg->buff_idx = param->fence_index;

    out_cfg->interface_idx = param->if_fence_index;

    out_cfg->frm_sequence  = param->sequence;

    HWC_LOGD("(%d) Output+ (ion=%d/idx=%d/if_idx=%d/sec=%d/fmt=%d"
             "/x=%d y=%d w=%d h=%d s=%d)",
             dpy, param->ion_fd, param->fence_index, param->if_fence_index, param->secure,
             out_cfg->fmt, param->dst_crop.left, param->dst_crop.top,
             param->dst_crop.getWidth(), param->dst_crop.getHeight(), param->pitch);

    m_frame_cfg[dpy].output_en = true;

    if (!m_caps_info.is_support_frame_cfg_ioctl)
        legacySetOutputBuffer(dpy);
}

void DispDevice::prepareOverlayPresentFence(int dpy, OverlayPrepareParam* param)
{
    int session_id = m_frame_cfg[dpy].session_id;
    if (DISP_INVALID_SESSION == session_id)
    {
        HWC_LOGW("(%d) Failed to preapre invalid DispSession (present)", dpy);
        return;
    }

    {
        disp_present_fence fence;

        fence.session_id = session_id;

        int err = WDT_IOCTL(m_dev_fd, DISP_IOCTL_GET_PRESENT_FENCE, &fence);
        if (err < 0)
        {
            IOLOGE(dpy, err, "DISP_IOCTL_GET_PRESENT_FENCE");
        }

        param->fence_index = fence.present_fence_index;
        param->fence_fd    = dupCloseAndSetFd(&fence.present_fence_fd);
        HWC_LOGV("(%d) DispDevice::prepareOverlayPresentFence() idx:%d fence fd:%d",
            dpy, param->fence_index, param->fence_fd);
    }
}

status_t DispDevice::waitVSync(int dpy, nsecs_t *ts)
{
    int session_id = m_frame_cfg[dpy].session_id;
    if (DISP_INVALID_SESSION == session_id)
    {
        HWC_LOGW("Failed to wait vsync for invalid DispSession (dpy=%d)", dpy);
        return BAD_VALUE;
    }

    disp_session_vsync_config config;
    memset(&config, 0, sizeof(config));

    config.session_id = session_id;

    int err = WDT_IOCTL(m_dev_fd, DISP_IOCTL_WAIT_FOR_VSYNC, &config);
    if (err < 0)
    {
        IOLOGE(dpy, err, "DISP_IOCTL_WAIT_FOR_VSYNC");
        return BAD_VALUE;
    }

    *ts = config.vsync_ts;

    return NO_ERROR;
}

void DispDevice::setPowerMode(int dpy,int mode)
{
    if (HWCMediator::getInstance().m_features.control_fb)
    {
        HWC_LOGD("DispDevice::setPowerMode() dpy:%d mode:%d", dpy, mode);
#ifdef USE_SWWATCHDOG
        AUTO_WDT(1000);
#endif
        char filename[32] = {0};
        snprintf(filename, sizeof(filename), "/dev/graphics/fb%d", dpy);
        int fb_fd = open(filename, O_RDWR);
        if (fb_fd <= 0)
        {
            HWC_LOGE("Failed to open fb%d device: %s", dpy, strerror(errno));
            return;
        }

        int err = NO_ERROR;
        switch (mode)
        {
            case HWC_POWER_MODE_OFF:
            {
                err = WDT_IOCTL(fb_fd, FBIOBLANK, FB_BLANK_POWERDOWN);
#ifdef USE_SWWATCHDOG
                if (dpy == HWC_DISPLAY_PRIMARY)
                    SWWatchDog::suspend();
#endif
                break;
            }
            case HWC_POWER_MODE_NORMAL:
            {
#ifdef USE_SWWATCHDOG
                if (dpy == HWC_DISPLAY_PRIMARY)
                    SWWatchDog::resume();
#endif
                err = WDT_IOCTL(fb_fd, FBIOBLANK, FB_BLANK_UNBLANK);
                m_color_transform_info[dpy].resend_color_transform = true;
                break;
            }
            case HWC_POWER_MODE_DOZE:
            {
                if (HWCMediator::getInstance().m_features.aod)
                {
                    err = WDT_IOCTL(fb_fd, MTKFB_SET_AOD_POWER_MODE, MTKFB_AOD_DOZE);
                }
                else
                {
                    err = WDT_IOCTL(fb_fd, FBIOBLANK, FB_BLANK_UNBLANK);
                    HWC_LOGE("setPowerMode: receive HWC_POWER_MODE_DOZE without aod enabled");
                }
                m_color_transform_info[dpy].resend_color_transform = true;
                break;
            }
            case HWC_POWER_MODE_DOZE_SUSPEND:
            {
                if (HWCMediator::getInstance().m_features.aod)
                {
                    err = WDT_IOCTL(fb_fd, MTKFB_SET_AOD_POWER_MODE, MTKFB_AOD_DOZE_SUSPEND);
                }
                else
                {
                    err = WDT_IOCTL(fb_fd, FBIOBLANK, FB_BLANK_UNBLANK);
                    HWC_LOGE("setPowerMode: receive HWC_POWER_MODE_DOZE_SUSPEND without aod enabled ");
                }
                break;
            }

            default:
                HWC_LOGE("setPowerMode: receive unknown power mode: %d", mode);
        }

        if (err != NO_ERROR) {
            HWC_LOGE("Failed to set power(%s) to fb%d device: %s", mode, dpy, strerror(errno));
        }

        protectedClose(fb_fd);
    }
}

unsigned int DispDevice::getDeviceId(int dpy)
{
    unsigned int device_id = dpy;
    if (dpy == HWC_DISPLAY_EXTERNAL)
    {
        device_id = g_ext_device_id[HWCMediator::getInstance().m_features.dual_display];
    }
    return device_id;
}

status_t DispDevice::waitAllJobDone(const int dpy)
{
    if (isFenceWaitSupported())
    {
        if (static_cast<int32_t>(m_frame_cfg[dpy].session_id) == DISP_INVALID_SESSION)
        {
            HWC_LOGW("try to wait for an invalid display session");
            return INVALID_OPERATION;
        }

        int err = WDT_IOCTL(m_dev_fd, DISP_IOCTL_WAIT_ALL_JOBS_DONE, m_frame_cfg[dpy].session_id);
        if (err < 0)
        {
            IOLOGE(dpy, err, "DISP_IOCTL_WAIT_ALL_JOBS_DONE");
            return err;
        }
    }

    return NO_ERROR;
}

status_t DispDevice::waitRefreshRequest(unsigned int* type)
{
    // NOTE: don't use WDT_IOCTL since this DISP_IOCTL_WAIT_DISP_SELF_REFRESH will
    // blocked by driver until self refresh is needed
    int err = ioctl(m_dev_fd, DISP_IOCTL_WAIT_DISP_SELF_REFRESH, type);
    if (err < 0)
    {
        IOLOGE(HWC_DISPLAY_PRIMARY, err, "DISP_IOCTL_WAIT_DISP_SELF_REFRESH");
        return BAD_VALUE;
    }

    return NO_ERROR;
}

void DispDevice::setLastValidColorTransform(const int32_t& dpy, sp<ColorTransform> color_transform)
{
    m_color_transform_info[dpy].last_valid_color_transform = color_transform;
}

// ---------------------------------------------------------------------------

#ifdef USES_GRALLOC1
ANDROID_SINGLETON_STATIC_INSTANCE(GrallocDevice);

GrallocDevice::GrallocDevice()
    : m_dev(NULL)
{
    const hw_module_t* module;
    int err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);
    if (err)
    {
        HWC_LOGE("Failed to open gralloc device: %s (%d)", strerror(-err), err);
        return;
    }

    err = gralloc1_open(module, &m_dev);
    if (err)
    {
        HWC_LOGE("Failed to call gralloc1_open(): %s (%d)", strerror(-err), err);
    }

    initDispatch();
}

GrallocDevice::~GrallocDevice()
{
    if (m_dev) gralloc1_close(m_dev);
}

template<typename T>
void GrallocDevice::initDispatch(gralloc1_function_descriptor_t desc, T* outPfn)
{
    auto pfn = m_dev->getFunction(m_dev, desc);
    if (!pfn) {
        HWC_LOGE("Failed to get gralloc1 function : %d", desc);
    }
    *outPfn = reinterpret_cast<T>(pfn);
}

void GrallocDevice::initDispatch()
{
    initDispatch(GRALLOC1_FUNCTION_DUMP, &m_dispatch.dump);
    initDispatch(GRALLOC1_FUNCTION_CREATE_DESCRIPTOR, &m_dispatch.createDescriptor);
    initDispatch(GRALLOC1_FUNCTION_DESTROY_DESCRIPTOR, &m_dispatch.destroyDescriptor);
    initDispatch(GRALLOC1_FUNCTION_SET_DIMENSIONS, &m_dispatch.setDimensions);
    initDispatch(GRALLOC1_FUNCTION_SET_FORMAT, &m_dispatch.setFormat);
    initDispatch(GRALLOC1_FUNCTION_SET_CONSUMER_USAGE, &m_dispatch.setConsumerUsage);
    initDispatch(GRALLOC1_FUNCTION_SET_PRODUCER_USAGE, &m_dispatch.setProducerUsage);
    initDispatch(GRALLOC1_FUNCTION_ALLOCATE, &m_dispatch.allocate);
    initDispatch(GRALLOC1_FUNCTION_RELEASE, &m_dispatch.release);
    initDispatch(GRALLOC1_FUNCTION_LOCK, &m_dispatch.lock);
    initDispatch(GRALLOC1_FUNCTION_UNLOCK, &m_dispatch.unlock);
}

status_t GrallocDevice::alloc(AllocParam& param)
{
    HWC_ATRACE_CALL();
    status_t err;

    if (!m_dev) return NO_INIT;

    if ((param.width == 0) || (param.height == 0))
    {
        HWC_LOGE("Empty buffer(w=%u h=%u) allocation is not allowed",
                param.width, param.height);
        return INVALID_OPERATION;
    }

    gralloc1_buffer_descriptor_t descriptor;
    err = createDescriptor(param, &descriptor);
    if (err != GRALLOC1_ERROR_NONE)
    {
        HWC_LOGE("Failed to create descriptor buffer(w=%u h=%u f=%d usage=%08x): %s (%d)",
                param.width, param.height, param.format, param.usage,
                strerror(-err), err);
    }

    err = m_dispatch.allocate(m_dev, 1, &descriptor, &param.handle);

    if (err == GRALLOC1_ERROR_NONE)
    {
        HWC_LOGD("alloc gralloc memory(%p)", param.handle);
        // TODO: add allocated buffer record
    }
    else
    {
        HWC_LOGE("Failed to allocate buffer(w=%u h=%u f=%d usage=%08x): %s (%d)",
                param.width, param.height, param.format, param.usage,
                strerror(-err), err);
    }

    return err;
}

status_t GrallocDevice::free(buffer_handle_t handle)
{
    HWC_ATRACE_CALL();
    status_t err;

    if (!m_dev) return NO_INIT;

    err = m_dispatch.release(m_dev, handle);
    if (err == GRALLOC1_ERROR_NONE)
    {
        HWC_LOGD("free gralloc memory(%p)", handle);
        // TODO: remove allocated buffer record
    }
    else
    {
        HWC_LOGE("Failed to free buffer handle(%p): %s (%d)",
                handle, strerror(-err), err);
    }

    return err;
}

status_t GrallocDevice::createDescriptor(const AllocParam& param,
    gralloc1_buffer_descriptor_t* outDescriptor) {
    gralloc1_buffer_descriptor_t descriptor;

    status_t err = m_dispatch.createDescriptor(m_dev, &descriptor);

    if (err == GRALLOC1_ERROR_NONE)
    {
        err = m_dispatch.setDimensions(m_dev, descriptor, param.width, param.height);
    }

    if (err == GRALLOC1_ERROR_NONE) {
        err = m_dispatch.setFormat(m_dev, descriptor,
                                    static_cast<int32_t>(param.format));
    }

    if (err == GRALLOC1_ERROR_NONE) {
        err = m_dispatch.setProducerUsage(m_dev, descriptor, param.usage);
    }
    if (err == GRALLOC1_ERROR_NONE) {
        err = m_dispatch.setConsumerUsage(m_dev, descriptor, param.usage);
    }

    if (err == GRALLOC1_ERROR_NONE) {
        *outDescriptor = descriptor;
    } else {
        m_dispatch.destroyDescriptor(m_dev, descriptor);
    }

    return err;
}


void GrallocDevice::dump() const
{
    // TODO: dump allocated buffer record
}

#else // USES_GRALLOC1

ANDROID_SINGLETON_STATIC_INSTANCE(GrallocDevice);

GrallocDevice::GrallocDevice()
    : m_dev(NULL)
{
    const hw_module_t* module;
    int err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);
    if (err)
    {
        HWC_LOGE("Failed to open gralloc device: %s (%d)", strerror(-err), err);
        return;
    }

    gralloc_open(module, &m_dev);
}

GrallocDevice::~GrallocDevice()
{
    if (m_dev) gralloc_close(m_dev);
}

status_t GrallocDevice::alloc(AllocParam& param)
{
    HWC_ATRACE_CALL();
    status_t err;

    if (!m_dev) return NO_INIT;

    if ((param.width == 0) || (param.height == 0))
    {
        HWC_LOGE("Empty buffer(w=%u h=%u) allocation is not allowed",
                param.width, param.height);
        return INVALID_OPERATION;
    }

    err = m_dev->alloc(m_dev,
        param.width, param.height, param.format,
        param.usage, &param.handle, &param.stride);
    if (err == NO_ERROR)
    {
        HWC_LOGD("alloc gralloc memory(%p)", param.handle);
        // TODO: add allocated buffer record
    }
    else
    {
        HWC_LOGE("Failed to allocate buffer(w=%u h=%u f=%d usage=%08x): %s (%d)",
                param.width, param.height, param.format, param.usage,
                strerror(-err), err);
    }

    return err;
}

status_t GrallocDevice::free(buffer_handle_t handle)
{
    HWC_ATRACE_CALL();
    status_t err;

    if (!m_dev) return NO_INIT;

    err = m_dev->free(m_dev, handle);
    if (err == NO_ERROR)
    {
        HWC_LOGD("free gralloc memory(%p)", handle);
        // TODO: remove allocated buffer record
    }
    else
    {
        HWC_LOGE("Failed to free buffer handle(%p): %s (%d)",
                handle, strerror(-err), err);
    }

    return err;
}

void GrallocDevice::dump() const
{
    // TODO: dump allocated buffer record
}
#endif // USES_GRALLOC1

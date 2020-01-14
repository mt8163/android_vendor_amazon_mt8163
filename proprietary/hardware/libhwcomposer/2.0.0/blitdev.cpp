#define DEBUG_LOG_TAG "BLTDEV"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <hardware/hwcomposer_defs.h>
#include <linux/disp_session.h>

#include "utils/debug.h"
#include "utils/tools.h"
#include "sync.h"
#include "overlay.h"
#include "hwdev.h"

#include "blitdev.h"

enum
{
    BLIT_INVALID_SESSION = -1,
    BLIT_VIRTUAL_SESSION = 0x80002,
};

#define checkValidDpyRetNon(dpy) \
    do {if (HWC_DISPLAY_VIRTUAL != dpy){ \
            HWC_LOGW("(%d) Failed to %s", dpy, __func__); \
            return;} \
    } while(0)

#define checkValidDpyRetVal(dpy, val) \
    do {if (HWC_DISPLAY_VIRTUAL != dpy){ \
            HWC_LOGW("(%d) Failed to %s", dpy, __func__); \
            return val;} \
    } while(0)

#define checkValidSessionRetNon(dpy, session) \
    do {if (BLIT_INVALID_SESSION == session){ \
        HWC_LOGW("(%d) Failed to %s (id=0x%x)", dpy, __func__, session); \
        return;} \
    } while(0)

#define checkValidSessionRetVal(dpy, session, val) \
    do {if (BLIT_INVALID_SESSION == session){ \
        HWC_LOGW("(%d) Failed to %s (id=0x%x)", dpy, __func__, session); \
        return val;} \
    } while(0)

#define HWC_ATRACE_BUFFER_INFO(string, n1, n2, n3, n4)                            \
        if (ATRACE_ENABLED()) {                                                   \
            char ___traceBuf[1024];                                               \
            snprintf(___traceBuf, 1024, "%s(%d:%d): %u %d", (string),             \
                (n1), (n2), (n3), (n4));                                          \
            android::ScopedTrace ___bufTracer(ATRACE_TAG, ___traceBuf);           \
        }

#define ALIGN_CEIL(x,a)     (((x) + (a) - 1L) & ~((a) - 1L))
// ---------------------------------------------------------------------------

extern DP_PROFILE_ENUM mapDpColorRange(const uint32_t range);

static void releaseFence(int fd, sp<SyncFence> sync_fence, unsigned int sync_marker)
{
    if (fd != -1)
    {
        unsigned int curr_marker = sync_fence->getCurrMarker();

        while (sync_marker > curr_marker)
        {
            HWC_LOGD("Release releaseFence (fd=%d) (%d->%d)",
                fd, curr_marker, sync_marker);

            status_t err = sync_fence->inc(fd);
            if (err)
            {
                HWC_LOGD("Failed to release releaseFence (fd=%d): %s(%d)",
                    fd, strerror(-err), err);
            }

            curr_marker++;
        }
    }
}

BlitDevice::BlitDevice()
    : m_sync_input_fence(new SyncFence(HWC_DISPLAY_VIRTUAL))
    , m_sync_output_fence(new SyncFence(HWC_DISPLAY_VIRTUAL))
    , m_session_id(BLIT_INVALID_SESSION)
    , m_state(OVL_IN_PARAM_DISABLE)
    , m_disp_session_state(DISP_INVALID_SESSION_MODE)
{
    HWC_LOGI("create BlitDevice");

    memset(&m_caps_info, 0, sizeof(m_caps_info));
    m_caps_info.max_layer_num = 1;
    m_caps_info.is_support_frame_cfg_ioctl = false;
    m_caps_info.is_output_rotated = false;
    m_caps_info.lcm_degree = 0;

    memset(&m_disp_session_info, 0, sizeof(m_disp_session_info));
}

BlitDevice::~BlitDevice()
{
    HWC_LOGI("~BlitDevice");
    m_sync_input_fence = NULL;
    m_sync_output_fence = NULL;

    {
        AutoMutex l(m_vector_lock);

        m_ion_flush_vector.clear();
    }
}

void BlitDevice::initOverlay()
{
}


uint32_t BlitDevice::getDisplayRotation(uint32_t /*dpy*/)
{
    return 0;
}


void BlitDevice::getSupportedResolution(KeyedVector<int, OvlInSizeArbitrator::InputSize>& /*config*/)
{
}

bool BlitDevice::isDispRszSupported()
{
    return false;
}

bool BlitDevice::isDispRpoSupported()
{
    return false;
}

bool BlitDevice::isPartialUpdateSupported()
{
    return false;
}

bool BlitDevice::isFenceWaitSupported()
{
    return false;
}

bool BlitDevice::isConstantAlphaForRGBASupported()
{
    return false;
}

bool BlitDevice::isDispSelfRefreshSupported()
{
    return false;
}

int BlitDevice::getMaxOverlayInputNum()
{
    return m_caps_info.max_layer_num;
}

uint32_t BlitDevice::getMaxOverlayHeight()
{
    return 0;
}

uint32_t BlitDevice::getMaxOverlayWidth()
{
    return 0;
}

status_t BlitDevice::createOverlaySession(int dpy, DISP_MODE mode)
{
    checkValidDpyRetVal(dpy, INVALID_OPERATION);

    if (BLIT_INVALID_SESSION != m_session_id)
    {
        HWC_LOGW("(%d) Failed to create existed BlitSession (id=0x%x)", dpy, m_session_id);
        return INVALID_OPERATION;
    }

    {
        AutoMutex mutex(m_state_lock);
        m_session_id = BLIT_VIRTUAL_SESSION;
        memset(&m_disp_session_info, 0, sizeof(m_disp_session_info));
        m_disp_session_info.session_id = BLIT_VIRTUAL_SESSION;
        m_disp_session_info.maxLayerNum = 1;
        m_disp_session_info.isHwVsyncAvailable = false;
        m_disp_session_info.isConnected = true;
        m_disp_session_info.isHDCPSupported = 0;
    }

    HWC_LOGD("(%d) Create BlitSession (id=0x%x, mode=%d)", dpy, m_session_id, mode);

    return NO_ERROR;
}

void BlitDevice::destroyOverlaySession(int dpy)
{
    checkValidDpyRetNon(dpy);

    checkValidSessionRetNon(dpy, m_session_id);

    HWC_LOGD("(%d) Destroy BlitSession (id=0x%x)", dpy, m_session_id);

    {
        AutoMutex mutex(m_state_lock);
        m_session_id = BLIT_INVALID_SESSION;
        memset(&m_disp_session_info, 0, sizeof(m_disp_session_info));
    }
}

status_t BlitDevice::triggerOverlaySession(
    int dpy, int /*present_fence_idx*/, int /*ovlp_layer_num*/, int /*prev_present_fence_fd*/,
    EXTD_TRIGGER_MODE /*trigger_mode*/)
{
    checkValidDpyRetVal(dpy, INVALID_OPERATION);

    checkValidSessionRetVal(dpy, m_session_id, INVALID_OPERATION);

    status_t err = NO_ERROR;

    m_blit_stream.setOrientation(DpBlitStream::ROT_0);

    // setup for PQ
    DpPqParam dppq_param;
    dppq_param.enable = false;
    dppq_param.scenario = MEDIA_VIDEO;
    dppq_param.u.video.id = 0;
    dppq_param.u.video.timeStamp = 0;
    m_blit_stream.setPQParameter(dppq_param);

    HWC_LOGD("INVALIDATE/s_flush=%x/s_range=%d/s_sec=%d"
        "/s_acq=%d/s_ion=%d/s_fmt=%x"
        "/d_flush=%x/d_range=%d/d_sec=%d"
        "/d_rel=%d/d_ion=%d/d_fmt=%x"
        "/(%d,%d,%d,%d)->(%d,%d,%d,%d)",
        m_cur_params.src_is_need_flush, m_cur_params.src_range, m_cur_params.src_is_secure,
        m_cur_params.src_fence_index, m_cur_params.src_ion_fd, m_cur_params.src_fmt,
        m_cur_params.dst_is_need_flush, m_cur_params.dst_range, m_cur_params.dst_is_secure,
        m_cur_params.dst_fence_index, m_cur_params.dst_ion_fd, m_cur_params.dst_fmt,
        m_cur_params.src_crop.top, m_cur_params.src_crop.left, m_cur_params.src_crop.right, m_cur_params.src_crop.bottom,
        m_cur_params.dst_crop.top, m_cur_params.dst_crop.left, m_cur_params.dst_crop.right, m_cur_params.dst_crop.bottom);

    DP_STATUS_ENUM status = m_blit_stream.invalidate();
    if (DP_STATUS_RETURN_SUCCESS != status)
    {
        HWC_LOGE("INVALIDATE/blit fail/err=%d", status);
    }

    IONDevice::getInstance().ionClose(m_cur_params.src_ion_fd);
    IONDevice::getInstance().ionClose(m_cur_params.dst_ion_fd);

    // no fence fd, use ion fd instead for debugging
    releaseFence(m_cur_params.src_ion_fd, m_sync_input_fence, m_cur_params.src_fence_index);
    releaseFence(m_cur_params.dst_ion_fd, m_sync_output_fence, m_cur_params.dst_fence_index);

    if (m_cur_params.src_is_secure)
    {
        // TODO: must guarantee life cycle of the secure buffer
    }
    else
    {
        // TODO: should be removed after intergrating prepare hehavior to DpBlitStream
    }

    if (m_cur_params.dst_is_secure)
    {
        // TODO: must guarantee life cycle of the secure buffer
    }
    else
    {
    }

    // extension mode
    if (DisplayManager::m_profile_level & PROFILE_TRIG)
    {
        char atrace_tag[256];
        sprintf(atrace_tag, "BLT-SMS");
        HWC_ATRACE_ASYNC_END(atrace_tag, m_cur_params.job_sequence);
    }

    return err;
}

void BlitDevice::disableOverlaySession(
    int dpy,  OverlayPortParam* const* /*params*/, int /*num*/)
{
    checkValidDpyRetNon(dpy);

    checkValidSessionRetNon(dpy, m_session_id);

    HWC_LOGD("(%d) Disable BlitSession (id=0x%x)", dpy, m_session_id);
}

status_t BlitDevice::setOverlaySessionMode(int dpy, DISP_MODE mode)
{
    checkValidDpyRetVal(dpy, INVALID_OPERATION);

    checkValidSessionRetVal(dpy, m_session_id, INVALID_OPERATION);

    HWC_LOGD("(%d) Set BlitSessionMode (id=0x%x mode=%s)", dpy, m_session_id, getSessionModeString(mode).string());

    {
        AutoMutex l(m_state_lock);
        m_disp_session_state = mode;

    }

    if (m_disp_session_state != DISP_SESSION_DECOUPLE_MODE)
    {
        HWC_LOGE("BlitDevice only support decouple mode!");
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

DISP_MODE BlitDevice::getOverlaySessionMode(int dpy)
{
    checkValidSessionRetVal(dpy, m_session_id, DISP_INVALID_SESSION_MODE);

    DISP_MODE disp_mode = DISP_INVALID_SESSION_MODE;
    {
        AutoMutex mutex(m_state_lock);
        disp_mode = m_disp_session_state;
    }
    return disp_mode;
}

status_t BlitDevice::getOverlaySessionInfo(int dpy, disp_session_info* info)
{
    checkValidDpyRetVal(dpy, INVALID_OPERATION);

    checkValidSessionRetVal(dpy, m_session_id, INVALID_OPERATION);

    AutoMutex mutex(m_state_lock);
    if (m_session_id == BLIT_INVALID_SESSION)
    {
        return INVALID_OPERATION;
    }

    memcpy(info, &m_disp_session_info, sizeof(m_disp_session_info));
    return NO_ERROR;
}

int BlitDevice::getAvailableOverlayInput(int dpy)
{
    checkValidDpyRetVal(dpy, 0);

    checkValidSessionRetVal(dpy, m_session_id, 0);

    return 1;
}

void BlitDevice::prepareOverlayInput(
    int dpy, OverlayPrepareParam* param)
{
    int32_t input_ion_fd = -1;

    checkValidDpyRetNon(dpy);

    checkValidSessionRetNon(dpy, m_session_id);

    param->fence_fd = m_sync_input_fence->create();
    param->fence_index = m_sync_input_fence->getLastMarker();

    if (param->id != 0)
        HWC_LOGE("BlitDevice support only 1 ovl input!");

    // TODO: should integrate prepare hehavior to DpBlitStream
    IONDevice::getInstance().ionImport(param->ion_fd, &input_ion_fd);
    {
        AutoMutex l(m_vector_lock);

        m_ion_flush_vector.add(param->ion_fd, param->is_need_flush);
    }

    {
        AutoMutex l(m_input_ion_list_lock);
        m_input_ion_fd_list.push_back(input_ion_fd);
    }
    HWC_ATRACE_BUFFER_INFO("pre_input",
        dpy, param->id, param->fence_index, param->fence_fd);
}

void BlitDevice::updateOverlayInputs(
    int dpy, OverlayPortParam* const* params, int /*num*/, sp<ColorTransform> /*color_transform*/)
{
    checkValidDpyRetNon(dpy);

    checkValidSessionRetNon(dpy, m_session_id);

    OverlayPortParam* param = params[0];

    if (OVL_IN_PARAM_ENABLE != param->state)
    {
        HWC_LOGI("updateOverlayInputs ignore state(%d)!", param->state);
        m_state = param->state;

        return;
    }

    DpSecure is_dp_secure = DP_SECURE_NONE;

    int src_ion_fd =-1;
    std::list<int32_t>::iterator front_ion_fd;
    {
        AutoMutex l(m_input_ion_list_lock);
        front_ion_fd = m_input_ion_fd_list.begin();
        src_ion_fd = *front_ion_fd;
        m_input_ion_fd_list.pop_front();
    }
    DpColorFormat src_dpformat;
    unsigned int  src_pitch;
    bool secure = param->secure;

    {
        unsigned int  src_plane;
        unsigned int  src_size[3];

        unsigned int stride = param->pitch;
        unsigned int height = param->src_crop.bottom;

        switch (param->format)
        {
            case HAL_PIXEL_FORMAT_RGBA_8888:
            case HAL_PIXEL_FORMAT_RGBX_8888:
                src_pitch = stride * 4;
                src_plane = 1;
                src_size[0] = src_pitch * height;
                src_dpformat = DP_COLOR_RGBA8888;
                break;

            case HAL_PIXEL_FORMAT_BGRA_8888:
            case HAL_PIXEL_FORMAT_BGRX_8888:
            case HAL_PIXEL_FORMAT_IMG1_BGRX_8888:
                src_pitch = stride * 4;
                src_plane = 1;
                src_size[0] = src_pitch * height;
                src_dpformat = DP_COLOR_BGRA8888;
                break;

            case HAL_PIXEL_FORMAT_RGB_888:
                src_pitch = stride * 3;
                src_plane = 1;
                src_size[0] = src_pitch * height;
                src_dpformat = DP_COLOR_RGB888;
                break;

            case HAL_PIXEL_FORMAT_RGB_565:
                src_pitch = stride * 2;
                src_plane = 1;
                src_size[0] = src_pitch * height;
                src_dpformat = DP_COLOR_RGB565;
                break;
#if 0 // ovl input didn't support YV12
            case HAL_PIXEL_FORMAT_YV12:
                {
                    src_pitch    = stride;
                    unsigned int src_pitch_uv = ALIGN_CEIL((stride / 2), 16);
                    src_plane = 3;
                    unsigned int src_size_luma = src_pitch * height;
                    unsigned int src_size_chroma = config->src_pitch_uv * (height / 2);
                    src_size[0] = src_size_luma;
                    src_size[1] = src_size_chroma;
                    src_size[2] = src_size_chroma;
                    src_dpformat = DP_COLOR_YV12;
                }
                break;
#endif
            case HAL_PIXEL_FORMAT_YUYV:
                src_pitch = stride * 2;
                src_plane = 1;
                src_size[0] = src_pitch * height;
                src_dpformat = DP_COLOR_YUYV;
                break;

            default:
                HWC_LOGW("Input color format for DP is invalid (0x%x)", param->format);
                return;
        }

        if (secure)
        {
            void* src_addr[3];
            src_addr[0] = (void*)(uintptr_t)param->mva;
            src_addr[1] = (void*)(uintptr_t)param->mva;
            src_addr[2] = (void*)(uintptr_t)param->mva;

            m_blit_stream.setSrcBuffer(src_addr, src_size, src_plane);
            is_dp_secure = DP_SECURE;
        }
        else
        {
            m_blit_stream.setSrcBuffer(src_ion_fd, src_size, src_plane);
        }
    }
    //-----------------------------------------------------------

    bool is_need_flush = false;
    {
        AutoMutex l(m_vector_lock);

        is_need_flush = m_ion_flush_vector.valueFor(src_ion_fd);

        m_ion_flush_vector.removeItem(src_ion_fd);
    }

    DP_PROFILE_ENUM dp_range = mapDpColorRange(param->color_range);

    DpRect src_dp_roi;
    int width = param->src_crop.getWidth();
    int height = param->src_crop.getHeight();

    src_dp_roi.x = param->src_crop.left;
    src_dp_roi.y = param->src_crop.top;
#if 1
    src_dp_roi.w = width;
    src_dp_roi.h = height;
#else
    // TODO: need to align 2bytes?
    src_dp_roi.w = ALIGN_FLOOR(param->src_crop.getWidth(), 2);
    src_dp_roi.h = ALIGN_FLOOR(param->src_crop.getHeight(), 2);
#endif
    // [NOTE] setSrcConfig provides y and uv pitch configuration
    // if uv pitch is 0, DP would calculate it according to y pitch
    m_blit_stream.setSrcConfig(
        param->pitch, height,
        src_pitch, 0,
        src_dpformat, dp_range,
        eInterlace_None, &src_dp_roi,
        is_dp_secure, is_need_flush);

    m_cur_params.src_ion_fd = src_ion_fd;
    m_cur_params.src_fence_index = param->fence_index;
    m_cur_params.src_fmt = param->format;
    m_cur_params.src_crop = param->src_crop;
    m_cur_params.src_range = param->color_range;
    m_cur_params.src_is_need_flush = is_need_flush;
    m_cur_params.src_is_secure = secure;

    m_state = OVL_IN_PARAM_ENABLE;
}

void BlitDevice::prepareOverlayOutput(int dpy, OverlayPrepareParam* param)
{
    int32_t output_ion_fd = -1;

    checkValidDpyRetNon(dpy);

    checkValidSessionRetNon(dpy, m_session_id);

    param->fence_fd = m_sync_output_fence->create();
    param->fence_index = m_sync_output_fence->getLastMarker();
    param->if_fence_index = -1;
    param->if_fence_fd    = -1;

    IONDevice::getInstance().ionImport(param->ion_fd, &output_ion_fd, "BlitDevice::prepareOverlayOutput()");

    {
        AutoMutex l(m_output_ion_list_lock);
        m_output_ion_fd_list.push_back(output_ion_fd);
    }
    HWC_ATRACE_BUFFER_INFO("pre_output",
        dpy, param->id, param->fence_index, param->fence_fd);
}

void BlitDevice::enableOverlayOutput(int dpy, OverlayPortParam* param)
{
    checkValidDpyRetNon(dpy);

    checkValidSessionRetNon(dpy, m_session_id);

    DpSecure is_dp_secure = DP_SECURE_NONE;

    int dst_ion_fd = -1;
    std::list<int32_t>::iterator front_ion_fd;
    {
        AutoMutex l(m_output_ion_list_lock);
        front_ion_fd = m_output_ion_fd_list.begin();
        dst_ion_fd = *front_ion_fd;
        m_output_ion_fd_list.pop_front();
    }

    DpColorFormat dst_dpformat;
    unsigned int  dst_pitch;
    unsigned int dst_pitch_uv = 0;
    bool secure = param->secure;

    {
        unsigned int  dst_plane;
        unsigned int  dst_size[3];

        unsigned int stride = param->pitch;
        unsigned int height = param->dst_crop.bottom;

        switch (param->format)
        {
            case HAL_PIXEL_FORMAT_RGBA_8888:
            case HAL_PIXEL_FORMAT_RGBX_8888:
                dst_pitch = stride * 4;
                dst_plane = 1;
                dst_size[0] = dst_pitch * height;
                dst_dpformat = DP_COLOR_RGBA8888;
                break;

            case HAL_PIXEL_FORMAT_BGRA_8888:
            case HAL_PIXEL_FORMAT_BGRX_8888:
            case HAL_PIXEL_FORMAT_IMG1_BGRX_8888:
                dst_pitch = stride * 4;
                dst_plane = 1;
                dst_size[0] = dst_pitch * height;
                dst_dpformat = DP_COLOR_BGRA8888;
                break;

            case HAL_PIXEL_FORMAT_RGB_888:
                dst_pitch = stride * 3;
                dst_plane = 1;
                dst_size[0] = dst_pitch * height;
                dst_dpformat = DP_COLOR_RGB888;
                break;

            case HAL_PIXEL_FORMAT_RGB_565:
                dst_pitch = stride * 2;
                dst_plane = 1;
                dst_size[0] = dst_pitch * height;
                dst_dpformat = DP_COLOR_RGB565;
                break;

            case HAL_PIXEL_FORMAT_YV12:
                {
                    dst_pitch    = stride;
                    dst_pitch_uv = ALIGN_CEIL((stride / 2), 16);
                    dst_plane = 3;
                    unsigned int dst_size_luma = dst_pitch * height;
                    unsigned int dst_size_chroma = dst_pitch_uv * (height / 2);
                    dst_size[0] = dst_size_luma;
                    dst_size[1] = dst_size_chroma;
                    dst_size[2] = dst_size_chroma;
                    dst_dpformat = DP_COLOR_YV12;
                }
                break;

            case HAL_PIXEL_FORMAT_YUYV:
                dst_pitch = stride * 2;
                dst_plane = 1;
                dst_size[0] = dst_pitch * height;
                dst_dpformat = DP_COLOR_YUYV;
                break;

            default:
                HWC_LOGW("Output color format for DP is invalid (0x%x)", param->format);
                return;
        }

        if (secure)
        {
            void* dst_addr[3];
            dst_addr[0] = (void*)(uintptr_t)param->mva;
            dst_addr[1] = (void*)(uintptr_t)param->mva;
            dst_addr[2] = (void*)(uintptr_t)param->mva;

            m_blit_stream.setDstBuffer(dst_addr, dst_size, dst_plane);
            is_dp_secure = DP_SECURE;
        }

        m_blit_stream.setDstBuffer(dst_ion_fd, dst_size, dst_plane);
    }
    //-----------------------------------------------------------

    DP_PROFILE_ENUM dp_range = mapDpColorRange(param->color_range);

    DpRect dst_dp_roi;
    int width = param->dst_crop.getWidth();
    int height = param->dst_crop.getHeight();

    dst_dp_roi.x = param->dst_crop.left;
    dst_dp_roi.y = param->dst_crop.top;
#if 1
    dst_dp_roi.w = width;
    dst_dp_roi.h = height;
#else
    // TODO: need to align 2 bytes?
    dst_dp_roi.w = ALIGN_FLOOR(param->dst_crop.getWidth(), 2);
    dst_dp_roi.h = ALIGN_FLOOR(param->dst_crop.getHeight(), 2);
#endif

    // [NOTE] setDstConfig provides y and uv pitch configuration
    // if uv pitch is 0, DP would calculate it according to y pitch
    // ROI designates the dimension and the position of the bitblited image
    m_blit_stream.setDstConfig(
        dst_dp_roi.w, dst_dp_roi.h,
        dst_pitch, dst_pitch_uv,
        dst_dpformat, dp_range,
        eInterlace_None, &dst_dp_roi, is_dp_secure, false);

    m_cur_params.dst_ion_fd = dst_ion_fd;
    m_cur_params.dst_fence_index = param->fence_index;
    m_cur_params.dst_fmt = param->format;
    m_cur_params.dst_crop = param->dst_crop;
    m_cur_params.dst_range = param->color_range;
    m_cur_params.dst_is_need_flush = false;
    m_cur_params.dst_is_secure = secure;
    m_cur_params.job_sequence = param->sequence;
    // extension mode
    if (DisplayManager::m_profile_level & PROFILE_TRIG)
    {
        char atrace_tag[256];
        sprintf(atrace_tag, "BLT-SMS");
        HWC_ATRACE_ASYNC_BEGIN(atrace_tag, param->sequence);
    }
}

void BlitDevice::disableOverlayOutput(int /*dpy*/)
{
}

void BlitDevice::prepareOverlayPresentFence(int dpy, OverlayPrepareParam* param)
{
    checkValidDpyRetNon(dpy);

    checkValidSessionRetNon(dpy, m_session_id);

    param->fence_index = -1;
    param->fence_fd    = -1;

    HWC_LOGD("(%d) Prepare Present Fence (id=0x%x)", dpy, m_session_id);
}

status_t BlitDevice::waitVSync(int /*dpy*/, nsecs_t* /*ts*/)
{
    HWC_LOGE("Do not call %s", __func__);
    return INVALID_OPERATION;
}

void BlitDevice::setPowerMode(int /*dpy*/, int /*mode*/)
{
    HWC_LOGE("setPowerMode error! (id=0x%x)", m_session_id);
}


disp_caps_info* BlitDevice::getCapsInfo()
{
    return &m_caps_info;
}

bool BlitDevice::queryValidLayer(disp_layer_info* /*disp_layer*/)
{
    HWC_LOGE("Do NOT call %s", __func__);
    return false;
}

status_t BlitDevice::waitAllJobDone(const int /*dpy*/)
{
    return NO_ERROR;
}

void BlitDevice::setLastValidColorTransform(const int32_t& /*dpy*/, sp<ColorTransform> /*color_transform*/)
{
    HWC_LOGE("Do NOT call %s", __func__);
}

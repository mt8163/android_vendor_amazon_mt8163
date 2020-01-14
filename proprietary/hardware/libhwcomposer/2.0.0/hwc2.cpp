#define DEBUG_LOG_TAG "HWC"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#define LOG_TAG "hwcomposer"
#include <cstring>
#include <sstream>
#include <cutils/properties.h>
#include <sync/sync.h>
#include <sw_sync.h>
#include <sys/resource.h>

#include "gralloc_mtk_defs.h"

#include "ui/Rect.h"

#include "utils/debug.h"
#include "utils/tools.h"
#include "utils/devicenode.h"

#include "hwc2.h"
#include "hwdev.h"
#include "platform.h"
#include "display.h"
#include "overlay.h"
#include "dispatcher.h"
#include "worker.h"
#include "composer.h"
#include "bliter.h"
#include "bliter_ultra.h"
#include "blitdev.h"
#include "asyncblitdev.h"
#include "sync.h"

#include "utils/transform.h"
#include "ui/gralloc_extra.h"
#include "ui/Region.h"
#include <utils/SortedVector.h>
#include <utils/String8.h>
// todo: cache
// #include "cache.h"
#include <linux/disp_session.h>

#ifdef USES_PQSERVICE
#include <vendor/mediatek/hardware/pq/2.0/IPictureQuality.h>
using android::hardware::hidl_array;
using vendor::mediatek::hardware::pq::V2_0::IPictureQuality;
using vendor::mediatek::hardware::pq::V2_0::Result;
#endif

int32_t checkMirrorPath(const vector<sp<HWCDisplay> >& displays, bool *ultra_scenario);

const char* HWC2_PRESENT_VALI_STATE_PRESENT_DONE_STR = "PD";
const char* HWC2_PRESENT_VALI_STATE_CHECK_SKIP_VALI_STR = "CSV";
const char* HWC2_PRESENT_VALI_STATE_VALIDATE_STR = "V";
const char* HWC2_PRESENT_VALI_STATE_VALIDATE_DONE_STR = "VD";
const char* HWC2_PRESENT_VALI_STATE_PRESENT_STR = "P";
const char* HWC2_PRESENT_VALI_STATE_UNKNOWN_STR = "UNK";

const char* getPresentValiStateString(const HWC_VALI_PRESENT_STATE& state)
{
    switch(state)
    {
        case HWC_VALI_PRESENT_STATE_PRESENT_DONE:
            return HWC2_PRESENT_VALI_STATE_PRESENT_DONE_STR;

        case HWC_VALI_PRESENT_STATE_CHECK_SKIP_VALI:
            return HWC2_PRESENT_VALI_STATE_CHECK_SKIP_VALI_STR;

        case HWC_VALI_PRESENT_STATE_VALIDATE:
            return HWC2_PRESENT_VALI_STATE_VALIDATE_STR;

        case HWC_VALI_PRESENT_STATE_VALIDATE_DONE:
            return HWC2_PRESENT_VALI_STATE_VALIDATE_DONE_STR;

        case HWC_VALI_PRESENT_STATE_PRESENT:
            return HWC2_PRESENT_VALI_STATE_PRESENT_STR;

        default:
            HWC_LOGE("%s unknown state:%d", __func__, state);
            return HWC2_PRESENT_VALI_STATE_UNKNOWN_STR;
    }
}

bool isDispConnected(const uint64_t& display)
{
    return DisplayManager::getInstance().m_data[display].connected;
}

void adjustFdLimit() {
    struct rlimit limit;

    if (0 == getrlimit(RLIMIT_NOFILE, &limit)) {
        limit.rlim_cur = limit.rlim_max;
        if (0 == setrlimit(RLIMIT_NOFILE, &limit)) {
            HWC_LOGI("FD resource: cur[%lu]  max[%lu]\n", limit.rlim_cur, limit.rlim_max);
        } else {
            HWC_LOGW("failed to set resource limitation");
        }
    } else {
        HWC_LOGW("failed to get resource limitation");
    }
}

// -----------------------------------------------------------------------------
std::atomic<int64_t> HWCLayer::id_count(0);

HWCBuffer::~HWCBuffer()
{
    if (getReleaseFenceFd() != -1)
        protectedClose(getReleaseFenceFd());

    if (getPrevReleaseFenceFd() != -1)
        protectedClose(getPrevReleaseFenceFd());
}

void HWCBuffer::setReleaseFenceFd(const int32_t& fence_fd, const bool& is_disp_connected)
{
    if (fence_fd >= 0 && m_release_fence_fd != -1)
    {
        if (is_disp_connected)
        {
            HWC_LOGE("(%" PRIu64 ") fdleak detect: %s layer_id:%d release_fence_fd:%d hnd:%p",
                m_disp_id, __func__, m_layer_id, m_release_fence_fd, m_hnd);
            AbortMessager::getInstance().abort();
        }
        else
        {
            HWC_LOGW("(%" PRIu64 ") fdleak detect: %s layer_id:%d release_fence_fd:%d hnd:%p",
                m_disp_id, __func__, m_layer_id, m_release_fence_fd, m_hnd);
            ::protectedClose(m_release_fence_fd);
            m_release_fence_fd = -1;
        }
    }
    m_release_fence_fd = fence_fd;
    HWC_LOGV("(%" PRIu64 ") fdleak detect: %s layer id:%d release_fence_fd:%d hnd:%p",
         m_disp_id, __func__, m_layer_id, m_release_fence_fd, m_hnd);
}

void HWCBuffer::setPrevReleaseFenceFd(const int32_t& fence_fd, const bool& is_disp_connected)
{
    if (fence_fd >= 0 && m_prev_release_fence_fd != -1)
    {
        if (is_disp_connected)
        {
            HWC_LOGE("(%" PRIu64 ") fdleak detect: %s layer id:%d prev_release_fence_fd:%d hnd:%p",
                m_disp_id, __func__, m_layer_id, m_prev_release_fence_fd, m_hnd);
            AbortMessager::getInstance().abort();
        }
        else
        {
            HWC_LOGE("(%" PRIu64 ") fdleak detect: %s layer id:%d prev_release_fence_fd:%d hnd:%p",
                m_disp_id, __func__, m_layer_id, m_prev_release_fence_fd, m_hnd);
            ::protectedClose(m_prev_release_fence_fd);
            m_prev_release_fence_fd = -1;
        }
    }
    m_prev_release_fence_fd = fence_fd;
    HWC_LOGV("(%" PRIu64 ") fdleak detect: %s layer id:%d prev_release_fence_fd:%d hnd:%p",
         m_disp_id, __func__, m_layer_id, m_prev_release_fence_fd, m_hnd);
}

void HWCBuffer::setAcquireFenceFd(const int32_t& fence_fd, const bool& is_disp_connected)
{
    if (fence_fd >= 0 && m_acquire_fence_fd != -1)
    {
        if (is_disp_connected)
        {
            HWC_LOGE("(%" PRIu64 ") fdleak detect: %s layer id:%d acquire_fence_fd:%d hnd:%p",
                m_disp_id, __func__, m_layer_id, m_acquire_fence_fd, m_hnd);
            AbortMessager::getInstance().abort();
        }
        else
        {
            HWC_LOGE("(%" PRIu64 ") fdleak detect: %s layer id:%d acquire_fence_fd:%d hnd:%p",
                m_disp_id, __func__, m_layer_id, m_acquire_fence_fd, m_hnd);
            ::protectedClose(m_acquire_fence_fd);
            m_acquire_fence_fd = -1;
        }
    }

    m_acquire_fence_fd = fence_fd;
    HWC_LOGV("(%" PRIu64 ") fdleak detect: %s layer id:%d acquire_fence_fd:%d hnd:%p",
        m_disp_id, __func__, m_layer_id, m_acquire_fence_fd, m_hnd);
}

int32_t HWCBuffer::afterPresent(const bool& is_disp_connected, const bool& is_ct)
{
    if (getAcquireFenceFd() > -1)
    {
        if (is_disp_connected)
        {
            HWC_LOGE("(%" PRIu64 ") error: layer(%d) has fd(%d)", m_disp_id, m_layer_id, m_acquire_fence_fd);
            return 1;
        }
        else
        {
            HWC_LOGE("(%" PRIu64 ") error: layer(%d) has fd(%d)", m_disp_id, m_layer_id, m_acquire_fence_fd);
            ::protectedClose(getAcquireFenceFd());
            setAcquireFenceFd(-1, is_disp_connected);
        }
    }

    setPrevHandle(getHandle());

    if (!is_ct)
    {
        setHandle(nullptr);
    }

    setBufferChanged(false);
    return 0;
}

// -----------------------------------------------------------------------------
HWCLayer::HWCLayer(const wp<HWCDisplay>& disp, const uint64_t& disp_id, const bool& is_ct)
    : m_mtk_flags(0)
    , m_id(++id_count)
    , m_is_ct(is_ct)
    , m_disp(disp)
    , m_hwlayer_type(HWC_LAYER_TYPE_NONE)
    , m_hwlayer_type_line(-1)
    , m_sf_comp_type(0)
    , m_dataspace(0)
    , m_blend(HWC2_BLEND_MODE_NONE)
    , m_plane_alpha(0.0f)
    , m_z_order(0)
    , m_transform(0)
    , m_state_changed(false)
    , m_disp_id(disp_id)
    , m_hwc_buf(new HWCBuffer(m_disp_id, m_id, is_ct))
    , m_is_visible(false)
    , m_sf_comp_type_call_from_sf(0)
    , m_last_comp_type_call_from_sf(0)
    , m_layer_caps(0)
    , m_layer_color(0)
{
    memset(&m_damage, 0, sizeof(m_damage));
    memset(&m_display_frame, 0, sizeof(m_display_frame));
    memset(&m_source_crop, 0, sizeof(m_source_crop));
    memset(&m_visible_region, 0, sizeof(m_visible_region));
    memset(&m_mdp_dst_roi, 0, sizeof(m_mdp_dst_roi));

    if (m_hwc_buf == nullptr)
        HWC_LOGE("%s allocate HWCBuffer for m_hwc_buf fail", __func__);
}

HWCLayer::~HWCLayer()
{
    if (m_damage.rects != nullptr)
        free((void*)m_damage.rects);

    if (m_visible_region.rects != nullptr)
        free((void*)m_visible_region.rects);
}

#define SET_LINE_NUM(RTLINE, TYPE) ({ \
                            *RTLINE = __LINE__; \
                            TYPE; \
                        })

String8 HWCLayer::toString8()
{
    auto& display_frame = getDisplayFrame();
    auto& src_crop = getSourceCrop();

    String8 ret;
    ret.appendFormat("id:%" PRIu64 " v:%d acq:%d hnd:%p,%d,%" PRIu64 " w:%d,%d h:%d,%d f:%u sz:%d z:%u c:%x %s(%s,%s%d,%s,%d) s[%.1f,%.1f,%.1f,%.1f]->d[%d,%d,%d,%d] t:%d d(s%d,b%d)",
        getId(),
        isVisible(),
        getAcquireFenceFd(),
        getHandle(),
        getPrivateHandle().ion_fd,
        getPrivateHandle().alloc_id,
        getPrivateHandle().width,
        getPrivateHandle().y_stride,
        getPrivateHandle().height,
        getPrivateHandle().vstride,
        getPrivateHandle().format,
        getPrivateHandle().size,
        getZOrder(),
        getLayerColor(),
        getCompString(getCompositionType()),
        getHWLayerString(getHwlayerType()),
        getCompString(getSFCompositionType()),
        isSFCompositionTypeCallFromSF(),
        getCompString(getLastCompTypeCallFromSF()),
        getHwlayerTypeLine(),
        src_crop.left,
        src_crop.top,
        src_crop.right,
        src_crop.bottom,
        display_frame.left,
        display_frame.top,
        display_frame.right,
        display_frame.bottom,
        getTransform(),
        isStateChanged(),
        isBufferChanged());
    return ret;
}

// return final transform rectify with prexform
uint32_t HWCLayer::getXform() const
{
    uint32_t xform = getTransform();
    const PrivateHandle& priv_hnd = getPrivateHandle();
    rectifyXformWithPrexform(&xform, priv_hnd.prexform);
    return xform;
}

bool HWCLayer::needRotate() const
{
    return getXform() != Transform::ROT_0;
}

bool HWCLayer::needScaling() const
{
    if (getXform() & HAL_TRANSFORM_ROT_90)
    {
        return (WIDTH(getSourceCrop()) != HEIGHT(getDisplayFrame())) ||
                (HEIGHT(getSourceCrop()) != WIDTH(getDisplayFrame()));
    }

    return (WIDTH(getSourceCrop()) != WIDTH(getDisplayFrame())) ||
            (HEIGHT(getSourceCrop()) != HEIGHT(getDisplayFrame()));
}

void HWCLayer::validate()
{
    const int& compose_level = Platform::getInstance().m_config.compose_level;
    int32_t line = -1;

    if (getSFCompositionType() == HWC2_COMPOSITION_CLIENT)
    {
        setHwlayerType(HWC_LAYER_TYPE_INVALID, __LINE__);
        return;
    }

    if (getSFCompositionType() == HWC2_COMPOSITION_SOLID_COLOR)
    {
        if (WIDTH(m_display_frame) <= 0 || HEIGHT(m_display_frame) <= 0)
        {
            setHwlayerType(HWC_LAYER_TYPE_INVALID, __LINE__);
            return;
        }
        setHwlayerType(HWC_LAYER_TYPE_DIM, __LINE__);

        return;
    }

    // checking handle cannot be placed before checking dim layer
    // because handle of dim layer is nullptr.
    if (getHwcBuffer() == nullptr || getHwcBuffer()->getHandle() == nullptr)
    {
        setHwlayerType(HWC_LAYER_TYPE_INVALID, __LINE__);
        return;
    }

    // for drm video
    sp<HWCDisplay> disp = m_disp.promote();
    if (disp == NULL)
        HWC_LOGE("%s: HWCDisplay Promoting failed!", __func__);

    const int32_t buffer_type = (getPrivateHandle().ext_info.status & GRALLOC_EXTRA_MASK_TYPE);

    if ((getPrivateHandle().usage & GRALLOC_USAGE_PROTECTED) && !disp->getSecure())
    {
        setHwlayerType(HWC_LAYER_TYPE_INVALID, __LINE__);
        return;
    }

    if (getPrivateHandle().usage & GRALLOC_USAGE_SECURE)
    {
        if (buffer_type == GRALLOC_EXTRA_BIT_TYPE_VIDEO ||
            buffer_type == GRALLOC_EXTRA_BIT_TYPE_CAMERA ||
            getPrivateHandle().format == HAL_PIXEL_FORMAT_YV12)
        {
            // for MM case
            if (DisplayManager::getInstance().getVideoHdcp() >
                DisplayManager::getInstance().m_data[disp->getId()].hdcp_version)
            {
                setHwlayerType(HWC_LAYER_TYPE_INVALID, __LINE__);
                return;
            }
        }
        else if (DisplayManager::getInstance().m_data[disp->getId()].hdcp_version > 0 &&
                 disp->getId() != HWC_DISPLAY_PRIMARY)
        {
            // for UI case
            setHwlayerType(HWC_LAYER_TYPE_INVALID, __LINE__);
            return;
        }
    }

    // to debug which layer has been selected as UIPQ layer
    if (HWCMediator::getInstance().m_features.global_pq &&
        (compose_level & COMPOSE_DISABLE_UI) == 0 &&
        HWC_DISPLAY_PRIMARY == m_disp_id &&
        (getPrivateHandle().ext_info.status2 & GRALLOC_EXTRA_BIT2_UI_PQ_ON) &&
        Platform::getInstance().m_config.uipq_debug)
    {
        setHwlayerType(HWC_LAYER_TYPE_UIPQ_DEBUG, __LINE__);
        return;
    }

    // for ui layer
    if ((compose_level & COMPOSE_DISABLE_UI) == 0 &&
        Platform::getInstance().isUILayerValid(this, &line))
    {
        if (getSFCompositionType() == HWC2_COMPOSITION_CURSOR)
        {
            setHwlayerType(HWC_LAYER_TYPE_CURSOR, __LINE__);
            return;
        }

        if (HWCMediator::getInstance().m_features.global_pq &&
            HWC_DISPLAY_PRIMARY == m_disp_id &&
            (getPrivateHandle().ext_info.status2 & GRALLOC_EXTRA_BIT2_UI_PQ_ON) &&
            WIDTH(m_display_frame) > 1 &&
            HEIGHT(m_display_frame) > 1)
        {
            setHwlayerType(HWC_LAYER_TYPE_UIPQ, __LINE__);
            return;
        }

        setHwlayerType(HWC_LAYER_TYPE_UI, __LINE__);
        return;
    }

    // for mdp layer
    if (compose_level & COMPOSE_DISABLE_MM)
    {
        setHwlayerType(HWC_LAYER_TYPE_INVALID, __LINE__);
        return;
     }

    if (Platform::getInstance().isMMLayerValid(this, &line))
    {
        setHwlayerType(HWC_LAYER_TYPE_MM, __LINE__);
        return;
    }

    setHwlayerType(HWC_LAYER_TYPE_INVALID, line == -1 ? line : line + 10000);
}

int32_t HWCLayer::afterPresent(const bool& is_disp_connected)
{
    // HWCLayer should check its acquire fence first!
    // SF may give a layer with zero width or height, and the layer is not in the
    // getVisibleLayersSortedByZ(). Therefore, its acquire fence is not processed.
    // HWC2 should process the acquire fence via afterPresent(). the task should
    // be done by HWCLayer::afterPresent() because HWCBuffer does NOT have display
    // frame information.
    int32_t needAbort = 0;
    if (getAcquireFenceFd() > -1)
    {
        auto& f = getDisplayFrame();
        if (f.left == f.right || f.top == f.bottom)
        {
            ::protectedClose(getAcquireFenceFd());
            setAcquireFenceFd(-1, is_disp_connected);
        }
        else
        {
            if (is_disp_connected)
            {
                HWC_LOGE("(%" PRIu64 ") unclose acquire fd(%d) of layer(%d)", m_disp_id, getAcquireFenceFd(), getId());
                return 1;
            }
            else
            {
                HWC_LOGW("(%" PRIu64 ") unclose acquire fd(%d) of layer(%d)", m_disp_id, getAcquireFenceFd(), getId());
                ::protectedClose(getAcquireFenceFd());
                setAcquireFenceFd(-1, is_disp_connected);
            }
        }
    }

    // Careful!!! HWCBuffer::afterPresent() should be behind of the layer with zero
    // width or height checking
    if (getHwcBuffer() != nullptr)
        needAbort = getHwcBuffer()->afterPresent(is_disp_connected, isClientTarget());

    setStateChanged(false);
    setVisible(false);
    return needAbort;
}

void HWCLayer::toBeDim()
{
    m_priv_hnd.format = HAL_PIXEL_FORMAT_DIM;
}

int32_t HWCLayer::getCompositionType() const
{
    switch (m_hwlayer_type) {
        case HWC_LAYER_TYPE_NONE:
            return HWC2_COMPOSITION_INVALID;

        case HWC_LAYER_TYPE_INVALID:
            return HWC2_COMPOSITION_CLIENT;

        case HWC_LAYER_TYPE_FBT:
        case HWC_LAYER_TYPE_UI:
        case HWC_LAYER_TYPE_MM:
        case HWC_LAYER_TYPE_DIM:
        case HWC_LAYER_TYPE_MM_HIGH:
        case HWC_LAYER_TYPE_MM_FBT:
        case HWC_LAYER_TYPE_UIPQ_DEBUG:
        case HWC_LAYER_TYPE_UIPQ:
            return HWC2_COMPOSITION_DEVICE;

        case HWC_LAYER_TYPE_CURSOR:
            return HWC2_COMPOSITION_CURSOR;

        case HWC_LAYER_TYPE_WORMHOLE:
            return HWC2_COMPOSITION_DEVICE;
    }
    return HWC2_COMPOSITION_CLIENT;
};

void HWCLayer::setSFCompositionType(const int32_t& sf_comp_type, const bool& call_from_sf)
{
    m_sf_comp_type = sf_comp_type;
    m_sf_comp_type_call_from_sf = call_from_sf;

    if (call_from_sf)
    {
        m_last_comp_type_call_from_sf = sf_comp_type;
    }
}

void HWCLayer::setHandle(const buffer_handle_t& hnd)
{
    m_hwc_buf->setHandle(hnd);
}

void HWCLayer::setReleaseFenceFd(const int32_t& fence_fd, const bool& is_disp_connected)
{
    m_hwc_buf->setReleaseFenceFd(fence_fd, is_disp_connected);
}

void HWCLayer::setPrevReleaseFenceFd(const int32_t& fence_fd, const bool& is_disp_connected)
{
    m_hwc_buf->setPrevReleaseFenceFd(fence_fd, is_disp_connected);
}

void HWCLayer::setAcquireFenceFd(const int32_t& acquire_fence_fd, const bool& is_disp_connected)
{
    m_hwc_buf->setAcquireFenceFd(acquire_fence_fd, is_disp_connected);
}

void HWCLayer::setDataspace(const int32_t& dataspace)
{
    if (m_dataspace != dataspace)
    {
        setStateChanged(true);
        m_dataspace = dataspace;
    }
}

void HWCLayer::setDamage(const hwc_region_t& damage)
{
    if (!isHwcRegionEqual(m_damage, damage))
    {
        setStateChanged(true);
        copyHwcRegion(&m_damage, damage);
    }
}

void HWCLayer::setBlend(const int32_t& blend)
{
    if (m_blend != blend)
    {
        setStateChanged(true);
        m_blend = blend;
    }
}

void HWCLayer::setDisplayFrame(const hwc_rect_t& display_frame)
{
    if (memcmp(&m_display_frame, &display_frame, sizeof(hwc_rect_t)) != 0)
    {
        setStateChanged(true);
        m_display_frame = display_frame;
    }
}

void HWCLayer::setSourceCrop(const hwc_frect_t& source_crop)
{
    if (memcmp(&m_source_crop, &source_crop, sizeof(hwc_frect_t)) != 0)
    {
        setStateChanged(true);
        m_source_crop = source_crop;
    }
}

void HWCLayer::setPlaneAlpha(const float& plane_alpha)
{
    if (m_plane_alpha != plane_alpha)
    {
        setStateChanged(true);
        m_plane_alpha = plane_alpha;
    }
}

void HWCLayer::setZOrder(const uint32_t& z_order)
{
    if (m_z_order != z_order)
    {
        setStateChanged(true);
        m_z_order = z_order;
    }
}

void HWCLayer::setTransform(const int32_t& transform)
{
    if (m_transform != transform)
    {
        setStateChanged(true);
        m_transform = transform;
    }
}

void HWCLayer::setVisibleRegion(const hwc_region_t& visible_region)
{
    if (!isHwcRegionEqual(m_visible_region, visible_region))
    {
        setStateChanged(true);
        copyHwcRegion(&m_visible_region, visible_region);
    }
}

void HWCLayer::setLayerColor(const hwc_color_t& color)
{
    uint32_t new_color = color.a << 24 | color.r << 16 | color.g << 8 | color.b;
    if (m_layer_color != new_color)
    {
        setStateChanged(true);
        m_layer_color = new_color;
    }
}

// -----------------------------------------------------------------------------

void findGlesRange(const vector<sp<HWCLayer> >& layers, int32_t* head, int32_t* tail)
{
    auto&& head_iter = find_if(layers.begin(), layers.end(),
        [](const sp<HWCLayer>& layer)
        {
            return layer->getCompositionType() == HWC2_COMPOSITION_CLIENT;
        });

    auto&& tail_iter = find_if(layers.rbegin(), layers.rend(),
        [](const sp<HWCLayer>& layer)
        {
            return layer->getCompositionType() == HWC2_COMPOSITION_CLIENT;
        });

    *head = head_iter != layers.end() ? head_iter - layers.begin() : -1;
    *tail = tail_iter != layers.rend() ? layers.rend() - tail_iter - 1 : -1;
}

inline uint32_t extendMDPCapacity(
    const vector<sp<HWCLayer> >& /*layers*/, const uint32_t& /*mm_layer_num*/,
    const uint32_t& camera_layer_num, const uint32_t& /*video_layer_num*/)
{
    if (Platform::getInstance().m_config.extend_mdp_capacity)
    {
        // rule 1: no layer came from camera
        if (camera_layer_num != 0)
        {
            return 0;
        }

        // rule 2: Primary diaplay only
        if (DisplayManager::getInstance().m_data[HWC_DISPLAY_EXTERNAL].connected ||
            DisplayManager::getInstance().m_data[HWC_DISPLAY_VIRTUAL].connected)
        {
            return 0;
        }

        return 1;
    }

    return 0;
}

HWCDisplay::HWCDisplay(const int64_t& disp_id, const int32_t& type)
    : m_mtk_flags(0)
    , m_type(type)
    , m_outbuf(nullptr)
    , m_is_validated(false)
    , m_disp_id(disp_id)
    , m_gles_head(-1)
    , m_gles_tail(-1)
    , m_retire_fence_fd(-1)
    , m_mir_src(-1)
    , m_power_mode(HWC2_POWER_MODE_ON)
    , m_color_transform_hint(HAL_COLOR_TRANSFORM_IDENTITY)
    , m_color_mode(HAL_COLOR_MODE_NATIVE)
    , m_need_av_grouping(false)
    , m_color_transform_ok(true)
    , m_color_transform(new ColorTransform(HAL_COLOR_TRANSFORM_IDENTITY, false))
    , m_ccorr_state(NOT_FBT_ONLY)
    , m_prev_available_input_layer_num(0)
    , m_vali_present_state(HWC_VALI_PRESENT_STATE_PRESENT_DONE)
    , m_is_visible_layer_changed(false)
{
    switch (disp_id)
    {
        case HWC_DISPLAY_PRIMARY:
            hwc2_layer_t id = -1;
            createLayer(&id, true);
            m_ct = getLayer(id);
    }
}

void HWCDisplay::init()
{
    switch (getId())
    {
        case HWC_DISPLAY_EXTERNAL:
            {
                hwc2_layer_t id = -1;
                createLayer(&id, true);
                m_ct = getLayer(id);
                m_mtk_flags = 0;
                m_is_validated = false;
                m_gles_head = -1;
                m_gles_tail = -1;
                m_retire_fence_fd = -1;
                m_mir_src = -1;
                // external display will not present until SF setPowerMode ON
                m_power_mode = HWC2_POWER_MODE_OFF;
                m_color_transform_hint = HAL_COLOR_TRANSFORM_IDENTITY;
                m_color_mode = HAL_COLOR_MODE_NATIVE;
            }
            break;

        case HWC_DISPLAY_VIRTUAL:
            {
                m_outbuf = new HWCBuffer(getId(), -1, false);
                hwc2_layer_t id = -1;
                createLayer(&id, true);
                m_ct = getLayer(id);
                m_mir_src = -1;
                m_mtk_flags = 0;
                m_is_validated = false;
                m_gles_head = -1;
                m_gles_tail = -1;
                m_retire_fence_fd = -1;
                m_mir_src = -1;
                m_power_mode = HWC2_POWER_MODE_ON;
                m_color_transform_hint = HAL_COLOR_TRANSFORM_IDENTITY;
                m_color_mode = HAL_COLOR_MODE_NATIVE;
            }
            break;
    }
}

void HWCDisplay::initPrevCompTypes()
{
    auto&& layers = getVisibleLayersSortedByZ();
    m_prev_comp_types.resize(layers.size());
    for (size_t i = 0; i < m_prev_comp_types.size(); ++i)
        m_prev_comp_types[i] = layers[i]->getCompositionType();
}

int32_t HWCDisplay::setColorTransform(const float* matrix, const int32_t& hint)
{
    m_color_transform_hint = hint;
    m_color_transform = new ColorTransform(matrix, hint, true);

    if (!HWCMediator::getInstance().m_features.is_support_pq ||
        !Platform::getInstance().m_config.support_color_transform ||
        getId() == HWC_DISPLAY_VIRTUAL)
    {
        m_color_transform_ok = (hint == HAL_COLOR_TRANSFORM_IDENTITY);
        m_color_transform->dirty = false;
        return HWC2_ERROR_UNSUPPORTED;
    }
    else
    {
#ifdef USES_PQSERVICE
        if (getId() == HWC_DISPLAY_PRIMARY)
        {
            sp<IPictureQuality> pq_service = IPictureQuality::tryGetService();
            if (pq_service == nullptr)
            {
                HWC_LOGE("cannot find PQ service!");
                m_color_transform_ok = false;
            }
            else
            {
                const int32_t dimension = 4;
                hidl_array<float, 4, 4> send_matrix;
                for (int32_t i = 0; i < dimension; ++i)
                {
                    DbgLogger logger(DbgLogger::TYPE_HWC_LOG, 'D', "matrix ");
                    for (int32_t j = 0; j < dimension; ++j)
                    {
                        send_matrix[i][j] = matrix[i * dimension + j];
                        logger.printf("%f,", send_matrix[i][j]);
                    }
                }
                m_color_transform_ok = (pq_service->setColorTransform(send_matrix, hint, 1) == Result::OK);
                HWC_LOGI("(%" PRIu64 " %s ) hint:%d ok:%d", getId(), __func__, hint, m_color_transform_ok);
                if (m_color_transform_ok)
                {
                    HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->setLastValidColorTransform(HWC_DISPLAY_PRIMARY, m_color_transform);
                }
            }
        }
        else
        {
            m_color_transform_ok = false;
        }
#else
        m_color_transform_ok = (hint == HAL_COLOR_TRANSFORM_IDENTITY);
#endif
    }
    return m_color_transform_ok ? HWC2_ERROR_NONE : HWC2_ERROR_UNSUPPORTED;
}

void HWCDisplay::setJobDisplayOrientation()
{
    const int32_t disp_id = static_cast<int32_t>(getId());
    auto&& layers = getVisibleLayersSortedByZ();
    DispatcherJob* job = HWCDispatcher::getInstance().getExistJob(disp_id);
    //HWC decides how MDP rotates the display WDMA output buffer to fit TV out in mirror path,
    //so orienatation of source and sink display is needed in the original solution. Unfortunately,
    //SF does NOT tell HWC about display orientation, so we must figure out the other solution without modification AOSP code.
    //The importance is how many degrees to rotate the WDMA buffer not the sigle display orientation.
    //Therefore, the single layer's transform on the source and sink display can be used in this case.
    //SF provides how a single layer should be rotated on the source and sink display to HWC,
    //and the rotation can be used for the WDMA output buffer,too.
    if (job != nullptr)
    {
        // job->disp_ori_rot has initialized with 0 when job create
        if (layers.size())
        {
            for (auto& layer : layers)
            {
                if (layer->getSFCompositionType() != HWC2_COMPOSITION_SOLID_COLOR)
                {
                    job->disp_ori_rot = layer->getTransform();
                    break;
                }
            }
        }
    }
}

void HWCDisplay::validate()
{
    if (!isConnected())
    {
        HWC_LOGE("%s: the display(%" PRId64 ") is not connected!", __func__, m_disp_id);
        return;
    }

    setValiPresentState(HWC_VALI_PRESENT_STATE_VALIDATE, __LINE__);
    m_is_validated = true;

    auto&& layers = getVisibleLayersSortedByZ();
    const int32_t disp_id = static_cast<int32_t>(getId());
    DispatcherJob* job = HWCDispatcher::getInstance().getExistJob(disp_id);
    const int32_t ovl_layer_max = (job != NULL) ? job->num_layers : 0;
    const bool force_gpu_compose = isForceGpuCompose();

    if (getMirrorSrc() != -1)
    {
        for (auto& layer : layers)
        {
            layer->setHwlayerType(HWC_LAYER_TYPE_WORMHOLE, __LINE__);
        }
    }
    else if (force_gpu_compose)
    {
        for (auto& layer : layers)
            layer->setHwlayerType(HWC_LAYER_TYPE_INVALID, __LINE__);
    }
    else
    {
        const uint32_t MAX_MM_NUM = 1;
        uint32_t mm_layer_num = 0;
        uint32_t camera_layer_num = 0;
        uint32_t video_layer_num = 0;
        for (auto& layer : layers)
        {
            layer->validate();

            if (layer->getHwlayerType() == HWC_LAYER_TYPE_MM ||
                layer->getHwlayerType() == HWC_LAYER_TYPE_MM_HIGH ||
                layer->getHwlayerType() == HWC_LAYER_TYPE_UIPQ)
                ++mm_layer_num;

            switch (layer->getPrivateHandle().ext_info.status & GRALLOC_EXTRA_MASK_TYPE)
            {
                case GRALLOC_EXTRA_BIT_TYPE_CAMERA:
                    ++camera_layer_num;
                break;

                case GRALLOC_EXTRA_BIT_TYPE_VIDEO:
                    ++video_layer_num;
                break;
            }
        }

        int mm_ui_num = 0;
        mm_ui_num = mm_layer_num - camera_layer_num - video_layer_num;

        // need to judge MDP capacity
        if (mm_layer_num > MAX_MM_NUM)
        {
            // for MM_UI layer too much case, we will set MM_UI layer into GLES
            if (mm_ui_num > 1 && mm_layer_num > 2 && layers.size() >= 4)
            {
                for (auto& layer : layers)
                {
                    if ((layer->getHwlayerType() == HWC_LAYER_TYPE_MM ||
                        layer->getHwlayerType() == HWC_LAYER_TYPE_UIPQ) &&
                        (((layer->getPrivateHandle().ext_info.status & GRALLOC_EXTRA_MASK_TYPE) != GRALLOC_EXTRA_BIT_TYPE_CAMERA) &&
                        ((layer->getPrivateHandle().ext_info.status & GRALLOC_EXTRA_MASK_TYPE) != GRALLOC_EXTRA_BIT_TYPE_VIDEO)))
                    {
                        layer->setHwlayerType(HWC_LAYER_TYPE_INVALID, __LINE__);
                        --mm_layer_num;
                    }
                }
            }

            // calculate extend mdp capacity for Primary display.
            uint32_t EXTEND_MAX_MM_NUM = disp_id == HWC_DISPLAY_PRIMARY ?
                extendMDPCapacity(layers, mm_layer_num, camera_layer_num, video_layer_num) : 0;

            mm_layer_num = 0;
            for (auto& layer : layers)
            {
                if (layer->getHwlayerType() == HWC_LAYER_TYPE_MM &&
                    ((layer->getPrivateHandle().usage & GRALLOC_USAGE_PROTECTED) != 0 ||
                     layer->getPrivateHandle().sec_handle != 0))
                {
                    // secure MM layer, never composed by GLES
                    ++mm_layer_num;
                }
                else
                {
                    if (mm_layer_num >= (MAX_MM_NUM + EXTEND_MAX_MM_NUM) &&
                        (layer->getHwlayerType() == HWC_LAYER_TYPE_MM ||
                         layer->getHwlayerType() == HWC_LAYER_TYPE_UIPQ))
                        layer->setHwlayerType(HWC_LAYER_TYPE_INVALID, __LINE__);

                    if (layer->getHwlayerType() == HWC_LAYER_TYPE_MM ||
                        layer->getHwlayerType() == HWC_LAYER_TYPE_UIPQ)
                        ++mm_layer_num;
                }
            }
        }
    }
/*
    auto&& print_layers = m_layers;
    for (auto& kv : print_layers)
    {
        auto& layer = kv.second;
        auto& display_frame = layer->getDisplayFrame();
        HWC_LOGD("(%d) layer id:%" PRIu64 " hnd:%x z:%d hwlayer type:%s(%s,%s) line:%d displayf:[%d,%d,%d,%d] tr:%d",
            getId(),
            layer->getId(),
            layer->getHandle(),
            layer->getZOrder(),
            getCompString(layer->getCompositionType()),
            getHWLayerString(layer->getHwlayerType()),
            getCompString(layer->getSFCompositionType()),
            layer->getHwlayerTypeLine(),
            display_frame.left,
            display_frame.top,
            display_frame.right,
            display_frame.bottom,
            layer->getTransform()
        );
    }
*/
    HWC_LOGD("(%" PRIu64 ") VAL/l:%d/max:%d/fg:%d", getId(), layers.size(), ovl_layer_max, force_gpu_compose);
}


inline static void fillHwLayer(
    const uint64_t& dpy, DispatcherJob* job, const sp<HWCLayer>& layer,
    const int& ovl_idx, const int& layer_idx, const int& ext_sel_layer)
{
    HWC_ATRACE_FORMAT_NAME("HWC(h:%p)", layer->getHandle());
    const PrivateHandle* priv_handle = &layer->getPrivateHandle();

    if (ovl_idx < 0 || ovl_idx >= job->num_layers) {
        HWC_LOGE("try to fill HWLayer with invalid index: 0x%x(0x%x)", ovl_idx, job->num_layers);
        abort();
    }
    HWLayer* hw_layer  = &job->hw_layers[ovl_idx];
    hw_layer->enable   = true;
    hw_layer->index    = layer_idx;
    hw_layer->type     = layer->getHwlayerType() != HWC_LAYER_TYPE_MM_HIGH ? layer->getHwlayerType() : HWC_LAYER_TYPE_MM;
    hw_layer->dirty    = HWCDispatcher::getInstance().decideDirtyAndFlush(
        dpy, priv_handle, ovl_idx, layer->isBufferChanged() || layer->isStateChanged(),
        layer->getHwlayerType(), layer->getId(), layer->getLayerCaps());
    hw_layer->hwc2_layer_id = layer->getId();

    if (HWCMediator::getInstance().m_features.global_pq)
    {
        if (layer->getHwlayerType() == HWC_LAYER_TYPE_UIPQ_DEBUG)
        {
            hw_layer->type = HWC_LAYER_TYPE_UI;
            hw_layer->enable = false;
            job->uipq_index = layer_idx;
        }
        if (layer->getHwlayerType() == HWC_LAYER_TYPE_UIPQ)
        {
            hw_layer->type = HWC_LAYER_TYPE_MM;
            hw_layer->dirty = true;
            job->uipq_index = layer_idx;
        }
    }

    if (layer->getHwlayerType() == HWC_LAYER_TYPE_MM ||
        layer->getHwlayerType() == HWC_LAYER_TYPE_UIPQ)
    {
        hw_layer->mdp_dst_roi.left = layer->getMdpDstRoi().left;
        hw_layer->mdp_dst_roi.top = layer->getMdpDstRoi().top;
        hw_layer->mdp_dst_roi.right = layer->getMdpDstRoi().right;
        hw_layer->mdp_dst_roi.bottom = layer->getMdpDstRoi().bottom;
    }
    hw_layer->ext_sel_layer = Platform::getInstance().m_config.enable_smart_layer ? ext_sel_layer : -1;
    hw_layer->layer_caps    = layer->getLayerCaps();
    hw_layer->layer_color = layer->getLayerColor();

    memcpy(&hw_layer->priv_handle, priv_handle, sizeof(PrivateHandle));

    HWC_LOGV("(%" PRIu64 ")   layer(%" PRIu64 ") hnd:%p caps:%d z:%d hw_layer->index:%d ovl_idx:%d  isenable:%d type:%d hwlayer->priv_handle->ion_fd:%d dirty:%d",
        dpy, layer->getId(), layer->getHandle(), layer->getLayerCaps(), layer->getZOrder(), hw_layer->index, ovl_idx, hw_layer->enable ,hw_layer->type, hw_layer->priv_handle.ion_fd, hw_layer->dirty);
}

inline void setupHwcLayers(const sp<HWCDisplay>& display, DispatcherJob* job)
{
    HWC_LOGV("(%" PRIu64 ") + setupHwcLayers", display->getId());
    int32_t gles_head = -1, gles_tail = -1;
    const uint64_t disp_id = display->getId();

    display->getGlesRange(&gles_head, &gles_tail);

    job->num_ui_layers = 0;
    job->num_mm_layers = 0;
    const bool hrt_from_driver =
        (0 != (HWCMediator::getInstance().getOvlDevice(disp_id)->getCapsInfo()->disp_feature & DISP_FEATURE_HRT));

    auto&& layers = display->getCommittedLayers();

    int ovl_index = 0;
    for (size_t i = 0; i < layers.size(); ++i, ++ovl_index)
    {
        sp<HWCLayer> layer = layers[i];
        if (layer->isClientTarget())
            continue;

        int ext_sel_layer = -1;

        int32_t ovl_id = 0;
        if (hrt_from_driver)
        {
            const int32_t hrt_idx = (gles_head == -1 || int32_t(i) < gles_head ? i : i + (gles_tail - gles_head + 1));
            ovl_id = job->layer_info.layer_config_list[hrt_idx].ovl_id;
            ext_sel_layer = job->layer_info.layer_config_list[hrt_idx].ext_sel_layer;
        }
        else
        {
            ovl_id = (gles_head == -1 || static_cast<int32_t>(i) < gles_head) ? ovl_index : ovl_index + 1;
            ext_sel_layer = -1;
        }
        HWC_LOGV("(%" PRIu64 ")   setupHwcLayers i:%d ovl_id:%d hrt_from_driver:%d gles_head:%d, ovl_index:%d",
            display->getId(), i, ovl_id, hrt_from_driver, gles_head, ovl_index);

        fillHwLayer(disp_id, job, layer, ovl_id, i, ext_sel_layer);

        const PrivateHandle* priv_handle = &layer->getPrivateHandle();
        // check if any protect layer exist
        job->protect |= (priv_handle->usage & GRALLOC_USAGE_PROTECTED);

        // check if need to enable secure composition
        job->secure |= (priv_handle->usage & GRALLOC_USAGE_SECURE);

        // check if need to set video timestamp
        int buffer_type = (priv_handle->ext_info.status & GRALLOC_EXTRA_MASK_TYPE);
        if (buffer_type == GRALLOC_EXTRA_BIT_TYPE_VIDEO)
            job->timestamp = priv_handle->ext_info.timestamp;

        const int32_t type = layer->getHwlayerType();
        switch (type)
        {
            case HWC_LAYER_TYPE_UI:
            case HWC_LAYER_TYPE_DIM:
            case HWC_LAYER_TYPE_CURSOR:
                ++job->num_ui_layers;
                break;

            case HWC_LAYER_TYPE_MM_HIGH:
                // [WORKAROUND]`
                job->force_wait = true;

            case HWC_LAYER_TYPE_MM:
                ++job->num_mm_layers;
                break;

            case HWC_LAYER_TYPE_UIPQ_DEBUG:
                if (HWCMediator::getInstance().m_features.global_pq)
                    ++job->num_ui_layers;
                else
                    HWC_LOGE("global pq feature not support!");
                break;

            case HWC_LAYER_TYPE_UIPQ:
                if (HWCMediator::getInstance().m_features.global_pq)
                    ++job->num_mm_layers;
                else
                    HWC_LOGE("global pq feature not support!");
                break;
        }
    }
    HWC_LOGV("(%" PRIu64 ") - setupHwcLayers", display->getId());
}

void HWCDisplay::setGlesRange(const int32_t& gles_head, const int32_t& gles_tail)
{
    m_gles_head = gles_head;
    m_gles_tail = gles_tail;

    if (gles_head == -1)
        return;

    HWC_LOGV("setGlesRange() gles_head:%d gles_tail:%d", gles_head, gles_tail);
    auto&& layers = getVisibleLayersSortedByZ();
    for (int32_t i = gles_head; i <= gles_tail; ++i)
    {
        auto& layer = layers[i];
        if (layer->getHwlayerType() != HWC_LAYER_TYPE_INVALID)
            layer->setHwlayerType(HWC_LAYER_TYPE_INVALID, __LINE__);
    }
}

static void calculateFbtRoi(
    const sp<HWCLayer>& fbt_layer,
    const vector<sp<HWCLayer> >& layers,
    const int32_t& gles_head, const int32_t& gles_tail,
    const bool& isDisabled, const int32_t& /*disp_width*/, const int32_t& /*disp_height*/)
{
    if (!HWCMediator::getInstance().m_features.fbt_bound ||
        isDisabled)
    {
        hwc_frect_t src_crop;
        src_crop.left = src_crop.top = 0;
        src_crop.right = fbt_layer->getPrivateHandle().width;
        src_crop.bottom = fbt_layer->getPrivateHandle().height;
        fbt_layer->setSourceCrop(src_crop);

        hwc_rect_t display_frame;
        display_frame.left = display_frame.top = 0;
        display_frame.right = fbt_layer->getPrivateHandle().width;
        display_frame.bottom = fbt_layer->getPrivateHandle().height;
        fbt_layer->setDisplayFrame(display_frame);
        return;
    }

    Region fbt_roi_region;
    for (int32_t i = gles_head; i <= gles_tail; ++i)
    {
        auto& layer = layers[i];
        auto& display_frame = layer->getDisplayFrame();
        Rect gles_layer_rect(
                display_frame.left,
                display_frame.top,
                display_frame.right,
                display_frame.bottom);
        fbt_roi_region = fbt_roi_region.orSelf(gles_layer_rect);
    }
    Rect fbt_roi = fbt_roi_region.getBounds();
    if (!fbt_roi.isEmpty())
    {
        hwc_frect_t src_crop;
        src_crop.left = fbt_roi.left;
        src_crop.top = fbt_roi.top;
        src_crop.right = fbt_roi.right;
        src_crop.bottom = fbt_roi.bottom;
        fbt_layer->setSourceCrop(src_crop);

        hwc_rect_t display_frame;
        display_frame.left = fbt_roi.left;
        display_frame.right = fbt_roi.right;
        display_frame.top = fbt_roi.top;
        display_frame.bottom = fbt_roi.bottom;
        fbt_layer->setDisplayFrame(display_frame);
    }
}

static void setupGlesLayers(const sp<HWCDisplay>& display, DispatcherJob* job)
{
    int32_t gles_head = -1, gles_tail = -1;
    display->getGlesRange(&gles_head, &gles_tail);
    if (gles_head == -1)
    {
        job->fbt_exist = false;
        return ;
    }

    if (gles_head != -1 && display->getClientTarget()->getHandle() == nullptr)
    {
        // SurfaceFlinger might not setClientTarget while VDS disconnect.
        if (display->getId() == HWC_DISPLAY_PRIMARY)
        {
            HWC_LOGE("(%" PRIu64 ") %s: HWC does not receive client target's handle, g[%d,%d]", display->getId(), __func__, gles_head, gles_tail);
            job->fbt_exist = false;
            return ;
        }
        else
        {
            HWC_LOGW("(%" PRIu64 ") %s: HWC does not receive client target's handle, g[%d,%d] acq_fd:%d", display->getId(), __func__, gles_head, gles_tail, display->getClientTarget()->getAcquireFenceFd());
        }
    }

    job->fbt_exist = true;

    auto&& visible_layers = display->getVisibleLayersSortedByZ();
    auto&& commit_layers = display->getCommittedLayers();

    // close acquire fence of gles layers
    {
        for (size_t i = gles_head ; int32_t(i) <= gles_tail; ++i)
        {
            auto& layer = visible_layers[i];
            const int32_t acquire_fence_fd = layer->getAcquireFenceFd();
            if (acquire_fence_fd != -1)
            {
                protectedClose(layer->getAcquireFenceFd());
                layer->setAcquireFenceFd(-1, display->isConnected());
            }
        }
    }

    const bool hrt_from_driver =
        (0 != (HWCMediator::getInstance().getOvlDevice(display->getId())->getCapsInfo()->disp_feature & DISP_FEATURE_HRT));
    const int32_t ovl_id = hrt_from_driver ? job->layer_info.layer_config_list[gles_head].ovl_id : gles_head;
    const int32_t fbt_hw_layer_idx = hrt_from_driver ? ovl_id : gles_head;
    HWC_LOGV("setupGlesLayers() gles[%d,%d] fbt_hw_layer_idx:%d", gles_head, gles_tail, fbt_hw_layer_idx);

    sp<HWCLayer> fbt_layer = display->getClientTarget();

    // set roi of client target
    const bool disable_fbt_roi = job->has_s3d_layer;
    calculateFbtRoi(fbt_layer, visible_layers, gles_head, gles_tail, disable_fbt_roi, display->getWidth(), display->getHeight());

    if (fbt_hw_layer_idx < 0 || fbt_hw_layer_idx >= job->num_layers) {
        HWC_LOGE("try to fill HWLayer with invalid index for client target: 0x%x(0x%x)", fbt_hw_layer_idx, job->num_layers);
        abort();
    }
    HWLayer* fbt_hw_layer = &job->hw_layers[fbt_hw_layer_idx];
    HWC_ATRACE_FORMAT_NAME("GLES CT(h:%p)");
    fbt_hw_layer->enable  = true;
    fbt_hw_layer->index   = commit_layers.size() - 1;
    fbt_hw_layer->hwc2_layer_id = fbt_layer->getId();
    // sw tcon is too slow, so we want to use MDP to backup this framebuffer.
    // then SurfaceFlinger can use this framebuffer immediately.
    // therefore change its composition type to mm
    if (DisplayManager::getInstance().m_data[display->getId()].subtype == HWC_DISPLAY_EPAPER)
    {
        fbt_hw_layer->type = HWC_LAYER_TYPE_MM_FBT;
        job->mm_fbt = true;
    }
    else
    {
        fbt_hw_layer->type = HWC_LAYER_TYPE_FBT;
    }
    fbt_hw_layer->dirty   = HWCDispatcher::getInstance().decideCtDirtyAndFlush(display->getId(), fbt_hw_layer_idx);
    fbt_hw_layer->ext_sel_layer = -1;

    const PrivateHandle* priv_handle = &fbt_layer->getPrivateHandle();
    memcpy(&fbt_hw_layer->priv_handle, priv_handle, sizeof(PrivateHandle));
    if (hrt_from_driver && Platform::getInstance().m_config.enable_smart_layer)
    {
        fbt_hw_layer->ext_sel_layer = job->layer_info.layer_config_list[gles_head].ext_sel_layer;
    }
}

void HWCDisplay::updateGlesRange()
{
    if (!isConnected())
        return;

    int32_t gles_head = -1, gles_tail = -1;
    findGlesRange(getVisibleLayersSortedByZ(), &gles_head, &gles_tail);
    HWC_LOGV("updateGlesRange() gles_head:%d , gles_tail:%d", gles_head, gles_tail);
    setGlesRange(gles_head, gles_tail);

    const int32_t disp_id = getId();
    DispatcherJob* job = HWCDispatcher::getInstance().getExistJob(disp_id);
    if (job != NULL)
    {
        job->layer_info.gles_head = gles_head;
        job->layer_info.gles_tail = gles_tail;
    }
}

void HWCDisplay::acceptChanges()
{
}

void HWCDisplay::setRetireFenceFd(const int32_t& retire_fence_fd, const bool& is_disp_connected)
{
    if (retire_fence_fd >= 0 && m_retire_fence_fd != -1)
    {
        if (is_disp_connected)
        {
            HWC_LOGE("(%" PRIu64 ") fdleak detect: %s retire_fence_fd:%d",
                getId(), __func__, m_retire_fence_fd);
            AbortMessager::getInstance().abort();
        }
        else
        {
            HWC_LOGW("(%" PRIu64 ") fdleak detect: %s retire_fence_fd:%d",
                getId(), __func__, m_retire_fence_fd);
            ::protectedClose(m_retire_fence_fd);
            m_retire_fence_fd = -1;
        }
    }
    m_retire_fence_fd = retire_fence_fd;
}

void HWCDisplay::setColorTransformForJob(DispatcherJob* const job)
{
    // ccorr state transition
    const bool fbt_only =
        job->fbt_exist && job->num_ui_layers + job->num_mm_layers + getInvisibleLayersSortedByZ().size() == 0;
    if (fbt_only)
    {
        switch (m_ccorr_state)
        {
            case FIRST_FBT_ONLY:
                m_ccorr_state = STILL_FBT_ONLY;
                break;

            case NOT_FBT_ONLY:
                m_ccorr_state = FIRST_FBT_ONLY;
                break;

            default:
                break;
        }
    }
    else
    {
        switch (m_ccorr_state)
        {
            case FIRST_FBT_ONLY:
            case STILL_FBT_ONLY:
                m_ccorr_state = NOT_FBT_ONLY;
                break;

            default:
                break;
        }
    }

    // ccorr state action
    job->color_transform = nullptr;
    switch (m_ccorr_state)
    {
        case FIRST_FBT_ONLY:
            {
                sp<ColorTransform> color = new ColorTransform(HAL_COLOR_TRANSFORM_IDENTITY, true);
                job->color_transform = color;
                m_color_transform->dirty = true;
            }
            break;

        case STILL_FBT_ONLY:
            job->color_transform = nullptr;
            break;

        case NOT_FBT_ONLY:
            if (m_color_transform != nullptr && m_color_transform->dirty)
            {
                sp<ColorTransform> color = new ColorTransform(
                        m_color_transform->matrix,
                        m_color_transform->hint,
                        true);
                m_color_transform->dirty = false;
                job->color_transform = color;
            }
            break;
    }
}

void HWCDisplay::beforePresent(const int32_t num_validate_display)
{
    if (getMirrorSrc() != -1)
    {
        auto&& layers = getVisibleLayersSortedByZ();
        for (auto& layer : layers)
        {
            const int32_t acquire_fence_fd = layer->getAcquireFenceFd();
            if (acquire_fence_fd != -1)
            {
                protectedClose(acquire_fence_fd);
                layer->setAcquireFenceFd(-1, isConnected());
            }
        }
    }

    if (getId() == HWC_DISPLAY_VIRTUAL)
    {
        if (getOutbuf() == nullptr)
        {
            HWC_LOGE("(%" PRIu64 ") outbuf missing", getId());
            clearAllFences();
            return;
        }
        else if (getOutbuf()->getHandle() == nullptr)
        {
            HWC_LOGE("(%" PRIu64 ") handle of outbuf missing", getId());
            clearAllFences();
            return;
        }
    }

    updateGlesRange();

    DispatcherJob* job = HWCDispatcher::getInstance().getExistJob(getId());
    if (NULL != job)
    {
        HWCDispatcher::getInstance().initPrevHwLayers(this, job);

        getGlesRange(&job->layer_info.gles_head, &job->layer_info.gles_tail);

        if (getMirrorSrc() != -1)
        {
            // prepare job in job group
            job->fbt_exist     = false;
            job->num_ui_layers = 0;
            // [NOTE] treat mirror source as mm layer
            job->num_mm_layers = 1;
            job->hw_layers[0].enable = true;
            job->hw_layers[0].type = HWC_LAYER_TYPE_MM;
            job->disp_mir_id   = getMirrorSrc();
            //job->disp_ori_rot   = (getMtkFlags() & HWC_ORIENTATION_MASK) >> 16;

            HWCDispatcher::getInstance().configMirrorJob(job);
        }
        else
        {
            setupHwcLayers(this, job);
            setupGlesLayers(this, job);

            job->disp_mir_id    = HWC_MIRROR_SOURCE_INVALID;
            //job->disp_ori_rot   = (getMtkFlags() & HWC_ORIENTATION_MASK) >> 16;
            job->post_state     = job->layer_info.disp_dirty ? HWC_POST_INPUT_DIRTY : HWC_POST_INPUT_NOTDIRTY;

            // [WORKAROUND]
            // No need to force wait since UI layer does not exist
            if (job->force_wait && !job->num_ui_layers)
                job->force_wait = false;

            // NOTE: enable this profiling to observe hwc recomposition
            if (DisplayManager::m_profile_level & PROFILE_TRIG)
            {
                char tag[16];
                snprintf(tag, sizeof(tag), "HWC_COMP_%" PRIu64, getId());
                ATRACE_INT(tag, job->layer_info.disp_dirty ? 1 : 0);
            }
        }

        HWCDispatcher::getInstance().fillPrevHwLayers(this, job);

        setColorTransformForJob(job);

        if (job->layer_info.max_overlap_layer_num == -1)
        {
            job->layer_info.max_overlap_layer_num = job->num_ui_layers
                                                  + job->num_mm_layers
                                                  + (job->fbt_exist ? 1 : 0);
        }

        job->is_full_invalidate =
            HWCMediator::getInstance().getOvlDevice(getId())->isPartialUpdateSupported() ? isGeometryChanged() : true;

        if (needDoAvGrouping(num_validate_display))
            job->need_av_grouping = true;

        HWC_LOGD("(%" PRIu64 ") VAL list=%d/max=%d/fbt=%d[%d,%d:%d,%d](%s)/ui=%d/mm=%d/ovlp=%d/fi=%d/mir=%d",
            getId(), getVisibleLayersSortedByZ().size(), job->num_layers,
            job->fbt_exist,job->layer_info.hwc_gles_head, job->layer_info.hwc_gles_tail, job->layer_info.gles_head, job->layer_info.gles_tail, job->mm_fbt ? "MM" : "OVL",
            job->num_ui_layers, job->num_mm_layers, job->layer_info.max_overlap_layer_num,
            job->is_full_invalidate, job->disp_mir_id);

        // todo:
        // if (HWCDispatcher::getInstance().m_ultra_scenario &&
        //        Platform::getInstance().m_config.use_async_bliter &&
        //        Platform::getInstance().m_config.use_async_bliter_ultra)
        // {
        //    check_ultra_mdp(num_display, displays);
        // }
        setPrevAvailableInputLayerNum(job->num_layers);
    }
    else
    {
        clearAllFences();
        HWC_LOGE("(%" PRIu64 ") job is null", getId());
    }
}

void HWCDisplay::present()
{
    setValiPresentState(HWC_VALI_PRESENT_STATE_PRESENT, __LINE__);

    if (getId() == HWC_DISPLAY_VIRTUAL)
    {
        if (getOutbuf() == nullptr)
        {
            HWC_LOGE("(%" PRIu64 ") outbuf missing", getId());
            return;
        }
        else if (getOutbuf()->getHandle() == nullptr)
        {
            HWC_LOGE("(%" PRIu64 ") handle of outbuf missing", getId());
            return;
        }
    }

    HWCDispatcher::getInstance().setJob(this);
}

void HWCDisplay::afterPresent()
{
    setLastCommittedLayers(getCommittedLayers());
    setMirrorSrc(-1);

    int32_t needAbort = 0;
    for (auto& kv : m_layers)
    {
        auto& layer = kv.second;
        needAbort += layer->afterPresent(isConnected());
    }
    if (needAbort) {
        HWC_LOGE("There is %d layers which acquire fence were not closed", needAbort);
        AbortMessager::getInstance().abort();
    }

    if (getOutbuf() != nullptr)
    {
        getOutbuf()->afterPresent(isConnected());
        if (getOutbuf()->getReleaseFenceFd() != -1)
        {
            if (isConnected())
            {
                HWC_LOGE("(%" PRIu64 ") %s getReleaseFenceFd:%d", getId(), __func__, getOutbuf()->getReleaseFenceFd());
                AbortMessager::getInstance().abort();
            }
            else
            {
                HWC_LOGW("(%" PRIu64 ") %s getReleaseFenceFd:%d", getId(), __func__, getOutbuf()->getReleaseFenceFd());
                ::protectedClose(getOutbuf()->getReleaseFenceFd());
                getOutbuf()->setReleaseFenceFd(-1, isConnected());
            }
        }
    }

    // just set as -1, do not close!!!
    if (getRetireFenceFd() > -1)
    {
        if (isConnected())
        {
            HWC_LOGE("(%" PRIu64 ") %s getRetireFenceFd():%d", getId(), __func__, getRetireFenceFd());
            AbortMessager::getInstance().abort();
        }
        else
        {

            HWC_LOGW("(%" PRIu64 ") %s getRetireFenceFd():%d", getId(), __func__, getRetireFenceFd());
            ::protectedClose(getRetireFenceFd());
            setRetireFenceFd(-1, isConnected());
        }
    }

#ifdef USES_PQSERVICE
    // check if need to refresh display
    if (HWCMediator::getInstance().m_features.global_pq &&
        HWCMediator::getInstance().m_features.is_support_pq &&
        getId() == HWC_DISPLAY_PRIMARY &&
        !DisplayManager::getInstance().m_data[HWC_DISPLAY_EXTERNAL].connected &&
        !DisplayManager::getInstance().m_data[HWC_DISPLAY_VIRTUAL].connected)
    {
        sp<IPictureQuality> pq_service = IPictureQuality::getService();
        if (pq_service == nullptr)
        {
            HWC_LOGE("cannot find PQ service for pq update!");
        }
        else
        {
            bool need_refresh = false;
            // UIPQ dc effect is gradient by frame, it may need refresh display
            // because dc status has not reach stable yet. Consider uipq layer
            // is composed by device or client, we check dc status after display
            // present. 0: unstable; 1: stable
            pq_service->getGlobalPQStableStatus(
                [&] (Result retval, int32_t stable_status)
                {
                    if (retval == Result::OK)
                    {
                        need_refresh = (0 == stable_status) ? true : false;
                    }
                    else
                    {
                        HWC_LOGE("getGlobalPQStableStatus failed!");
                    }
                });
            if (need_refresh)
            {
                // clear stable flag to avoid always refresh if bypass MDP by accident.
                if (pq_service->setGlobalPQStableStatus(1) != Result::OK)
                {
                    HWC_LOGE("setGlobalPQStableStatus failed!");
                }
                DisplayManager::getInstance().refresh(HWC_DISPLAY_PRIMARY);
            }
        }
    }
#endif
}

void HWCDisplay::clear()
{
    m_outbuf = nullptr;
    m_changed_comp_types.clear();
    m_layers.clear();
    m_ct = nullptr;
    m_prev_comp_types.clear();
    m_pending_removed_layers_id.clear();
}

bool HWCDisplay::isConnected() const
{
    return DisplayManager::getInstance().m_data[m_disp_id].connected;
}

bool HWCDisplay::isValidated() const
{
    return m_is_validated;
}

void HWCDisplay::removePendingRemovedLayers()
{
    AutoMutex l(m_pending_removed_layers_mutex);
    for (auto& layer_id : m_pending_removed_layers_id)
    {
        if (m_layers.find(layer_id) != m_layers.end())
        {
            auto& layer = m_layers[layer_id];
            if (layer->isVisible())
            {
                HWC_LOGE("(%" PRIu64 ") false removed layer %s", getId(), layer->toString8().string());
            }
            else
            {
#ifdef MTK_USER_BUILD
                HWC_LOGV("(%" PRIu64 ") %s: destroy layer id(%" PRIu64 ")", getId(), __func__, layer_id);
#else
                HWC_LOGD("(%" PRIu64 ") %s: destroy layer id(%" PRIu64 ")", getId(), __func__, layer_id);
#endif
            }
            layer = nullptr;
            m_layers.erase(layer_id);
        }
        else
        {
            HWC_LOGE("(%" PRIu64 ") %s: destroy layer id(%" PRIu64 ") failed", getId(), __func__, layer_id);
        }
    }
    m_pending_removed_layers_id.clear();
}

void HWCDisplay::getChangedCompositionTypes(
    uint32_t* out_num_elem,
    hwc2_layer_t* out_layers,
    int32_t* out_types) const
{
    if (out_num_elem != NULL)
        *out_num_elem = m_changed_comp_types.size();

    if (out_layers != NULL)
        for (size_t i = 0; i < m_changed_comp_types.size(); ++i)
            out_layers[i] = m_changed_comp_types[i]->getId();

    if (out_types != NULL)
    {
        for (size_t i = 0; i < m_changed_comp_types.size(); ++i)
        {
            out_types[i] = m_changed_comp_types[i]->getCompositionType();
        }
    }
}

sp<HWCLayer> HWCDisplay::getLayer(const hwc2_layer_t& layer_id)
{
    const auto& iter = m_layers.find(layer_id);
    if (iter == m_layers.end())
    {
        HWC_LOGE("(%" PRIu64 ") %s %" PRIu64, getId(), __func__, layer_id);
        for (auto& kv : m_layers)
        {
            auto& layer = kv.second;
            HWC_LOGE("(%" PRIu64 ") getLayer() %s", getId(), layer->toString8().string());
        }
        if (HWC_DISPLAY_EXTERNAL == getId())
        {
            HWC_LOGE("warning!!! external display getLayer failed!");
        }
        else
        {
            abort();
        }
    }
    return (iter == m_layers.end()) ? nullptr : iter->second;
}

void HWCDisplay::checkVisibleLayerChange(const std::vector<sp<HWCLayer> > &prev_visible_layers)
{
    m_is_visible_layer_changed = false;
    if (m_visible_layers.size() != prev_visible_layers.size())
    {
        m_is_visible_layer_changed = true;
    }
    else
    {
        for(size_t i = 0; i < prev_visible_layers.size(); i++)
        {
            if (prev_visible_layers[i]->getId() != m_visible_layers[i]->getId())
            {
                m_is_visible_layer_changed = true;
                break;
            }
        }
    }

    if (isVisibleLayerChanged())
    {
        for (auto& layer : getVisibleLayersSortedByZ())
        {
            layer->setStateChanged(true);
        }
    }
}

void HWCDisplay::buildVisibleAndInvisibleLayersSortedByZ()
{
    const std::vector<sp<HWCLayer> > prev_visible_layers(m_visible_layers);
    m_visible_layers.clear();
    {
        AutoMutex l(m_pending_removed_layers_mutex);
        for(auto &kv : m_layers)
        {
            auto& layer = kv.second;
            if (m_pending_removed_layers_id.find(kv.first) == m_pending_removed_layers_id.end() &&
                !layer->isClientTarget())
            {
                m_visible_layers.push_back(kv.second);
            }
        }
    }

    sort(m_visible_layers.begin(), m_visible_layers.end(),
        [](const sp<HWCLayer>& a, const sp<HWCLayer>& b)
        {
            return a->getZOrder() < b->getZOrder();
        });

    m_invisible_layers.clear();
    if (m_visible_layers.size() > 1 &&
        Platform::getInstance().m_config.remove_invisible_layers)
    {
        const uint32_t black_mask = 0x0;
        for (auto iter = m_visible_layers.begin(); iter != m_visible_layers.end();)
        {
            auto& layer = (*iter);
            if (layer->getSFCompositionType() == HWC2_COMPOSITION_SOLID_COLOR &&
                (((layer->getLayerColor() << 8) >> 8) | black_mask) == 0U)
            {
                m_invisible_layers.push_back(layer);
                m_visible_layers.erase(iter);
                continue;
            }
            break;
        }
    }

    checkVisibleLayerChange(prev_visible_layers);
}

const vector<sp<HWCLayer> >& HWCDisplay::getVisibleLayersSortedByZ()
{
    return m_visible_layers;
}

const vector<sp<HWCLayer> >& HWCDisplay::getInvisibleLayersSortedByZ()
{
    return m_invisible_layers;
}

void HWCDisplay::buildCommittedLayers()
{
    auto& visible_layers = getVisibleLayersSortedByZ();
    m_committed_layers.clear();
    for(auto &layer : visible_layers)
    {
        auto& f = layer->getDisplayFrame();
        HWC_LOGV("(%" PRIu64 ")  getCommittedLayers() i:%d f[%d,%d,%d,%d] is_ct:%d comp:%s, %d",
            getId(),
            layer->getId(),
            f.left,
            f.top,
            f.right,
            f.bottom,
            layer->isClientTarget(),
            getCompString(layer->getCompositionType()),
            getHWLayerString(layer->getHwlayerType()));
        if (f.left != f.right && f.top != f.bottom &&
            !layer->isClientTarget() &&
            (layer->getCompositionType() == HWC2_COMPOSITION_DEVICE ||
             layer->getCompositionType() == HWC2_COMPOSITION_CURSOR) &&
            layer->getHwlayerType() != HWC_LAYER_TYPE_WORMHOLE)
        {
            HWC_LOGV("(%" PRIu64 ")  getCommittedLayers() i:%d added",
                getId(), layer->getId());
            m_committed_layers.push_back(layer);
        }
    }

    sp<HWCLayer> ct = getClientTarget();
    HWC_LOGV("(%" PRIu64 ")  getCommittedLayers() ct handle:%p",
        getId(), ct->getHandle());
    if (ct->getHandle() != nullptr)
        m_committed_layers.push_back(ct);
}

const vector<sp<HWCLayer> >& HWCDisplay::getCommittedLayers()
{
    return m_committed_layers;
}

sp<HWCLayer> HWCDisplay::getClientTarget()
{
    if (m_layers.size() < 1)
    {
        HWC_LOGE("%s: there is no client target layer at display(%" PRId64 ")", __func__, m_disp_id);
        return nullptr;
    }
    return m_ct;
}

int32_t HWCDisplay::getWidth() const
{
    return DisplayManager::getInstance().m_data[m_disp_id].width;
}

int32_t HWCDisplay::getHeight() const
{
    return DisplayManager::getInstance().m_data[m_disp_id].height;
}

int32_t HWCDisplay::getVsyncPeriod() const
{
    return DisplayManager::getInstance().m_data[m_disp_id].refresh;
}

int32_t HWCDisplay::getDpiX() const
{
    return DisplayManager::getInstance().m_data[m_disp_id].xdpi;
}

int32_t HWCDisplay::getDpiY() const
{
    return DisplayManager::getInstance().m_data[m_disp_id].ydpi;
}

int32_t HWCDisplay::getSecure() const
{
    return DisplayManager::getInstance().m_data[m_disp_id].secure;
}

void HWCDisplay::setPowerMode(const int32_t& mode)
{
    // screen blanking based on early_suspend in the kernel
    HWC_LOGI("Display(%" PRId64 ") SetPowerMode(%d)", m_disp_id, mode);
    m_power_mode = mode;
    DisplayManager::getInstance().setDisplayPowerState(m_disp_id, mode);

    HWCDispatcher::getInstance().setPowerMode(m_disp_id, mode);

    DisplayManager::getInstance().setPowerMode(m_disp_id, mode);

    // disable mirror mode when display blanks
    if ((Platform::getInstance().m_config.mirror_state & MIRROR_DISABLED) != MIRROR_DISABLED)
    {
        if (mode == HWC2_POWER_MODE_OFF || HWC2_POWER_MODE_DOZE_SUSPEND == mode)
        {
            Platform::getInstance().m_config.mirror_state |= MIRROR_PAUSED;
        }
        else
        {
            Platform::getInstance().m_config.mirror_state &= ~MIRROR_PAUSED;
        }
    }
}

void HWCDisplay::setVsyncEnabled(const int32_t& enabled)
{
    DisplayManager::getInstance().requestVSync(m_disp_id, enabled);
}

void HWCDisplay::getType(int32_t* out_type) const
{
    *out_type = m_type;
}

int32_t HWCDisplay::createLayer(hwc2_layer_t* out_layer, const bool& is_ct)
{
    sp<HWCLayer> layer = new HWCLayer(this, getId(), is_ct);
    if(layer == nullptr)
    {
        HWC_LOGE("%s: Fail to alloc a layer", __func__);
        return HWC2_ERROR_NO_RESOURCES;
    }

    m_layers[layer->getId()] = layer;
    *out_layer = layer->getId();

    if (is_ct)
    {
        layer->setPlaneAlpha(1.0f);
        layer->setBlend(HWC2_BLEND_MODE_PREMULTIPLIED);
        layer->setHwlayerType(HWC_LAYER_TYPE_FBT, __LINE__);
    }
    HWC_LOGD("(%" PRIu64 ") %s out_layer:%" PRIu64, getId(), __func__, *out_layer);
    return HWC2_ERROR_NONE;
}

int32_t HWCDisplay::destroyLayer(const hwc2_layer_t& layer_id)
{
    HWC_LOGD("(%" PRIu64 ") %s layer:% " PRIu64, getId(), __func__, layer_id);

    AutoMutex l(m_pending_removed_layers_mutex);
    std::pair<std::set<uint64_t>::iterator, bool> ret = m_pending_removed_layers_id.insert(layer_id);
    if (ret.second == false)
    {
        HWC_LOGE("(%" PRIu64 ") To destroy layer id(%" PRIu64 ") twice", getId(), layer_id);
    }
    return HWC2_ERROR_NONE;
}

void HWCDisplay::clearAllFences()
{
    const int32_t retire_fence_fd = getRetireFenceFd();

    // if copyvds is false, if composition type is GLES only
    // HWC must take the acquire fence of client target as retire fence,
    // so retire fence cannot be closed.
    int32_t gles_head = -1, gles_tail = -1;
    getGlesRange(&gles_head, &gles_tail);
    if (retire_fence_fd != -1 &&
        !(getId() == HWC_DISPLAY_VIRTUAL && gles_head == 0 && gles_tail == static_cast<int32_t>(getVisibleLayersSortedByZ().size() - 1) &&
          !HWCMediator::getInstance().m_features.copyvds))
    {
        protectedClose(retire_fence_fd);
        setRetireFenceFd(-1, isConnected());
    }

    if (getOutbuf() != nullptr)
    {
        const int32_t outbuf_acquire_fence_fd = getOutbuf()->getAcquireFenceFd();
        if (outbuf_acquire_fence_fd != -1)
        {
            protectedClose(outbuf_acquire_fence_fd);
            getOutbuf()->setAcquireFenceFd(-1, isConnected());
        }

        const int32_t outbuf_release_fence_fd = getOutbuf()->getReleaseFenceFd();
        if (outbuf_release_fence_fd != -1)
        {
            protectedClose(outbuf_release_fence_fd);
            getOutbuf()->setReleaseFenceFd(-1, isConnected());
        }
    }

    for (auto& kv : m_layers)
    {
        auto& layer = kv.second;
        const int32_t release_fence_fd = layer->getReleaseFenceFd();
        if (release_fence_fd != -1)
        {
            protectedClose(release_fence_fd);
            layer->setReleaseFenceFd(-1, isConnected());
        }

        const int32_t acquire_fence_fd = layer->getAcquireFenceFd();
        if (acquire_fence_fd != -1) {
            protectedClose(acquire_fence_fd);
            layer->setAcquireFenceFd(-1, isConnected());
        }
    }
}

void HWCDisplay::clearDisplayFencesAndFbtFences()
{
    const int32_t retire_fence_fd = getRetireFenceFd();

    // if copyvds is false, if composition type is GLES only
    // HWC must take the acquire fence of client target as retire fence,
    // so retire fence cannot be closed.
    if (retire_fence_fd != -1 &&
        !(getMirrorSrc() == -1 && !HWCMediator::getInstance().m_features.copyvds))
    {
        ::protectedClose(retire_fence_fd);
        setRetireFenceFd(-1, isConnected());
    }

    if (getOutbuf() != nullptr)
    {
        const int32_t outbuf_acquire_fence_fd = getOutbuf()->getAcquireFenceFd();
        if (outbuf_acquire_fence_fd != -1)
        {
            ::protectedClose(outbuf_acquire_fence_fd);
            getOutbuf()->setAcquireFenceFd(-1, isConnected());
        }

        const int32_t outbuf_release_fence_fd = getOutbuf()->getReleaseFenceFd();
        if (outbuf_release_fence_fd != -1)
        {
            protectedClose(outbuf_release_fence_fd);
            getOutbuf()->setReleaseFenceFd(-1, isConnected());
        }
    }

    auto&& ct = getClientTarget();

    const int32_t release_fence_fd = ct->getReleaseFenceFd();
    if (release_fence_fd != -1)
    {
        ::protectedClose(release_fence_fd);
        ct->setReleaseFenceFd(-1, isConnected());
    }

    const int32_t acquire_fence_fd = ct->getAcquireFenceFd();
    if (acquire_fence_fd != -1) {
        ::protectedClose(acquire_fence_fd);
        ct->setAcquireFenceFd(-1, isConnected());
    }
}

void HWCDisplay::getReleaseFenceFds(
    uint32_t* out_num_elem,
    hwc2_layer_t* out_layer,
    int32_t* out_fence_fd)
{
    static bool flip = 0;

    if (!flip)
    {
        *out_num_elem = 0;
        for (auto& kv : m_layers)
        {
            auto& layer = kv.second;

            if (layer->isClientTarget())
                continue;

            if (layer->getPrevReleaseFenceFd() != -1)
                ++(*out_num_elem);
        }
    }
    else
    {
        int32_t out_fence_fd_cnt = 0;
        for (auto& kv : m_layers)
        {
            auto& layer = kv.second;

            if (layer->isClientTarget())
                continue;

            if (layer->getPrevReleaseFenceFd() != -1)
            {
                out_layer[out_fence_fd_cnt] = layer->getId();
                const int32_t prev_rel_fd = layer->getPrevReleaseFenceFd();
#ifdef USES_FENCE_RENAME
                const int32_t hwc_to_sf_rel_fd = sync_merge("HWC_to_SF_rel", prev_rel_fd, prev_rel_fd);
                if (hwc_to_sf_rel_fd < 0)
                {
                    HWC_LOGE("(%" PRIu64 ") %s layer(% " PRId64 ") merge fence failed", getId(), __func__, layer->getId());
                }
                out_fence_fd[out_fence_fd_cnt] = hwc_to_sf_rel_fd;
#else
                out_fence_fd[out_fence_fd_cnt] = ::dup(prev_rel_fd);
#endif
                ::protectedClose(prev_rel_fd);
                layer->setPrevReleaseFenceFd(-1, isConnected());
                // just set release fence fd as -1, and do not close it!!!
                // release fences cannot close here
                ++out_fence_fd_cnt;
            }
            layer->setPrevReleaseFenceFd(layer->getReleaseFenceFd(), isConnected());
            layer->setReleaseFenceFd(-1, isConnected());
        }
        //{
        //    DbgLogger logger(DbgLogger::TYPE_HWC_LOG, 'D',"(%d) hwc2::getReleaseFenceFds() out_num_elem:%d", getId(), *out_num_elem);
        //    for (int i = 0; i < *out_num_elem; ++i)
        //        logger.printf("(layer id:%d fence fd:%d) ",
        //                out_layer[i],
        //                out_fence_fd[i]);
        //}
    }
    flip = !flip;
}

void HWCDisplay::getClientTargetSupport(
    const uint32_t& /*width*/, const uint32_t& /*height*/,
    const int32_t& /*format*/, const int32_t& /*dataspace*/)
{
    /*
    auto& layer = getClientTarget();
    if (layer != nullptr)
        layer->setFormat
    */
}

void HWCDisplay::setOutbuf(const buffer_handle_t& handle, const int32_t& release_fence_fd)
{
    HWC_LOGV("(%" PRIu64 ") HWCDisplay::setOutbuf() handle:%x release_fence_fd:%d m_outbuf:%p", getId(), handle, release_fence_fd, m_outbuf.get());
    m_outbuf->setHandle(handle);
    m_outbuf->setupPrivateHandle();
    m_outbuf->setReleaseFenceFd(release_fence_fd, isConnected());
}

void HWCDisplay::dump(String8* dump_str)
{
    dump_str->appendFormat("Display(%" PRIu64 ")\n", getId());
    dump_str->appendFormat(" visible_layers:%zu invisible_layers:%zu commit_layers:%zu\n",
        getVisibleLayersSortedByZ().size(),
        getInvisibleLayersSortedByZ().size(),
        getCommittedLayers().size());

    dump_str->appendFormat("+----------+--------------+---------+-------+---------------------+---+\n");
    dump_str->appendFormat("| layer id |       handle |     fmt | blend |           comp      | tr|\n");
    for (auto& layer : getVisibleLayersSortedByZ())
    {
        dump_str->appendFormat("+----------+--------------+---------+-------+---------------------+---+\n");
        dump_str->appendFormat("|%9" PRId64 " | %12p |%8x | %5s | %3s(%4s,%3s,%5d) | %2d|\n",
            layer->getId(),
            layer->getHandle(),
            layer->getPrivateHandle().format,
            getBlendString(layer->getBlend()),
            getCompString(layer->getCompositionType()),
            getHWLayerString(layer->getHwlayerType()),
            getCompString(layer->getSFCompositionType()),
            layer->getHwlayerTypeLine(),
            layer->getTransform()
            );
    }

    dump_str->appendFormat("+----------+--------------+---------+-------+---------------------+---+\n");

    if (getInvisibleLayersSortedByZ().size())
    {
        dump_str->appendFormat("+----------+--------------+---------+-------+---------------------+---+\n");
        dump_str->appendFormat("| layer id |       handle |     fmt | blend |           comp      | tr|\n");
        for (auto& layer : getInvisibleLayersSortedByZ())
        {
            dump_str->appendFormat("+----------+--------------+---------+-------+---------------------+---+\n");
            dump_str->appendFormat("|%9" PRId64 " | %12p |%8x | %5s | %3s(%4s,%3s,%5d) | %2d|\n",
                    layer->getId(),
                    layer->getHandle(),
                    layer->getPrivateHandle().format,
                    getBlendString(layer->getBlend()),
                    getCompString(layer->getCompositionType()),
                    getHWLayerString(layer->getHwlayerType()),
                    getCompString(layer->getSFCompositionType()),
                    layer->getHwlayerTypeLine(),
                    layer->getTransform()
                    );
        }

        dump_str->appendFormat("+----------+--------------+---------+-------+---------------------+---+\n");
    }
}

bool HWCDisplay::needDoAvGrouping(const int32_t num_validate_display)
{
    if (!Platform::getInstance().m_config.av_grouping)
    {
        m_need_av_grouping = false;
        return m_need_av_grouping;
    }

    m_need_av_grouping = false;

    if ((getId() == HWC_DISPLAY_PRIMARY) && (num_validate_display == 1)) {
        auto&& layers = getVisibleLayersSortedByZ();
        int num_video_layer = 0;
        for (size_t i = 0; i < layers.size(); i++) {
            sp<HWCLayer> layer = layers[i];
            int type = layer->getPrivateHandle().ext_info.status & GRALLOC_EXTRA_MASK_TYPE;
            if (type == GRALLOC_EXTRA_BIT_TYPE_VIDEO) {
                num_video_layer++;
            }
        }
        if (num_video_layer == 1) {
            m_need_av_grouping = true;
        }
    }

    return m_need_av_grouping;
}

bool HWCDisplay::isForceGpuCompose()
{
    const int32_t disp_id = static_cast<int32_t>(getId());
    if (getColorTransformHint() != HAL_COLOR_TRANSFORM_IDENTITY &&
        !m_color_transform_ok)
    {
        return true;
    }

    if (!(Platform::getInstance().m_config.disable_color_transform_for_secondary_displays))
    {
        sp<HWCDisplay> vir_hwc_disp = HWCMediator::getInstance().getHWCDisplay(HWC_DISPLAY_VIRTUAL);
        if (getId() == HWC_DISPLAY_PRIMARY &&
            vir_hwc_disp != nullptr &&
            vir_hwc_disp->isValid() &&
            getColorTransformHint() != HAL_COLOR_TRANSFORM_IDENTITY)
        {
            return true;
        }

        sp<HWCDisplay> pri_hwc_disp = HWCMediator::getInstance().getHWCDisplay(HWC_DISPLAY_PRIMARY);
        if (getId() == HWC_DISPLAY_VIRTUAL &&
            pri_hwc_disp != nullptr &&
            pri_hwc_disp->getColorTransformHint() != HAL_COLOR_TRANSFORM_IDENTITY)
        {
            return true;
        }
    }

    if (DisplayManager::getInstance().m_data[disp_id].subtype == HWC_DISPLAY_EPAPER)
    {
        return true;
    }

    if (getId() > HWC_DISPLAY_PRIMARY &&
        !Platform::getInstance().m_config.is_support_ext_path_for_virtual)
    {
        return true;
    }

    return false;
}

void HWCDisplay::setupPrivateHandleOfLayers()
{
    ATRACE_CALL();
    auto layers = getVisibleLayersSortedByZ();

    if (layers.size() > 0)
    {
        // RGBA layer_0, alpha value don't care.
        auto& layer = layers[0];
        if (layer != nullptr &&
            layer->getHwcBuffer() != nullptr &&
            layer->getHwcBuffer()->getHandle() != nullptr &&
            layer->getPrivateHandle().format == HAL_PIXEL_FORMAT_RGBA_8888)
        {
            layer->getEditablePrivateHandle().format = HAL_PIXEL_FORMAT_RGBX_8888;
        }
    }

    for (auto& layer : layers)
    {
        const unsigned int prev_format = layer->getPrivateHandle().format;
#ifndef MTK_USER_BUILD
        char str[256];
        snprintf(str, 256, "setupPrivateHandle %d %d(%p)", layer->isStateChanged(), layer->isBufferChanged(), layer->getHandle());
        ATRACE_NAME(str);
#endif
        if (Platform::getInstance().m_config.always_setup_priv_hnd ||
            layer->isStateChanged() || layer->isBufferChanged())
        {
#ifndef MTK_USER_BUILD
            ATRACE_NAME("setupPrivateHandle");
#endif
            layer->setupPrivateHandle();
        }

        // opaque RGBA layer can be processed as RGBX
        if (layer->getBlend() == HWC2_BLEND_MODE_NONE &&
            layer->getPrivateHandle().format == HAL_PIXEL_FORMAT_RGBA_8888)
        {
            layer->getEditablePrivateHandle().format = HAL_PIXEL_FORMAT_RGBX_8888;
        }

        if (prev_format != layer->getPrivateHandle().format)
        {
            layer->setStateChanged(true);
        }
    }
}

void HWCDisplay::setValiPresentState(HWC_VALI_PRESENT_STATE val, const int32_t& line)
{
    DispatcherJob* job = HWCDispatcher::getInstance().getExistJob(static_cast<int32_t>(getId()));
    AbortMessager::getInstance().printf("(%" PRIu64 ") (L%d) set s:%s jobID:%d",
        m_disp_id, line, getPresentValiStateString(val), (job) ? job->sequence : -1);
    m_vali_present_state = val;
}
// -----------------------------------------------------------------------------
DisplayListener::DisplayListener(
    const HWC2_PFN_HOTPLUG callback_hotplug,
    const hwc2_callback_data_t callback_hotplug_data,
    const HWC2_PFN_VSYNC callback_vsync,
    const hwc2_callback_data_t callback_vsync_data,
    const HWC2_PFN_REFRESH callback_refresh,
    const hwc2_callback_data_t callback_refresh_data)
    : m_callback_hotplug(callback_hotplug)
    , m_callback_hotplug_data(callback_hotplug_data)
    , m_callback_vsync(callback_vsync)
    , m_callback_vsync_data(callback_vsync_data)
    , m_callback_refresh(callback_refresh)
    , m_callback_refresh_data(callback_refresh_data)
{
}

void DisplayListener::onVSync(int dpy, nsecs_t timestamp, bool enabled)
{
    if (HWC_DISPLAY_PRIMARY == dpy && enabled && m_callback_vsync)
    {
        m_callback_vsync(m_callback_vsync_data, dpy, timestamp);
    }

    HWCDispatcher::getInstance().onVSync(dpy);
}

void DisplayListener::onPlugIn(int dpy)
{
    HWCDispatcher::getInstance().onPlugIn(dpy);
}

void DisplayListener::onPlugOut(int dpy)
{
    HWCDispatcher::getInstance().onPlugOut(dpy);
}

void DisplayListener::onHotPlugExt(int dpy, int connected)
{
    if (m_callback_hotplug &&
        (dpy == HWC_DISPLAY_PRIMARY ||
         dpy == HWC_DISPLAY_EXTERNAL))
    {
        m_callback_hotplug(m_callback_hotplug_data, dpy, connected);
    }
}

void DisplayListener::onRefresh(int dpy)
{
    if (m_callback_refresh)
    {
        m_callback_refresh(m_callback_refresh_data, dpy);
    }
}

void DisplayListener::onRefresh(int dpy, unsigned int /*type*/)
{
    if (m_callback_refresh) {
        HWC_LOGI("fire a callback of refresh to SF");
        m_callback_refresh(m_callback_refresh_data, dpy);
    }
    HWCMediator::getInstance().addDriverRefreshCount();
}
// -----------------------------------------------------------------------------

ANDROID_SINGLETON_STATIC_INSTANCE(HWCMediator);

HWC2Api* g_hwc2_api = &HWCMediator::getInstance();

const char* g_set_buf_from_sf_log_prefix = "[HWC] setBufFromSf";
const char* g_set_comp_from_sf_log_prefix = "[HWC] setCompFromSf";

HWCMediator::HWCMediator()
    : m_need_validate(HWC_SKIP_VALIDATE_NOT_SKIP)
    , m_last_SF_validate_num(0)
    , m_validate_seq(0)
    , m_present_seq(0)
    , m_vsync_offset_state(true)
    , m_set_buf_from_sf_log(DbgLogger::TYPE_HWC_LOG, 'D', g_set_buf_from_sf_log_prefix)
    , m_set_comp_from_sf_log(DbgLogger::TYPE_HWC_LOG, 'D', g_set_comp_from_sf_log_prefix)
    , m_driver_refresh_count(0)
    , m_is_valied(false)
    , m_is_init_disp_manager(false)
    , m_callback_hotplug(nullptr)
    , m_callback_hotplug_data(nullptr)
    , m_callback_vsync(nullptr)
    , m_callback_vsync_data(nullptr)
    , m_callback_refresh(nullptr)
    , m_callback_refresh_data(nullptr)
{
    sp<IOverlayDevice> primary_disp_dev = &DispDevice::getInstance();
    sp<IOverlayDevice> virtual_disp_dev = nullptr;
    if (Platform::getInstance().m_config.blitdev_for_virtual)
    {
        if (Platform::getInstance().m_config.is_async_blitdev)
        {
            virtual_disp_dev = new AsyncBlitDevice();
        }
        else
        {
            virtual_disp_dev = new BlitDevice();
        }
    }
    else
    {
        virtual_disp_dev = &DispDevice::getInstance();
    }
    m_disp_devs.resize(3, primary_disp_dev);
    m_disp_devs[HWC_DISPLAY_VIRTUAL] = virtual_disp_dev;

    Debugger::getInstance();
    Debugger::getInstance().m_logger = new Debugger::LOGGER();

    m_displays.push_back(new HWCDisplay(HWC_DISPLAY_PRIMARY, HWC2_DISPLAY_TYPE_PHYSICAL));
    m_displays.push_back(new HWCDisplay(HWC_DISPLAY_EXTERNAL, HWC2_DISPLAY_TYPE_PHYSICAL));
    m_displays.push_back(new HWCDisplay(HWC_DISPLAY_VIRTUAL, HWC2_DISPLAY_TYPE_VIRTUAL));
    /*
    // check if virtual display could be composed by hwc
    status_t err = DispDevice::getInstance().createOverlaySession(HWC_DISPLAY_VIRTUAL);
    m_is_support_ext_path_for_virtual = (err == NO_ERROR);
    DispDevice::getInstance().destroyOverlaySession(HWC_DISPLAY_VIRTUAL);
    */

    m_capabilities.clear();
    char value[PROPERTY_VALUE_MAX];
    property_get("vendor.debug.hwc.is_skip_validate", value, "-1");
    if (-1 != atoi(value))
    {
        Platform::getInstance().m_config.is_skip_validate = atoi(value);
    }

    if (Platform::getInstance().m_config.is_skip_validate == 1)
    {
        m_capabilities.push_back(HWC2_CAPABILITY_SKIP_VALIDATE);
    }

    // set hwc pid to property for watchdog
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", getpid());
    int err = property_set("vendor.debug.sf.hwc_pid", buf);
    if (err < 0) {
        HWC_LOGI("failed to set HWC pid to debug.sf.hwc_pid");
    }

    memset(buf, '\0', 16);
    if (Platform::getInstance().m_config.latch_unsignaled_buffer)
        snprintf(buf, sizeof(buf), "%d", 1);
    else
        snprintf(buf, sizeof(buf), "%d", 0);
    err = property_set("vendor.debug.sf.latch_unsignaled", buf);
    if (err < 0) {
        HWC_LOGI("failed to set vendor.debug.sf.latch_unsignaled");
    }

    adjustFdLimit();
}

HWCMediator::~HWCMediator()
{
}

void HWCMediator::addHWCDisplay(const sp<HWCDisplay>& display)
{
    m_displays.push_back(display);
}

void HWCMediator::open(/*hwc_private_device_t* device*/)
{
}

void HWCMediator::close(/*hwc_private_device_t* device*/)
{
}

void HWCMediator::getCapabilities(
        uint32_t* out_count,
        int32_t* /*hwc2_capability_t*/ out_capabilities)
{
    if (out_capabilities == NULL)
    {
        *out_count = m_capabilities.size();
        return;
    }

    for(uint32_t i = 0; i < *out_count; ++i)
    {
        out_capabilities[i] = m_capabilities[i];
    }
}

bool HWCMediator::hasCapabilities(int32_t capability)
{
    for (size_t i = 0; i < m_capabilities.size(); ++i)
    {
        if (m_capabilities[i] == capability)
        {
            return true;
        }
    }

    return false;
}

void HWCMediator::createExternalDisplay()
{
    if (m_displays[HWC_DISPLAY_EXTERNAL]->isConnected())
    {
        HWC_LOGE("external display is already connected %s", __func__);
        abort();
    }
    else
    {
        m_displays[HWC_DISPLAY_EXTERNAL]->init();
    }
}

void HWCMediator::destroyExternalDisplay()
{
    if (m_displays[HWC_DISPLAY_EXTERNAL]->isConnected())
    {
        HWC_LOGE("external display is not disconnected %s", __func__);
        abort();
    }
    else
    {
        m_displays[HWC_DISPLAY_EXTERNAL]->clear();
    }
}

/* Device functions */
int32_t /*hwc2_error_t*/ HWCMediator::deviceCreateVirtualDisplay(
    hwc2_device_t* /*device*/,
    uint32_t width,
    uint32_t height,
    int32_t* /*android_pixel_format_t*/ format,
    hwc2_display_t* outDisplay)
{
    *outDisplay = HWC_DISPLAY_VIRTUAL;

    if (m_displays[*outDisplay]->isConnected())
    {
        return HWC2_ERROR_NO_RESOURCES;
    }

    if (Platform::getInstance().m_config.only_wfd_by_hwc &&
        !DisplayManager::getInstance().checkIsWfd())
    {
        return HWC2_ERROR_NO_RESOURCES;
    }

    if (width > HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->getMaxOverlayWidth() ||
        height > HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->getMaxOverlayHeight())
    {
        HWC_LOGI("(%" PRIu64 ") %s hwc not support width:%u x %u limit: %u x %u", *outDisplay, __func__, width, height,
            HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->getMaxOverlayWidth(),
            HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->getMaxOverlayHeight());
        return HWC2_ERROR_NO_RESOURCES;
    }

    if (Platform::getInstance().m_config.only_wfd_by_dispdev &&
        HWCMediator::getInstance().m_features.copyvds)
    {
        if (DisplayManager::getInstance().checkIsWfd())
        {
            if (getOvlDevice(*outDisplay)->getType() != OVL_DEVICE_TYPE_OVL)
            {
                m_disp_devs.pop_back();
                m_disp_devs.push_back(&DispDevice::getInstance());
                HWC_LOGI("virtual display change to use DispDevice");
            }
        }
        else
        {
            if (getOvlDevice(*outDisplay)->getType() != OVL_DEVICE_TYPE_BLITDEV)
            {
                m_disp_devs.pop_back();
                if (Platform::getInstance().m_config.is_async_blitdev)
                {
                    m_disp_devs.push_back(new AsyncBlitDevice());
                    HWC_LOGI("virtual display change to use AsyncBlitDevice");
                }
                else
                {
                    m_disp_devs.push_back(new BlitDevice());
                    HWC_LOGI("virtual display change to use BlitDevice");
                }
            }
        }
    }

    HWC_LOGI("(%" PRIu64 ") %s format:%d", *outDisplay, __func__, *format);
    m_displays[*outDisplay]->init();
    DisplayManager::getInstance().hotplugVir(
        HWC_DISPLAY_VIRTUAL, true, width, height, *format);
    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::deviceDestroyVirtualDisplay(
    hwc2_device_t* /*device*/,
    hwc2_display_t display)
{
    if (!m_displays[display]->isConnected())
    {
        return HWC2_ERROR_BAD_PARAMETER;
    }
    HWC_LOGI("(%" PRIu64 ") %s", display, __func__);
    const uint32_t width = 0, height = 0;
    const int32_t format = 0;
    DisplayManager::getInstance().hotplugVir(
        HWC_DISPLAY_VIRTUAL, false, width, height, format);
    m_displays[display]->clear();
    return HWC2_ERROR_NONE;
}

void HWCMediator::deviceDump(hwc2_device_t* /*device*/, uint32_t* outSize, char* outBuffer)
{
    static String8 m_dump_str;
    String8 dump_str;
    if (outBuffer)
    {
        memcpy(outBuffer, const_cast<char*>(m_dump_str.string()), m_dump_str.size());
        outBuffer[*outSize - 1] = '\0';
    }
    else
    {
        for (auto& display : m_displays)
        {
            if (!display->isConnected())
                continue;
            display->dump(&dump_str);
        }
        char value[PROPERTY_VALUE_MAX];

        // force full invalidate
        property_get("vendor.debug.hwc.forceFullInvalidate", value, "-1");
        if (-1 != atoi(value))
            Platform::getInstance().m_config.force_full_invalidate = atoi(value);

        property_get("vendor.debug.hwc.rgba_rotate", value, "-1");
        if (-1 != atoi(value))
            Platform::getInstance().m_config.enable_rgba_rotate = atoi(value);

        property_get("vendor.debug.hwc.rgbx_scaling", value, "-1");
        if (-1 != atoi(value))
            Platform::getInstance().m_config.enable_rgbx_scaling = atoi(value);

        // check compose level
        property_get("vendor.debug.hwc.compose_level", value, "-1");
        if (-1 != atoi(value))
            Platform::getInstance().m_config.compose_level = atoi(value);

        property_get("vendor.debug.hwc.enableUBL", value, "-1");
        if (-1 != atoi(value))
            Platform::getInstance().m_config.use_async_bliter_ultra = (0 != atoi(value));

        // switch AsyncBltUltraDebug
        property_get("vendor.debug.hwc.debugUBL", value, "-1");
        if (-1 != atoi(value))
            UltraBliter::getInstance().debug(atoi(value));

        property_get("vendor.debug.hwc.prexformUI", value, "-1");
        if (-1 != atoi(value))
            Platform::getInstance().m_config.prexformUI = atoi(value);

        property_get("vendor.debug.hwc.skip_log", value, "-1");
        if (-1 != atoi(value))
            Debugger::m_skip_log = atoi(value);

        // check mirror state
        property_get("vendor.debug.hwc.mirror_state", value, "-1");
        if (-1 != atoi(value))
            Platform::getInstance().m_config.mirror_state = atoi(value);

        // dynamic change mir format for mhl_output
        property_get("vendor.debug.hwc.mhl_output", value, "-1");
        if (-1 != atoi(value))
            Platform::getInstance().m_config.format_mir_mhl = atoi(value);

        // check profile level
        property_get("vendor.debug.hwc.profile_level", value, "-1");
        if (-1 != atoi(value))
            DisplayManager::m_profile_level = atoi(value);

        // check the maximum scale ratio of mirror source
        property_get("vendor.debug.hwc.mir_scale_ratio", value, "0");
        if (!(strlen(value) == 1 && value[0] == '0'))
            Platform::getInstance().m_config.mir_scale_ratio = strtof(value, NULL);

        property_get("persist.vendor.debug.hwc.log", value, "0");
        if (!(strlen(value) == 1 && value[0] == '0'))
            Debugger::getInstance().setLogThreshold(value[0]);

        // set disp secure for test
        property_get("vendor.debug.hwc.force_pri_secure", value, "-1");
        if (-1 != atoi(value))
            DisplayManager::getInstance().m_data[HWC_DISPLAY_PRIMARY].secure = atoi(value);

        property_get("vendor.debug.hwc.force_vir_secure", value, "-1");
        if (-1 != atoi(value))
            DisplayManager::getInstance().m_data[HWC_DISPLAY_VIRTUAL].secure = atoi(value);

        property_get("vendor.debug.hwc.ext_layer", value, "-1");
        if (-1 != atoi(value))
            Platform::getInstance().m_config.enable_smart_layer = atoi(value);

        // 0: All displays' jobs are dispatched when they are added into job queue
        // 1: Only external display's jobs are dispatched when external display's vsync is received
        // 2: external and wfd displays' jobs are dispatched when they receive VSync
        property_get("vendor.debug.hwc.trigger_by_vsync", value, "-1");
        if (-1 != atoi(value))
            m_features.trigger_by_vsync = atoi(value);

        property_get("vendor.debug.hwc.async_bliter", value, "-1");
        if (-1 != atoi(value))
        {
            Platform::getInstance().m_config.use_async_bliter = atoi(value);
        }

        property_get("vendor.debug.hwc.av_grouping", value, "-1");
        if (-1 != atoi(value))
        {
            Platform::getInstance().m_config.av_grouping = atoi(value);
        }

        // force hwc to wait fence for display
        property_get("vendor.debug.hwc.waitFenceForDisplay", value, "-1");
        if (-1 != atoi(value))
        {
            Platform::getInstance().m_config.wait_fence_for_display = atoi(value);
        }

        property_get("vendor.debug.hwc.always_setup_priv_hnd", value, "-1");
        if (-1 != atoi(value))
        {
            Platform::getInstance().m_config.always_setup_priv_hnd = atoi(value);
        }

        property_get("vendor.debug.hwc.disable_uipq_layer", value, "-1");
        if (-1 != atoi(value))
        {
            Platform::getInstance().m_config.uipq_debug = atoi(value);
        }

        property_get("vendor.debug.hwc.only_wfd_by_hwc", value, "-1");
        if (-1 != atoi(value))
        {
            Platform::getInstance().m_config.only_wfd_by_hwc = atoi(value);
        }

        property_get("vendor.debug.hwc.wdt_ioctl", value, "-1");
        if (-1 != atoi(value))
        {
            Platform::getInstance().m_config.wdt_ioctl = atoi(value);
        }

        property_get("vendor.debug.hwc.dump_buf", value, "-1");
        if ('-' != value[0])
        {
            if (value[0] == 'M' || value[0] == 'U' || value[0] == 'C')
            {
                Platform::getInstance().m_config.dump_buf_type = value[0];
                Platform::getInstance().m_config.dump_buf = atoi(value + 1);
            }
            else if(isdigit(value[0]))
            {
                Platform::getInstance().m_config.dump_buf_type = 'A';
                Platform::getInstance().m_config.dump_buf = atoi(value);
            }
        }
        else
        {
            Platform::getInstance().m_config.dump_buf_type = 'A';
            Platform::getInstance().m_config.dump_buf = 0;
        }

        property_get("vendor.debug.hwc.dump_buf_cont", value, "-1");
        if ('-' != value[0])
        {
            if (value[0] == 'M' || value[0] == 'U' || value[0] == 'C')
            {
                Platform::getInstance().m_config.dump_buf_cont_type = value[0];
                Platform::getInstance().m_config.dump_buf_cont = atoi(value + 1);
            }
            else if(isdigit(value[0]))
            {
                Platform::getInstance().m_config.dump_buf_cont_type = 'A';
                Platform::getInstance().m_config.dump_buf_cont = atoi(value);
            }
        }
        else
        {
            Platform::getInstance().m_config.dump_buf_cont_type = 'A';
            Platform::getInstance().m_config.dump_buf_cont = 0;
        }

        property_get("vendor.debug.hwc.dump_buf_log_enable", value, "-1");
        if (-1 != atoi(value))
        {
            Platform::getInstance().m_config.dump_buf_log_enable = atoi(value);
        }

        property_get("vendor.debug.hwc.fill_black_debug", value, "-1");
        if (-1 != atoi(value))
        {
            Platform::getInstance().m_config.fill_black_debug = atoi(value);
        }

        property_get("vendor.debug.hwc.is_skip_validate", value, "-1");
        if (-1 != atoi(value))
        {
            Platform::getInstance().m_config.is_skip_validate = atoi(value);

            if (Platform::getInstance().m_config.is_skip_validate == 0)
            {
                std::vector<int32_t>::iterator capbility = std::find(m_capabilities.begin(), m_capabilities.end(), HWC2_CAPABILITY_SKIP_VALIDATE);
                if (capbility != m_capabilities.end())
                {
                    m_capabilities.erase(capbility);
                }
            }
            else
            {
                if (hasCapabilities(HWC2_CAPABILITY_SKIP_VALIDATE) == false)
                {
                    m_capabilities.push_back(HWC2_CAPABILITY_SKIP_VALIDATE);
                }
            }
        }

        property_get("vendor.debug.hwc.color_transform", value, "-1");
        if (-1 != atoi(value))
        {
            Platform::getInstance().m_config.support_color_transform = atoi(value);
        }

        property_get("vendor.debug.hwc.enable_rpo", value, "-1");
        if (1 == atoi(value))
        {
            HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->getCapsInfo()->disp_feature |= DISP_FEATURE_RPO;
        }
        else if (0 == atoi(value))
        {
            HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->getCapsInfo()->disp_feature &= ~DISP_FEATURE_RPO;
        }

        property_get("vendor.debug.hwc.rpo_ui_max_src_width", value, "-1");
        if (-1 != atoi(value))
        {
            Platform::getInstance().m_config.rpo_ui_max_src_width = atoi(value);
        }

        property_get("vendor.debug.hwc.mdp_scale_percentage", value, "-1");
        const double num_double = atof(value);
        if (fabs(num_double - (-1)) > 0.05f)
        {
            Platform::getInstance().m_config.mdp_scale_percentage = num_double;
        }

        property_get("vendor.debug.hwc.extend_mdp_cap", value, "-1");
        if (-1 != atoi(value))
        {
            Platform::getInstance().m_config.extend_mdp_capacity = atoi(value);
        }

        property_get("vendor.debug.hwc.disp_support_decompress", value, "-1");
        if (-1 != atoi(value))
        {
            Platform::getInstance().m_config.disp_support_decompress = atoi(value);
        }

        property_get("vendor.debug.hwc.mdp_support_decompress", value, "-1");
        if (-1 != atoi(value))
        {
            Platform::getInstance().m_config.mdp_support_decompress = atoi(value);
        }

        property_get("vendor.debug.hwc.disable_color_transform_for_secondary_displays", value, "-1");
        if (-1 != atoi(value))
        {
            Platform::getInstance().m_config.disable_color_transform_for_secondary_displays = atoi(value);
        }

        property_get("vendor.debug.hwc.remove_invisible_layers", value, "-1");
        if (-1 != atoi(value))
        {
            Platform::getInstance().m_config.remove_invisible_layers = atoi(value);
        }

        m_hrt.dump(&dump_str);

        HWCDispatcher::getInstance().dump(&dump_str);
        dump_str.appendFormat("\n");
        Debugger::getInstance().dump(&dump_str);
        dump_str.appendFormat("\n[Driver Support]\n");
#ifndef MTK_USER_BUILD
        dump_str.appendFormat("  res_switch:%d\n", HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->isDispRszSupported());
        dump_str.appendFormat("  rpo:%d max_w,h:%d,%d ui_max_src_width:%d\n",
            HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->isDispRpoSupported(),
            HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->getCapsInfo()->rsz_in_max[0],
            HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->getCapsInfo()->rsz_in_max[1],
            Platform::getInstance().m_config.rpo_ui_max_src_width);
        dump_str.appendFormat("  partial_update:%d\n", HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->isPartialUpdateSupported());
        dump_str.appendFormat("  waits_fences:%d\n", HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->isFenceWaitSupported());
        dump_str.appendFormat("  ConstantAlphaForRGBA:%d\n", HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->isConstantAlphaForRGBASupported());
        dump_str.appendFormat("  ext_path_for_virtual:%d\n", Platform::getInstance().m_config.is_support_ext_path_for_virtual);

        dump_str.appendFormat("  self_refresh:%d\n", HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->isDispSelfRefreshSupported());
#else
        dump_str.appendFormat("  %d,%d-%d-%d-%d,%d,%d,%d,%d\n",
            HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->isDispRszSupported(),
            HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->isDispRpoSupported(),
            HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->getCapsInfo()->rsz_in_max[0],
            HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->getCapsInfo()->rsz_in_max[1],
            Platform::getInstance().m_config.rpo_ui_max_src_width,
            HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->isPartialUpdateSupported(),
            HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->isFenceWaitSupported(),
            HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->isConstantAlphaForRGBASupported(),
            Platform::getInstance().m_config.is_support_ext_path_for_virtual);

        dump_str.appendFormat("  %d\n",
            HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->isDispSelfRefreshSupported());

#endif
        dump_str.appendFormat("\n[HWC Property]\n");
#ifndef MTK_USER_BUILD
        dump_str.appendFormat("  force_full_invalidate(vendor.debug.hwc.forceFullInvalidate):%d\n", Platform::getInstance().m_config.force_full_invalidate);
        dump_str.appendFormat("  wait_fence_for_display(vendor.debug.hwc.waitFenceForDisplay):%d\n", Platform::getInstance().m_config.wait_fence_for_display);
        dump_str.appendFormat("  rgba_rotate(vendor.debug.hwc.rgba_rotate):%d\n", Platform::getInstance().m_config.enable_rgba_rotate);
        dump_str.appendFormat("  rgba_rotate(vendor.debug.hwc.rgbx_scaling):%d\n", Platform::getInstance().m_config.enable_rgbx_scaling);
        dump_str.appendFormat("  compose_level(vendor.debug.hwc.compose_level):%d, ", Platform::getInstance().m_config.compose_level);
        dump_str.appendFormat("  mirror_state(vendor.debug.hwc.mirror_state):%d\n", Platform::getInstance().m_config.mirror_state);

        dump_str.appendFormat("  enableUBL(vendor.debug.hwc.enableUBL):%d\n", Platform::getInstance().m_config.use_async_bliter_ultra);
        dump_str.appendFormat("  prexformUI(vendor.debug.hwc.prexformUI):%d\n", Platform::getInstance().m_config.prexformUI);
        dump_str.appendFormat("  log_level(persist.vendor.debug.hwc.log):%c, ", Debugger::getInstance().getLogThreshold());
        dump_str.appendFormat("  skip_period_log(vendor.debug.hwc.skip_log):%d\n", Debugger::m_skip_log);
        dump_str.appendFormat("  mhl_output(vendor.debug.hwc.mhl_output):%d\n", Platform::getInstance().m_config.format_mir_mhl);

        dump_str.appendFormat("  profile_level(vendor.debug.hwc.profile_level):%d\n", DisplayManager::m_profile_level);
        dump_str.appendFormat("  mir_scale_ratio(vendor.debug.hwc.mir_scale_ratio):%f\n", Platform::getInstance().m_config.mir_scale_ratio);
        dump_str.appendFormat("  force_pri_secure(vendor.debug.hwc.force_pri_secure):%d\n", DisplayManager::getInstance().m_data[HWC_DISPLAY_PRIMARY].secure);
        dump_str.appendFormat("  force_vir_secure(vendor.debug.hwc.force_vir_secure):%d\n", DisplayManager::getInstance().m_data[HWC_DISPLAY_VIRTUAL].secure);
        dump_str.appendFormat("  ext layer:%d(vendor.debug.hwc.ext_layer)\n", Platform::getInstance().m_config.enable_smart_layer);

        dump_str.appendFormat("  trigger_by_vsync(vendor.debug.hwc.trigger_by_vsync):%d\n", m_features.trigger_by_vsync);
        dump_str.appendFormat("  async_bliter(vendor.debug.hwc.async_bliter):%d\n", Platform::getInstance().m_config.use_async_bliter);
        dump_str.appendFormat("  AV_grouping(vendor.debug.hwc.av_grouping):%d\n", Platform::getInstance().m_config.av_grouping);
        dump_str.appendFormat("  DumpBuf(vendor.debug.hwc.dump_buf):%c-%d, DumpBufCont(debug.hwc.dump_buf_cont):%c-%d log:%d\n",
            Platform::getInstance().m_config.dump_buf_type, Platform::getInstance().m_config.dump_buf,
            Platform::getInstance().m_config.dump_buf_cont_type, Platform::getInstance().m_config.dump_buf_cont,
            Platform::getInstance().m_config.dump_buf_log_enable);

        dump_str.appendFormat("  fill_black_debug(vendor.debug.hwc.fill_black_debug):%d\n", Platform::getInstance().m_config.fill_black_debug);
        dump_str.appendFormat("  Always_Setup_Private_Handle(vendor.debug.hwc.always_setup_priv_hnd):%d\n", Platform::getInstance().m_config.always_setup_priv_hnd);
        dump_str.appendFormat("  wdt_ioctl(vendor.debug.hwc.wdt_ioctl):%d\n", Platform::getInstance().m_config.wdt_ioctl);
        dump_str.appendFormat("  only_wfd_by_hwc(vendor.debug.hwc.only_wfd_by_hwc):%d\n", Platform::getInstance().m_config.only_wfd_by_hwc);
        dump_str.appendFormat("  blitdev_for_virtual(vendor.debug.hwc.blitdev_for_virtual):%d\n", Platform::getInstance().m_config.blitdev_for_virtual);

        dump_str.appendFormat("  is_skip_validate(vendor.debug.hwc.is_skip_validate):%d\n", Platform::getInstance().m_config.is_skip_validate);
        dump_str.appendFormat("  support_color_transform(vendor.debug.hwc.color_transform):%d\n", Platform::getInstance().m_config.support_color_transform);
        dump_str.appendFormat("  mdp_scaling_percentage(vendor.debug.hwc.mdp_scale_percentage):%.2f\n", Platform::getInstance().m_config.mdp_scale_percentage);
        dump_str.appendFormat("  ExtendMDP(vendor.debug.hwc.extend_mdp_cap):%d\n", Platform::getInstance().m_config.extend_mdp_capacity);
        dump_str.appendFormat("  disp_support_decompress(vendor.debug.hwc.disp_support_decompress):%d\n", Platform::getInstance().m_config.disp_support_decompress);

        dump_str.appendFormat("  mdp_support_decompress(vendor.debug.hwc.mdp_support_decompress):%d\n", Platform::getInstance().m_config.mdp_support_decompress);
        dump_str.appendFormat("  mdp_support_decompress(vendor.debug.hwc.disable_color_transform_for_secondary_displays):%d\n", Platform::getInstance().m_config.disable_color_transform_for_secondary_displays);
        dump_str.appendFormat("  mdp_support_decompress(vendor.debug.hwc.remove_invisible_layers):%d\n", Platform::getInstance().m_config.remove_invisible_layers);
        dump_str.appendFormat("\n");
#else // MTK_USER_BUILD
        dump_str.appendFormat("  %d,%d,%d,%d,%d,%d, ",
                Platform::getInstance().m_config.force_full_invalidate,
                Platform::getInstance().m_config.wait_fence_for_display,
                Platform::getInstance().m_config.enable_rgba_rotate,
                Platform::getInstance().m_config.enable_rgbx_scaling,
                Platform::getInstance().m_config.compose_level,
                Platform::getInstance().m_config.mirror_state);

        dump_str.appendFormat("%d,%d,%c,%d,%d, ",
                Platform::getInstance().m_config.use_async_bliter_ultra,
                Platform::getInstance().m_config.prexformUI,
                Debugger::getInstance().getLogThreshold(),
                Debugger::m_skip_log,
                Platform::getInstance().m_config.format_mir_mhl);

        dump_str.appendFormat("%d,%f,%d,%d,%d, ",
                DisplayManager::m_profile_level,
                Platform::getInstance().m_config.mir_scale_ratio,
                DisplayManager::getInstance().m_data[HWC_DISPLAY_PRIMARY].secure,
                DisplayManager::getInstance().m_data[HWC_DISPLAY_VIRTUAL].secure,
                Platform::getInstance().m_config.enable_smart_layer);

        dump_str.appendFormat("%d,%d,%d,%c-%d,%c-%d,%d, ",
                m_features.trigger_by_vsync,
                Platform::getInstance().m_config.use_async_bliter,
                Platform::getInstance().m_config.av_grouping,
                Platform::getInstance().m_config.dump_buf_type, Platform::getInstance().m_config.dump_buf,
                Platform::getInstance().m_config.dump_buf_cont_type, Platform::getInstance().m_config.dump_buf_cont,
                Platform::getInstance().m_config.dump_buf_log_enable);

        dump_str.appendFormat("%d,%d,%d,%d,%d,%d,%d, ",
                Platform::getInstance().m_config.fill_black_debug,
                Platform::getInstance().m_config.always_setup_priv_hnd,
                Platform::getInstance().m_config.wdt_ioctl,
                Platform::getInstance().m_config.only_wfd_by_hwc,
                Platform::getInstance().m_config.blitdev_for_virtual,
                Platform::getInstance().m_config.is_skip_validate,
                Platform::getInstance().m_config.support_color_transform);

        dump_str.appendFormat("%.2f,%d,%d,%d,%d\n\n",
                Platform::getInstance().m_config.mdp_scale_percentage,
                Platform::getInstance().m_config.extend_mdp_capacity,
                Platform::getInstance().m_config.disp_support_decompress,
                Platform::getInstance().m_config.mdp_support_decompress,
                Platform::getInstance().m_config.disable_color_transform_for_secondary_displays);
#endif // MTK_USER_BUILD
        *outSize = dump_str.size() + 1;
        m_dump_str = dump_str;
    }
}

uint32_t HWCMediator::deviceGetMaxVirtualDisplayCount(hwc2_device_t* /*device*/)
{
    return 1;
}

int32_t /*hwc2_error_t*/ HWCMediator::deviceRegisterCallback(
    hwc2_device_t* /*device*/,
    int32_t /*hwc2_callback_descriptor_t*/ descriptor,
    hwc2_callback_data_t callback_data,
    hwc2_function_pointer_t pointer)
{
    switch (descriptor)
    {
        case HWC2_CALLBACK_HOTPLUG:
            {
                m_callback_hotplug = reinterpret_cast<HWC2_PFN_HOTPLUG>(pointer);
                m_callback_hotplug_data = callback_data;
                sp<DisplayListener> listener = (DisplayListener *) DisplayManager::getInstance().getListener().get();
                HWC_LOGW("Register hotplug callback");
                if (listener != NULL)
                {
                    listener->m_callback_hotplug =  m_callback_hotplug;
                    listener->m_callback_hotplug_data = m_callback_hotplug_data;
                    DisplayManager::getInstance().resentCallback();
                }
            }
            break;

        case HWC2_CALLBACK_VSYNC:
            {
                m_callback_vsync = reinterpret_cast<HWC2_PFN_VSYNC>(pointer);
                m_callback_vsync_data = callback_data;
                sp<DisplayListener> listener = (DisplayListener *) DisplayManager::getInstance().getListener().get();
                HWC_LOGW("Register vsync callback");
                if (listener != NULL)
                {
                    listener->m_callback_vsync = m_callback_vsync;
                    listener->m_callback_vsync_data =m_callback_vsync_data;
                }
            }
            break;
        case HWC2_CALLBACK_REFRESH:
            {
                m_callback_refresh = reinterpret_cast<HWC2_PFN_REFRESH>(pointer);
                m_callback_refresh_data = callback_data;
                sp<DisplayListener> listener = (DisplayListener *) DisplayManager::getInstance().getListener().get();
                HWC_LOGW("Register refresh callback");
                if (listener != NULL)
                {
                    listener->m_callback_refresh = m_callback_refresh;
                    listener->m_callback_refresh_data = m_callback_refresh_data;
                }
            }
            break;

        default:
            HWC_LOGE("%s: unknown descriptor(%d)", __func__, descriptor);
            return HWC2_ERROR_BAD_PARAMETER;
    }

    if (m_callback_vsync && m_callback_hotplug && m_callback_refresh && !m_is_init_disp_manager)
    {
        m_is_init_disp_manager = true;
        DisplayManager::getInstance().setListener(
            new DisplayListener(
                m_callback_hotplug,
                m_callback_hotplug_data,
                m_callback_vsync,
                m_callback_vsync_data,
                m_callback_refresh,
                m_callback_refresh_data));
        // initialize DisplayManager
        DisplayManager::getInstance().init();
    }
    return HWC2_ERROR_NONE;
}

/* Display functions */
int32_t /*hwc2_error_t*/ HWCMediator::displayAcceptChanges(
    hwc2_device_t* /*device*/,
    hwc2_display_t display)
{
    if (!DisplayManager::getInstance().m_data[display].connected)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    m_displays[display]->acceptChanges();

    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::displayCreateLayer(
    hwc2_device_t* /*device*/,
    hwc2_display_t display,
    hwc2_layer_t* out_layer)
{
    if (!DisplayManager::getInstance().m_data[display].connected)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    return m_displays[display]->createLayer(out_layer, false);
}

int32_t /*hwc2_error_t*/ HWCMediator::displayDestroyLayer(
    hwc2_device_t* /*device*/,
    hwc2_display_t display,
    hwc2_layer_t layer)
{
    if (!DisplayManager::getInstance().m_data[display].connected)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    sp<HWCLayer> hwc_layer = m_displays[display]->getLayer(layer);
    if (hwc_layer == nullptr)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") does not contain layer(%" PRIu64 ")", __func__, display, layer);
        return HWC2_ERROR_BAD_LAYER;
    }

    return m_displays[display]->destroyLayer(layer);
}

int32_t /*hwc2_error_t*/ HWCMediator::displayGetActiveConfig(
    hwc2_device_t* /*device*/,
    hwc2_display_t display,
    hwc2_config_t* out_config)
{
    if (!DisplayManager::getInstance().m_data[display].connected)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    *out_config = 0;

    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::displayGetChangedCompositionTypes(
    hwc2_device_t* /*device*/,
    hwc2_display_t display,
    uint32_t* out_num_elem,
    hwc2_layer_t* out_layers,
    int32_t* /*hwc2_composition_t*/ out_types)
{
    if (!DisplayManager::getInstance().m_data[display].connected)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    if (!m_displays[display]->isValidated())
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") does not validate yet", __func__, display);
        return HWC2_ERROR_NOT_VALIDATED;
    }

    m_displays[display]->getChangedCompositionTypes(out_num_elem, out_layers, out_types);
    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::displayGetClientTargetSupport(
    hwc2_device_t* /*device*/,
    hwc2_display_t /*display*/,
    uint32_t /*width*/,
    uint32_t /*height*/,
    int32_t /*android_pixel_format_t*/ /*format*/,
    int32_t /*android_dataspace_t*/ /*dataspace*/)
{
    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::displayGetColorMode(
    hwc2_device_t* /*device*/,
    hwc2_display_t /*display*/,
    uint32_t* out_num_modes,
    int32_t* out_modes)
{
    if (out_modes == nullptr)
    {
        *out_num_modes = 1;
    }
    else
    {
        out_modes[0] = HAL_COLOR_MODE_NATIVE;
    }

    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::displayGetAttribute(
    hwc2_device_t* /*device*/,
    hwc2_display_t display,
    hwc2_config_t config,
    int32_t /*hwc2_attribute_t*/ attribute,
    int32_t* out_value)
{
    if (!DisplayManager::getInstance().m_data[display].connected)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    if (config != 0)
    {
        HWC_LOGE("%s: unknown display config id(%d)!", __func__, config);
        return HWC2_ERROR_BAD_CONFIG;
    }

    switch (attribute)
    {
        case HWC2_ATTRIBUTE_WIDTH:
            *out_value = m_displays[display]->getWidth();
            break;

        case HWC2_ATTRIBUTE_HEIGHT:
            *out_value = m_displays[display]->getHeight();
            break;

        case HWC2_ATTRIBUTE_VSYNC_PERIOD:
            *out_value = m_displays[display]->getVsyncPeriod();
            break;

        case HWC2_ATTRIBUTE_DPI_X:
            *out_value = m_displays[display]->getDpiX();
            break;

        case HWC2_ATTRIBUTE_DPI_Y:
            *out_value = m_displays[display]->getDpiY();
            break;

        case HWC2_ATTRIBUTE_VALIDATE_DATA:
            unpackageMtkData(*(reinterpret_cast<HwcValidateData*>(out_value)));
            break;

        default:
            HWC_LOGE("%s: unknown attribute(%d)!", __func__, attribute);
            return HWC2_ERROR_BAD_CONFIG;
    }
    return HWC2_ERROR_NONE;
}

void HWCMediator::unpackageMtkData(const HwcValidateData& val_data)
{
    for (auto& kv : val_data.layers)
    {
        auto& layer = kv.second;
        const int64_t& layer_id = layer->id;
        const int64_t& disp_id = layer->disp_id;

        sp<HWCLayer>&& hwc_layer = m_displays[disp_id]->getLayer(layer_id);
        if (hwc_layer != nullptr)
            hwc_layer->setMtkFlags(layer->flags);
        else
            HWC_LOGE("unpackageMtkData(): invalid layer id(%" PRId64 ") disp(%" PRId64 ")", layer_id, disp_id);
    }

    for (auto& display : val_data.displays)
    {
        const int64_t& id = display.id;

        sp<HWCDisplay>& hwc_display = m_displays[id];
        if (hwc_display != nullptr)
            hwc_display->setMtkFlags(display.flags);
    }
}

int32_t /*hwc2_error_t*/ HWCMediator::displayGetConfigs(
    hwc2_device_t* /*device*/,
    hwc2_display_t display,
    uint32_t* out_num_configs,
    hwc2_config_t* out_configs)
{
    if (!DisplayManager::getInstance().m_data[display].connected)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    if (out_configs == nullptr)
    {
        *out_num_configs = 1;
    }
    else
    {
        out_configs[0] = 0;
    }

    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::displayGetName(
    hwc2_device_t* /*device*/,
    hwc2_display_t /*display*/,
    uint32_t* /*out_lens*/,
    char* /*out_name*/)
{
    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::displayGetRequests(
    hwc2_device_t* /*device*/,
    hwc2_display_t display,
    int32_t* /*hwc2_display_request_t*/ /*out_display_requests*/,
    uint32_t* out_num_elem,
    hwc2_layer_t* /*out_layer*/,
    int32_t* /*hwc2_layer_request_t*/ /*out_layer_requests*/)
{
    if (!DisplayManager::getInstance().m_data[display].connected)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    *out_num_elem = 0;

    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::displayGetType(
    hwc2_device_t* /*device*/,
    hwc2_display_t display,
    int32_t* /*hwc2_display_type_t*/ out_type)
{
    if (!DisplayManager::getInstance().m_data[display].connected)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    m_displays[display]->getType(out_type);

    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::displayGetDozeSupport(
    hwc2_device_t* /*device*/,
    hwc2_display_t display,
    int32_t* out_support)
{
    auto& hwc_display = m_displays[display];
    if (!hwc_display->isConnected())
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    if (display == HWC_DISPLAY_PRIMARY)
    {
        *out_support = HWCMediator::getInstance().m_features.aod;
    }
    else
    {
        *out_support = false;
    }

    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::displayGetHdrCapability(
    hwc2_device_t* /*device*/,
    hwc2_display_t /*display*/,
    uint32_t* /*out_num_types*/,
    int32_t* /*android_hdr_t*/ /*out_types*/,
    float* /*out_max_luminance*/,
    float* /*out_max_avg_luminance*/,
    float* /*out_min_luminance*/)
{
    // TODO: to ask PQ that this platform support HDR?
    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::displayGetReleaseFence(
    hwc2_device_t* /*device*/,
    hwc2_display_t display,
    uint32_t* out_num_elem,
    hwc2_layer_t* out_layer,
    int32_t* out_fence)
{
    if (!DisplayManager::getInstance().m_data[display].connected)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    m_displays[display]->getReleaseFenceFds(out_num_elem, out_layer, out_fence);
    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::displayPresent(
    hwc2_device_t* /*device*/,
    hwc2_display_t display,
    int32_t* out_retire_fence)
{
    HWC_LOGV("(%" PRIu64 ") %s", display, __func__);
    AbortMessager::getInstance().printf("(%" PRIu64 ") %s s:%s=>", display, __func__, getPresentValiStateString(m_displays[display]->getValiPresentState()));
    if (!DisplayManager::getInstance().m_data[display].connected)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display);
        unlockRefreshThread(display);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    if (hasCapabilities(HWC2_CAPABILITY_SKIP_VALIDATE) &&
        (m_displays[display]->getValiPresentState() == HWC_VALI_PRESENT_STATE_PRESENT_DONE ||
         m_displays[display]->getValiPresentState() == HWC_VALI_PRESENT_STATE_VALIDATE ||
         m_displays[display]->getValiPresentState() == HWC_VALI_PRESENT_STATE_CHECK_SKIP_VALI))
    {
        if (m_displays[display]->getValiPresentState() == HWC_VALI_PRESENT_STATE_PRESENT_DONE)
        {
            buildVisibleAndInvisibleLayerForAllDisplay();

            bool ultra_scenario = false;
            int32_t mirror_sink_dpy = -1;
            if (m_displays[HWC_DISPLAY_EXTERNAL]->isConnected() ||
                m_displays[HWC_DISPLAY_VIRTUAL]->isConnected())
            {
                mirror_sink_dpy = checkMirrorPath(m_displays, &ultra_scenario);
                HWCDispatcher::getInstance().m_ultra_scenario = ultra_scenario;
            }

            const bool use_decouple_mode = mirror_sink_dpy != -1;
            HWCDispatcher::getInstance().setSessionMode(HWC_DISPLAY_PRIMARY, use_decouple_mode);

            prepareForValidation();
            setNeedValidate(HWC_SKIP_VALIDATE_NOT_SKIP);
            setValiPresentStateOfAllDisplay(HWC_VALI_PRESENT_STATE_CHECK_SKIP_VALI, __LINE__);
            if (checkSkipValidate() == true)
                setNeedValidate(HWC_SKIP_VALIDATE_SKIP);
            else
                setNeedValidate(HWC_SKIP_VALIDATE_NOT_SKIP);
        }

        if (getNeedValidate() == HWC_SKIP_VALIDATE_NOT_SKIP)
        {
            return HWC2_ERROR_NOT_VALIDATED;
        }
        else
        {
            m_hrt.run(m_displays, true);
            updateGlesRangeForAllDisplays();
            setValiPresentStateOfAllDisplay(HWC_VALI_PRESENT_STATE_VALIDATE_DONE, __LINE__);
        }
    }

    if (m_displays[display]->getValiPresentState() == HWC_VALI_PRESENT_STATE_VALIDATE_DONE)
    {
        for (auto& hwc_display : m_displays)
        {
            if (!hwc_display->isConnected())
                continue;

            hwc_display->buildCommittedLayers();
            HWC_LOGV("(%" PRIu64 ") %s getCommittedLayers() size:%d", hwc_display->getId(), __func__, hwc_display->getCommittedLayers().size());
        }

        for (auto& hwc_display : m_displays)
        {
            if (!hwc_display->isValid())
                continue;

            hwc_display->beforePresent(getLastSFValidateNum());
        }

        adjustVsyncOffset();

        for (auto& hwc_display : m_displays)
        {
            if (!hwc_display->isValid())
                continue;

            hwc_display->present();
        }

        HWCDispatcher::getInstance().trigger();
    }

    if (display == HWC_DISPLAY_PRIMARY)
    {
        if (m_displays[display]->getRetireFenceFd() == -1)
        {
            *out_retire_fence = -1;
        }
        else
        {
#ifdef USES_FENCE_RENAME
            *out_retire_fence = sync_merge("HWC_to_SF_present", m_displays[display]->getRetireFenceFd(), m_displays[display]->getRetireFenceFd());
            if (*out_retire_fence < 0)
            {
                HWC_LOGE("(%" PRIu64 ") %s merge present fence(%d) failed", display, __func__, m_displays[display]->getRetireFenceFd());
            }
#else
            *out_retire_fence = ::dup(m_displays[display]->getRetireFenceFd());
#endif
            ::protectedClose(m_displays[display]->getRetireFenceFd());
            m_displays[display]->setRetireFenceFd(-1, isDispConnected(display));
        }
    }
    else
    {
        *out_retire_fence = dupCloseFd(m_displays[display]->getRetireFenceFd());
        m_displays[display]->setRetireFenceFd(-1, isDispConnected(display));
    }
    HWC_LOGV("(%" PRIu64 ") %s out_retire_fence:%d", display, __func__, *out_retire_fence);

    m_validate_seq = 0;
    ++m_present_seq;
    m_displays[display]->afterPresent();
    m_displays[display]->setValiPresentState(HWC_VALI_PRESENT_STATE_PRESENT_DONE, __LINE__);
    unlockRefreshThread(display);
    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::displaySetActiveConfig(
    hwc2_device_t* /*device*/,
    hwc2_display_t display,
    hwc2_config_t config_id)
{
    if (!DisplayManager::getInstance().m_data[display].connected)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    if (config_id != 0)
    {
        HWC_LOGE("%s: wrong config id(%d)", __func__, config_id);
        return HWC2_ERROR_BAD_CONFIG;
    }
    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::displaySetClientTarget(
    hwc2_device_t* /*device*/,
    hwc2_display_t display,
    buffer_handle_t handle,
    int32_t acquire_fence,
    int32_t dataspace,
    hwc_region_t damage)
{
    if (!DisplayManager::getInstance().m_data[display].connected)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    sp<HWCLayer> ct = m_displays[display]->getClientTarget();

    ct->setHandle(handle);
    if (ct->getAcquireFenceFd() != -1) {
        ::protectedClose(ct->getAcquireFenceFd());
        ct->setAcquireFenceFd(-1, isDispConnected(display));
    }
    ct->setAcquireFenceFd(acquire_fence, isDispConnected(display));
    ct->setDataspace(dataspace);
    ct->setDamage(damage);
    ct->setupPrivateHandle();

    if (display == HWC_DISPLAY_VIRTUAL && m_displays[display]->getMirrorSrc() == -1 &&
        !Platform::getInstance().m_config.is_support_ext_path_for_virtual &&
        !HWCMediator::getInstance().m_features.copyvds)
    {
        const int32_t dup_acq_fence_fd = ::dup(acquire_fence);
        HWC_LOGV("(%" PRIu64 ") setClientTarget() handle:%p acquire_fence:%d(%d)", display, handle, acquire_fence, dup_acq_fence_fd);
        m_displays[display]->setRetireFenceFd(dup_acq_fence_fd, true);
    }
    else
    {
        HWC_LOGV("(%" PRIu64 ") setClientTarget() handle:%p acquire_fence:%d", display, handle, acquire_fence);
    }
    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::displaySetColorMode(
    hwc2_device_t* /*device*/,
    hwc2_display_t display,
    int32_t mode)
{
    if (!m_displays[display]->isConnected())
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display);
        return HWC2_ERROR_BAD_DISPLAY;
    }
    HWC_LOGI("(%" PRIu64 ") %s mode:%d", display, __func__, mode);

    m_displays[display]->setColorMode(mode);
    if (HAL_COLOR_MODE_NATIVE == mode)
    {
        return HWC2_ERROR_NONE;
    }
    else
    {
        return HWC2_ERROR_UNSUPPORTED;
    }
}

int32_t /*hwc2_error_t*/ HWCMediator::displaySetColorTransform(
    hwc2_device_t* /*device*/,
    hwc2_display_t display,
    const float* matrix,
    int32_t /*android_color_transform_t*/ hint)
{
    if (!m_displays[display]->isConnected())
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    if (display == HWC_DISPLAY_PRIMARY ||
        display == HWC_DISPLAY_VIRTUAL)
    {
        return m_displays[display]->setColorTransform(matrix, hint);
    }
    else
    {
        return HWC2_ERROR_UNSUPPORTED;
    }
    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::displaySetOutputBuffer(
    hwc2_device_t* /*device*/,
    hwc2_display_t display,
    buffer_handle_t buffer,
    int32_t release_fence)
{
    if (display != HWC_DISPLAY_VIRTUAL)
    {
        HWC_LOGE("%s: invalid display(%" PRIu64 ")", __func__, display);
        return HWC2_ERROR_UNSUPPORTED;
    }

    if (!m_displays[display]->isConnected())
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    const int32_t dup_fd = ::dup(release_fence);
    HWC_LOGV("(%" PRIu64 ") %s outbuf fence:%d->%d", display, __func__, release_fence, dup_fd);
    m_displays[display]->setOutbuf(buffer, dup_fd);
    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::displaySetPowerMode(
    hwc2_device_t* /*device*/,
    hwc2_display_t display,
    int32_t /*hwc2_power_mode_t*/ mode)
{
    if (!DisplayManager::getInstance().m_data[display].connected)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    HWC_LOGD("%s display:%" PRIu64 " mode:%d", __func__, display, mode);

    switch (mode)
    {
        case HWC2_POWER_MODE_OFF:
        case HWC2_POWER_MODE_ON:
        case HWC2_POWER_MODE_DOZE:
        case HWC2_POWER_MODE_DOZE_SUSPEND:
            m_displays[display]->setPowerMode(mode);
            break;

        default:
            HWC_LOGE("%s: display(%" PRIu64 ") a unknown parameter(%d)!", __func__, display, mode);
            return HWC2_ERROR_BAD_PARAMETER;
    }

    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::displaySetVsyncEnabled(
    hwc2_device_t* /*device*/,
    hwc2_display_t display,
    int32_t /*hwc2_vsync_t*/ enabled)
{
    if (!DisplayManager::getInstance().m_data[display].connected)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    switch (enabled)
    {
        case HWC2_VSYNC_ENABLE:
            m_displays[display]->setVsyncEnabled(true);
            break;

        case HWC2_VSYNC_DISABLE:
            m_displays[display]->setVsyncEnabled(false);
            break;

        default:
            HWC_LOGE("%s: display( %" PRIu64 ") a unknown parameter(%d)!", __func__, display, enabled);
            return HWC2_ERROR_BAD_PARAMETER;
    }
    return HWC2_ERROR_NONE;
}

bool Hrt::isEnabled() const
{
    return 0 != (HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->getCapsInfo()->disp_feature & DISP_FEATURE_HRT);
}

void Hrt::fillLayerConfigList(const vector<sp<HWCDisplay> >& displays)
{
    for (auto& display : displays)
    {
        if (!display->isConnected())
            continue;

        const int32_t disp_id = static_cast<int32_t>(display->getId());
        const vector<sp<HWCLayer> >& layers = display->getVisibleLayersSortedByZ();
        const int&& layers_num = layers.size();

        // reallocate layer_config_list if needed
        if (layers_num > m_layer_config_len[disp_id])
        {
            if (NULL != m_layer_config_list[disp_id])
                free(m_layer_config_list[disp_id]);

            m_layer_config_len[disp_id] = layers_num;
            m_layer_config_list[disp_id] = (layer_config*)calloc(m_layer_config_len[disp_id], sizeof(layer_config));
            if (NULL == m_layer_config_list[disp_id])
            {
                HWC_LOGE("(%d) Failed to malloc layer_config_list (len=%d)", disp_id, layers_num);
                m_layer_config_len[disp_id] = 0;
                return;
            }
        }

        // init and get PrivateHandle
        layer_config* layer_config = m_layer_config_list[disp_id];

        for (auto& layer : layers)
        {
            layer_config->ovl_id        = -1;
            layer_config->ext_sel_layer = -1;
            layer_config->src_fmt       =
                (layer->getHwlayerType() == HWC_LAYER_TYPE_DIM) ?
                    DISP_FORMAT_DIM : mapDispInFormat(layer->getPrivateHandle().format);
            layer_config->dst_offset_y  = getDstTop(layer);
            layer_config->dst_offset_x  = getDstLeft(layer);
            layer_config->dst_width     = getDstWidth(layer);
            layer_config->dst_height    = getDstHeight(layer);
            layer_config->layer_caps    = layer->getLayerCaps();

            const PrivateHandle& priv_hnd = layer->getPrivateHandle();
            switch(layer->getHwlayerType())
            {
                case HWC_LAYER_TYPE_DIM:
                    layer_config->src_width = getDstWidth(layer);
                    layer_config->src_height = getDstHeight(layer);
                    break;

                case HWC_LAYER_TYPE_MM:
                case HWC_LAYER_TYPE_MM_HIGH:
                    layer_config->src_width = WIDTH(layer->getMdpDstRoi());
                    layer_config->src_height = HEIGHT(layer->getMdpDstRoi());
                    break;

                default:
                    if (layer->getHwlayerType() == HWC_LAYER_TYPE_UI &&
                        (priv_hnd.prexform & HAL_TRANSFORM_ROT_90))
                    {
                        layer_config->src_width  = getSrcHeight(layer);
                        layer_config->src_height = getSrcWidth(layer);
                    }
                    else
                    {
                        layer_config->src_width  = getSrcWidth(layer);
                        layer_config->src_height = getSrcHeight(layer);
                    }
                    break;
            }

            ++layer_config;
        }
    }
}

void Hrt::fillDispLayer(const vector<sp<HWCDisplay> >& displays)
{
    memset(&m_disp_layer, 0, sizeof(disp_layer_info));
    m_disp_layer.hrt_num = -1;
    for (auto& display : displays)
    {
        const int32_t disp_id = static_cast<int32_t>(display->getId());
        const size_t disp_input = (disp_id == HWC_DISPLAY_PRIMARY) ? 0 : 1;

        m_disp_layer.gles_head[disp_input] = -1;
        m_disp_layer.gles_tail[disp_input] = -1;
    }

    // prepare disp_layer_info for ioctl
    for (auto& display : displays)
    {
        // driver only supports two displays at the same time
        // disp_input 0: primary display; disp_input 1: secondry display(MHL or vds)
        // fill display info
        const uint64_t disp_id = display->getId();
        const size_t disp_input = (disp_id == HWC_DISPLAY_PRIMARY) ? 0 : 1;

        if (!display->isConnected() ||
            display->getMirrorSrc() != -1 ||
            HWCMediator::getInstance().getOvlDevice(display->getId())->getType() == OVL_DEVICE_TYPE_BLITDEV)
        {
            continue;
        }

        const int layers_num = display->getVisibleLayersSortedByZ().size();

        m_disp_layer.input_config[disp_input] = m_layer_config_list[disp_id];

        switch (disp_id) {
            case HWC_DISPLAY_PRIMARY:
                m_disp_layer.disp_mode[disp_input] =
                    HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->getOverlaySessionMode(disp_id);
                break;

            case HWC_DISPLAY_EXTERNAL:
                m_disp_layer.disp_mode[disp_input] = DISP_SESSION_DIRECT_LINK_MODE;
                break;

            case HWC_DISPLAY_VIRTUAL:
                m_disp_layer.disp_mode[disp_input] = DISP_SESSION_DECOUPLE_MODE;
                break;

            default:
                HWC_LOGE("%s: Unknown disp_id(" PRIu64 ")", __func__, disp_id);
        }

        m_disp_layer.layer_num[disp_input] =
            (m_layer_config_len[disp_id] < layers_num) ? m_layer_config_len[disp_id] : layers_num;

        display->getGlesRange(
            &m_disp_layer.gles_head[disp_input],
            &m_disp_layer.gles_tail[disp_input]);
        HWC_LOGV("%s disp:%" PRIu64 " m_disp_layer.gles_head[disp_input]:%d, m_disp_layer.gles_tail[disp_input]:%d",
            __func__, disp_id, m_disp_layer.gles_head[disp_input],m_disp_layer.gles_tail[disp_input] );
    }
}

void Hrt::fillLayerInfoOfDispatcherJob(const std::vector<sp<HWCDisplay> >& displays)
{
    // DbgLogger logger(DbgLogger::TYPE_HWC_LOG, 'D',"fillLayerInfoOfDispatcherJob()");
    for (auto& display : displays)
    {
        const int32_t disp_id = static_cast<int32_t>(display->getId());
        DispatcherJob* job = HWCDispatcher::getInstance().getExistJob(disp_id);

        if (!display->isConnected() || NULL == job || display->getMirrorSrc() != -1)
        {
            HWC_LOGV("fillLayerInfoOfDispatcherJob() job:%p display->getMirrorSrc():%d", job, display->getMirrorSrc());
            continue;
        }

        // only support two display at the same time
        // index 0: primary display; index 1: secondry display(MHL or vds)
        // fill display info
        const int disp_input = (disp_id == HWC_DISPLAY_PRIMARY) ? 0 : 1;

        job->layer_info.max_overlap_layer_num = m_disp_layer.hrt_num;
        job->layer_info.gles_head = m_disp_layer.gles_head[disp_input];
        job->layer_info.gles_tail = m_disp_layer.gles_tail[disp_input];
        HWC_LOGV("fillLayerInfoOfDispatcherJob() disp:%d gles_head:%d gles_tail:%d", disp_id, job->layer_info.gles_head, job->layer_info.gles_tail);

        // fill layer info
        job->layer_info.layer_config_list = m_disp_layer.input_config[disp_input];

        for (size_t i = 0; i < display->getVisibleLayersSortedByZ().size(); ++i)
        {
            if (static_cast<int32_t>(i) >= m_disp_layer.layer_num[disp_input])
                break;

            auto& layer = display->getVisibleLayersSortedByZ()[i];
            layer->setLayerCaps(m_disp_layer.input_config[disp_input][i].layer_caps);
        }
        // for (int32_t i = 0 ; i < m_disp_layer.layer_num[disp_input]; ++i)
        //    logger.printf("i:%d ovl_id:%d caps:%d, ", i, m_disp_layer.input_config[disp_input][i].ovl_id, m_disp_layer.input_config[disp_input][i].layer_caps);
    }
}

void Hrt::setCompType(const std::vector<sp<HWCDisplay> >& displays)
{
    for (auto& display : displays)
    {
        const int32_t disp_id = static_cast<int32_t>(display->getId());

        DispatcherJob* job = HWCDispatcher::getInstance().getExistJob(disp_id);

        if (!display->isConnected() || NULL == job || display->getMirrorSrc() != -1)
            continue;

        // only support two display at the same time
        // index 0: primary display; index 1: secondry display(MHL or vds)
        // fill display info
        int32_t gles_head = -1, gles_tail = -1;
        display->getGlesRange(&gles_head, &gles_tail);
        if (gles_head != job->layer_info.gles_head || gles_tail != job->layer_info.gles_tail)
        {
            gles_head = job->layer_info.gles_head;
            gles_tail = job->layer_info.gles_tail;
            display->setGlesRange(gles_head, gles_tail);
        }
    }
}

void Hrt::dump(String8* str)
{
    str->appendFormat("%s\n", m_hrt_result.str().c_str());
}

void Hrt::printQueryValidLayerResult()
{
    m_hrt_result.str("");
    m_hrt_result << "[HRT]";
    for (int32_t i = 0; i < 2; ++i)
    {
        for (int32_t j = 0; j < m_disp_layer.layer_num[i]; ++j)
        {
            const auto& cfg = m_disp_layer.input_config[i][j];
            m_hrt_result << " [(" << i << "," << j <<
                ") s_wh:" << cfg.src_width << "," << cfg.src_height <<
                " d_xywh:" << cfg.dst_offset_x << "," << cfg.dst_offset_y << "," << cfg.dst_width << ","<< cfg.dst_height <<
                " caps:" << cfg.layer_caps << "]";
        }
    }
    HWC_LOGD("%s", m_hrt_result.str().c_str());
}

void Hrt::modifyMdpDstRoiIfRejectedByRpo(const std::vector<sp<HWCDisplay> >& displays)
{
    for (auto& disp : displays)
    {
        if (!disp->isValid())
            continue;

        for (auto& layer : disp->getVisibleLayersSortedByZ())
        {
            if (layer->getHwlayerType() == HWC_LAYER_TYPE_MM &&
                (WIDTH(layer->getMdpDstRoi()) != WIDTH(layer->getDisplayFrame()) ||
                HEIGHT(layer->getMdpDstRoi()) != HEIGHT(layer->getDisplayFrame())) &&
                (layer->getLayerCaps() & DISP_RSZ_LAYER) == 0)
            {
                layer->editMdpDstRoi() = layer->getDisplayFrame();
            }
        }
    }
}

void Hrt::run(vector<sp<HWCDisplay> >& displays, const bool& is_skip_validate)
{
    if (0 == isEnabled())
    {
        for (auto& hwc_display : displays)
        {
            if (!hwc_display->isValid())
                continue;

            if (hwc_display->getMirrorSrc() != -1)
                continue;

            int32_t gles_head = -1, gles_tail = -1;
            hwc_display->getGlesRange(&gles_head, &gles_tail);

            const int32_t disp_id = static_cast<int32_t>(hwc_display->getId());
            DispatcherJob* job = HWCDispatcher::getInstance().getExistJob(disp_id);

            if (NULL == job)
                continue;

            const int32_t max_layer = job->num_layers;
            const bool only_hwc_comp = (gles_tail == -1);
            const int32_t gles_count = only_hwc_comp ? 0 : gles_tail - gles_head + 1;
            const int32_t hwc_count = hwc_display->getVisibleLayersSortedByZ().size() - gles_count;
            const int32_t committed_count = only_hwc_comp ? hwc_count : hwc_count + 1;
            const int32_t over_layer_count = (committed_count > max_layer) ?
                committed_count - max_layer + 1: 0;
            if (over_layer_count > 0)
            {
                int32_t new_gles_tail = -1;
                int32_t new_gles_head = -1;
                if (gles_tail == -1 && gles_head == -1)
                {
                    new_gles_tail = hwc_display->getVisibleLayersSortedByZ().size() - 1;
                    new_gles_head = hwc_display->getVisibleLayersSortedByZ().size() - 1 - over_layer_count + 1;
                    hwc_display->setGlesRange(new_gles_head, new_gles_tail);
                }
                else if (gles_tail > -1 && gles_head > -1 && gles_tail >= gles_head)
                {
                    const int32_t hwc_num_after_gles_tail = hwc_display->getVisibleLayersSortedByZ().size() - 1 - gles_tail;
                    const int32_t hwc_num_before_gles_head = gles_head;
                    const int32_t excess_layer = over_layer_count > hwc_num_after_gles_tail ? over_layer_count - hwc_num_after_gles_tail - 1: 0;
                    if (excess_layer > hwc_num_before_gles_head)
                    {
                        HWC_LOGE("wrong GLES head range (%d,%d) (%d,%d)", gles_head, gles_tail, hwc_count, max_layer);
                        abort();
                    }
                    new_gles_tail = excess_layer == 0 ? gles_tail + over_layer_count - 1 : hwc_display->getVisibleLayersSortedByZ().size() - 1;
                    new_gles_head = excess_layer == 0 ? gles_head : gles_head - excess_layer;
                    hwc_display->setGlesRange(new_gles_head, new_gles_tail);
                }
                else
                {
                    HWC_LOGE("wrong GLES range (%d,%d) (%d,%d)", gles_head, gles_tail, hwc_count, max_layer);
                    abort();
                }
                hwc_display->setGlesRange(new_gles_head, new_gles_tail);

            }
        }
        return;
    }

    if (is_skip_validate)
    {
        fillLayerInfoOfDispatcherJob(displays);
        return;
    }

    fillLayerConfigList(displays);

    fillDispLayer(displays);

    if (HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->queryValidLayer(&m_disp_layer))
    {
        fillLayerInfoOfDispatcherJob(displays);
        setCompType(displays);
        if (HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->isDispRpoSupported())
        {
            modifyMdpDstRoiIfRejectedByRpo(displays);
            printQueryValidLayerResult();
        }
    }
    else
    {
        HWC_LOGE("%s: an error when hrt calculating!", __func__);

        for (auto& display : displays)
        {
            if (!display->isConnected())
                continue;

            const int32_t disp_id = static_cast<int32_t>(display->getId());
            DispatcherJob* job = HWCDispatcher::getInstance().getExistJob(disp_id);

            if (NULL == job || display->getMirrorSrc() != -1)
                continue;

            job->layer_info.max_overlap_layer_num = -1;
        }
    }
}

static int32_t findLimitedVideo(const vector<sp<HWCLayer> >& layers)
{
    for (auto& layer : layers)
    {
        if (layer->getHandle() == nullptr)
            continue;

        const PrivateHandle& hnd = layer->getPrivateHandle();
        const int&& type = (hnd.ext_info.status & GRALLOC_EXTRA_MASK_TYPE);
        const size_t&& size = hnd.width * hnd.height;
        if ((type == GRALLOC_EXTRA_BIT_TYPE_VIDEO ||
            type == GRALLOC_EXTRA_BIT_TYPE_CAMERA ||
            hnd.format == HAL_PIXEL_FORMAT_YV12) &&
            size >= Platform::getInstance().getLimitedVideoSize())
        {
            return layer->getId();
        }
    }
    return -1;
}

static bool isMirrorList(const vector<sp<HWCLayer> >& src_layers,
                         const vector<sp<HWCLayer> >& sink_layers,
                         const int32_t& src_disp,
                         const int32_t& sink_disp)
{
    bool ret = false;
    DbgLogger logger(DbgLogger::TYPE_HWC_LOG | DbgLogger::TYPE_DUMPSYS, 'D',"mirror?(%d->%d): ", src_disp, sink_disp);

    if (src_disp == sink_disp)
    {
        logger.printf("E-same_dpy");
        return ret;
    }

    logger.printf("I-size(%zu|%zu) ", sink_layers.size(), src_layers.size());

    vector<uint64_t> src_layers_alloc_id;
    vector<uint64_t> sink_layers_alloc_id;

    for (auto& layer : src_layers)
    {
        HWC_LOGV("isMirrorList 1 layer->getHandle():%x", layer->getHandle());
        if (layer->getCompositionType() != HWC2_COMPOSITION_SIDEBAND)
        {
            auto& hnd =layer->getPrivateHandle();
            src_layers_alloc_id.push_back(hnd.alloc_id);
        }
        // todo: check sidebandStream
    }

    HWC_LOGV("src_layers_alloc_id size:%d", src_layers_alloc_id.size());

    for (auto& layer : sink_layers)
    {
        HWC_LOGV("isMirrorList 2 layer->getHandle():%x", layer->getHandle());
        if (layer->getCompositionType() != HWC2_COMPOSITION_SIDEBAND)
        {
            auto& hnd =layer->getPrivateHandle();
            sink_layers_alloc_id.push_back(hnd.alloc_id);
        }
        // todo: check sidebandStream
    }

    if (src_layers_alloc_id == sink_layers_alloc_id)
    {
        logger.printf("T2 ");
        ret = true;
    }

    logger.printf("E-%d", ret);
    return ret;
}

// checkMirrorPath() checks if mirror path exists
int32_t checkMirrorPath(const vector<sp<HWCDisplay> >& displays, bool *ultra_scenario)
{
    DbgLogger logger(DbgLogger::TYPE_HWC_LOG | DbgLogger::TYPE_DUMPSYS, 'D', "chkMir(%zu: ", displays.size());

    const DisplayData* display_data = DisplayManager::getInstance().m_data;

    *ultra_scenario = false;

    // display id of mirror source
    const int32_t mir_dpy = HWC_DISPLAY_PRIMARY;
    auto&& src_layers = displays[HWC_DISPLAY_PRIMARY]->getVisibleLayersSortedByZ();
    for (int32_t i = 1; i <= HWC_DISPLAY_VIRTUAL; ++i)
    {
        auto& display = displays[i];
        const int32_t disp_id = static_cast<int32_t>(display->getId());
        auto&& layers = display->getVisibleLayersSortedByZ();
        if (DisplayManager::MAX_DISPLAYS <= disp_id)
            continue;

        if (!display->isConnected())
            continue;

        if (HWC_DISPLAY_PRIMARY == disp_id)
        {
            display->setMirrorSrc(-1);
            logger.printf("(%d:L%d) ", i, __LINE__);
            continue;
        }

        if (listForceGPUComp(layers))
        {
            display->setMirrorSrc(-1);
            logger.printf("(%d:L%d) ", i, __LINE__);
            continue;
        }

        const bool has_limit_video = findLimitedVideo(display->getVisibleLayersSortedByZ()) >= -1;

        const bool is_mirror_list = isMirrorList(src_layers, layers, mir_dpy, disp_id);

        // if hdcp checking is handled by display driver, the extension path must be applied.
        if (Platform::getInstance().m_config.bypass_wlv1_checking && listSecure(layers))
        {
            display->setMirrorSrc(-1);
            logger.printf("(%d:L%d) ", i, __LINE__);
            continue;
        }

        // 4k mhl has 4k video with mirror mode, so need to block 4k video at primary display
        if (disp_id == HWC_DISPLAY_EXTERNAL && is_mirror_list &&
            DisplayManager::getInstance().isUltraDisplay(disp_id) && has_limit_video)
        {
            *ultra_scenario = true;
            logger.printf("(%d:L%d) ", i, __LINE__);
            continue;
        }

        if ((Platform::getInstance().m_config.mirror_state & MIRROR_DISABLED) ||
            (Platform::getInstance().m_config.mirror_state & MIRROR_PAUSED))
        {
            // disable mirror mode
            // either the mirror state is disabled or the mirror source is blanked
            display->setMirrorSrc(-1);
            logger.printf("(%d:L%d) ", disp_id, __LINE__);
            continue;
        }

        // the layer list is different with primary display
        if (!is_mirror_list)
        {
            display->setMirrorSrc(-1);
            logger.printf("(%d:L%d) ", disp_id, __LINE__);
            continue;
        }

        if (!display_data[i].secure && listSecure(layers))
        {
            // disable mirror mode
            // if any secure or protected layer exists in mirror source
            display->setMirrorSrc(-1);
            logger.printf("(%d:L%d) ", i, __LINE__);
            continue;
        }

        if (!HWCMediator::getInstance().m_features.copyvds &&
            layers.empty())
        {
            // disable mirror mode
            // since force copy vds is not used
            display->setMirrorSrc(-1);
            logger.printf("(%d:L%d) ", i, __LINE__);
            continue;
        }

        // check enlargement ratio (i.e. scale ratio > 0)
        if (Platform::getInstance().m_config.mir_scale_ratio > 0)
        {
            float scaled_ratio = display_data[i].pixels /
               static_cast<float>(display_data[mir_dpy].pixels);

            if (scaled_ratio > Platform::getInstance().m_config.mir_scale_ratio)
            {
                // disable mirror mode
                // since scale ratio exceeds the maximum one
                display->setMirrorSrc(-1);
                logger.printf("(%d:L%d) ", i, __LINE__);
                continue;
            }
        }

        if (display_data[i].is_s3d_support &&
            HWC_DISPLAY_EXTERNAL == disp_id && listS3d(layers))
        {
            display->setMirrorSrc(-1);
            logger.printf("(%d:L%d) ", i, __LINE__);
            continue;
        }

        if (display_data[i].is_s3d_support &&
            HWC_DISPLAY_EXTERNAL == disp_id && listS3d(layers))
        {
            display->setMirrorSrc(-1);
            logger.printf("(%d:L%d) ", i, __LINE__);
            continue;
        }

        display->setMirrorSrc(mir_dpy);
        logger.printf("mir");
        return disp_id;
    }
    logger.printf("!mir");
    return -1;
}

void HWCMediator::updateGlesRangeForAllDisplays()
{
    for (auto& display : m_displays)
    {
        if (!display->isConnected())
            continue;

        display->updateGlesRange();
    }
}

int32_t /*hwc2_error_t*/ HWCMediator::displayValidateDisplay(
    hwc2_device_t* /*device*/,
    hwc2_display_t display,
    uint32_t* out_num_types,
    uint32_t* out_num_requests)
{
    AbortMessager::getInstance().printf("(%" PRIu64 ") %s s:%s", display, __func__, getPresentValiStateString(m_displays[display]->getValiPresentState()));
    lockRefreshThread(display);
    if (!DisplayManager::getInstance().m_data[display].connected)
    {
        unlockRefreshThread(display);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    if (m_displays[display]->getValiPresentState() == HWC_VALI_PRESENT_STATE_PRESENT_DONE)
    {
        buildVisibleAndInvisibleLayerForAllDisplay();
        bool ultra_scenario = false;
        int32_t mirror_sink_dpy = -1;
        if (m_displays[HWC_DISPLAY_EXTERNAL]->isConnected() ||
            m_displays[HWC_DISPLAY_VIRTUAL]->isConnected())
        {
            mirror_sink_dpy = checkMirrorPath(m_displays, &ultra_scenario);
            HWCDispatcher::getInstance().m_ultra_scenario = ultra_scenario;
        }

        const bool use_decouple_mode = mirror_sink_dpy != -1;
        HWCDispatcher::getInstance().setSessionMode(HWC_DISPLAY_PRIMARY, use_decouple_mode);

        prepareForValidation();
        setNeedValidate(HWC_SKIP_VALIDATE_NOT_SKIP);
    }

    if (m_displays[display]->getValiPresentState() == HWC_VALI_PRESENT_STATE_PRESENT_DONE ||
        m_displays[display]->getValiPresentState() == HWC_VALI_PRESENT_STATE_CHECK_SKIP_VALI)
    {
        validate();
        countdowmSkipValiRelatedNumber();
    }

    vector<sp<HWCLayer> > changed_comp_types;
    const vector<int32_t>& prev_comp_types = m_displays[display]->getPrevCompTypes();
    {
        // DbgLogger logger(DbgLogger::TYPE_HWC_LOG, 'D', "(%d) validateDisplay(): ", display);

        auto&& layers = m_displays[display]->getVisibleLayersSortedByZ();

        for (size_t i = 0; i < layers.size(); ++i)
        {
            if (layers[i]->getCompositionType() != prev_comp_types[i])
            {
                //logger.printf(" [layer(%d) comp type change:(%s->%s)]",
                //        layers[i]->getZOrder(),
                //        getCompString(prev_comp_types[i]),
                //        getCompString(layers[i]->getCompositionType()));
                changed_comp_types.push_back(layers[i]);
                layers[i]->setSFCompositionType(layers[i]->getCompositionType(), false);
            }

#ifndef MTK_USER_BUILD
            HWC_LOGD("(%" PRIu64 ") val %s", display, layers[i]->toString8().string());
#else
            HWC_LOGV("(%" PRIu64 ") val %s", display, layers[i]->toString8().string());
#endif
        }
    }

    m_displays[display]->moveChangedCompTypes(&changed_comp_types);
    *out_num_types = changed_comp_types.size();
    *out_num_requests = 0;

    ++m_validate_seq;
    setLastSFValidateNum(m_validate_seq);
    m_present_seq = 0;
    m_displays[display]->setValiPresentState(HWC_VALI_PRESENT_STATE_VALIDATE_DONE, __LINE__);

    return HWC2_ERROR_NONE;
}

/* Layer functions */
int32_t /*hwc2_error_t*/ HWCMediator::layerSetCursorPosition(
    hwc2_device_t* /*device*/,
    hwc2_display_t display,
    hwc2_layer_t /*layer*/,
    int32_t /*x*/,
    int32_t /*y*/)
{
    if (!DisplayManager::getInstance().m_data[display].connected)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::layerSetBuffer(
    hwc2_device_t* /*device*/,
    hwc2_display_t display,
    hwc2_layer_t layer,
    buffer_handle_t buffer,
    int32_t acquire_fence)
{
    if (!DisplayManager::getInstance().m_data[display].connected)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    sp<HWCLayer> hwc_layer = m_displays[display]->getLayer(layer);
    if (hwc_layer == nullptr)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") does not contain layer(%" PRIu64 ")", __func__, display, layer);
        return HWC2_ERROR_BAD_LAYER;
    }

    HWC_LOGV("(%" PRIu64 ") layerSetBuffer() layer id:%" PRIu64 " hnd:%p acquire_fence:%d", display, layer, buffer, acquire_fence);

    if (buffer)
    {
        editSetBufFromSfLog().printf(" %" PRIu64 ":%" PRIu64 ",%x", display, layer, ((const intptr_t)(buffer) & 0xffff0) >> 4);
    }
    else
    {
        editSetBufFromSfLog().printf(" %" PRIu64 ":%" PRIu64 ",null", display, layer);
    }

    hwc_layer->setHandle(buffer);
    hwc_layer->setAcquireFenceFd(acquire_fence, isDispConnected(display));

    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::layerSetSurfaceDamage(
    hwc2_device_t* /*device*/,
    hwc2_display_t display,
    hwc2_layer_t layer_id,
    hwc_region_t damage)
{
    if (!DisplayManager::getInstance().m_data[display].connected)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    sp<HWCLayer> layer = m_displays[display]->getLayer(layer_id);
    if (layer == nullptr)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") does not contain layer(%" PRIu64 ")", __func__, display, layer_id);
        return HWC2_ERROR_BAD_LAYER;
    }

    layer->setDamage(damage);

    return HWC2_ERROR_NONE;
}

/* Layer state functions */
int32_t /*hwc2_error_t*/ HWCMediator::layerStateSetBlendMode(
    hwc2_device_t* /*device*/,
    hwc2_display_t display,
    hwc2_layer_t layer_id,
    int32_t /*hwc2_blend_mode_t*/ mode)
{
    if (!DisplayManager::getInstance().m_data[display].connected)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    sp<HWCLayer> layer = m_displays[display]->getLayer(layer_id);
    if (layer == nullptr)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") does not contain layer(%" PRIu64 ")", __func__, display, layer_id);
        return HWC2_ERROR_BAD_LAYER;
    }

    switch (mode)
    {
        case HWC2_BLEND_MODE_NONE:
        case HWC2_BLEND_MODE_PREMULTIPLIED:
        case HWC2_BLEND_MODE_COVERAGE:
            layer->setBlend(mode);
            break;
        default:
            HWC_LOGE("%s: unknown mode(%d)", __func__, mode);
            return HWC2_ERROR_BAD_PARAMETER;
    }
    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::layerStateSetColor(
    hwc2_device_t* /*device*/,
    hwc2_display_t display_id,
    hwc2_layer_t layer_id,
    hwc_color_t color)
{
    if (!DisplayManager::getInstance().m_data[display_id].connected)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display_id);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    sp<HWCLayer> layer = m_displays[display_id]->getLayer(layer_id);
    if (layer == nullptr)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") does not contain layer(%" PRIu64 ")", __func__, display_id, layer_id);
        return HWC2_ERROR_BAD_LAYER;
    }

    layer->setLayerColor(color);

    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::layerStateSetCompositionType(
    hwc2_device_t* /*device*/,
    hwc2_display_t display_id,
    hwc2_layer_t layer_id,
    int32_t /*hwc2_composition_t*/ type)
{
    if (!DisplayManager::getInstance().m_data[display_id].connected)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display_id);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    HWC_LOGV("(%" PRIu64 ") layerStateSetCompositionType() layer id:%" PRIu64 " type:%s", display_id, layer_id, getCompString(type));
    editSetCompFromSfLog().printf(" (%" PRIu64 ":%" PRIu64 ",%s)", display_id, layer_id, getCompString(type));

    auto&& layer = m_displays[display_id]->getLayer(layer_id);
    if (layer == nullptr)
    {
        HWC_LOGE("(%" PRIu64 ") %s: the display does NOT contain layer(%" PRIu64 ")", display_id, __func__, layer_id);
        return HWC2_ERROR_BAD_LAYER;
    }

    switch (type)
    {
        case HWC2_COMPOSITION_CLIENT:
            if (layer->getHwlayerType() != HWC_LAYER_TYPE_INVALID)
                layer->setStateChanged(true);
            layer->setHwlayerType(HWC_LAYER_TYPE_INVALID, __LINE__);
            break;

        case HWC2_COMPOSITION_DEVICE:
            if (layer->getHwlayerType() != HWC_LAYER_TYPE_UI)
                layer->setStateChanged(true);
            layer->setHwlayerType(HWC_LAYER_TYPE_UI, __LINE__);
            break;

        case HWC2_COMPOSITION_SIDEBAND:
            abort();

        case HWC2_COMPOSITION_SOLID_COLOR:
            if (layer->getHwlayerType() != HWC_LAYER_TYPE_DIM)
                layer->setStateChanged(true);
            layer->toBeDim();
            layer->setHwlayerType(HWC_LAYER_TYPE_DIM, __LINE__);
            break;

        case HWC2_COMPOSITION_CURSOR:
            if (layer->getHwlayerType() != HWC_LAYER_TYPE_CURSOR)
                layer->setStateChanged(true);
            layer->setHwlayerType(HWC_LAYER_TYPE_CURSOR, __LINE__);
            break;
    }
    layer->setSFCompositionType(type, true);

    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::layerStateSetDataSpace(
    hwc2_device_t* /*device*/,
    hwc2_display_t /*display*/,
    hwc2_layer_t /*layer*/,
    int32_t /*android_dataspace_t*/ /*dataspace*/)
{
    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::layerStateSetDisplayFrame(
    hwc2_device_t* /*device*/,
    hwc2_display_t display,
    hwc2_layer_t layer_id,
    hwc_rect_t frame)
{
    if (!DisplayManager::getInstance().m_data[display].connected)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    sp<HWCLayer> layer = m_displays[display]->getLayer(layer_id);
    if (layer == nullptr)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") does not contain layer(%" PRIu64 ")", __func__, display, layer_id);
        return HWC2_ERROR_BAD_LAYER;
    }

    DisplayData* disp_info = &(DisplayManager::getInstance().m_data[display]);

    if (frame.right > disp_info->width)
    {
        HWC_LOGW("%s: (%" PRIu64 ") layer id:%" PRIu64 " displayframe width(%d) > display device width(%d)",
            __func__, display, layer_id, WIDTH(frame), disp_info->width);
        frame.right = disp_info->width;
    }

    if (frame.bottom > disp_info->height)
    {
        HWC_LOGW("%s: (%" PRIu64 ") layer id:%" PRIu64 " displayframe height(%d) > display device height(%d)",
            __func__, display, layer_id, HEIGHT(frame), disp_info->height);
        frame.bottom = disp_info->height;
    }

    HWC_LOGV("%s: (%" PRIu64 ") layer id:%" PRIu64 " frame[%d,%d,%d,%d] ",
        __func__, display, layer_id, frame.left, frame.top, frame.right, frame.bottom);

    layer->setDisplayFrame(frame);

    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::layerStateSetPlaneAlpha(
    hwc2_device_t* /*device*/,
    hwc2_display_t display,
    hwc2_layer_t layer_id,
    float alpha)
{
    if (!DisplayManager::getInstance().m_data[display].connected)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    sp<HWCLayer> layer = m_displays[display]->getLayer(layer_id);
    if (layer == nullptr)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") does not contain layer(%" PRIu64 ")", __func__, display, layer_id);
        return HWC2_ERROR_BAD_LAYER;
    }

    layer->setPlaneAlpha(alpha);

    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::layerStateSetSidebandStream(
    hwc2_device_t* /*device*/,
    hwc2_display_t /*display*/,
    hwc2_layer_t /*layer*/,
    const native_handle_t* /*stream*/)
{
    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::layerStateSetSourceCrop(
    hwc2_device_t* /*device*/,
    hwc2_display_t display,
    hwc2_layer_t layer_id,
    hwc_frect_t crop)
{
    if (!DisplayManager::getInstance().m_data[display].connected)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    sp<HWCLayer> layer = m_displays[display]->getLayer(layer_id);
    if (layer == nullptr)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") does not contain layer(%" PRIu64 ")", __func__, display, layer_id);
        return HWC2_ERROR_BAD_LAYER;
    }

    layer->setSourceCrop(crop);
    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::layerStateSetTransform(
    hwc2_device_t* /*device*/,
    hwc2_display_t display,
    hwc2_layer_t layer_id,
    int32_t /*hwc_transform_t*/ transform)
{
    if (!DisplayManager::getInstance().m_data[display].connected)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    sp<HWCLayer> layer = m_displays[display]->getLayer(layer_id);
    if (layer == nullptr)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") does not contain layer(%" PRIu64 ")", __func__, display, layer_id);
        return HWC2_ERROR_BAD_LAYER;
    }

    switch (transform)
    {
        case 0:
        case HWC_TRANSFORM_FLIP_H:
        case HWC_TRANSFORM_FLIP_V:
        case HWC_TRANSFORM_ROT_90:
        case HWC_TRANSFORM_ROT_180:
        case HWC_TRANSFORM_ROT_270:
        case HWC_TRANSFORM_FLIP_H_ROT_90:
        case HWC_TRANSFORM_FLIP_V_ROT_90:
        case Transform::ROT_INVALID:
            layer->setTransform(transform);
            break;

        default:
            HWC_LOGE("%s: unknown transform(%d)", __func__, transform);
            return HWC2_ERROR_BAD_PARAMETER;
    }
    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::layerStateSetVisibleRegion(
    hwc2_device_t* /*device*/,
    hwc2_display_t display,
    hwc2_layer_t layer_id,
    hwc_region_t visible)
{
    if (!DisplayManager::getInstance().m_data[display].connected)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    sp<HWCLayer> layer = m_displays[display]->getLayer(layer_id);
    if (layer == nullptr)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") does not contain layer(%" PRIu64 ")", __func__, display, layer_id);
        return HWC2_ERROR_BAD_LAYER;
    }

    HWC_LOGV("(%" PRIu64 ") layerSetVisibleRegion() layer id:%" PRIu64, display, layer_id);
    layer->setVisibleRegion(visible);
    layer->setVisible(true);

    return HWC2_ERROR_NONE;
}

int32_t /*hwc2_error_t*/ HWCMediator::layerStateSetZOrder(
    hwc2_device_t* /*device*/,
    hwc2_display_t display,
    hwc2_layer_t layer_id,
    uint32_t z)
{
    if (!DisplayManager::getInstance().m_data[display].connected)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") is not connected", __func__, display);
        return HWC2_ERROR_BAD_DISPLAY;
    }

    sp<HWCLayer> layer = m_displays[display]->getLayer(layer_id);
    if (layer == nullptr)
    {
        HWC_LOGE("%s: the display(%" PRIu64 ") does not contain layer(%" PRIu64 ")", __func__, display, layer_id);
        return HWC2_ERROR_BAD_LAYER;
    }

    layer->setZOrder(z);
    return HWC2_ERROR_NONE;
}

void HWCMediator::adjustVsyncOffset()
{
    bool need_disable_vsync_offset = false;
    for (auto& display : m_displays)
    {
        need_disable_vsync_offset |= display->needDisableVsyncOffset();
    }
    if (m_vsync_offset_state != !need_disable_vsync_offset) {
        m_vsync_offset_state = !need_disable_vsync_offset;
        setMergeMdInfo2Ged(need_disable_vsync_offset);
    }
}

void HWCMediator::countdowmSkipValiRelatedNumber()
{
    for (size_t i = 0; i < m_displays.size(); ++i)
    {
        if (!m_displays[i]->isConnected())
            continue;

        HWCDispatcher::getInstance().decSessionModeChanged();
    }

    for (size_t i = 0; i < m_displays.size(); ++i)
    {
        if (!m_displays[i]->isValid())
            continue;

        const int32_t disp_id = static_cast<int32_t>(m_displays[i]->getId());

        HWCDispatcher::getInstance().decOvlEnginePowerModeChanged(disp_id);
    }

    HWCMediator::getInstance().decDriverRefreshCount();
}

bool HWCMediator::checkSkipValidate()
{
    DbgLogger logger(DbgLogger::TYPE_HWC_LOG | DbgLogger::TYPE_DUMPSYS, 'D', "SkipV(%zu: ", m_displays.size());
    bool has_valid_display = false;

    if (HWCMediator::getInstance().getDriverRefreshCount() > 0)
    {
        logger.printf("no skip vali(L%d) ", __LINE__);
        return false;
    }

    // If a display just change the power mode and it's session mode change,
    // we should not skip validate
    for (size_t i = 0; i < m_displays.size(); ++i)
    {
        if (!m_displays[i]->isConnected())
            continue;

        if (HWCDispatcher::getInstance().getSessionModeChanged() > 0)
        {
            logger.printf("no skip vali(%d:L%d) ", i, __LINE__);
            return false;
        }
    }

    for (size_t i = 0; i < m_displays.size(); ++i)
    {
        if (!m_displays[i]->isValid())
        {
            continue;
        }

        const int32_t disp_id = static_cast<int32_t>(m_displays[i]->getId());
        has_valid_display = true;

        if (m_displays[i]->getId() > HWC_DISPLAY_PRIMARY)
        {
            logger.printf("no skip vali(%d:L%d) ", i, __LINE__);
            return false;
        }

        if (m_displays[i]->isForceGpuCompose())
        {
            logger.printf("no skip vali(%d:L%d) ", i, __LINE__);
            return false;
        }

        auto&& layers = m_displays[i]->getVisibleLayersSortedByZ();

        for (size_t j = 0; j < layers.size(); ++j)
        {
            if (layers[j]->isStateChanged())
            {
                logger.printf("no skip vali(%d:L%d) ", i, __LINE__);
                return false;
            }

            if (layers[j]->getHwlayerType() == HWC_LAYER_TYPE_INVALID)
            {
                logger.printf("no skip vali(%d:L%d) ", i, __LINE__);
                return false;
            }
        }

        DispatcherJob* job = HWCDispatcher::getInstance().getExistJob(disp_id);
        if (job == NULL || m_displays[i]->getPrevAvailableInputLayerNum() != job->num_layers)
        {
            logger.printf("no skip vali(%d:L%d) ", i, __LINE__);
            return false;
        }

        if (m_displays[i]->isVisibleLayerChanged())
        {
            logger.printf("no skip vali(%d:L%d) ", i, __LINE__);
            return false;
        }

        if (HWCDispatcher::getInstance().getOvlEnginePowerModeChanged(disp_id) > 0)
        {
            logger.printf("no skip vali(%d:L%d) ", i, __LINE__);
            return false;
        }

        // if there exists secure layers, we do not skip validate.
        if (listSecure(layers))
        {
            logger.printf("no skip vali(%d:L%d) ", i, __LINE__);
            return false;
        }
    }

    if (has_valid_display)
        logger.printf("do skip vali");

    return (has_valid_display)? true : false;
}

int HWCMediator::getValidDisplayNum()
{
    int count = 0;
    for (size_t i = 0; i<m_displays.size(); ++i)
    {
        if (m_displays[i]->isValid())
            count++;
    }

    return count;
}

void HWCMediator::buildVisibleAndInvisibleLayerForAllDisplay()
{
    // build visible layers
    for (auto& hwc_display : m_displays)
    {
        if (!hwc_display->isValid())
            continue;

        hwc_display->removePendingRemovedLayers();
        hwc_display->buildVisibleAndInvisibleLayersSortedByZ();
        hwc_display->setupPrivateHandleOfLayers();
    }
}

void HWCMediator::prepareForValidation()
{
    editSetBufFromSfLog().flushOut();
    editSetBufFromSfLog().printf(g_set_buf_from_sf_log_prefix);

    if (editSetCompFromSfLog().getLen() != static_cast<int>(strlen(g_set_comp_from_sf_log_prefix)))
    {
        editSetCompFromSfLog().flushOut();
        editSetCompFromSfLog().printf(g_set_comp_from_sf_log_prefix);
    }

    // validate all displays
    {
        for (auto& hwc_display : m_displays)
        {
            if (!hwc_display->isValid())
                continue;

            HWCDispatcher::getInstance().getJob(static_cast<int32_t>(hwc_display->getId()));
            hwc_display->initPrevCompTypes();
            hwc_display->setJobDisplayOrientation();
        }
    }
}

void HWCMediator::setValiPresentStateOfAllDisplay(const HWC_VALI_PRESENT_STATE& val, const int32_t& line)
{
    for (size_t i = 0; i < m_displays.size(); ++i)
    {
        if (!m_displays[i]->isValid())
        {
            continue;
        }
        m_displays[i]->setValiPresentState(val, line);
    }
}

void calculateMdpDstRoi(sp<HWCLayer> layer, const double& mdp_scale_percentage, const int32_t& /*z_seq*/)
{
    if (layer->getHwlayerType() != HWC_LAYER_TYPE_MM)
        return;

    if (!HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->isDispRpoSupported())
    {
        layer->editMdpDstRoi() = layer->getDisplayFrame();
        return;
    }

    const bool need_mdp_rot = (layer->getLayerCaps() & MDP_ROT_LAYER);
    const bool need_mdp_rsz = (layer->getLayerCaps() & MDP_RSZ_LAYER);

    // process rotation
    if (need_mdp_rot)
    {
        layer->editMdpDstRoi().left = 0;
        layer->editMdpDstRoi().top = 0;
        switch (layer->getTransform())
        {
            case HAL_TRANSFORM_ROT_90:
            case HAL_TRANSFORM_ROT_270:
                layer->editMdpDstRoi().right  = HEIGHT(layer->getSourceCrop());
                layer->editMdpDstRoi().bottom = WIDTH(layer->getSourceCrop());
                break;

            default:
                layer->editMdpDstRoi().right  = WIDTH(layer->getSourceCrop());
                layer->editMdpDstRoi().bottom = HEIGHT(layer->getSourceCrop());
                break;
        }
    }
    else
    {
        layer->editMdpDstRoi().left = layer->getSourceCrop().left;
        layer->editMdpDstRoi().top = layer->getSourceCrop().top;
        layer->editMdpDstRoi().right = layer->getSourceCrop().right;
        layer->editMdpDstRoi().bottom = layer->getSourceCrop().bottom;
    }

    if (need_mdp_rsz)
    {
        const int32_t src_w = WIDTH(layer->editMdpDstRoi());
        const int32_t src_h = HEIGHT(layer->editMdpDstRoi());
        const int32_t dst_w = WIDTH(layer->getDisplayFrame());
        const int32_t dst_h = HEIGHT(layer->getDisplayFrame());

        const int32_t max_src_w_of_disp_rsz = HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->getCapsInfo()->rsz_in_max[0];

        const bool is_any_shrank = (src_w >= dst_w || src_h >= dst_h);

        if (!is_any_shrank)
        {
            const double max_width_scale_percentage_of_mdp =
                static_cast<double>(max_src_w_of_disp_rsz - src_w) / (dst_w - src_w);

            const double final_mdp_scale_percentage = min(max_width_scale_percentage_of_mdp, mdp_scale_percentage);

            const int& dst_l = layer->getDisplayFrame().left;
            const int& dst_t = layer->getDisplayFrame().top;
            layer->editMdpDstRoi().left  = dst_l;
            layer->editMdpDstRoi().top   = dst_t;
            layer->editMdpDstRoi().right = dst_l +
                src_w * (1 - final_mdp_scale_percentage) + dst_w * final_mdp_scale_percentage;
            layer->editMdpDstRoi().bottom = dst_t +
                src_h * (1 - mdp_scale_percentage) + dst_h * mdp_scale_percentage;
        }
    }
    else
    {
        // only rotation without resizing
        layer->editMdpDstRoi().left = layer->getDisplayFrame().left;
        layer->editMdpDstRoi().top = layer->getDisplayFrame().top;
        layer->editMdpDstRoi().right = layer->getDisplayFrame().right;
        layer->editMdpDstRoi().bottom = layer->getDisplayFrame().bottom;
    }
}

void HWCMediator::validate()
{
    // check if mirror mode exists
    {
        for (auto& hwc_display : m_displays)
        {
            if (!hwc_display->isValid())
                continue;

            for (auto& hwc_layer : hwc_display->getVisibleLayersSortedByZ())
                hwc_layer->setLayerCaps(0);

            hwc_display->validate();
        }
        updateGlesRangeForAllDisplays();
    }

    {
        // check hrt
        for (auto& hwc_display : m_displays)
        {
            if (!hwc_display->isValid())
                continue;

            for (size_t i = 0; i < hwc_display->getVisibleLayersSortedByZ().size(); ++i)
            {
                calculateMdpDstRoi(
                    hwc_display->getVisibleLayersSortedByZ()[i],
                    Platform::getInstance().m_config.mdp_scale_percentage,
                    i);
            }

            int32_t gles_head = -1, gles_tail = -1;
            hwc_display->getGlesRange(&gles_head, &gles_tail);

            const int32_t disp_id = static_cast<int32_t>(hwc_display->getId());
            DispatcherJob* job = HWCDispatcher::getInstance().getExistJob(disp_id);
            if (job != NULL)
            {
                job->layer_info.hwc_gles_head = gles_head;
                job->layer_info.hwc_gles_tail = gles_tail;
            }
        }
        m_hrt.run(m_displays, false);
        updateGlesRangeForAllDisplays();
    }

    // for limitation of max layer number of blitdev
    {
        for (auto& hwc_display : m_displays)
        {
            if (!hwc_display->isConnected() || hwc_display->getPowerMode() == HWC2_POWER_MODE_OFF)
                continue;

            if (HWCMediator::getInstance().getOvlDevice(hwc_display->getId())->getType() != OVL_DEVICE_TYPE_BLITDEV)
                continue;

            if (hwc_display->getMirrorSrc() != -1)
                continue;

            auto& layers = hwc_display->getVisibleLayersSortedByZ();
            for (size_t i = 0; i < layers.size(); ++i)
            {
                layers[i]->setHwlayerType(HWC_LAYER_TYPE_INVALID, __LINE__);
            }
            hwc_display->updateGlesRange();
        }
    }
}

bool HWCMediator::isAllDisplayPresentDone()
{
    for(auto& hwc_display : m_displays)
    {
        if (!hwc_display->isValid())
            continue;

        if (hwc_display->getValiPresentState() != HWC_VALI_PRESENT_STATE_PRESENT_DONE)
            return false;
    }
    return true;
}

void HWCMediator::lockRefreshThread(hwc2_display_t display)
{
    if(!isValidated())
    {
        AbortMessager::getInstance().printf("(%" PRIu64 ") HWCMediator m_refresh_vali_lock lock +", display);
        lockRefreshVali();
        AbortMessager::getInstance().printf("(%" PRIu64 ") HWCMediator m_refresh_vali_lock lock -", display);
        setValidated(true);
    }
}

void HWCMediator::unlockRefreshThread(hwc2_display_t display)
{
    if(isValidated())
    {
        if(isAllDisplayPresentDone())
        {
            setValidated(false);
            unlockRefreshVali();
            AbortMessager::getInstance().printf("(%" PRIu64 ") HWCMediator m_refresh_vali_lock unlock", display);
        }
    }
}


#include "platform_common.h"
#undef DEBUG_LOG_TAG
#define DEBUG_LOG_TAG "PLTC"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS

#include <utility>

#include <cutils/properties.h>

#include <DpAsyncBlitStream.h>

#include "hwdev.h"
#include "utils/tools.h"
#include "platform_common.h"
#include "utils/transform.h"
extern unsigned int mapDpOrientation(const uint32_t transform);

void PlatformCommon::initOverlay()
{
}

bool PlatformCommon::isUILayerValid(
    int /*dpy*/, struct hwc_layer_1* layer, PrivateHandle* priv_handle)
{
    if (priv_handle->format != HAL_PIXEL_FORMAT_YUYV &&
        priv_handle->format != HAL_PIXEL_FORMAT_YCbCr_422_I &&
        priv_handle->format != HAL_PIXEL_FORMAT_IMG1_BGRX_8888 &&
        (priv_handle->format < HAL_PIXEL_FORMAT_RGBA_8888 ||
         priv_handle->format > HAL_PIXEL_FORMAT_BGRA_8888))
        return false;

    // hw does not support HWC_BLENDING_COVERAGE
    if (layer->blending == HWC_BLENDING_COVERAGE)
        return false;

    // opaqaue layer should ignore alpha channel
    if (layer->blending == HWC_BLENDING_NONE)
    {
        switch (priv_handle->format)
        {
            case HAL_PIXEL_FORMAT_RGBA_8888:
                priv_handle->format = HAL_PIXEL_FORMAT_RGBX_8888;
                break;

            case HAL_PIXEL_FORMAT_BGRA_8888:
                return false;

            default:
                break;
        }
    }

    int w = getSrcWidth(layer);
    int h = getSrcHeight(layer);

    // ovl cannot accept <=0
    if (w <= 0 || h <= 0)
        return false;

    // [NOTE]
    // Since OVL does not support float crop, adjust coordinate to interger
    // as what SurfaceFlinger did with hwc before version 1.2
    int src_left = getSrcLeft(layer);
    int src_top = getSrcTop(layer);

    // cannot handle source negative offset
    if (src_left < 0 || src_top < 0)
        return false;

    // switch width and height for prexform with ROT_90
    if (0 != priv_handle->prexform)
    {
        DbgLogger logger(DbgLogger::TYPE_DUMPSYS, 'I',
            "prexformUI:%d x:%d, prex:%d, f:%d/%d, s:%d/%d",
            m_config.prexformUI, layer->transform, priv_handle->prexform,
            WIDTH(layer->displayFrame), HEIGHT(layer->displayFrame), w, h);

        if (0 == m_config.prexformUI)
            return false;

        if (0 != (priv_handle->prexform & HAL_TRANSFORM_ROT_90))
            SWAP(w, h);
    }

    // cannot handle rotation
    if (layer->transform != priv_handle->prexform)
        return false;

    if (!DispDevice::getInstance().isDispRpoSupported())
    {
        // cannot handle scaling
        if (WIDTH(layer->displayFrame) != w || HEIGHT(layer->displayFrame) != h)
            return false;
    }
    return true;
}

bool PlatformCommon::isMMLayerValid(
    int dpy, struct hwc_layer_1* layer, PrivateHandle* priv_handle, bool& /*is_high*/)
{
    // only use MM layer without any blending consumption
    if (layer->blending == HWC_BLENDING_COVERAGE)
        return false;

    const int srcWidth = getSrcWidth(layer);
    const int srcHeight = getSrcHeight(layer);

    // check src rect is not empty
    if (srcWidth <= 1 || srcHeight <= 1)
        return false;

    const int dstWidth = WIDTH(layer->displayFrame);
    const int dstHeight = HEIGHT(layer->displayFrame);

    // constraint to prevent bliter making error.
    // bliter would crop w or h to 0
    if (dstWidth <= 1 ||  dstHeight <= 1)
        return false;

    const int srcLeft = getSrcLeft(layer);
    const int srcTop = getSrcTop(layer);

    // cannot handle source negative offset
    if (srcLeft < 0 || srcTop < 0)
        return false;

    int buffer_type = (priv_handle->ext_info.status & GRALLOC_EXTRA_MASK_TYPE);

    if (!(buffer_type == GRALLOC_EXTRA_BIT_TYPE_VIDEO ||
        buffer_type == GRALLOC_EXTRA_BIT_TYPE_CAMERA ||
        priv_handle->format == HAL_PIXEL_FORMAT_YV12))
    {
        // Because limit is 9, but HWC will change size to align 2. so use 11 to be determine rule
        if (srcWidth < 11 || srcHeight < 11 || dstWidth < 11 || dstHeight < 11)
        {
            HWC_LOGD("layers with scaling cannot be processed by HWC because of src(w,h,x,y)=(%d,%d,%d,%d) dst(w,h)=(%d,%d)",
                srcWidth, srcHeight, srcLeft, srcTop, dstWidth, dstHeight);
            return false;
        }

    }

    // judge the layer need or not rotate by MDP
    // transform rectify by pre-rotation
    uint32_t xform = layer->transform;
    rectifyXformWithPrexform(&xform, priv_handle->prexform);
    if (xform != Transform::ROT_0)
    {
        // RGBA rotate
        if (priv_handle->format == HAL_PIXEL_FORMAT_RGBA_8888 ||
            priv_handle->format == HAL_PIXEL_FORMAT_BGRA_8888 ||
            priv_handle->format == HAL_PIXEL_FORMAT_RGBX_8888 ||
            priv_handle->format == HAL_PIXEL_FORMAT_BGRX_8888 ||
            priv_handle->format == HAL_PIXEL_FORMAT_RGB_888 ||
            priv_handle->format == HAL_PIXEL_FORMAT_RGB_565)
        {
            // RGB layer can not rotate and scaling at the same time
            if (dstWidth != srcHeight || dstHeight != srcWidth)
            {
                return false;
            }
            // RGB layer can not rotate with source crop
            if (srcLeft != 0 || srcTop != 0)
            {
                return false;
            }
            // RDMA limitation, RGB source width must > 8
            // HWC will align srouce (w,h) to even number, so we should consider srcWidth == 9
            if (srcWidth <= 9)
            {
                return false;
            }
        }
    }
    else
    {
        // Can not support RGBA scale, or the alpha value will miss.
        if (priv_handle->format == HAL_PIXEL_FORMAT_RGBA_8888 ||
            priv_handle->format == HAL_PIXEL_FORMAT_BGRA_8888)
        {
            // RGBA only support rotate
            return false;
        }
    }

    // Because MDP can not process odd width and height, we will calculate the
    // crop area and roi later. Then we may adjust the size of source and
    // destination buffer. This behavior may cause that the scaling rate
    // increases, and therefore the scaling rate is over the limitation of MDP.
    const bool is_blit_valid =
        DpAsyncBlitStream::queryHWSupport(
            srcWidth, srcHeight, dstWidth, dstHeight, mapDpOrientation(layer->transform)) &&
        DpAsyncBlitStream::queryHWSupport(
            srcWidth - 1, srcHeight - 1, dstWidth + 2, dstHeight + 2, mapDpOrientation(layer->transform));
    if (!is_blit_valid)
        return false;

    const int secure = (priv_handle->usage & (GRALLOC_USAGE_PROTECTED | GRALLOC_USAGE_SECURE));

    const bool is_disp_valid = (dpy == HWC_DISPLAY_PRIMARY) || secure ||
                         (!(dstWidth & 0x01) && !(dstHeight & 0x01));

    return is_disp_valid;
}

#ifdef USE_HWC2
bool PlatformCommon::isUILayerValid(const sp<HWCLayer>& layer, int32_t* line)
{
    const PrivateHandle& priv_hnd = layer->getPrivateHandle();
    if (isCompressData(&priv_hnd) && !m_config.disp_support_decompress)
    {
        *line = __LINE__;
        return false;
    }

    if (priv_hnd.format != HAL_PIXEL_FORMAT_YUYV &&
        priv_hnd.format != HAL_PIXEL_FORMAT_YCbCr_422_I &&
        priv_hnd.format != HAL_PIXEL_FORMAT_IMG1_BGRX_8888 &&
        (priv_hnd.format < HAL_PIXEL_FORMAT_RGBA_8888 ||
         priv_hnd.format > HAL_PIXEL_FORMAT_RGBA_FP16))
    {
        *line = __LINE__;
        return false;
    }

    switch (layer->getBlend())
    {
        case HWC2_BLEND_MODE_COVERAGE:
            // hw does not support HWC_BLENDING_COVERAGE
            *line = __LINE__;
            return false;

        case HWC2_BLEND_MODE_NONE:
            // opaqaue layer should ignore alpha channel
            if (priv_hnd.format == HAL_PIXEL_FORMAT_BGRA_8888)
            {
                *line = __LINE__;
                return false;
            }
    }

    if (!DispDevice::getInstance().isConstantAlphaForRGBASupported())
    {
        // [NOTE]
        // 1. overlay engine does not support RGBX format
        //    the only exception is that the format is RGBX and the constant alpha is 0xFF
        //    in such a situation, the display driver would disable alpha blending automatically,
        //    treating this format as RGBA with ignoring the undefined alpha channel
        // 2. overlay does not support using constant alpah
        //    and premult blending at same time
        if ((layer->getPlaneAlpha() != 1.0f) &&
            (priv_hnd.format == HAL_PIXEL_FORMAT_RGBX_8888 ||
            priv_hnd.format == HAL_PIXEL_FORMAT_IMG1_BGRX_8888 ||
            layer->getBlend() == HWC2_BLEND_MODE_PREMULTIPLIED))
        {
            *line = __LINE__;
            return false;
        }
    }

    int w = getSrcWidth(layer);
    int h = getSrcHeight(layer);

    // ovl cannot accept <=0
    if (w <= 0 || h <= 0)
    {
        *line = __LINE__;
        return false;
    }

    // [NOTE]
    // Since OVL does not support float crop, adjust coordinate to interger
    // as what SurfaceFlinger did with hwc before version 1.2
    const int src_left = getSrcLeft(layer);
    const int src_top = getSrcTop(layer);

    // cannot handle source negative offset
    if (src_left < 0 || src_top < 0)
    {
        *line = __LINE__;
        return false;
    }

    // switch width and height for prexform with ROT_90
    if (0 != priv_hnd.prexform)
    {
        DbgLogger logger(DbgLogger::TYPE_DUMPSYS, 'I',
            "prexformUI:%d x:%d, prex:%d, f:%d/%d, s:%d/%d",
            m_config.prexformUI, layer->getTransform(), priv_hnd.prexform,
            WIDTH(layer->getDisplayFrame()), HEIGHT(layer->getDisplayFrame()), w, h);

        if (0 == m_config.prexformUI)
        {
            *line = __LINE__;
            return false;
        }

        if (0 != (priv_hnd.prexform & HAL_TRANSFORM_ROT_90))
            SWAP(w, h);
    }

    // cannot handle rotation
    if (layer->getTransform() != static_cast<int32_t>(priv_hnd.prexform))
    {
        *line = __LINE__;
        return false;
    }

    // for scaling case
    if (WIDTH(layer->getDisplayFrame()) != w || HEIGHT(layer->getDisplayFrame()) != h)
    {
        if (!DispDevice::getInstance().isDispRpoSupported())
        {
            *line = __LINE__;
            return false;
        }
        else
        {
            const uint32_t src_crop_width = (layer->getXform() & HAL_TRANSFORM_ROT_90) ?
                HEIGHT(layer->getSourceCrop()):WIDTH(layer->getSourceCrop());
            if (src_crop_width > HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->getCapsInfo()->rsz_in_max[0] &&
                (priv_hnd.format == HAL_PIXEL_FORMAT_RGBX_8888 ||
                 priv_hnd.format == HAL_PIXEL_FORMAT_BGRX_8888 ||
                 priv_hnd.format == HAL_PIXEL_FORMAT_RGB_888 ||
                 priv_hnd.format == HAL_PIXEL_FORMAT_RGB_565))
            {
                *line = __LINE__;
                return false;
            }
        }
    }
    *line = __LINE__;
    return true;
}

bool PlatformCommon::isMMLayerValid(const sp<HWCLayer>& layer, int32_t* line)
{
    if (layer->getBlend() == HWC2_BLEND_MODE_COVERAGE)
    {
        // only use MM layer without any blending consumption
        *line = __LINE__;
        return false;
    }

    const int srcWidth = getSrcWidth(layer);
    const int srcHeight = getSrcHeight(layer);
    const int dstWidth = WIDTH(layer->getDisplayFrame());
    const int dstHeight = HEIGHT(layer->getDisplayFrame());
    if (srcWidth < 4 || srcHeight < 4 ||
        dstWidth < 4 || dstHeight < 4)
    {
        // Prevent bliter error.
        // RGB serise buffer bound with HW limitation, must large than 3x3
        // YUV serise buffer need to prevent width/height align to 0
        *line = __LINE__;
        return false;
    }

    const int srcLeft = getSrcLeft(layer);
    const int srcTop = getSrcTop(layer);
    if (srcLeft < 0 || srcTop < 0)
    {
        // cannot handle source negative offset
        *line = __LINE__;
        return false;
    }

    const PrivateHandle& priv_hnd = layer->getPrivateHandle();

    if (priv_hnd.format == HAL_PIXEL_FORMAT_RGBA_1010102 ||
        priv_hnd.format == HAL_PIXEL_FORMAT_RGBA_FP16)
    {
        *line = __LINE__;
        return false;
    }

    if (isCompressData(&priv_hnd) && !m_config.mdp_support_decompress)
    {
        *line = __LINE__;
        return false;
    }

    int32_t layer_caps = 0;
    if (priv_hnd.format == HAL_PIXEL_FORMAT_RGBA_8888 ||
        priv_hnd.format == HAL_PIXEL_FORMAT_BGRA_8888)
    {
        if (!m_config.enable_rgba_rotate)
        {
            // MDP cannot handle RGBA scale and rotate.
            *line = __LINE__;
            return false;
        }

        // MDP doesn't support RGBA scale, it must handle by DISP_RSZ or GLES
        if (layer->needScaling() &&
            !HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->isDispRpoSupported())
        {
            // Both of MDP and DISP cannot handle RGBA scaling
            *line = __LINE__;
            return false;
        }

        layer_caps |= layer->needRotate() ? MDP_ROT_LAYER : 0;
        if ((layer_caps & MDP_ROT_LAYER) &&
            (srcLeft != 0 || srcTop != 0 || srcWidth <= 8))
        {
            // MDP cannot handle RGBA rotate which buffer not align LR corner
            HWC_LOGD("RGBA rotate cannot handle by HWC such src(x,y,w,h)=(%d,%d,%d,%d) dst(w,h)=(%d,%d)",
                        srcLeft, srcTop, srcWidth, srcHeight, dstWidth, dstHeight);
            *line = __LINE__;
            return false;
        }
    }
    else if (priv_hnd.format == HAL_PIXEL_FORMAT_RGBX_8888 ||
             priv_hnd.format == HAL_PIXEL_FORMAT_BGRX_8888 ||
             priv_hnd.format == HAL_PIXEL_FORMAT_RGB_888 ||
             priv_hnd.format == HAL_PIXEL_FORMAT_RGB_565)
    {
        if (layer->needScaling())
        {
            if (!m_config.enable_rgbx_scaling &&
                !HWCMediator::getInstance().getOvlDevice(HWC_DISPLAY_PRIMARY)->isDispRpoSupported())
            {
                // Both of MDP and DISP cannot handle RGBX scaling
                *line = __LINE__;
                return false;
            }
            layer_caps |= m_config.enable_rgbx_scaling ? MDP_RSZ_LAYER : 0;
        }

        layer_caps |= layer->needRotate() ? MDP_ROT_LAYER : 0;
        if ((layer_caps & MDP_ROT_LAYER) &&
            (srcLeft != 0 || srcTop != 0 || srcWidth <= 8))
        {
            // MDP cannot handle RGBX rotate which buffer not align LR corner
            HWC_LOGD("RGBX rotate cannot handle by HWC such src(x,y,w,h)=(%d,%d,%d,%d) dst(w,h)=(%d,%d)",
                        srcLeft, srcTop, srcWidth, srcHeight, dstWidth, dstHeight);
            *line = __LINE__;
            return false;
        }
    }
    else
    {
        layer_caps |= layer->needRotate() ? MDP_ROT_LAYER : 0;
        const double& mdp_scale_percentage = m_config.mdp_scale_percentage;
        if (layer->needScaling() &&
            !(fabs(mdp_scale_percentage - 0.0f) < 0.05f))
        {
            layer_caps |= MDP_RSZ_LAYER;
        }
    }

    // Because MDP can not process odd width and height, we will calculate the
    // crop area and roi later. Then we may adjust the size of source and
    // destination buffer. This behavior may cause that the scaling rate
    // increases, and therefore the scaling rate is over the limitation of MDP.
    const bool is_blit_valid =
        DpAsyncBlitStream::queryHWSupport(
            srcWidth, srcHeight, dstWidth, dstHeight, mapDpOrientation(layer->getXform())) &&
        DpAsyncBlitStream::queryHWSupport(
            srcWidth - 1, srcHeight - 1, dstWidth + 2, dstHeight + 2, mapDpOrientation(layer->getXform()));
    if (!is_blit_valid)
    {
        *line = __LINE__;
        return false;
    }

    const int secure = (priv_hnd.usage & (GRALLOC_USAGE_PROTECTED | GRALLOC_USAGE_SECURE));

    sp<HWCDisplay> disp = layer->getDisplay().promote();
    bool is_disp_valid = true;
    if (disp != nullptr)
    {
        is_disp_valid = (disp->getId() == HWC_DISPLAY_PRIMARY) || secure ||
                         (!(dstWidth & 0x01) && !(dstHeight & 0x01));
    }

    if (!is_disp_valid)
    {
        *line = __LINE__;
        return false;
    }

    layer->setLayerCaps(layer->getLayerCaps() | layer_caps);
    *line = __LINE__;
    return true;
}
#endif // USE_HWC2

size_t PlatformCommon::getLimitedVideoSize()
{
    // 4k resolution
    return 3840 * 2160;
}

size_t PlatformCommon::getLimitedExternalDisplaySize()
{
    // 2k resolution
    return 2048 * 1080;
}

PlatformCommon::PlatformConfig::PlatformConfig()
    : platform(PLATFORM_NOT_DEFINE)
    , compose_level(COMPOSE_DISABLE_ALL)
    , mirror_state(MIRROR_DISABLED)
    , overlay_cap(OVL_CAP_UNDEFINE)
    , bq_count(3)
    , mir_scale_ratio(0.0f)
    , format_mir_mhl(MIR_FORMAT_UNDEFINE)
    , ovl_overlap_limit(0)
#ifdef BYPASS_WLV1_CHECKING
    , bypass_wlv1_checking(true)
#else
    , bypass_wlv1_checking(false)
#endif
    , prexformUI(1)
    , rdma_roi_update(0)
    , force_full_invalidate(false)
    , use_async_bliter(false)
    , use_async_bliter_ultra(false)
    , wait_fence_for_display(false)
    , enable_smart_layer(false)
    , enable_rgba_rotate(false)
    , enable_rgbx_scaling(true)
    , av_grouping(true)
    , dump_buf_type('A')
    , dump_buf(0)
    , dump_buf_cont_type('A')
    , dump_buf_cont(0)
    , dump_buf_log_enable(false)
    , fill_black_debug(false)
    , always_setup_priv_hnd(false)
    , uipq_debug(false)
#ifdef MTK_USER_BUILD
    , wdt_ioctl(false)
#else
    , wdt_ioctl(true)
#endif
    , only_wfd_by_hwc(false)
    , only_wfd_by_dispdev(false)
    , blitdev_for_virtual(false)
    , is_async_blitdev(false)
    , is_support_ext_path_for_virtual(true)
    , is_skip_validate(true)
    , support_color_transform(false)
    , mdp_scale_percentage(1.f)
    , extend_mdp_capacity(false)
    , rpo_ui_max_src_width(0)
    , disp_support_decompress(false)
    , mdp_support_decompress(false)
    , disp_wdma_fmt_for_vir_disp(HAL_PIXEL_FORMAT_RGB_888)
    , disable_color_transform_for_secondary_displays(true)
    , remove_invisible_layers(true)
    , latch_unsignaled_buffer(true)
{
    char value[PROPERTY_VALUE_MAX];
    property_get("vendor.debug.hwc.blitdev_for_virtual", value, "-1");
    const int32_t num_value = atoi(value);
    if (-1 != num_value)
    {
        if (num_value)
        {
            blitdev_for_virtual = true;
            is_support_ext_path_for_virtual = true;
        }
        else
        {
            blitdev_for_virtual = false;
        }
    }
}

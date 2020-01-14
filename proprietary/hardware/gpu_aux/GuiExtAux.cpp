
#include <GuiExtAux.h>
#include <graphics_mtk_defs.h>
#include <gralloc1_mtk_defs.h>
#include <string.h>
#include <hardware/gralloc.h>
#include <ui/gralloc_extra.h>
#include <DpBlitStream.h>
#include <dlfcn.h>
#include <fcntl.h>

#include "utils.h"
#include "mtk_queue.h"

using namespace android;

int guiExtIsMultipleDisplayScene(void)
{
#define LIB_FULL_NAME "libgui_ext.so"
#define FUNC_FULL_NAME "guiExtIsMultipleDisplays"

    typedef status_t (*FNguiExtIsMultipleDisplays)(int32_t *const);

    static int inited;
    static FNguiExtIsMultipleDisplays pfn;
    static void *handle;

    if (inited == 0) {
        const char *err_str = NULL;

        inited = 1; /* inited */

        handle = dlopen(LIB_FULL_NAME, RTLD_NOW);
        if (handle == NULL) {
            MTK_LOGE("dlopen " LIB_FULL_NAME " failed");
        }

        dlerror(); /* clear error */
        pfn = reinterpret_cast<FNguiExtIsMultipleDisplays>(dlsym(handle, FUNC_FULL_NAME));

        if ((err_str = dlerror())) {
            MTK_LOGE("dlsym " FUNC_FULL_NAME " failed, [%s]", err_str);
        } else {
            inited = 2;    /* init success */
        }
    }

    int ret = 0;
    int is_multipledisplay = 0;

    if (inited == 2) {
        ret = pfn(&is_multipledisplay);
        if (ret) {
            MTK_LOGE("call " LIB_FULL_NAME " - " FUNC_FULL_NAME " ret error: %d", ret);
        }
    }

    return (is_multipledisplay > 0);
}

struct GuiExtAuxBufferItem {
public:
    android_native_buffer_t *mSrcBuffer;
    sp<GPUAUXBuffer>         mMTKBuffer;

    GuiExtAuxBufferItem() :
        mSrcBuffer(NULL),
        mMTKBuffer(NULL)
    {}

    ~GuiExtAuxBufferItem()
    {
        mMTKBuffer = NULL;
    }
};

struct GuiExtAuxBufferQueue {
public:
    Mutex                 mMutex;

    GPUAUXBufferQueue     mMTKQueue;
    GuiExtAuxBufferItem  *mSlots;

    int                   width;
    int                   height;
    int                   format;
    int                   num_slots;

    DpBlitStream          bltStream;

    GuiExtAuxBufferQueue(int _width, int _height, int _format, int _num_slots) :
        mMTKQueue(_num_slots),
        mSlots(new GuiExtAuxBufferItem[_num_slots]),
        width(_width),
        height(_height),
        format(_format),
        num_slots(_num_slots)
    {}

    ~GuiExtAuxBufferQueue()
    {
        delete [] mSlots;
    }
};

/* // unused function, mark it.
static buffer_handle_t guiExtAuxGetBufferHandle(android_native_buffer_t *buffer)
{
    if ((buffer->common.magic   == ANDROID_NATIVE_BUFFER_MAGIC) &&
        (buffer->common.version == sizeof(android_native_buffer_t))) {
        return buffer->handle;
    }
    return 0;
}
*/

void guiExtAuxSetWidthHieghtFromSrcBuffer(GuiExtAuxBufferQueueHandle bq, android_native_buffer_t *src_buffer)
{
    GuiExtAuxSetSize(bq, src_buffer->width, src_buffer->height);
}

static int guiExtAuxIsUIPQFormat(int format)
{
    if (
        (format == HAL_PIXEL_FORMAT_RGBX_8888) ||
        (format == HAL_PIXEL_FORMAT_RGBA_8888) ||
        (format == HAL_PIXEL_FORMAT_RGB_888)   ||
        (format == HAL_PIXEL_FORMAT_RGB_565)   ||
        0) {
        return 1;
    }

    return 0;
}

/*
 * return -1, use the defalut value.
 * reutrn corresponding profile, otherwise.
 */
static int guiExtAuxGetYUVColorSpace(GPUAUXBufferInfo_t *info, DP_PROFILE_ENUM *ret)
{
    switch (info->status & GRALLOC_EXTRA_MASK_YUV_COLORSPACE) {
        case GRALLOC_EXTRA_BIT_YUV_BT601_NARROW:
            *ret = DP_PROFILE_BT601;
            return 0;
        case GRALLOC_EXTRA_BIT_YUV_BT601_FULL:
            *ret = DP_PROFILE_FULL_BT601;
            return 0;
        case GRALLOC_EXTRA_BIT_YUV_BT709_NARROW:
            *ret = DP_PROFILE_BT709;
            return 0;
    }

    /* UI PQ should use JPEG. */
    if (guiExtAuxIsUIPQFormat(info->format)) {
        *ret = DP_PROFILE_JPEG;
        return 0;
    }

    *ret = DP_PROFILE_BT601;
    return -1;
}

static void guiExtAuxSetYUVColorSpace(android_native_buffer_t *buffer, DP_PROFILE_ENUM value)
{
    int mask = GRALLOC_EXTRA_MASK_YUV_COLORSPACE;
    int bit = 0;

    switch (value) {
        case DP_PROFILE_BT601:
            bit = GRALLOC_EXTRA_BIT_YUV_BT601_NARROW;
            break;
        case DP_PROFILE_FULL_BT601:
            bit = GRALLOC_EXTRA_BIT_YUV_BT601_FULL;
            break;
        case DP_PROFILE_BT709:
            bit = GRALLOC_EXTRA_BIT_YUV_BT709_NARROW;
            break;

        default:
            return;
    }

    MTKGralloc::getInstance()->setBufferPara(buffer, mask, bit);
}

static void guiExtAuxDokickConversion(GuiExtAuxBufferQueueHandle bq, android_native_buffer_t *src_buffer)
{
    /* update the w/h for output buffer */
    guiExtAuxSetWidthHieghtFromSrcBuffer(bq, src_buffer);

    int bufSlot;
    int fence_fd;

    GuiExtAuxDequeueBuffer(bq, &bufSlot, &fence_fd);

    if (fence_fd >= 0) {
        close(fence_fd);
        fence_fd = -1;
    }

    GuiExtAuxQueueBuffer(bq, bufSlot, -1);
}

static GuiExtAuxBufferItemHandle guiExtAuxFindItem(GuiExtAuxBufferQueueHandle bq, android_native_buffer_t *buffer)
{
    for (int i = 0; i < bq->num_slots; ++i) {
        android_native_buffer_t *b = GuiExtAuxRequestBuffer(bq, i);

        if (b == buffer) {
            return &bq->mSlots[i];
        }
    }

    return NULL;
}

extern "C" { /* begin of extern "C" */

    int GuiExtAuxIsSupportFormat(android_native_buffer_t *anb)
    {
        if (anb != NULL) {
            int format = anb->format;

            if ((format == HAL_PIXEL_FORMAT_I420) ||
                (format == HAL_PIXEL_FORMAT_YCbCr_420_888) ||
                (format == HAL_PIXEL_FORMAT_NV12_BLK) ||
                (format == HAL_PIXEL_FORMAT_NV12_BLK_FCM) ||
                (format == HAL_PIXEL_FORMAT_YUV_PRIVATE) ||
                (format == HAL_PIXEL_FORMAT_NV12_BLK_10BIT_H) ||
                (format == HAL_PIXEL_FORMAT_NV12_BLK_10BIT_V) ||
                (format == HAL_PIXEL_FORMAT_YUV_PRIVATE_10BIT) ||
                (format == HAL_PIXEL_FORMAT_UFO) ||
                (format == HAL_PIXEL_FORMAT_UFO_10BIT_H) ||
                (format == HAL_PIXEL_FORMAT_UFO_10BIT_V) ||
                (format == HAL_PIXEL_FORMAT_YCbCr_422_I) ||
                0) {
                return 1;
            }
        }

        return 0;
    }

    /*
     * This interface is to support UI PQ format.
     * We don't want to change GuiExtAuxIsSupportFormat behavior that may
     * cause in-use DDK failed.
     */
    int GuiExtAuxIsUIPQFormat(android_native_buffer_t *anb)
    {
        if (anb != NULL) {
            int format = anb->format;

            if (guiExtAuxIsUIPQFormat(format)) {
                return 1;
            }
        }

        return 0;
    }

    GuiExtAuxBufferQueueHandle GuiExtAuxCreateBufferQueue(int width, int height, int output_format, int num_max_buffer)
    {
        GuiExtAuxBufferQueueHandle bq;

        if ((output_format != HAL_PIXEL_FORMAT_YV12) &&
            (output_format != HAL_PIXEL_FORMAT_RGBA_8888) &&
            (output_format != HAL_PIXEL_FORMAT_RGB_888)) {
            MTK_LOGE("Unsupported color format %d", output_format);
            return NULL;
        }

        if (num_max_buffer < 1) {
            num_max_buffer = 2;
            MTK_LOGE("num_max_buffer(%d) < 1", num_max_buffer);
        }

        bq = new GuiExtAuxBufferQueue(width, height, output_format, num_max_buffer);

        if (bq == NULL) {
            MTK_LOGE("GPU_AUX_createBufferQueue allocate fail, out of memory");
            return NULL;
        }

        return bq;

    }

    void GuiExtAuxDestroyBufferQueue(GuiExtAuxBufferQueueHandle bq)
    {
        delete bq;
    }

    void GuiExtAuxSetSize(GuiExtAuxBufferQueueHandle bq, int width, int height)
    {
        bq->width = width;
        bq->height = height;
    }

    void GuiExtAuxSetName(GuiExtAuxBufferQueueHandle bq, const char *name)
    {
        bq->mMTKQueue.setConsumerName(name);
    }

    void GuiExtAuxKickConversion(GuiExtAuxBufferQueueHandle bq, android_native_buffer_t *src_buffer)
    {
        guiExtAuxDokickConversion(bq, src_buffer);
    }

    static int guiExtIsSurfaceFlinger()
    {
        static int bInitAppName = -1;

        if (bInitAppName == -1) {
            char path[PATH_MAX];
            char cmdline[PATH_MAX] = "";
            FILE *file;

            snprintf(path, PATH_MAX, "/proc/%d/cmdline", getpid());
            file = fopen(path, "r");
            if (file) {
                fgets(cmdline, sizeof(cmdline) - 1, file);
                fclose(file);
            } else {
                /* Open file fail, should never happen. What can we do?  */
                MTK_LOGE("open [%s] fail.", path);
            }
            bInitAppName = (strcmp(cmdline, "/system/bin/surfaceflinger") == 0);
        }

        return bInitAppName;
    }

    void GuiExtSetDstColorSpace(android_native_buffer_t *dst_buffer, android_native_buffer_t *src_buffer)
    {
        GPUAUXBufferInfo_t src_info = MTKGralloc::getInstance()->getBufferInfo(src_buffer);

        DP_PROFILE_ENUM out_dp_profile;
        if (guiExtAuxGetYUVColorSpace(&src_info, &out_dp_profile) != 0) {
            MTK_LOGW("src does not specify the COLORSPACE, use default: narrow-range");
        }

        guiExtAuxSetYUVColorSpace(dst_buffer, out_dp_profile);
    }

    static int PrivateFormat2HALFormat(int status)
    {
#undef GEN_CASE
#define GEN_CASE(f) case GRALLOC_EXTRA_BIT_CM_##f: return HAL_PIXEL_FORMAT_##f

        switch (status & GRALLOC_EXTRA_MASK_CM) {
                GEN_CASE(YV12);
                GEN_CASE(I420);
                GEN_CASE(NV12_BLK);
                GEN_CASE(NV12_BLK_FCM);
                GEN_CASE(NV12_BLK_10BIT_H);
                GEN_CASE(NV12_BLK_10BIT_V);
                GEN_CASE(YUYV);
                GEN_CASE(NV12);
        }
        return -1;
    }

/* // unused function, mark it.
    static DpColorFormat PrivateFormat2MDPFormat(int status)
    {
#undef GEN_CASE
#define GEN_CASE(f) case GRALLOC_EXTRA_BIT_CM_##f: return DP_COLOR_##f
        switch (status & GRALLOC_EXTRA_MASK_CM) {
                GEN_CASE(YV12);
                GEN_CASE(I420);
        }
        return (DpColorFormat) - 1;
    }
*/

    int GuiExtAuxDoConversionIfNeed(GuiExtAuxBufferQueueHandle bq, android_native_buffer_t *dst_buffer, android_native_buffer_t *src_buffer, int crop_width, int crop_height)
    {
        ATRACE_CALL();

        int err = 0;
        void *src_yp;

        int lockret;

        DpColorFormat dp_out_fmt;
        DpBlitStream &bltStream = bq->bltStream;

        unsigned int src_offset[2] = {0, 0};
        unsigned int src_size[3];
        unsigned int dst_size[3];
        DpRect src_roi;
        DpRect dst_roi;

        DpColorFormat dp_in_fmt;
        int plane_num;

        GPUAUXBufferInfo_t src_info = MTKGralloc::getInstance()->getBufferInfo(src_buffer);
        GPUAUXBufferInfo_t dst_info = MTKGralloc::getInstance()->getBufferInfo(dst_buffer);

        GuiExtAuxBufferItemHandle hnd;

        struct timeval target_end;

        // pre-rotation
        int prot = 0;

        if (src_info.err != 0 || dst_info.err != 0) {
            MTK_LOGE("retrive info fail: src:%p dst:%p", src_buffer, dst_buffer);
            return -1;
        }

        if (src_info.usage & (GRALLOC1_PRODUCER_USAGE_PROTECTED | GRALLOC1_USAGE_SECURE)) {
            MTK_LOGV("skip, cannot convert protect / secure buffer");
            return 0;
        }

        if (guiExtIsSurfaceFlinger()) {
            if ((src_info.status & GRALLOC_EXTRA_MASK_AUX_DIRTY) == 0) {
                if (guiExtAuxIsUIPQFormat(src_info.format)) {
                    /* Launch an app when enable UI PQ, the start window no pq flag,
                          but the 1st frame of the app has PQ, which will lead to distortion.
                          Add this restriction to let the start window has PQ. */
                    if ((src_info.status & GRALLOC_EXTRA_MASK_TYPE) == GRALLOC_EXTRA_BIT_TYPE_GPU) {
                        return 0;
                    }
                } else {
                    /* conversion already be done. */
                    return 0;
                }
            }

            /* clear AUX dirty bit */
            gralloc_extra_ion_sf_info_t sf_info;

            gralloc_extra_query(src_buffer->handle, GRALLOC_EXTRA_GET_IOCTL_ION_SF_INFO, &sf_info);
            gralloc_extra_sf_set_status(&sf_info, GRALLOC_EXTRA_MASK_AUX_DIRTY, 0);
            gralloc_extra_perform(src_buffer->handle, GRALLOC_EXTRA_SET_IOCTL_ION_SF_INFO, &sf_info);
        }

        hnd = guiExtAuxFindItem(bq, dst_buffer);

        hnd->mSrcBuffer = src_buffer;

        Mutex::Autolock l(bq->mMutex);

        /* set SRC config */
        {
            int src_format = 0;
            int src_width = 0;
            int src_height = 0;
            int src_y_stride = 0;
            int src_uv_stride = 0;
            int src_size_luma = 0;
            int src_size_chroma = 0;
            int src_no_pq = 0;

            if ((src_info.width  <= 0) ||
                (src_info.height <= 0) ||
                (src_info.stride <= 0)) {
                MTK_LOGE("Invalid buffer width %d, height %d, or stride %d", src_info.width, src_info.height, src_info.stride);
                return -1;
            }

            src_format = src_info.format;

            if (
                (src_info.format == HAL_PIXEL_FORMAT_YUV_PRIVATE) ||
                (src_info.format == HAL_PIXEL_FORMAT_YUV_PRIVATE_10BIT) ||
                (src_info.format == HAL_PIXEL_FORMAT_YCbCr_420_888) ||
                0) {
                src_format = PrivateFormat2HALFormat(src_info.status);

                if (src_format == -1) {
                    MTK_LOGE("unexpected format for variable format: 0x%x, status: 0x%x",
                             src_info.format,
                             (int)(src_info.status & GRALLOC_EXTRA_MASK_CM));
                }
            }

#define VIDEOBUFFER_ISVALID(v) (!!((v)&0x80000000))
#define VIDEOBUFFER_YALIGN(v) ({int t=((v)&0x7FFFFFFF)>>25;t==0?1:(t<<1);})
#define VIDEOBUFFER_CALIGN(v) ({int t=((v)&0x01FFFFFF)>>19;t==0?1:(t<<1);})
#define VIDEOBUFFER_HALIGN(v) ({int t=((v)&0x0007FFFF)>>13;t==0?1:(t<<1);})

            //MTK_LOGE("debug format 0x%x, 0x%x wh, %dx%d", src_info.format, src_format, src_info.width, src_info.height);
            //MTK_LOGE("debug swh, %dx%d, 0x%08x a:v%d h%d y%d c%d",
            //    src_info.stride, src_info.vertical_stride,
            //    src_info.videobuffer_status,
            //    VIDEOBUFFER_ISVALID(src_info.videobuffer_status),
            //    VIDEOBUFFER_HALIGN(src_info.videobuffer_status),
            //    VIDEOBUFFER_YALIGN(src_info.videobuffer_status),
            //    VIDEOBUFFER_CALIGN(src_info.videobuffer_status)
            //    );

            switch (src_format) {
                case HAL_PIXEL_FORMAT_I420:
                    src_width     = src_info.width;

                    if (VIDEOBUFFER_ISVALID(src_info.videobuffer_status)) {
                        src_height    = ALIGN(src_info.height, VIDEOBUFFER_HALIGN(src_info.videobuffer_status));
                        src_y_stride  = ALIGN(src_width, VIDEOBUFFER_YALIGN(src_info.videobuffer_status));
                        src_uv_stride = ALIGN(src_y_stride / 2, VIDEOBUFFER_CALIGN(src_info.videobuffer_status));
                    } else {
                        src_height    = src_info.height;
                        src_y_stride  = src_info.stride;
                        src_uv_stride = src_y_stride / 2;
                    }

                    plane_num = 3;
                    src_size_luma   = src_height * src_y_stride;
                    src_size_chroma = src_height * src_uv_stride / 2;
                    src_offset[0]   = src_size_luma;
                    src_offset[1]   = src_size_luma + src_size_chroma;
                    src_size[0]     = src_size_luma;
                    src_size[1]     = src_size_chroma;
                    src_size[2]     = src_size_chroma;
                    dp_in_fmt       = DP_COLOR_I420;
                    break;

                case HAL_PIXEL_FORMAT_YV12:
                    src_width     = src_info.width;

                    if (VIDEOBUFFER_ISVALID(src_info.videobuffer_status)) {
                        src_height    = ALIGN(src_info.height, VIDEOBUFFER_HALIGN(src_info.videobuffer_status));
                        src_y_stride  = ALIGN(src_width, VIDEOBUFFER_YALIGN(src_info.videobuffer_status));
                        src_uv_stride = ALIGN(src_y_stride / 2, VIDEOBUFFER_CALIGN(src_info.videobuffer_status));
                    } else {
                        src_height    = src_info.height;
                        src_y_stride  = src_info.stride;
                        src_uv_stride = ALIGN(src_y_stride / 2, 16);
                    }

                    plane_num = 3;
                    src_size_luma   = src_height * src_y_stride;
                    src_size_chroma = src_height * src_uv_stride / 2;
                    src_offset[0]   = src_size_luma;
                    src_offset[1]   = src_size_luma + src_size_chroma;
                    src_size[0]     = src_size_luma;
                    src_size[1]     = src_size_chroma;
                    src_size[2]     = src_size_chroma;
                    dp_in_fmt       = DP_COLOR_YV12;
                    break;

                case HAL_PIXEL_FORMAT_NV12:
                    src_width     = src_info.width;
                    if (VIDEOBUFFER_ISVALID(src_info.videobuffer_status)) {
                        src_height = ALIGN(src_info.height, VIDEOBUFFER_HALIGN(src_info.videobuffer_status));
                        src_y_stride  = ALIGN(src_width, VIDEOBUFFER_YALIGN(src_info.videobuffer_status));
                        src_uv_stride = ALIGN(src_y_stride, VIDEOBUFFER_CALIGN(src_info.videobuffer_status));
                    } else {
                        src_height = src_info.height;
                        src_y_stride  = ALIGN(src_info.stride, 16);
                        src_uv_stride = ALIGN(src_y_stride, 8);
                    }

                    plane_num = 2;
                    src_size_luma   = src_height * src_y_stride;
                    src_size_chroma = src_height * src_uv_stride;
                    src_offset[0]   = src_size_luma;
                    src_size[0]     = src_size_luma;
                    src_size[1]     = src_size_chroma;
                    dp_in_fmt       = DP_COLOR_NV12;
                    break;

                case HAL_PIXEL_FORMAT_NV12_BLK_10BIT_H:
                    if (VIDEOBUFFER_ISVALID(src_info.videobuffer_status)) {
                        src_width  = ALIGN(src_info.width, VIDEOBUFFER_YALIGN(src_info.videobuffer_status));
                        src_height = ALIGN(src_info.height, VIDEOBUFFER_HALIGN(src_info.videobuffer_status));
                    } else {
                        src_width  = ALIGN(src_info.width, 16);
                        src_height = ALIGN(src_info.height, 32);
                    }
                    src_y_stride  = src_width * 40;
                    src_uv_stride = src_y_stride / 2;

                    plane_num = 2;
                    // total bytes for 10 bit format
                    src_size_luma   = ALIGN(((src_width * src_height) * 5) >> 2, 512);
                    src_offset[0]   = src_size_luma;
                    src_size[0]     = src_size_luma;
                    src_size[1]     = src_size_luma >> 1;
                    src_size[2]     = 0;

                    dp_in_fmt       = DP_COLOR_420_BLKP_10_H;
                    break;

                case HAL_PIXEL_FORMAT_NV12_BLK_10BIT_V:
                    if (VIDEOBUFFER_ISVALID(src_info.videobuffer_status)) {
                        src_width  = ALIGN(src_info.width, VIDEOBUFFER_YALIGN(src_info.videobuffer_status));
                        src_height = ALIGN(src_info.height, VIDEOBUFFER_HALIGN(src_info.videobuffer_status));
                    } else {
                        src_width  = ALIGN(src_info.width, 16);
                        src_height = ALIGN(src_info.height, 32);
                    }
                    src_y_stride  = src_width * 40;
                    src_uv_stride = src_y_stride / 2;

                    plane_num = 2;
                    // total bytes for 10 bit format
                    src_size_luma   = ALIGN(((src_width * src_height) * 5) >> 2, 512);
                    src_offset[0]   = src_size_luma;
                    src_size[0]     = src_size_luma;
                    src_size[1]     = src_size_luma >> 1;
                    src_size[2]     = 0;

                    dp_in_fmt       = DP_COLOR_420_BLKP_10_V;
                    break;

                case HAL_PIXEL_FORMAT_NV12_BLK:
                    if (VIDEOBUFFER_ISVALID(src_info.videobuffer_status)) {
                        src_width  = ALIGN(src_info.width, VIDEOBUFFER_YALIGN(src_info.videobuffer_status));
                        src_height = ALIGN(src_info.height, VIDEOBUFFER_HALIGN(src_info.videobuffer_status));
                    } else {
                        src_width  = ALIGN(src_info.width, 16);
                        src_height = ALIGN(src_info.height, 32);
                    }
                    src_y_stride  = src_width * 32;
                    src_uv_stride = src_y_stride / 2;

                    plane_num = 2;
                    src_size_luma   = src_width * src_height;
                    src_size_chroma = src_width * src_height / 2;
                    src_offset[0]   = src_size_luma;
                    src_size[0]     = src_size_luma;
                    src_size[1]     = src_size_chroma;
                    dp_in_fmt       = DP_COLOR_420_BLKP;
                    break;

                case HAL_PIXEL_FORMAT_NV12_BLK_FCM:
                    src_width     = ALIGN(src_info.width, 16);
                    src_height    = ALIGN(src_info.height, 32);
                    src_y_stride  = src_width * 32;
                    src_uv_stride = src_y_stride / 2;

                    plane_num = 2;
                    src_size_luma   = src_width * src_height;
                    src_size_chroma = src_width * src_height / 2;
                    src_offset[0]   = src_size_luma;
                    src_size[0]     = src_size_luma;
                    src_size[1]     = src_size_chroma;
                    dp_in_fmt       = DP_COLOR_420_BLKI;
                    break;

                case HAL_PIXEL_FORMAT_UFO:
                    if (VIDEOBUFFER_ISVALID(src_info.videobuffer_status)) {
                        src_width   = ALIGN(src_info.width, VIDEOBUFFER_YALIGN(src_info.videobuffer_status));
                        src_height  = ALIGN(src_info.height, VIDEOBUFFER_HALIGN(src_info.videobuffer_status));
                    } else {
                        src_width   = src_info.stride;
                        src_height  = src_info.vertical_stride;
                    }
                    src_y_stride  = src_width * 32;
                    src_uv_stride = src_y_stride / 2;

                    plane_num = 2;
                    {
                        int pic_size_y_bs = ALIGN(src_width * src_height, 512);

                        src_offset[0] = pic_size_y_bs;

                        src_size[0] = pic_size_y_bs;;
                        src_size[1] = pic_size_y_bs;
                        src_size[2] = 0;
                    }

                    dp_in_fmt       = DP_COLOR_420_BLKP_UFO;
                    break;

                case HAL_PIXEL_FORMAT_UFO_10BIT_H:
                    if (VIDEOBUFFER_ISVALID(src_info.videobuffer_status)) {
                        src_width   = ALIGN(src_info.width, VIDEOBUFFER_YALIGN(src_info.videobuffer_status));
                        src_height  = ALIGN(src_info.height, VIDEOBUFFER_HALIGN(src_info.videobuffer_status));
                    } else {
                        src_width   = src_info.stride;
                        src_height  = src_info.vertical_stride;
                    }
                    src_y_stride  = src_width * 40;
                    src_uv_stride = src_y_stride / 2;

                    plane_num = 2;
                    src_size_luma   = ((src_width * src_height) * 5) >> 2; //total bytes for 10 bit format
                    src_offset[0] = src_size_luma;
                    src_size[0] = src_size_luma;
                    src_size[1] = src_size_luma >> 1;
                    src_size[2] = 0;

                    dp_in_fmt       = DP_COLOR_420_BLKP_UFO_10_H;
                    break;

                case HAL_PIXEL_FORMAT_UFO_10BIT_V:
                    if (VIDEOBUFFER_ISVALID(src_info.videobuffer_status)) {
                        src_width   = ALIGN(src_info.width, VIDEOBUFFER_YALIGN(src_info.videobuffer_status));
                        src_height  = ALIGN(src_info.height, VIDEOBUFFER_HALIGN(src_info.videobuffer_status));
                    } else {
                        src_width   = src_info.stride;
                        src_height  = src_info.vertical_stride;
                    }
                    src_y_stride  = src_width * 40;
                    src_uv_stride = src_y_stride / 2;

                    plane_num = 2;
                    src_size_luma   = ((src_width * src_height) * 5) >> 2; //total bytes for 10 bit format
                    src_offset[0] = src_size_luma;
                    src_size[0] = src_size_luma;
                    src_size[1] = src_size_luma >> 1;
                    src_size[2] = 0;

                    dp_in_fmt       = DP_COLOR_420_BLKP_UFO_10_V;
                    break;

                case HAL_PIXEL_FORMAT_YCbCr_422_I:
                    src_width     = src_info.width;
                    src_height    = src_info.height;
                    src_y_stride    = src_info.width * 2;
                    src_uv_stride   = 0;

                    plane_num = 1;
                    src_size_luma   = src_info.width * src_info.height * 2;
                    src_size[0]     = src_size_luma;
                    dp_in_fmt       = DP_COLOR_YUYV;
                    src_no_pq = 1;

                    /* pre-rotation */
                    gralloc_extra_query(src_buffer->handle, GRALLOC_EXTRA_GET_ORIENTATION, &prot);
                    if ((prot & HAL_TRANSFORM_ROT_90) == HAL_TRANSFORM_ROT_90 ||
                        (prot & HAL_TRANSFORM_ROT_270) == HAL_TRANSFORM_ROT_270) {
                        int hStride2;
                        gralloc_extra_query(src_buffer->handle, GRALLOC_EXTRA_GET_BYTE_2ND_STRIDE, &hStride2);

                        //MTK_LOGE("Aux debug, --90,270 prot = 0x%x ,hStride2 = %d", prot ,hStride2);
                        src_y_stride    = hStride2 * 2;
                        src_width     = src_info.height;
                        src_height    = src_info.width;
                        src_size_luma = hStride2 * src_height * 2;
                        src_size[0]     = src_size_luma;
                    } else {
                        //MTK_LOGE("Aux debug, --other, prot = 0x%x ,hStride2 = %d", prot ,hStride2);
                        src_width     = src_info.width;
                        src_height    = src_info.height;
                    }
                    break;

                case HAL_PIXEL_FORMAT_RGBX_8888:
                case HAL_PIXEL_FORMAT_RGBA_8888:
                    src_width     = src_info.width;
                    src_height    = src_info.height;
                    src_y_stride  = src_info.stride * 4;
                    plane_num = 1;
                    src_size[0]     = src_y_stride * src_height;
                    src_size[1]     = 0;
                    src_size[2]     = 0;

                    dp_in_fmt     = DP_COLOR_RGBX8888;
                    break;

                case HAL_PIXEL_FORMAT_RGB_888:
                    src_width     = src_info.width;
                    src_height    = src_info.height;
                    src_y_stride  = src_info.stride * 3;
                    plane_num = 1;
                    src_size[0]     = src_y_stride * src_height;
                    src_size[1]     = 0;
                    src_size[2]     = 0;

                    dp_in_fmt     = DP_COLOR_RGB888;
                    break;

                case HAL_PIXEL_FORMAT_RGB_565:
                    src_width     = src_info.width;
                    src_height    = src_info.height;
                    src_y_stride  = src_info.stride * 2;
                    plane_num = 1;
                    src_size[0]     = src_y_stride * src_height;
                    src_size[1]     = 0;
                    src_size[2]     = 0;

                    dp_in_fmt     = DP_COLOR_RGB565;
                    break;

                default:
                    MTK_LOGE("src buffer format not support 0x%x, 0x%x", src_info.format, src_format);
                    return -1;
            }

            DpPqParam dppq_param;
            dppq_param.enable = true;
            dppq_param.scenario = MEDIA_VIDEO;
            dppq_param.u.video.id = src_info.pool_id;
            bool dp_flush_cache = false;

            /* For UI PQ, we are using the number 0xFFFFFF00 */
            if (guiExtAuxIsUIPQFormat(src_format)) {
                dppq_param.u.video.id = 0xFFFFFF00;
                dp_flush_cache = true;
            }

            dppq_param.u.video.timeStamp = src_info.timestamp;
            dppq_param.u.video.grallocExtraHandle = src_buffer->handle;
            dppq_param.u.video.isHDR2SDR = 0;

            /* No need to do PQ for video screen shot. */
            if (GuiExtAuxIsSupportFormat(src_buffer) &&
                ((src_info.status2 & GRALLOC_EXTRA_MASK2_VIDEO_PQ) == GRALLOC_EXTRA_BIT2_VIDEO_PQ_OFF)) {
                src_no_pq = 1;
            }

            if (!src_no_pq) {
                bltStream.setPQParameter(dppq_param);
            }

            if (src_info.ion_fd >= 0) {
                bltStream.setSrcBuffer(src_info.ion_fd, src_size, plane_num);
            } else {
                lockret = MTKGralloc::getInstance()->lockBuffer(src_buffer, MTKGralloc::getInstance()->getUsage(), &src_yp);

                if (0 != lockret) {
                    MTK_LOGE("lock src buffer fail");
                    return -1;
                }

                uintptr_t *src_addr[3] = {0, 0, 0};
                src_addr[0] = (uintptr_t *)src_yp;
                src_addr[1] = src_addr[0] + src_offset[0];
                src_addr[2] = src_addr[0] + src_offset[1];
                bltStream.setSrcBuffer((void **)src_addr, src_size, plane_num);
            }

            DpRect src_roi;
            src_roi.x = 0;
            src_roi.y = 0;
            if (crop_width > 1 && crop_height > 1) {
                src_roi.w = crop_width;
                src_roi.h = crop_height;
                dst_roi.w = crop_width;
                dst_roi.h = crop_height;
            } else {
                src_roi.w = src_info.width;
                src_roi.h = src_info.height;
                dst_roi.w = dst_info.width;
                dst_roi.h = dst_info.height;
            }

            if ((prot & HAL_TRANSFORM_ROT_90) == HAL_TRANSFORM_ROT_90 ||
                (prot & HAL_TRANSFORM_ROT_270) == HAL_TRANSFORM_ROT_270) {
                int temp = src_roi.w;
                src_roi.w = src_roi.h;
                src_roi.h = temp;
            }

            DP_PROFILE_ENUM src_dp_profile;
            guiExtAuxGetYUVColorSpace(&src_info, &src_dp_profile);

            bltStream.setSrcConfig(src_width, src_height, src_y_stride, src_uv_stride,
                                   dp_in_fmt, src_dp_profile, eInterlace_None, &src_roi, DP_SECURE_NONE, dp_flush_cache);
        }

        /* set DST config */
        {
            int dst_stride;
            int dst_pitch_uv;

            switch (dst_info.format) {
                case HAL_PIXEL_FORMAT_YV12:
                    plane_num = 3;
                    dp_out_fmt = DP_COLOR_YV12;
                    dst_stride =  dst_info.stride;
                    dst_pitch_uv = ALIGN((dst_stride / 2), 16);
                    dst_size[0] = dst_stride * dst_info.height;
                    dst_size[1] = dst_pitch_uv * (dst_info.height / 2);
                    dst_size[2] = dst_pitch_uv * (dst_info.height / 2);
                    break;

                case HAL_PIXEL_FORMAT_RGBA_8888:
                    plane_num = 1;
                    dp_out_fmt = DP_COLOR_RGBX8888;
                    dst_stride = dst_info.stride * 4;
                    dst_pitch_uv = 0;
                    dst_size[0] = dst_stride * dst_info.height;
                    break;

                case HAL_PIXEL_FORMAT_RGB_888:
                    plane_num = 1;
                    dp_out_fmt = DP_COLOR_RGB888;
                    dst_stride = ALIGN((dst_info.stride * 3), 64);
                    dst_pitch_uv = 0;
                    dst_size[0] = dst_stride * dst_info.height;
                    break;

                default:
                    if (src_info.ion_fd < 0) {
                        MTKGralloc::getInstance()->unlockBuffer(src_buffer);
                    }
                    MTK_LOGE("Unsupported dst color format %d\n", dst_info.format);
                    return -1;
            }

            if (dst_info.ion_fd >= 0) {
                bltStream.setDstBuffer(dst_info.ion_fd, dst_size, plane_num);
            } else {
                MTK_LOGE("dst is not a ion buffer");
                MTKGralloc::getInstance()->unlockBuffer(src_buffer);
                return -1;
            }

            int width_even;
            int height_even;

            /* Make sure the w and h are even numbers. */
            width_even = (dst_roi.w % 2) ? dst_roi.w - 1 : dst_roi.w;
            height_even = (dst_roi.h  % 2) ? dst_roi.h  - 1 : dst_roi.h ;

            dst_roi.x = 0;
            dst_roi.y = 0;
            dst_roi.w = width_even;
            dst_roi.h = height_even;

            DP_PROFILE_ENUM out_dp_profile;
            guiExtAuxGetYUVColorSpace(&dst_info, &out_dp_profile);

            bltStream.setDstConfig(dst_roi.w, dst_roi.h, dst_stride, dst_pitch_uv,
                                   dp_out_fmt, out_dp_profile, eInterlace_None, &dst_roi, DP_SECURE_NONE, false);

            if (HAL_TRANSFORM_ROT_90 == (prot & HAL_TRANSFORM_ROT_90)) {
                MTK_LOGD("Aux debug, ++90  prot = 0x%x \n", prot);
                if (HAL_TRANSFORM_FLIP_H == (prot & HAL_TRANSFORM_FLIP_H)) {
                    //MTK_LOGD("Aux debug, ++90 ,1, prot = 0x%x \n", prot );
                    bltStream.setOrientation(DpBlitStream::ROT_90 | DpBlitStream::FLIP_V);
                } else if (HAL_TRANSFORM_FLIP_V == (prot & HAL_TRANSFORM_FLIP_V)) {
                    //MTK_LOGD("Aux debug, ++90 ,2, prot = 0x%x \n", prot );
                    bltStream.setOrientation(DpBlitStream::ROT_90 | DpBlitStream::FLIP_H);
                } else {
                    //MTK_LOGD("Aux debug, ++90 ,3, prot = 0x%x \n", prot );
                    bltStream.setOrientation(DpBlitStream::ROT_270); ///(DpBlitStream::ROT_90 | DpBlitStream::FLIP_V); (DpBlitStream::ROT_90 | DpBlitStream::FLIP_H);
                }
            } else if (HAL_TRANSFORM_ROT_270 == (prot & HAL_TRANSFORM_ROT_270)) {
                MTK_LOGD("Aux debug, ++270  prot = 0x%x \n", prot);
                bltStream.setOrientation(DpBlitStream::ROT_90); ///(DpBlitStream::ROT_270 | DpBlitStream::FLIP_V); (DpBlitStream::ROT_90 | DpBlitStream::FLIP_H);
            } else if (prot == 0) {
                //MTK_LOGD("Aux debug, ++0,prot = 0x%x \n", prot );
                bltStream.setOrientation(DpBlitStream::ROT_0);///(DpBlitStream::ROT_90 | DpBlitStream::FLIP_V); (DpBlitStream::ROT_90 | DpBlitStream::FLIP_H);
            }
        }

        gettimeofday(&target_end, NULL);
        target_end.tv_usec += 1000;
        if (target_end.tv_usec > 1000000) {
            target_end.tv_sec++;
            target_end.tv_usec -= 1000000;
        }

        MTK_ATRACE_BEGIN("bltStream.invalidate()");
        if (bltStream.invalidate(&target_end) != DP_STATUS_RETURN_SUCCESS) {
            MTK_LOGE("DpBlitStream invalidate failed");
            err = -1;
        }
        MTK_ATRACE_END();

        if (!(src_info.ion_fd >= 0)) {
            MTKGralloc::getInstance()->unlockBuffer(src_buffer);
        }

        return err;
    }

    int GuiExtAuxAcquireBuffer(GuiExtAuxBufferQueueHandle bq, int *bufSlot, int *fence_fd)
    {
        status_t err = 0;

        android_native_buffer_t *pBuffer = NULL;

        err = bq->mMTKQueue.acquireBuffer(bufSlot, &pBuffer, fence_fd);

        if (err != NO_ERROR) {
            MTK_LOGE("acquireBuffer fail(%d)", err);
            return -err;
        }

        GuiExtAuxBufferItemHandle auxitem = &bq->mSlots[*bufSlot];

        if (pBuffer) {
            auxitem->mMTKBuffer = (GPUAUXBuffer *) pBuffer;
        }
        auxitem->mSrcBuffer = 0;

        return err;
    }

    int GuiExtAuxReleaseBuffer(GuiExtAuxBufferQueueHandle bq, int bufSlot, int fence_fd)
    {
        status_t err = 0;

        if (fence_fd >= 0) {
            close(fence_fd);
            fence_fd = -1;
        }

        err = bq->mMTKQueue.releaseBuffer(bufSlot);

        return err;
    }

    int GuiExtAuxDequeueBuffer(GuiExtAuxBufferQueueHandle bq, int *bufSlot, int *fence_fd)
    {
        ATRACE_CALL();

        int buf = -1;
        const int reqW = bq->width;
        const int reqH = bq->height;
        const int reqF = bq->format;

        status_t err;

        if (reqW == -1 || reqH == -1) {
            MTK_LOGE("please call setSize() beforce dequeueBuffer().");
            return -1;
        }

        android_native_buffer_t *pBuffer = NULL;

        err = bq->mMTKQueue.dequeueBuffer(reqW, reqH, reqF, &buf, &pBuffer, fence_fd);

        if (err == 0) {
            GuiExtAuxBufferItemHandle auxitem = &bq->mSlots[buf];

            auxitem->mMTKBuffer = (GPUAUXBuffer *)pBuffer;

            *bufSlot = buf;
        } else {
            MTK_LOGE("error %d", err);
        }

        return err;
    }

    int GuiExtAuxQueueBuffer(GuiExtAuxBufferQueueHandle bq, int bufSlot, int fence_fd)
    {
        ATRACE_CALL();

        if (fence_fd >= 0) {
            close(fence_fd);
            fence_fd = -1;
        }

        status_t err = bq->mMTKQueue.queueBuffer(bufSlot);

        return err;
    }

    android_native_buffer_t *GuiExtAuxRequestBuffer(GuiExtAuxBufferQueueHandle bq, int bufSlot)
    {
        return bq->mSlots[bufSlot].mMTKBuffer->getNativeBuffer();
    }

} /* end of extern "C" */

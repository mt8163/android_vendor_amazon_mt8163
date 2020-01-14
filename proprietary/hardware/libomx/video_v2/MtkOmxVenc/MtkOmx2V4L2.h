#ifndef __MTK_OMX_V4L2_H__
#define __MTK_OMX_V4L2_H__

#include "MtkOmxVenc.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/eventfd.h>
#include <poll.h>
#include <cutils/properties.h>
#include <linux/videodev2.h>
#include <linux/v4l2-controls.h>
#include <utils/Trace.h>

#include <stdlib.h>

#undef LOG_TAG
#define LOG_TAG "MtkOmx2V4L2"
#define PROFILING 1

#ifdef ATRACE_TAG
#undef ATRACE_TAG
#define ATRACE_TAG ATRACE_TAG_VIDEO
#endif//ATRACE_TAG

#define IOCTL_OR_ERROR_RETURN_VALUE(fd, type, arg, value) \
    do { \
        MTK_OMX_LOGD("%d    ioctl: " #type, __LINE__); \
        if (ioctl(fd, type, arg) != 0) { \
            int _ret = errno; \
            MTK_OMX_LOGE("    %s(): ioctl %s fail, ret: %d, %s\n", __func__, #type, _ret, strerror(_ret)); \
            return value; \
        } \
    } while (0)

#define IOCTL_OR_ERROR_LOG(fd, type, arg) \
    do { \
        MTK_OMX_LOGD("%d    ioctl: " #type, __LINE__); \
        if (ioctl(fd, type, arg) != 0) { \
            int _ret = errno; \
            MTK_OMX_LOGE("    %s(): ioctl %s fail, ret: %d, %s\n", __func__, #type, _ret, strerror(_ret)); \
        } \
    } while (0)

#define IN_FUNC() \
    MTK_OMX_LOGD("+ %s():%d\n", __func__, __LINE__)

#define OUT_FUNC() \
    MTK_OMX_LOGD("- %s():%d\n", __func__, __LINE__)

#define PROP() \
    MTK_OMX_LOGD(" --> %s : %d\n", __func__, __LINE__)

#ifdef V4L2
void v4l2_dump_qbuf(struct v4l2_buffer* qbuf);
void v412_dump_crop(struct v4l2_crop *crop);
void v4l2_dump_fmt(struct v4l2_format *format);
void v4l2_dump_select(struct v4l2_selection* sel);
void v4l2_dump_frmsizeenum(struct v4l2_frmsizeenum* fse);

void v4l2_reset_dma_out_buf(struct v4l2_buffer* qbuf, struct v4l2_plane* qbuf_planes, int idx, int dim);
void v4l2_reset_dma_in_buf(struct v4l2_buffer* qbuf, struct v4l2_plane* qbuf_planes, int idx, int dim);
void v4l2_reset_dma_in_fix_scale(struct v4l2_selection* sel, int w, int h);
void v4l2_reset_dma_out_fix_scale(struct v4l2_selection* sel, int w, int h);
void v4l2_reset_rect(struct v4l2_rect* rect, int w, int h);
int v4l2_get_nplane_by_pixelformat(__u32 pixelformat);

void video_encode_reset_in_fmt_pix_mp(struct v4l2_format *format, video_encode_param* encParam);
void video_encode_reset_out_fmt_pix_mp(struct v4l2_format *format, video_encode_param* encParam);
void v4l2_set_dma_in_memory(struct v4l2_buffer* qbuf, video_encode_param* encParam, int dmabuf_fd);
void v4l2_set_dma_out_memory(struct v4l2_buffer* qbuf, video_encode_param* encParam, int dmabuf_fd);
#endif

#endif //__MTK_OMX_V4L2_H__
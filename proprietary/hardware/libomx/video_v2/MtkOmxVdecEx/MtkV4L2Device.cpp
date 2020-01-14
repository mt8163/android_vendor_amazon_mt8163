
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <ion.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "utils/Log.h"
#include <cutils/log.h>
#include <cutils/properties.h>

#include "MtkV4L2Device.h"
#include "MtkOmxVdecEx.h"

#include "osal_utils.h" // for Vector usage
//#include <vector>
#include <algorithm>


#undef LOG_TAG
#define LOG_TAG "MtkV4L2Device"


const char kDecoderDevice[] = "/dev/video0";
const char kEncoderDevice[] = "";

typedef struct
{
    unsigned int entry_num;
    unsigned int data[100];
} DEVINFO_S;

static VAL_UINT32_T DEC_MAX_WIDTH = 4096;         ///< The maximum value of supported video width
static VAL_UINT32_T DEC_MAX_HEIGHT = 3120;         ///< The maximum value of supported video height

MtkV4L2Device_PROFILE_MAP_ENTRY H264ProfileMapTable[] =
{
    {V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE,              VDEC_DRV_H264_VIDEO_PROFILE_H264_BASELINE},
    {V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_BASELINE,  VDEC_DRV_H264_VIDEO_PROFILE_H264_CONSTRAINED_BASELINE},
    {V4L2_MPEG_VIDEO_H264_PROFILE_MAIN,                  VDEC_DRV_H264_VIDEO_PROFILE_H264_MAIN},
    {V4L2_MPEG_VIDEO_H264_PROFILE_EXTENDED,              VDEC_DRV_H264_VIDEO_PROFILE_H264_EXTENDED},
    {V4L2_MPEG_VIDEO_H264_PROFILE_HIGH,                  VDEC_DRV_H264_VIDEO_PROFILE_H264_HIGH},
    {V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_10,               VDEC_DRV_H264_VIDEO_PROFILE_H264_HIGH_10},
    {V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_422,              VDEC_DRV_H264_VIDEO_PROFILE_H264_HIGH422},
    {V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_444_PREDICTIVE,   VDEC_DRV_H264_VIDEO_PROFILE_H264_HIGH444},
    {V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_10_INTRA,         VDEC_DRV_H264_VIDEO_PROFILE_H264_HIGH_10_INTRA},
    {V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_422_INTRA,        VDEC_DRV_H264_VIDEO_PROFILE_H264_HIGH422_INTRA},
    {V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_444_INTRA,        VDEC_DRV_H264_VIDEO_PROFILE_H264_HIGH444_INTRA},
    {V4L2_MPEG_VIDEO_H264_PROFILE_CAVLC_444_INTRA,       VDEC_DRV_H264_VIDEO_PROFILE_H264_CAVLC444_INTRA},
    {V4L2_MPEG_VIDEO_H264_PROFILE_SCALABLE_BASELINE,     VDEC_DRV_H264_VIDEO_PROFILE_H264_SCALABLE_BASELINE},
    {V4L2_MPEG_VIDEO_H264_PROFILE_SCALABLE_HIGH,         VDEC_DRV_H264_VIDEO_PROFILE_H264_SCALABLE_HIGH},
    {V4L2_MPEG_VIDEO_H264_PROFILE_SCALABLE_HIGH_INTRA,   VDEC_DRV_H264_VIDEO_PROFILE_H264_SCALABLE_HIGH_INTRA},
    {V4L2_MPEG_VIDEO_H264_PROFILE_STEREO_HIGH,           VDEC_DRV_H264_VIDEO_PROFILE_UNKNOWN},
    {V4L2_MPEG_VIDEO_H264_PROFILE_MULTIVIEW_HIGH,        VDEC_DRV_H264_VIDEO_PROFILE_H264_MULTIVIEW_HIGH}
};

MtkV4L2Device_LEVEL_MAP_ENTRY H264LevelMapTable[] =
{
    {V4L2_MPEG_VIDEO_H264_LEVEL_1_0,    VDEC_DRV_VIDEO_LEVEL_1},
    {V4L2_MPEG_VIDEO_H264_LEVEL_1B,     VDEC_DRV_VIDEO_LEVEL_1b},
    {V4L2_MPEG_VIDEO_H264_LEVEL_1_1,    VDEC_DRV_VIDEO_LEVEL_1_1},
    {V4L2_MPEG_VIDEO_H264_LEVEL_1_2,    VDEC_DRV_VIDEO_LEVEL_1_2},
    {V4L2_MPEG_VIDEO_H264_LEVEL_1_3,    VDEC_DRV_VIDEO_LEVEL_1_3},
    {V4L2_MPEG_VIDEO_H264_LEVEL_2_0,    VDEC_DRV_VIDEO_LEVEL_2},
    {V4L2_MPEG_VIDEO_H264_LEVEL_2_1,    VDEC_DRV_VIDEO_LEVEL_2_1},
    {V4L2_MPEG_VIDEO_H264_LEVEL_2_2,    VDEC_DRV_VIDEO_LEVEL_2_2},
    {V4L2_MPEG_VIDEO_H264_LEVEL_3_0,    VDEC_DRV_VIDEO_LEVEL_3},
    {V4L2_MPEG_VIDEO_H264_LEVEL_3_1,    VDEC_DRV_VIDEO_LEVEL_3_1},
    {V4L2_MPEG_VIDEO_H264_LEVEL_3_2,    VDEC_DRV_VIDEO_LEVEL_3_2},
    {V4L2_MPEG_VIDEO_H264_LEVEL_4_0,    VDEC_DRV_VIDEO_LEVEL_4},
    {V4L2_MPEG_VIDEO_H264_LEVEL_4_1,    VDEC_DRV_VIDEO_LEVEL_4_1},
    {V4L2_MPEG_VIDEO_H264_LEVEL_4_2,    VDEC_DRV_VIDEO_LEVEL_4_2},
    {V4L2_MPEG_VIDEO_H264_LEVEL_5_0,    VDEC_DRV_VIDEO_LEVEL_5},
    {V4L2_MPEG_VIDEO_H264_LEVEL_5_1,    VDEC_DRV_VIDEO_LEVEL_5_1}
};

MtkV4L2Device_PROFILE_MAP_ENTRY MP4ProfileMapTable[] =
{
    {V4L2_MPEG_VIDEO_MPEG4_PROFILE_SIMPLE,              VDEC_DRV_MPEG_VIDEO_PROFILE_MPEG4_SIMPLE},
    //{OMX_VIDEO_MPEG4ProfileSimpleScalable,      VDEC_DRV_VIDEO_UNSUPPORTED},
    //{OMX_VIDEO_MPEG4ProfileCore,                VDEC_DRV_VIDEO_UNSUPPORTED},
    //{OMX_VIDEO_MPEG4ProfileMain,                VDEC_DRV_VIDEO_UNSUPPORTED},
    //{OMX_VIDEO_MPEG4ProfileNbit,                VDEC_DRV_VIDEO_UNSUPPORTED},
    //{OMX_VIDEO_MPEG4ProfileScalableTexture,     VDEC_DRV_VIDEO_UNSUPPORTED},
    //{OMX_VIDEO_MPEG4ProfileSimpleFace,          VDEC_DRV_VIDEO_UNSUPPORTED},
    //{OMX_VIDEO_MPEG4ProfileSimpleFBA,           VDEC_DRV_VIDEO_UNSUPPORTED},
    //{OMX_VIDEO_MPEG4ProfileBasicAnimated,       VDEC_DRV_VIDEO_UNSUPPORTED},
    //{OMX_VIDEO_MPEG4ProfileHybrid,              VDEC_DRV_VIDEO_UNSUPPORTED},
    //{OMX_VIDEO_MPEG4ProfileAdvancedRealTime,    VDEC_DRV_VIDEO_UNSUPPORTED},
    //{OMX_VIDEO_MPEG4ProfileCoreScalable,        VDEC_DRV_VIDEO_UNSUPPORTED},
    //{OMX_VIDEO_MPEG4ProfileAdvancedCoding,      VDEC_DRV_VIDEO_UNSUPPORTED},
    //{OMX_VIDEO_MPEG4ProfileAdvancedCore,        VDEC_DRV_VIDEO_UNSUPPORTED},
    //{OMX_VIDEO_MPEG4ProfileAdvancedScalable,    VDEC_DRV_VIDEO_UNSUPPORTED},
    {V4L2_MPEG_VIDEO_MPEG4_PROFILE_ADVANCED_SIMPLE,      VDEC_DRV_MPEG_VIDEO_PROFILE_MPEG4_ADVANCED_SIMPLE},
};

MtkV4L2Device_LEVEL_MAP_ENTRY MP4LevelMapTable[] =
{
    {V4L2_MPEG_VIDEO_MPEG4_LEVEL_0,     VDEC_DRV_VIDEO_LEVEL_0},
    {V4L2_MPEG_VIDEO_MPEG4_LEVEL_0B,    VDEC_DRV_VIDEO_LEVEL_1},
    {V4L2_MPEG_VIDEO_MPEG4_LEVEL_1,     VDEC_DRV_VIDEO_LEVEL_1},
    {V4L2_MPEG_VIDEO_MPEG4_LEVEL_2,     VDEC_DRV_VIDEO_LEVEL_2},
    {V4L2_MPEG_VIDEO_MPEG4_LEVEL_3,     VDEC_DRV_VIDEO_LEVEL_3},
    {V4L2_MPEG_VIDEO_MPEG4_LEVEL_4,     VDEC_DRV_VIDEO_LEVEL_4},
    {V4L2_MPEG_VIDEO_MPEG4_LEVEL_5,     VDEC_DRV_VIDEO_LEVEL_5},
};

MtkV4L2Device_PROFILE_MAP_ENTRY H265ProfileMapTable[] =
{
    {V4L2_MPEG_VIDEO_H265_PROFILE_MAIN,              VDEC_DRV_H265_VIDEO_PROFILE_H265_MAIN},
    {V4L2_MPEG_VIDEO_H265_PROFILE_MAIN10,            VDEC_DRV_H265_VIDEO_PROFILE_H265_MAIN_10},
    {V4L2_MPEG_VIDEO_H265_PROFILE_MAIN_STILL_PIC,    VDEC_DRV_H265_VIDEO_PROFILE_H265_STILL_IMAGE},
};

MtkV4L2Device_LEVEL_MAP_ENTRY H265LevelMapTable[] =
{
    {V4L2_MPEG_VIDEO_H265_LEVEL_MAIN_TIER_LEVEL_1,   VDEC_DRV_VIDEO_LEVEL_1},
    {V4L2_MPEG_VIDEO_H265_LEVEL_HIGH_TIER_LEVEL_1,  VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_1},
    {V4L2_MPEG_VIDEO_H265_LEVEL_MAIN_TIER_LEVEL_2,  VDEC_DRV_VIDEO_LEVEL_2},
    {V4L2_MPEG_VIDEO_H265_LEVEL_HIGH_TIER_LEVEL_2,  VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_2},
    {V4L2_MPEG_VIDEO_H265_LEVEL_MAIN_TIER_LEVEL_2_1,  VDEC_DRV_VIDEO_LEVEL_2_1},
    {V4L2_MPEG_VIDEO_H265_LEVEL_HIGH_TIER_LEVEL_2_1,   VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_2_1},
    {V4L2_MPEG_VIDEO_H265_LEVEL_MAIN_TIER_LEVEL_3,  VDEC_DRV_VIDEO_LEVEL_3},
    {V4L2_MPEG_VIDEO_H265_LEVEL_HIGH_TIER_LEVEL_3,  VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_3},
    {V4L2_MPEG_VIDEO_H265_LEVEL_MAIN_TIER_LEVEL_3_1,   VDEC_DRV_VIDEO_LEVEL_3_1},
    {V4L2_MPEG_VIDEO_H265_LEVEL_HIGH_TIER_LEVEL_3_1,  VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_3_1},
    {V4L2_MPEG_VIDEO_H265_LEVEL_MAIN_TIER_LEVEL_4,   VDEC_DRV_VIDEO_LEVEL_4},
    {V4L2_MPEG_VIDEO_H265_LEVEL_HIGH_TIER_LEVEL_4,  VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_4},
    {V4L2_MPEG_VIDEO_H265_LEVEL_MAIN_TIER_LEVEL_4_1,  VDEC_DRV_VIDEO_LEVEL_4_1},
    {V4L2_MPEG_VIDEO_H265_LEVEL_HIGH_TIER_LEVEL_4_1,   VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_4_1},
    {V4L2_MPEG_VIDEO_H265_LEVEL_MAIN_TIER_LEVEL_5,  VDEC_DRV_VIDEO_LEVEL_5},
    {V4L2_MPEG_VIDEO_H265_LEVEL_HIGH_TIER_LEVEL_5,  VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_5},
    {V4L2_MPEG_VIDEO_H265_LEVEL_MAIN_TIER_LEVEL_5_1,   VDEC_DRV_VIDEO_LEVEL_5_1},
    {V4L2_MPEG_VIDEO_H265_LEVEL_HIGH_TIER_LEVEL_5_1,  VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_5_1},
    {V4L2_MPEG_VIDEO_H265_LEVEL_MAIN_TIER_LEVEL_5_2,   VDEC_DRV_VIDEO_LEVEL_5_2},
    {V4L2_MPEG_VIDEO_H265_LEVEL_HIGH_TIER_LEVEL_5_2,  VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_5_2},
    {V4L2_MPEG_VIDEO_H265_LEVEL_MAIN_TIER_LEVEL_6,   VDEC_DRV_VIDEO_LEVEL_6},
    {V4L2_MPEG_VIDEO_H265_LEVEL_HIGH_TIER_LEVEL_6,  VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_6},
    {V4L2_MPEG_VIDEO_H265_LEVEL_MAIN_TIER_LEVEL_6_1,   VDEC_DRV_VIDEO_LEVEL_6_1},
    {V4L2_MPEG_VIDEO_H265_LEVEL_HIGH_TIER_LEVEL_6_1,  VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_6_1},
    {V4L2_MPEG_VIDEO_H265_LEVEL_MAIN_TIER_LEVEL_6_2,   VDEC_DRV_VIDEO_LEVEL_6_2},
    {V4L2_MPEG_VIDEO_H265_LEVEL_HIGH_TIER_LEVEL_6_2,  VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_6_2}
};

#define ENTER_FUNC MTK_V4L2DEVICE_LOGD("-> %s(), (%d) \n", __FUNCTION__, __LINE__);
#define EXIT_FUNC MTK_V4L2DEVICE_LOGD("%s(), (%d) -> \n", __FUNCTION__, __LINE__);

bool bMtkV4L2DeviceLogEnable = false;
#define MTK_V4L2DEVICE_LOGD(fmt, arg...)       \
    if (bMtkV4L2DeviceLogEnable) \
    {  \
        ALOGD("[0x%08x] " fmt, this, ##arg) ;  \
    }
#define MTK_V4L2DEVICE_LOGE(fmt, arg...)       ALOGE("[0x%08x] " fmt, this, ##arg)

#define CHECK_NULL_RETURN_VALUE(ptr, value) \
    do {                                    \
        if (NULL == ptr) {                  \
            return value;                   \
        }                                   \
    } while(0);                             \

#define IOCTL_OR_ERROR_RETURN_VALUE(type, arg, value)                   \
    do {                                                                \
        if (ioctl(mDeviceFd, type, arg) != 0) {                              \
            MTK_V4L2DEVICE_LOGE("[%s] ioctl(%u) failed. error = %s\n", __FUNCTION__, type, strerror(errno)); \
            EXIT_FUNC \
            return value;                                               \
        }                                                               \
    } while (0);

#define IOCTL_OR_ERROR_RETURN(type, arg) \
    IOCTL_OR_ERROR_RETURN_VALUE(type, arg, ((void)0));

#define IOCTL_OR_ERROR_RETURN_FALSE(type, arg) \
    IOCTL_OR_ERROR_RETURN_VALUE(type, arg, 0);



MtkV4L2Device::MtkV4L2Device()
{
    ENTER_FUNC

    MTK_V4L2DEVICE_LOGE("+ MtkV4L2Device()");

    INIT_MUTEX(mStateMutex);
    mClient 				= NULL;
	mDeviceFd 				= -1;
	mDevicePollInterruptFd	= -1;

	mFrameBufferSize		= -1;
    ChangeState(kUninitialized);

    MTK_V4L2DEVICE_LOGE("- MtkV4L2Device()");
    EXIT_FUNC
}

MtkV4L2Device::~MtkV4L2Device()
{
    ENTER_FUNC
    MTK_V4L2DEVICE_LOGE("+ ~MtkV4L2Device()");

    dumpDebugInfo();

    DESTROY_MUTEX(mStateMutex);

    MTK_V4L2DEVICE_LOGE("- ~MtkV4L2Device()");
    EXIT_FUNC
}

void MtkV4L2Device::ChangeState(MtkV4L2Device_State newState)
{
    LOCK(mStateMutex);

    char OriStateString[20];
    char NewStateString[20];
    GetStateString(mState, OriStateString);
    GetStateString(newState, NewStateString);

    mState = newState;
    MTK_V4L2DEVICE_LOGE("State change from %s --> %s", OriStateString, NewStateString);

    UNLOCK(mStateMutex);
}

int MtkV4L2Device::CheckState(MtkV4L2Device_State toBeCheckState)
{
    int ret;
    LOCK(mStateMutex);

    if (mState == toBeCheckState)
    {
        ret = 1;
    }
    else
    {
        ret = 0;
    }

    UNLOCK(mStateMutex);

    return ret;
}

void MtkV4L2Device::GetStateString(MtkV4L2Device_State state, char *stateString)
{
    switch (state)
    {
        case kUninitialized:
            sprintf(stateString, "kUninitialized");
            break;
        case kInitialized:
            sprintf(stateString, "kInitialized");
            break;
        case kDecoding:
            sprintf(stateString, "kDecoding");
            break;
        case kResetting:
            sprintf(stateString, "kResetting");
            break;
        case kAfterReset:
            sprintf(stateString, "kAfterReset");
            break;
        case kChangingResolution:
            sprintf(stateString, "kChangingResolution");
            break;
        case kFlushing:
            sprintf(stateString, "kFlushing");
            break;
        case kError:
            sprintf(stateString, "kError");
            break;

        default:
            sprintf(stateString, "unknown state");
            break;

    }
}

bool MtkV4L2Device::needToDumpDebugInfo()
{
    if (mDeadDumpCount > 10 ||
        mLastBitstreamQStreamOn != mBitstreamQStreamOn ||
        mLastFramebufferQStreamOn != mFramebufferQStreamOn ||
        mLastFBQR_size != mFBQR.getSize() ||
        mLastBSQR_size != mBSQR.getSize() ||
        mLastBitstreamFinish != mBitstreamFinish ||
        mLastFrameBufferFinish != mFrameBufferFinish)
    {
        return true;
    }

    return false;
}

void MtkV4L2Device::updateDebugInfo()
{
    mLastBitstreamQStreamOn         = mBitstreamQStreamOn;
    mLastFramebufferQStreamOn       = mFramebufferQStreamOn;
    mLastFBQR_size                  = mFBQR.getSize();
    mLastBSQR_size                  = mBSQR.getSize();
    mLastBitstreamFinish            = mBitstreamFinish;
    mLastFrameBufferFinish          = mFrameBufferFinish;

    mDeadDumpCount                  = 0;
}

void MtkV4L2Device::dumpDebugInfo()
{
    ENTER_FUNC

    char stateString[20];

    if (true == needToDumpDebugInfo())
    {
        GetStateString(mState, stateString);
        MTK_V4L2DEVICE_LOGE("BSQStreamOn(%d), FBQStreamOn(%d), queuedFBCount:%d, queuedBSCount:%d, BSFinish(%d), FBFinish(%d), mState(%s), mDeviceFd(%d), mDeadDumpCount(%d)",
    						mBitstreamQStreamOn,
                            mFramebufferQStreamOn,
                            mFBQR.getSize(),
                            mBSQR.getSize(),
                            mBitstreamFinish,
                            mFrameBufferFinish,
                            stateString,
                            mDeviceFd,
                            mDeadDumpCount);

        updateDebugInfo();
    }
    else
    {
        ++mDeadDumpCount;
    }

    EXIT_FUNC
}

int MtkV4L2Device::IsStreamOn(V4L2QueueType QueueType)
{
    ENTER_FUNC

    if (kBitstreamQueue == QueueType)
    {
        return (mBitstreamQStreamOn == 1)? 1:0;
    }
    else
    {
        return (mFramebufferQStreamOn == 1)? 1:0;
    }

    EXIT_FUNC

}

int MtkV4L2Device::StreamOnBitstream(void)
{
    ENTER_FUNC

	if (1 == mBitstreamQStreamOn)
	{
		MTK_V4L2DEVICE_LOGE("BSQ was already stream ON");
		dumpDebugInfo();

		return 1;
	}

    __u32 type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_STREAMON, &type);

    mBitstreamQStreamOn = 1;
    mBitstreamFinish    = 0;
	mMaxBitstreamSize	= 0;
	MTK_V4L2DEVICE_LOGE("BSQ stream ON");

    EXIT_FUNC

    return 1;
}

int MtkV4L2Device::StreamOnFrameBuffer(void)
{
    ENTER_FUNC

	if (1 == mFramebufferQStreamOn)
	{
		MTK_V4L2DEVICE_LOGE("FBQ was already stream ON");
		dumpDebugInfo();

		return 1;
	}

    __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_STREAMON, &type);

    mFramebufferQStreamOn   = 1;
    mFrameBufferFinish  	= 0;
	MTK_V4L2DEVICE_LOGE("FBQ stream ON");

    EXIT_FUNC

    return 1;
}

int MtkV4L2Device::StreamOffBitstream(void)
{
    ENTER_FUNC

	if (0 == mBitstreamQStreamOn)
	{
		MTK_V4L2DEVICE_LOGE("BSQ was already stream OFF");
		dumpDebugInfo();

		return 1;
	}

    __u32 type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	MTK_V4L2DEVICE_LOGE("BSQ stream OFF +");
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_STREAMOFF, &type);
    MTK_V4L2DEVICE_LOGE("BSQ stream OFF -");

    mBitstreamQStreamOn = 0;
    dumpDebugInfo();

    EXIT_FUNC

    return 1;
}

int MtkV4L2Device::StreamOffFrameBuffer(void)
{
    ENTER_FUNC

	if (0 == mFramebufferQStreamOn)
	{
		MTK_V4L2DEVICE_LOGE("FrameBuffer Queue was already stream OFF");
		dumpDebugInfo();

		return 1;
	}

    __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	MTK_V4L2DEVICE_LOGE("FBQ stream OFF +");
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_STREAMOFF, &type);
    MTK_V4L2DEVICE_LOGE("FBQ stream OFF -");

    mFramebufferQStreamOn = 0;
    dumpDebugInfo();

    EXIT_FUNC

    return 1;
}


int MtkV4L2Device::flushBitstreamQ()
{
    ENTER_FUNC

    if (mBSQR.getSize() == 0)
    {
        MTK_V4L2DEVICE_LOGE("No Need to Flush BSQ(%d)", mBSQR.getSize());
		dumpDebugInfo();
		StreamOffBitstream();

        return 0;
    }
    else
    {
        MTK_V4L2DEVICE_LOGE("Need to Flush BSQ(%d)", mBSQR.getSize());
        dumpDebugInfo();

		ChangeState(kFlushing);
		StreamOffBitstream();
    }

    EXIT_FUNC

    return 1;
}



int MtkV4L2Device::flushFrameBufferQ()
{
    ENTER_FUNC

    if (mFBQR.getSize() == 0)
    {
        MTK_V4L2DEVICE_LOGE("No Need to Flush FBQ(%d)", mFBQR.getSize());
		dumpDebugInfo();
		StreamOffFrameBuffer();

        return 0;
    }
    else
    {
        MTK_V4L2DEVICE_LOGE("Need to Flush FBQ(%d)", mFBQR.getSize());
        dumpDebugInfo();

		ChangeState(kFlushing);
		StreamOffFrameBuffer();
    }

    EXIT_FUNC

    return 1;
}



int MtkV4L2Device::queuedFrameBufferCount()
{
    return mFBQR.getSize();
}

int MtkV4L2Device::queuedBitstreamCount()
{
    return mBSQR.getSize();
}

int MtkV4L2Device::queueBitstream(int bitstreamIndex, int bitstreamDMAFd, int byteUsed, int maxSize, signed long long timestamp, int flags)
{
    ENTER_FUNC

    int ret = 1;

    struct v4l2_buffer qbuf;
    struct v4l2_plane qbuf_planes[2];

    if (CheckState(kFlushing) || CheckState(kChangingResolution) || 1 == mBitstreamFinish)
    {
    	char stateString[20];
    	GetStateString(mState, stateString);

        // We don't accept to queueBitstream.
        MTK_V4L2DEVICE_LOGE("Don't accept new bitstream state(%s), BitstreamFinish(%d), idx(%d), byteUsed(%d)", stateString,
        																										mBitstreamFinish,
        																										bitstreamIndex,
        																										byteUsed);
        ret = 0;
    }
    else
    {
        // When width/height is ready, we'll enter Decoding state
        if (CheckState(kInitialized) &&
            (0 < mWidth && 0 < mHeight))
        {
            ChangeState(kDecoding);
        }


        if (bitstreamDMAFd >= 0)
        {
            memset(&qbuf, 0, sizeof(qbuf));
            memset(qbuf_planes, 0, sizeof(qbuf_planes));

            qbuf.index                      = bitstreamIndex;
            qbuf.type                       = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
            qbuf.memory                     = V4L2_MEMORY_DMABUF;
            qbuf.m.planes                   = qbuf_planes;
            qbuf.m.planes[0].bytesused      = byteUsed;
            qbuf.m.planes[0].length         = maxSize;
            qbuf.m.planes[0].m.fd           = bitstreamDMAFd;//bitstream_mem_dmabuf_fd[inputIndex];// v4l2 todo
            qbuf.length                     = 1;
            qbuf.timestamp.tv_sec           = timestamp / 1000000;
            qbuf.timestamp.tv_usec          = timestamp % 1000000;
			qbuf.flags						= (flags & OMX_BUFFERFLAG_EOS)? 0x00100000/*V4L2_BUF_FLAG_LAST*/:0; //EOS only so far. It can be add new flags later.

            if(flags & OMX_BUFFERFLAG_EOS)
            {
                MTK_V4L2DEVICE_LOGE("queueBitstream(), EOS, flag(0x%08x)", qbuf.flags);
            }

            if (ioctl(mDeviceFd, VIDIOC_QBUF, &qbuf) != 0)
            {
                MTK_V4L2DEVICE_LOGE("queueBitstream(), idx(%d), byteUsed(%d), ioctl(%u) failed. error = %s\n", bitstreamIndex, byteUsed, VIDIOC_QBUF, strerror(errno));
            }
            else
            {

                MTK_V4L2DEVICE_LOGE("queueBitstream(), idx(%d), fd(0x%08x), byteUsed(%d), maxSize(%d), timestamp(%lld), flag(0x%08x)", bitstreamIndex, bitstreamDMAFd, byteUsed, maxSize, ((long long)qbuf.timestamp.tv_sec * 1000000) + ((long long)qbuf.timestamp.tv_usec), qbuf.flags);

				mBSQR.putElement(qbuf);
				dumpDebugInfo();

#if 0
                if (0 == byteUsed)
                {
                    ChangeState(kFlushing);
                }
#endif

                if (mMaxBitstreamSize == 0)
                {
                    mMaxBitstreamSize = maxSize;
                    MTK_V4L2DEVICE_LOGE("mMaxBitstreamSize(%d)", mMaxBitstreamSize);
                }
            }
        }
    }

    EXIT_FUNC
    return ret;
}

int MtkV4L2Device::queueFrameBuffer(int frameBufferIndex, int frameBufferDMAFd, int byteUsed)
{
    ENTER_FUNC

    int ret = 1;

    struct v4l2_buffer qbuf;
    struct v4l2_plane qbuf_planes[1];

    if (CheckState(kFlushing) || CheckState(kChangingResolution) || 1 == mFrameBufferFinish || frameBufferDMAFd < 0)
    {
        char stateString[20];
    	GetStateString(mState, stateString);

        // If Capture Queue is finish, do NOT allow to queue buffer any more.
        MTK_V4L2DEVICE_LOGE("Don't accept new FrameBuffer. state(%s), FrameBufferFinish(%d), idx(%d), FrameBufferDMAFd(%d)\n", stateString,
																															   mFrameBufferFinish,
																															   frameBufferIndex,
																															   frameBufferDMAFd);
        ret = 0;
    }
    else
    {
        memset(&qbuf, 0, sizeof(qbuf));
        memset(qbuf_planes, 0, sizeof(qbuf_planes));

        qbuf.index                      = frameBufferIndex;
        qbuf.type                       = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        qbuf.memory                     = V4L2_MEMORY_DMABUF;
        qbuf.m.planes                   = qbuf_planes;
        qbuf.m.planes[0].bytesused      = byteUsed*3/2; //176*160;
        qbuf.m.planes[0].length         = byteUsed*3/2; //176*160;//0;//y_size;// v4l2 todo
        qbuf.m.planes[0].m.fd           = frameBufferDMAFd;//output_mem_dmabuf_fd[outputIndex];// v4l2 todo
        /*
        qbuf.m.planes[1].bytesused      = byteUsed / 2; //176*160 / 2;
        qbuf.m.planes[1].length         = byteUsed / 2; //176*160/2;//cbcr_size;// v4l2 todo
        qbuf.m.planes[1].data_offset    = byteUsed; //176*160;
        qbuf.m.planes[1].m.fd           = frameBufferDMAFd;//output_mem_dmabuf_fd[outputIndex];// v4l2 todo
        */
        qbuf.length                     = 1;


        //IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QBUF, &qbuf);
        if (ioctl(mDeviceFd, VIDIOC_QBUF, &qbuf) != 0)
        {
            MTK_V4L2DEVICE_LOGE("queueFrameBuffer(), idx(%d), ioctl(%u), byteUsed(%d) failed. error = %s\n", frameBufferIndex, VIDIOC_QBUF, byteUsed, strerror(errno));
            ret = 0;
        }
        else
        {
            MTK_V4L2DEVICE_LOGE("queueFrameBuffer(), idx(%d), fd(0x%08x), byteUsed(%d)", frameBufferIndex, frameBufferDMAFd, byteUsed);
			mFBQR.putElement(qbuf);
            ret = 1;
        }
    }

    EXIT_FUNC

    return ret;
}

int MtkV4L2Device::deviceIoctl(int request, void *arg)
{
    return ioctl(mDeviceFd, request, arg);
}

int MtkV4L2Device::devicePoll(int *isTherePendingEvent, int timeout)
{
    ENTER_FUNC

    struct pollfd pollfds[2];
    nfds_t nfds;
    int pollfd = -1;

    pollfds[0].fd = mDevicePollInterruptFd;
    pollfds[0].events = POLLIN | POLLERR;
    nfds = 1;

    if (1)
    {
        pollfds[nfds].fd = mDeviceFd;
        pollfds[nfds].events = POLLIN | POLLOUT | POLLERR | POLLPRI;
        pollfd = nfds;
        nfds++;
    }

#if 0
    if (poll(pollfds, nfds, -1) == -1)
    {
        MTK_V4L2DEVICE_LOGE("poll() failed");
        return false;
    }
#else
    int ret = poll(pollfds, nfds, timeout);
    if (ret <= 0)
    {
        if (ret == 0)
        {
            MTK_V4L2DEVICE_LOGE("poll() %d ms time out", timeout);
            //dumpDebugInfo();
        }
        else
        {
            MTK_V4L2DEVICE_LOGE("poll() failed. ret(%d)", ret);
            dumpDebugInfo();
        }
        return 0;
    }
    else
    {
        //MTK_V4L2DEVICE_LOGE("poll() test. ret(%d)", ret);
    }


#endif

    if (NULL != isTherePendingEvent)
    {
        *isTherePendingEvent = (pollfd != -1 && pollfds[pollfd].revents & POLLPRI);
    }

    EXIT_FUNC

    return 1;
}

int MtkV4L2Device::dequeueBitstream(int *bitstreamIndex, int *isLastBitstream)
{
    ENTER_FUNC

    struct v4l2_buffer dqbuf;
    struct v4l2_plane planes[2];
    unsigned int address;
    int iRet;
	bool foundQueuedBS;

    if (NULL != isLastBitstream)
    {
        *isLastBitstream = 0;
    }

    // input buffer
    memset(&dqbuf, 0, sizeof(dqbuf));
    memset(planes, 0, sizeof(planes));
    dqbuf.index  = -1;
    dqbuf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    dqbuf.memory = V4L2_MEMORY_DMABUF;
    dqbuf.m.planes = planes;
    dqbuf.length = 1;

    iRet = deviceIoctl(VIDIOC_DQBUF, &dqbuf);

    if (iRet != 0)
    {

        *bitstreamIndex = -1;

        if (errno == EAGAIN)
        {
            MTK_V4L2DEVICE_LOGD(" No Bitstream buffer was dequeued. Try again\n");
        }
        else
        {
        	if (errno != EINVAL) // For reduce logs. This condition check can be removed.
        	{
            	MTK_V4L2DEVICE_LOGE("[%s] ioctl failed. error = %s\n", __FUNCTION__, strerror(errno));
				dumpDebugInfo();
        	}
        }

        //
        // We're in flushing. Return all queued buffers.
        //
        if (0 == mBitstreamQStreamOn && CheckState(kFlushing))
        {
            if (mBSQR.getSize() > 0)
            {
            	MTK_V4L2DEVICE_LOGE("In force return mode. QueuedBS(%d)", mBSQR.getSize());
                dqbuf = mBSQR.getElement(mBSQR.getSize()-1);
            }
            else
            {
                //v4l2 todo : do something to notify end ?
                goto NORMAL_EXIT;
            }
        }
    }


    if (dqbuf.index != -1)
    {
        MTK_V4L2DEVICE_LOGE("       DQ Bitstream buf idx=%d ByteUsed=%d, maxsize=%d\n", dqbuf.index, dqbuf.m.planes[0].bytesused, mMaxBitstreamSize);
        *bitstreamIndex = dqbuf.index;


		foundQueuedBS = mBSQR.eraseElement(dqbuf.index);

		if (false == foundQueuedBS)
		{
			MTK_V4L2DEVICE_LOGE("DQ BS NOT exist ? idx(%d)", dqbuf.index);
		}

        //bool bEOS = (mMaxBitstreamSize == dqbuf.m.planes[0].bytesused) ? true : false;
        bool bEOS = (0 == dqbuf.m.planes[0].bytesused) ? true : false;
        bool bFlushDone = (CheckState(kFlushing) && mBitstreamQStreamOn == 0 && mBSQR.getSize() == 0) ? true : false;
        //MTK_V4L2DEVICE_LOGE("bEOS(%d), bFlushDone(%d)", bEOS, bFlushDone);
        dumpDebugInfo();

        if (NULL != isLastBitstream &&
            (bEOS || bFlushDone))
        {
            *isLastBitstream    = 1;
            mBitstreamFinish    = 1;

            if (mMaxBitstreamSize == dqbuf.m.planes[0].bytesused)
            {
                MTK_V4L2DEVICE_LOGE("Bitstream Get Last Frame of max size!!\n");
            }

            if (CheckState(kFlushing) && mBitstreamQStreamOn == 0 && mBSQR.getSize() == 0)
            {
                MTK_V4L2DEVICE_LOGE("Bitstream Get Last Frame of Flush!!\n");
            }

            dumpDebugInfo();
        }

        checkFlushDone();
    }

NORMAL_EXIT:

    EXIT_FUNC

    return 1;
}

int MtkV4L2Device::dequeueFrameBuffer(int *frameBufferIndex, long long *timestamp, int *isLastFrame)
{
    ENTER_FUNC

    struct v4l2_buffer dqbuf;
    struct v4l2_plane planes[2];
    unsigned int address;
    int iRet;
	bool foundQueuedBuffer;

    if (NULL != isLastFrame)
    {
        *isLastFrame = 0;
    }

    memset(&dqbuf, 0, sizeof(dqbuf));
    memset(planes, 0, sizeof(planes));
    dqbuf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    dqbuf.memory = V4L2_MEMORY_DMABUF;
    dqbuf.m.planes = planes;
    dqbuf.length = 2;

    iRet = deviceIoctl(VIDIOC_DQBUF, &dqbuf);
    if (iRet != 0)
    {
        if (errno == EAGAIN)
        {
            MTK_V4L2DEVICE_LOGE(" No output buffer was dequeued. Try again...\n");

#if 1
            //MTK_V4L2DEVICE_LOGE("%s @ %d. mFrameBufferFinish(%d), mFramebufferQStreamOn(%d), mBitstreamFinish(%d), ", __FUNCTION__, __LINE__, mFrameBufferFinish, mFramebufferQStreamOn, mBitstreamFinish);
            dumpDebugInfo();
            if ((1 == mFrameBufferFinish && 0 == mFramebufferQStreamOn) ||
                (0 == mFramebufferQStreamOn && CheckState(kFlushing)))
            {
                // EOS or flush has been taken place
                // We need to force to take back all queued buffers and return to OMX component

                if (mFBQR.getSize() > 0)
                {
                    dqbuf = mFBQR.getElement(mFBQR.getSize() - 1);
                    MTK_V4L2DEVICE_LOGE("In force return mode. QueuedFB(%d)", mFBQR.getSize());
                }
                else
                {
                    //v4l2 todo : do something to notify end ?
                    goto NORMAL_EXIT;
                }

            }
            else
            {
                // Something did go wrong
                goto NORMAL_EXIT;
            }

#endif

            //goto NORMAL_EXIT;
        }
        else
        {
        	if (errno != EINVAL)
        	{
            	MTK_V4L2DEVICE_LOGE("[%s] ioctl failed. error = %s\n", __FUNCTION__, strerror(errno));
        	}

			dumpDebugInfo();
            *frameBufferIndex = -1;
#if 1
            if ((1 == mFrameBufferFinish && 0 == mFramebufferQStreamOn) ||
                (1 == mBitstreamFinish && CheckState(kFlushing)) ||
                (0 == mFramebufferQStreamOn && CheckState(kFlushing)))
            {
                // EOS or flush happened...
                // We need to force to take back all queued buffers and return to OMX component

                if (mFBQR.getSize() > 0)
                {
                    dqbuf = mFBQR.getElement(mFBQR.getSize() - 1);
                    MTK_V4L2DEVICE_LOGE("In force return mode. QueuedFB(%d)", mFBQR.getSize());
                }
                else
                {
                    //v4l2 todo : do something to notify end ?
                    goto ABNORMAL_EXIT;
                }

            }
            else
            {
                // Something did go wrong
                goto ABNORMAL_EXIT;
            }
#endif
        }
    }

    MTK_V4L2DEVICE_LOGE(" DQ FrameBuffer buf idx=%d ByteUsed=(Y:%d)(C:%d), ts(%lld)\n",
                        dqbuf.index, dqbuf.m.planes[0].bytesused, dqbuf.m.planes[1].bytesused, ((long long)dqbuf.timestamp.tv_sec * 1000000) + ((long long)dqbuf.timestamp.tv_usec));

	foundQueuedBuffer = mFBQR.eraseElement(dqbuf.index);

	if (false == foundQueuedBuffer)
	{
		MTK_V4L2DEVICE_LOGE("DQ FB NOT exist ? idx(%d)", dqbuf.index);
	}

    *frameBufferIndex   = dqbuf.index;
    *timestamp          = ((long long)dqbuf.timestamp.tv_sec * 1000000) + ((long long)dqbuf.timestamp.tv_usec);

    // flush done
    if (mFBQR.getSize() == 0 && CheckState(kFlushing) && mFramebufferQStreamOn == 0)
    {
        *isLastFrame        = 1;
        mFrameBufferFinish  = 1;

        MTK_V4L2DEVICE_LOGE("FrameBuffer Get Last Frame of Flush !!\n");

        checkFlushDone();

        goto NORMAL_EXIT;
    }

    // eos frame
    if (dqbuf.flags & 0x00100000)
    {
        *isLastFrame        = 2;
        mFrameBufferFinish  = 1;

        MTK_V4L2DEVICE_LOGE("FrameBuffer Get Last Frame of EOS-flag !!\n");

        goto NORMAL_EXIT;
    }

NORMAL_EXIT:
    EXIT_FUNC
    return 1;

ABNORMAL_EXIT:
    EXIT_FUNC
    return 0;

}

void MtkV4L2Device::checkFlushDone()
{
    if (CheckState(kFlushing))
    {
        bool isFrameBufferQueueFlushing 	= (mFramebufferQStreamOn == 0) ? true : false;
        bool isBitstreamQueueFlushing 		= (mBitstreamQStreamOn == 0) ? true : false;
        bool frameBufferQueueFlushDone 		= (mFBQR.getSize() == 0) ? true : false;
        bool bitstreamQueueFlushDone  		= (mBSQR.getSize() == 0) ? true : false;

		bool allFlushDone 				= ((isFrameBufferQueueFlushing && frameBufferQueueFlushDone) && (isBitstreamQueueFlushing && bitstreamQueueFlushDone))? true:false;
		bool onlyFramebufferQFlushDone 	= (isFrameBufferQueueFlushing && frameBufferQueueFlushDone && !isBitstreamQueueFlushing)? true:false;
		bool onlyBitstreamQFlushDone	= (!isFrameBufferQueueFlushing && isBitstreamQueueFlushing && bitstreamQueueFlushDone)? true:false;

        if (allFlushDone || onlyFramebufferQFlushDone || onlyBitstreamQFlushDone)
        {
            ChangeState(kInitialized);
        }
        else
        {
            dumpDebugInfo();
            MTK_V4L2DEVICE_LOGE("BSQFlushing(%d), FBQFlushing(%d), BSQFlushDone(%d), FBQFlushDone(%d)",
                isBitstreamQueueFlushing, isFrameBufferQueueFlushing, bitstreamQueueFlushDone, frameBufferQueueFlushDone);
        }
    }
}

int MtkV4L2Device::subscribeEvent(void)
{
    ENTER_FUNC

    struct v4l2_event_subscription sub;
    memset(&sub, 0, sizeof(sub));
    sub.type = V4L2_EVENT_SOURCE_CHANGE;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_SUBSCRIBE_EVENT, &sub);

    EXIT_FUNC

    return 1;
}

int MtkV4L2Device::dequeueEvent(void)
{
    ENTER_FUNC

    struct v4l2_event ev;
    memset(&ev, 0, sizeof(ev));

    while (deviceIoctl(VIDIOC_DQEVENT, &ev) == 0)
    {
        if (ev.type == V4L2_EVENT_SOURCE_CHANGE)
        {
            uint32_t changes = ev.u.src_change.changes;
            if (changes & V4L2_EVENT_SRC_CH_RESOLUTION)
            {
                MTK_V4L2DEVICE_LOGE("dequeueEvent(): got resolution change event.");
                return 1;
            }
        }
        else
        {
            MTK_V4L2DEVICE_LOGE("dequeueEvent(): got an event (%u) we haven't subscribed to.", ev.type);
        }
    }

    EXIT_FUNC

    return 0;
}

int MtkV4L2Device::getCapFmt(unsigned int *w, unsigned int *h)
{

    ENTER_FUNC

    // Get format ready before Q cap buffers.
    struct v4l2_format format;

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    IOCTL_OR_ERROR_RETURN_VALUE(VIDIOC_G_FMT, &format, 1); // return 1 will inform caller to try again

    MTK_V4L2DEVICE_LOGE("Cap: #planes: %d, color format: %d, W %d H %d, plane[0].size %d plane[1].size %d\n",
                        format.fmt.pix_mp.num_planes, format.fmt.pix_mp.pixelformat, format.fmt.pix_mp.width, format.fmt.pix_mp.height,
                        format.fmt.pix_mp.plane_fmt[0].sizeimage, format.fmt.pix_mp.plane_fmt[1].sizeimage);

    *w = mWidth  = format.fmt.pix_mp.width;
    *h = mHeight = format.fmt.pix_mp.height;

	mFrameBufferSize = (mWidth * mHeight * 3 >> 1);

    // When width/height is ready, we'll enter Decoding state
    if (CheckState(kInitialized) &&
        (0 < mWidth && 0 < mHeight))
    {
        ChangeState(kDecoding);
    }

    EXIT_FUNC

    return 0; // return 0 indicate we've got format
}



int MtkV4L2Device::initialize(V4L2DeviceType type, void *Client)
{
    ENTER_FUNC

    const char *device_path = NULL;

    switch (type)
    {
        case kDecoder:
            device_path = kDecoderDevice;
            mDeviceType = type;
            break;
        case kEncoder:
            device_path = kEncoderDevice;
            mDeviceType = type;
            break;
        default:
            MTK_V4L2DEVICE_LOGD("Initialize(): Unknown device type: %d", type);
            goto FAIL;
    }

    MTK_V4L2DEVICE_LOGD("Initialize(): opening device: %s", device_path);

    mDeviceFd = open(device_path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (mDeviceFd == -1)
    {
        MTK_V4L2DEVICE_LOGE("Initialize(): open device fail: errno %d, %s \n", errno, strerror(errno));
        goto FAIL;
    }

    mDevicePollInterruptFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (mDevicePollInterruptFd == -1)
    {
        MTK_V4L2DEVICE_LOGE("Initialize(): open device_poll_interrupt_fd_ fail:\n");
        goto FAIL;
    }

    mBitstreamQStreamOn      = 0;
    mFramebufferQStreamOn    = 0;
    mBitstreamFinish         = 0;
    mFrameBufferFinish       = 0;

    mLastBitstreamQStreamOn         = 0;
    mLastFramebufferQStreamOn       = 0;
    mLastFBQR_size                  = 0;
    mLastBSQR_size                  = 0;
    mLastBitstreamFinish            = 0;
    mLastFrameBufferFinish          = 0;
    mDeadDumpCount                  = 0;


	mMaxBitstreamSize		 = 0;

    mWidth                   = 0;
    mHeight                  = 0;

    mClient = (MtkOmxVdec *)Client;

    MTK_V4L2DEVICE_LOGE("Initialize(): open device success. Poll timeoout: %d (ms)\n", POLL_TIMEOUT);

    ChangeState(kInitialized);

    EXIT_FUNC

    return 1;

FAIL:

    EXIT_FUNC

    return 0;
}

void MtkV4L2Device::deinitialize()
{
    ENTER_FUNC

    if (mDeviceFd  != 0)
    {
        close(mDeviceFd);
		mDeviceFd = -1;
        MTK_V4L2DEVICE_LOGE("deinitialize(): close device... \n");
    }
    if (mDevicePollInterruptFd  != 0)
    {
        close(mDevicePollInterruptFd);
        mDevicePollInterruptFd = -1;
        MTK_V4L2DEVICE_LOGE("deinitialize(): close devicepollinterruptfd... \n");
    }

    EXIT_FUNC
}


void MtkV4L2Device::reset()
{
    ENTER_FUNC

    if (mDeviceFd == 0 || mDevicePollInterruptFd == 0)
    {
        MTK_V4L2DEVICE_LOGE("DeviceFd(%d), PollIntrFd(%d) should NOT be NULL \n", mDeviceFd, mDevicePollInterruptFd);
    }

	MTK_V4L2DEVICE_LOGE("Reset ...");
	mBitstreamFinish	= 0;
	mFrameBufferFinish	= 0;

	dumpDebugInfo();

    EXIT_FUNC
}



int MtkV4L2Device::getAspectRatio(v4l2_aspect_ratio *aspectRatio)
{
    ENTER_FUNC

    CHECK_NULL_RETURN_VALUE(aspectRatio, 0);

    //IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_G_CROP, crop_arg);

    aspectRatio->aspectRatioWidth   = 1;
    aspectRatio->aspectRatioHeight  = 1;

    EXIT_FUNC

    return 1;
}

int MtkV4L2Device::getBitDepthInfo(v4l2_bitdepth_info *bitDepthInfo)
{
    ENTER_FUNC

    CHECK_NULL_RETURN_VALUE(bitDepthInfo, 0);

    //IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_G_CROP, crop_arg);

    bitDepthInfo->bitDepthLuma      = 8;
    bitDepthInfo->bitDepthChroma    = 8;

    EXIT_FUNC

    return 1;
}


int MtkV4L2Device::getCrop(v4l2_crop *crop_arg)
{
    ENTER_FUNC

    CHECK_NULL_RETURN_VALUE(crop_arg, 0);

    crop_arg->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_G_CROP, crop_arg);

    MTK_V4L2DEVICE_LOGE("[Info] get Crop info left: %d top: %d width: %u height: %u!!\n", crop_arg->c.left, crop_arg->c.top, crop_arg->c.width, crop_arg->c.height);

    EXIT_FUNC

    // v4l2 todo: it is meant to return false. copy from the original common driver's behavior. need more check
    return 1;
}

int MtkV4L2Device::getDPBSize(uint *DPBSize)
{
    ENTER_FUNC

    CHECK_NULL_RETURN_VALUE(DPBSize, 0);

    // Number of output buffers we need.
    struct v4l2_control ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_G_CTRL, &ctrl);
    *DPBSize = (uint)ctrl.value;

    EXIT_FUNC

    return 1;

}

int MtkV4L2Device::getFrameInterval(uint *frameInterval)
{
    ENTER_FUNC

    CHECK_NULL_RETURN_VALUE(frameInterval, 0);

    struct v4l2_ext_control extControl;
    struct v4l2_ext_controls extControls;
    memset(&extControl, 0, sizeof(extControl));
    memset(&extControls, 0, sizeof(extControls));
    extControl.id = V4L2_CID_MPEG_MTK_FRAME_INTERVAL;
    extControl.size = sizeof(unsigned int);
    extControl.p_u32 = frameInterval;
    extControls.ctrl_class = V4L2_CTRL_CLASS_MPEG;
    extControls.count = 1;
    extControls.controls = &extControl;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_G_EXT_CTRLS, &extControls);

    MTK_V4L2DEVICE_LOGE("[Info] get frame interval %u!!\n", *frameInterval);

    EXIT_FUNC

    return 1;

}

int MtkV4L2Device::getInterlacing(uint *interlacing)
{
    ENTER_FUNC

#if 0
    CHECK_NULL_RETURN_VALUE(interlacing, false);

    // Number of output buffers we need.
    struct v4l2_control ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_INTERLACING_FOR_CAPTURE;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_G_CTRL, &ctrl);
    *interlacing = (uint)ctrl.value;
#endif

    *interlacing = 0;

    EXIT_FUNC

    return 1;

}

int MtkV4L2Device::getPicInfo(VDEC_DRV_PICINFO_T *PicInfo)
{
    ENTER_FUNC

    struct v4l2_format format;

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    IOCTL_OR_ERROR_RETURN_VALUE(VIDIOC_G_FMT, &format, 1);

    PicInfo->u4Width    = format.fmt.pix_mp.width;//1920;//176;
    PicInfo->u4Height   = format.fmt.pix_mp.height;//1080;//144;

    PicInfo->u4RealWidth  = format.fmt.pix_mp.width;//1920;//176;
    PicInfo->u4RealHeight = format.fmt.pix_mp.height;//1080;//144;

    PicInfo->u4PictureStructure = VDEC_DRV_PIC_STRUCT_CONSECUTIVE_FRAME;

    MTK_V4L2DEVICE_LOGE("[Info] get pic info %d %d (%d %d)!!\n", PicInfo->u4Width, PicInfo->u4Height, PicInfo->u4RealWidth, PicInfo->u4RealHeight);

    EXIT_FUNC

    return 1;

}

int MtkV4L2Device::getUFOCapability(int *UFOSupport)
{
    ENTER_FUNC

    // Need to add a new CID
#if 0
    CHECK_NULL_RETURN_VALUE(UFOSupport, false);

    // Number of output buffers we need.
    struct v4l2_control ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_INTERLACING_FOR_CAPTURE;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_G_CTRL, &ctrl);
    *interlacing = (uint)ctrl.value;
#endif

    // Shoule be removed
    *UFOSupport = 0;

    EXIT_FUNC

    return 1;

}

void MtkV4L2Device::checkEFuse(EFuseBond *bonds)
{

    ENTER_FUNC

    int fd = 0;
    int ret = 0;

    unsigned int devinfo_data = 3;

    memset(bonds, 0, sizeof(EFuseBond));

    char value[PROPERTY_VALUE_MAX];

    property_get("mtk.vcodec.720p", value, "0");
    bonds->videoBond = (atoi(value) == 0) ? 1 : 0;

    // u4VideoBond == 0 -> 2160p
    // u4VideoBond == 1 -> 1080p
    // u4videobond == 2 -> N/A
    // u4videobond == 3 -> Disable
    // u4DivxBond == 1 -> divx bonding

    DEVINFO_S devinfo;
    fd = open("/sys/bus/platform/drivers/dev_info/dev_info", O_RDONLY);
    if (fd >= 0)
    {
        ret = read(fd, (void *)&devinfo, sizeof(DEVINFO_S));
        devinfo_data = devinfo.data[3];     //devinfo index = 3
        if (ret == 0)
        {
            MTK_V4L2DEVICE_LOGE("eFuse read fail, limit to 720p");
            bonds->videoBond = 1;
        }
        else
        {
            //MFV_LOGE("eFuse register 0x%08X: u4DivxBond 0x%08X; u4HEVCBond 0x%08X; u4VideoBond 0x%08X", devinfo.data[3], devinfo.data[3]& (0x1<<13), devinfo.data[3]& (0x1<<14), devinfo.data[3]& (0x1<<15));
            if ((devinfo_data & (0x1 << 13)) == (0x1 << 13))
            {
                bonds->divxBond = 1; // Divx bonding No DIVX
            }
            if ((devinfo_data & (0x1 << 14)) == (0x1 << 14))
            {
                bonds->hevcBond = 1; // HEVC bonding No HEVC
            }
            if ((devinfo_data & (0x1 << 15)) == (0x1 << 15))
            {
                bonds->videoBond = 1; // 1080p
            }
        }
        close(fd);
    }
    else
    {
        //MTK_V4L2DEVICE_LOGE("eFuse dev open fail, limit to 720p");
        //bonds->videoBond = 1; // 1080p
        //bonds->divxBond = 1; // No DIVX
    }

    EXIT_FUNC

}

int MtkV4L2Device::getVideoProfileLevel(VAL_UINT32_T videoFormat, EFuseBond bonds, VDEC_DRV_QUERY_VIDEO_FORMAT_T *infoOut)
{
    ENTER_FUNC
    struct v4l2_frmsizeenum frmsizeenum;
    memset(&frmsizeenum, 0, sizeof(frmsizeenum));

    /* Todo: Need to check videoBond/lowMemoryBond here */

    switch (videoFormat)
    {
        case VDEC_DRV_VIDEO_FORMAT_H264:
            frmsizeenum.pixel_format = V4L2_PIX_FMT_H264;
            IOCTL_OR_ERROR_RETURN_VALUE(VIDIOC_ENUM_FRAMESIZES, &frmsizeenum, 1);
            infoOut->u4VideoFormat = VDEC_DRV_VIDEO_FORMAT_H264;
            infoOut->u4Width = frmsizeenum.stepwise.max_width;
            infoOut->u4Height = frmsizeenum.stepwise.max_height;
            infoOut->u4StrideAlign = frmsizeenum.stepwise.step_width;
            infoOut->u4SliceHeightAlign= frmsizeenum.stepwise.step_height;
            infoOut->u4Profile = H264ProfileMapTable[frmsizeenum.reserved[0]].vdecProfile;
            infoOut->u4Level = H264LevelMapTable[frmsizeenum.reserved[1]].vdecLevel;
            MTK_V4L2DEVICE_LOGE("V4L2: Profile %d, Level %d", frmsizeenum.reserved[0], frmsizeenum.reserved[1]);
            MTK_V4L2DEVICE_LOGE("H264 VDEC: Profile %d, Level %d align(%d/%d)", infoOut->u4Profile, infoOut->u4Level, infoOut->u4StrideAlign, infoOut->u4SliceHeightAlign);
            break;

        case VDEC_DRV_VIDEO_FORMAT_H265:
            frmsizeenum.pixel_format = V4L2_PIX_FMT_H265;
            IOCTL_OR_ERROR_RETURN_VALUE(VIDIOC_ENUM_FRAMESIZES, &frmsizeenum, 1);
            infoOut->u4VideoFormat = VDEC_DRV_VIDEO_FORMAT_H265;
            infoOut->u4Width = frmsizeenum.stepwise.max_width;
            infoOut->u4Height = frmsizeenum.stepwise.max_height;
            infoOut->u4StrideAlign = frmsizeenum.stepwise.step_width;
            infoOut->u4SliceHeightAlign= frmsizeenum.stepwise.step_height;
            infoOut->u4Profile = H265ProfileMapTable[frmsizeenum.reserved[0]].vdecProfile;
            infoOut->u4Level = H265LevelMapTable[frmsizeenum.reserved[1]].vdecLevel;
            MTK_V4L2DEVICE_LOGE("V4L2: Profile %d, Level %d", frmsizeenum.reserved[0], frmsizeenum.reserved[1]);
            MTK_V4L2DEVICE_LOGE("HEVC VDEC: Profile %d, Level %d align(%d/%d)", infoOut->u4Profile, infoOut->u4Level, infoOut->u4StrideAlign, infoOut->u4SliceHeightAlign);
            break;
        case VDEC_DRV_VIDEO_FORMAT_DIVX311:
        case VDEC_DRV_VIDEO_FORMAT_DIVX4:
        case VDEC_DRV_VIDEO_FORMAT_DIVX5:
        case VDEC_DRV_VIDEO_FORMAT_MPEG4:
        case VDEC_DRV_VIDEO_FORMAT_XVID:
        case VDEC_DRV_VIDEO_FORMAT_S263:
            frmsizeenum.pixel_format = V4L2_PIX_FMT_MPEG4;
            IOCTL_OR_ERROR_RETURN_VALUE(VIDIOC_ENUM_FRAMESIZES, &frmsizeenum, 1);
            infoOut->u4VideoFormat = videoFormat;
            infoOut->u4Width = frmsizeenum.stepwise.max_width;
            infoOut->u4Height = frmsizeenum.stepwise.max_height;
            infoOut->u4StrideAlign = frmsizeenum.stepwise.step_width;
            infoOut->u4SliceHeightAlign= frmsizeenum.stepwise.step_height;
            infoOut->u4Profile = MP4ProfileMapTable[frmsizeenum.reserved[0]].vdecProfile;
            infoOut->u4Level = MP4LevelMapTable[frmsizeenum.reserved[1]].vdecLevel;
            MTK_V4L2DEVICE_LOGE("V4L2: Profile %d, Level %d", frmsizeenum.reserved[0], frmsizeenum.reserved[1]);
            MTK_V4L2DEVICE_LOGE("MP4 VDEC: Profile %d, Level %d align(%d/%d)", infoOut->u4Profile, infoOut->u4Level, infoOut->u4StrideAlign, infoOut->u4SliceHeightAlign);
            //To be removed...
            infoOut->u4Profile = VDEC_DRV_MPEG_VIDEO_PROFILE_MPEG4_SIMPLE | VDEC_DRV_MPEG_VIDEO_PROFILE_MPEG4_ADVANCED_SIMPLE;
            infoOut->u4Level = VDEC_DRV_VIDEO_LEVEL_5;
            infoOut->u4StrideAlign = 16;
            infoOut->u4SliceHeightAlign= 32;
            break;

        case VDEC_DRV_VIDEO_FORMAT_H263:
            frmsizeenum.pixel_format = V4L2_PIX_FMT_H263;
            IOCTL_OR_ERROR_RETURN_VALUE(VIDIOC_ENUM_FRAMESIZES, &frmsizeenum, 1);
            infoOut->u4VideoFormat = videoFormat;
            infoOut->u4Width = frmsizeenum.stepwise.max_width;
            infoOut->u4Height = frmsizeenum.stepwise.max_height;
            infoOut->u4StrideAlign = frmsizeenum.stepwise.step_width;
            infoOut->u4SliceHeightAlign= frmsizeenum.stepwise.step_height;
            infoOut->u4Profile = VDEC_DRV_MPEG_VIDEO_PROFILE_H263_0;
            infoOut->u4Level = VDEC_DRV_VIDEO_LEVEL_4;
            MTK_V4L2DEVICE_LOGE("V4L2: Profile %d, Level %d", frmsizeenum.reserved[0], frmsizeenum.reserved[1]);
            MTK_V4L2DEVICE_LOGE("H263 VDEC: Profile %d, Level %d align(%d/%d)", infoOut->u4Profile, infoOut->u4Level, infoOut->u4StrideAlign, infoOut->u4SliceHeightAlign);
            //To be removed...
            infoOut->u4Profile = VDEC_DRV_MPEG_VIDEO_PROFILE_H263_0;
            infoOut->u4Level = VDEC_DRV_VIDEO_LEVEL_4;
            infoOut->u4StrideAlign = 16;
            infoOut->u4SliceHeightAlign= 32;
            break;

        case VDEC_DRV_VIDEO_FORMAT_MPEG2:
            frmsizeenum.pixel_format = V4L2_PIX_FMT_MPEG2;
            IOCTL_OR_ERROR_RETURN_VALUE(VIDIOC_ENUM_FRAMESIZES, &frmsizeenum, 1);
            infoOut->u4VideoFormat = videoFormat;
            infoOut->u4Width = frmsizeenum.stepwise.max_width;
            infoOut->u4Height = frmsizeenum.stepwise.max_height;
            infoOut->u4StrideAlign = frmsizeenum.stepwise.step_width;
            infoOut->u4SliceHeightAlign= frmsizeenum.stepwise.step_height;
            //infoOut->u4Profile = MP4ProfileMapTable[frmsizeenum.reserved[0]].vdecProfile;
            //infoOut->u4Level = MP4LevelMapTable[frmsizeenum.reserved[1]].vdecLevel;
            MTK_V4L2DEVICE_LOGE("V4L2: Profile %d, Level %d", frmsizeenum.reserved[0], frmsizeenum.reserved[1]);
            MTK_V4L2DEVICE_LOGE("MP2 VDEC: Profile %d, Level %d align(%d/%d)", infoOut->u4Profile, infoOut->u4Level, infoOut->u4StrideAlign, infoOut->u4SliceHeightAlign);
            //To be removed...
            infoOut->u4Profile = VDEC_DRV_MPEG_VIDEO_PROFILE_MPEG2_MAIN;
            infoOut->u4Level = VDEC_DRV_VIDEO_LEVEL_MEDIUM;
            infoOut->u4StrideAlign = 16;
            infoOut->u4SliceHeightAlign= 32;
            break;


        case VDEC_DRV_VIDEO_FORMAT_VP8:
            //infoOut->u4VideoFormat = VDEC_DRV_VIDEO_FORMAT_VP8;
            //infoOut->u4Profile = VDEC_DRV_MPEG_VIDEO_PROFILE_MPEG4_SIMPLE;
            //infoOut->u4Level = VDEC_DRV_VIDEO_LEVEL_MEDIUM;
            frmsizeenum.pixel_format = V4L2_PIX_FMT_VP8;
            IOCTL_OR_ERROR_RETURN_VALUE(VIDIOC_ENUM_FRAMESIZES, &frmsizeenum, 1);
            infoOut->u4VideoFormat = videoFormat;
            infoOut->u4Width = frmsizeenum.stepwise.max_width;
            infoOut->u4Height = frmsizeenum.stepwise.max_height;
            infoOut->u4StrideAlign = frmsizeenum.stepwise.step_width;
            infoOut->u4SliceHeightAlign= frmsizeenum.stepwise.step_height;
            //infoOut->u4Profile = MP4ProfileMapTable[frmsizeenum.reserved[0]].vdecProfile;
            //infoOut->u4Level = MP4LevelMapTable[frmsizeenum.reserved[1]].vdecLevel;
            MTK_V4L2DEVICE_LOGE("V4L2: Profile %d, Level %d", frmsizeenum.reserved[0], frmsizeenum.reserved[1]);
            MTK_V4L2DEVICE_LOGE("VP8 VDEC: Profile %d, Level %d align(%d/%d)", infoOut->u4Profile, infoOut->u4Level, infoOut->u4StrideAlign, infoOut->u4SliceHeightAlign);
            //To be removed...
            infoOut->u4Profile = VDEC_DRV_MPEG_VIDEO_PROFILE_MPEG4_SIMPLE;
            infoOut->u4Level = VDEC_DRV_VIDEO_LEVEL_MEDIUM;
            infoOut->u4StrideAlign = 32;
            infoOut->u4SliceHeightAlign= 16;
            break;
        case VDEC_DRV_VIDEO_FORMAT_VP9:
            frmsizeenum.pixel_format = V4L2_PIX_FMT_VP9;
            IOCTL_OR_ERROR_RETURN_VALUE(VIDIOC_ENUM_FRAMESIZES, &frmsizeenum, 1);
            infoOut->u4VideoFormat = videoFormat;
            infoOut->u4Width = frmsizeenum.stepwise.max_width;
            infoOut->u4Height = frmsizeenum.stepwise.max_height;
            infoOut->u4StrideAlign = frmsizeenum.stepwise.step_width;
            infoOut->u4SliceHeightAlign= frmsizeenum.stepwise.step_height;
            //infoOut->u4Profile = MP4ProfileMapTable[frmsizeenum.reserved[0]].vdecProfile;
            //infoOut->u4Level = MP4LevelMapTable[frmsizeenum.reserved[1]].vdecLevel;
            MTK_V4L2DEVICE_LOGE("V4L2: Profile %d, Level %d", frmsizeenum.reserved[0], frmsizeenum.reserved[1]);
            MTK_V4L2DEVICE_LOGE("VP9 VDEC: Profile %d, Level %d align(%d/%d)", infoOut->u4Profile, infoOut->u4Level, infoOut->u4StrideAlign, infoOut->u4SliceHeightAlign);
            //To be removed...
            infoOut->u4Profile = VDEC_DRV_VP9_VIDEO_PROFILE_0;
            infoOut->u4Level = VDEC_DRV_VIDEO_LEVEL_5;
            infoOut->u4StrideAlign = 64;
            infoOut->u4SliceHeightAlign= 64;

            break;
        case VDEC_DRV_VIDEO_FORMAT_VC1:
            frmsizeenum.pixel_format = V4L2_PIX_FMT_WVC1;
            MTK_V4L2DEVICE_LOGE("VC1 VDEC: issue VIDIOC_ENUM_FRAMESIZES");
            IOCTL_OR_ERROR_RETURN_VALUE(VIDIOC_ENUM_FRAMESIZES, &frmsizeenum, 1);
            infoOut->u4Width = frmsizeenum.stepwise.max_width;
            infoOut->u4Height = frmsizeenum.stepwise.max_height;
            infoOut->u4StrideAlign = frmsizeenum.stepwise.step_width;
            infoOut->u4SliceHeightAlign= frmsizeenum.stepwise.step_height;
            infoOut->u4VideoFormat = VDEC_DRV_VIDEO_FORMAT_VC1;
            infoOut->u4Profile = VDEC_DRV_MS_VIDEO_PROFILE_VC1_ADVANCED;
            infoOut->u4Level = VDEC_DRV_VIDEO_LEVEL_3;

            MTK_V4L2DEVICE_LOGE("VC1 VDEC: Profile %d, Level %d align(%d/%d) width/height(%d/%d)", infoOut->u4Profile, infoOut->u4Level, infoOut->u4StrideAlign, infoOut->u4SliceHeightAlign,infoOut->u4Width,infoOut->u4Height);
            break;
#if 0
        case VDEC_DRV_VIDEO_FORMAT_VP8_WEBP_PICTURE_MODE:
            infoOut->u4VideoFormat = VDEC_DRV_VIDEO_FORMAT_VP8_WEBP_PICTURE_MODE;
            infoOut->u4Profile = VDEC_DRV_MPEG_VIDEO_PROFILE_MPEG4_SIMPLE;
            infoOut->u4Level = VDEC_DRV_VIDEO_LEVEL_MEDIUM;
            break;

        case VDEC_DRV_VIDEO_FORMAT_VP8_WEBP_MB_ROW_MODE:
            infoOut->u4VideoFormat = VDEC_DRV_VIDEO_FORMAT_VP8_WEBP_MB_ROW_MODE;
            infoOut->u4Profile = VDEC_DRV_MPEG_VIDEO_PROFILE_MPEG4_SIMPLE;
            infoOut->u4Level = VDEC_DRV_VIDEO_LEVEL_MEDIUM;
            break;


#endif

        default:
            MTK_V4L2DEVICE_LOGE("[Error]Unknow u4VideoFormat return fail!!  0x%08x", videoFormat);
            infoOut->u4VideoFormat = (VDEC_DRV_VIDEO_FORMAT_H264 | VDEC_DRV_VIDEO_FORMAT_MPEG4);

            EXIT_FUNC

            return 0;
    }

    EXIT_FUNC

    return 1;
}


bool MtkV4L2Device::getPixelFormat(VDEC_DRV_PIXEL_FORMAT_T *pixelFormat)
{
    ENTER_FUNC

    struct v4l2_format fmtdesc;
    memset(&fmtdesc, 0, sizeof(fmtdesc));

    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    IOCTL_OR_ERROR_RETURN_VALUE(VIDIOC_G_FMT, &fmtdesc, 1);
    switch (fmtdesc.fmt.pix_mp.pixelformat)
    {
        case V4L2_PIX_FMT_MT21S:
            *pixelFormat = VDEC_DRV_PIXEL_FORMAT_YUV_420_PLANER_MTK;
            MTK_V4L2DEVICE_LOGE("Switch V4L2_PIX_FMT_MT21S to VDEC_DRV_PIXEL_FORMAT_YUV_420_PLANER_MTK");
            break;
        case V4L2_PIX_FMT_YVU420:
            *pixelFormat = VDEC_DRV_PIXEL_FORMAT_YUV_YV12;
            MTK_V4L2DEVICE_LOGE("Switch V4L2_PIX_FMT_YVU420 to VDEC_DRV_PIXEL_FORMAT_YUV_YV12");
            break;
        case V4L2_PIX_FMT_YUV420:
            *pixelFormat = VDEC_DRV_PIXEL_FORMAT_YUV_420_PLANER;
            MTK_V4L2DEVICE_LOGE("Switch V4L2_PIX_FMT_YUV420 to VDEC_DRV_PIXEL_FORMAT_YUV_420_PLANER");
            break;

        case V4L2_PIX_FMT_MT21C:
            *pixelFormat = VDEC_DRV_PIXEL_FORMAT_YUV_420_PLANER_UFO;
            MTK_V4L2DEVICE_LOGD("Switch V4L2_PIX_FMT_MT21C to VDEC_DRV_PIXEL_FORMAT_YUV_420_PLANER_UFO");
            break;

        case V4L2_PIX_FMT_NV12:
            *pixelFormat = VDEC_DRV_PIXEL_FORMAT_YUV_NV12;
            MTK_V4L2DEVICE_LOGE("Switch V4L2_PIX_FMT_NV12 to VDEC_DRV_PIXEL_FORMAT_YUV_NV12");
            break;
        default:
            *pixelFormat = VDEC_DRV_PIXEL_FORMAT_YUV_420_PLANER_MTK;
            MTK_V4L2DEVICE_LOGE("Switch pixel format fail!!  0x%08x(%d)", fmtdesc.fmt.pix_mp.pixelformat, fmtdesc.fmt.pix_mp.pixelformat);
            EXIT_FUNC
            return false;
    }

    EXIT_FUNC

    return true;
}


int MtkV4L2Device::checkVideoFormat(VDEC_DRV_QUERY_VIDEO_FORMAT_T *inputInfo, VDEC_DRV_QUERY_VIDEO_FORMAT_T *outputInfo)
{
    ENTER_FUNC

    CHECK_NULL_RETURN_VALUE(inputInfo, false);
    VDEC_DRV_QUERY_VIDEO_FORMAT_T localOutputInfo;

    EFuseBond bonds;
    checkEFuse(&bonds);

    P_VDEC_DRV_QUERY_VIDEO_FORMAT_T  pVFin  = inputInfo;
    P_VDEC_DRV_QUERY_VIDEO_FORMAT_T  pVFout = (NULL != outputInfo) ? outputInfo : &localOutputInfo;

    getVideoProfileLevel(pVFin->u4VideoFormat, bonds, pVFout);
    getPixelFormat(&pVFout->ePixelFormat);

    if ((pVFin->u4Profile & pVFout->u4Profile) == 0 ||
        pVFin->u4Level > pVFout->u4Level            ||
        pVFin->u4Width > pVFout->u4Width            ||
        pVFin->u4Height > pVFout->u4Height          ||
        pVFin->u4Width * pVFin->u4Height > pVFout->u4Width * pVFout->u4Height ||
        pVFout->u4Width == 0 ||
        pVFout->u4Height == 0)
    {
        MTK_V4L2DEVICE_LOGE("IN -> profile 0x%08x, level %d, width %d, height %d", pVFin->u4Profile, pVFin->u4Level, pVFin->u4Width, pVFin->u4Height);
        MTK_V4L2DEVICE_LOGE("OUT -> profile 0x%08x, level %d, width %d, height %d", pVFout->u4Profile, pVFout->u4Level, pVFout->u4Width, pVFout->u4Height);
        if (bonds.videoBond == 1)
        {
            MTK_V4L2DEVICE_LOGE("Over 1080p disabled by eFuse");
        }

        EXIT_FUNC

        return false;
    }

    EXIT_FUNC

    return true;
}




int MtkV4L2Device::requestBufferBitstream(uint bitstreamBufferCount)
{
    ENTER_FUNC

    // Input buffer
    struct v4l2_requestbuffers reqbufs;
    memset(&reqbufs, 0, sizeof(reqbufs));

    reqbufs.count  = bitstreamBufferCount;
    reqbufs.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    reqbufs.memory = V4L2_MEMORY_DMABUF;

    MTK_V4L2DEVICE_LOGE("BS ReqBuf. Cnt:%d", bitstreamBufferCount);

    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_REQBUFS, &reqbufs);

    EXIT_FUNC

    return 1;
}

int MtkV4L2Device::requestBufferFrameBuffer(uint frameBufferCount)
{
    ENTER_FUNC

    // Output buffer
    struct v4l2_requestbuffers reqbufs;
    memset(&reqbufs, 0, sizeof(reqbufs));

    // Allocate the output buffers.
    memset(&reqbufs, 0, sizeof(reqbufs));
    reqbufs.count  = frameBufferCount;
    reqbufs.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    reqbufs.memory = V4L2_MEMORY_DMABUF;

    MTK_V4L2DEVICE_LOGE("FB ReqBuf. Cnt:%d", frameBufferCount);

    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_REQBUFS, &reqbufs);

    EXIT_FUNC

    return 1;
}

int MtkV4L2Device::setFormatBistream(uint codecType, uint inputBufferSize)
{
    ENTER_FUNC

    v4l2_format inputFormat;
    memset(&inputFormat, 0, sizeof(inputFormat));

    inputFormat.type                                 = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    inputFormat.fmt.pix_mp.plane_fmt[0].sizeimage    = inputBufferSize;
    inputFormat.fmt.pix_mp.num_planes                = 1;

    switch (codecType)
    {
        case MTK_VDEC_CODEC_ID_AVC:
            inputFormat.fmt.pix_mp.pixelformat       = V4L2_PIX_FMT_H264;
            MTK_V4L2DEVICE_LOGE("[Info] codec type: V4L2_PIX_FMT_H264");
            break;
        case MTK_VDEC_CODEC_ID_H263:
            inputFormat.fmt.pix_mp.pixelformat       = V4L2_PIX_FMT_H263;
            MTK_V4L2DEVICE_LOGE("[Info] codec type: V4L2_PIX_FMT_H263");
            break;
        case MTK_VDEC_CODEC_ID_DIVX:
            inputFormat.fmt.pix_mp.pixelformat       = V4L2_PIX_FMT_DIVX;
            MTK_V4L2DEVICE_LOGE("[Info] codec type: V4L2_PIX_FMT_DIVX");
            break;
        case MTK_VDEC_CODEC_ID_DIVX3:
            inputFormat.fmt.pix_mp.pixelformat       = V4L2_PIX_FMT_DIVX3;
            MTK_V4L2DEVICE_LOGE("[Info] codec type: V4L2_PIX_FMT_DIVX3");
            break;
        case MTK_VDEC_CODEC_ID_XVID:
            inputFormat.fmt.pix_mp.pixelformat       = V4L2_PIX_FMT_XVID;
            MTK_V4L2DEVICE_LOGE("[Info] codec type: V4L2_PIX_FMT_XVID");
            break;
        case MTK_VDEC_CODEC_ID_S263:
            inputFormat.fmt.pix_mp.pixelformat       = V4L2_PIX_FMT_S263;
            MTK_V4L2DEVICE_LOGE("[Info] codec type: V4L2_PIX_FMT_S263");
            break;
        case MTK_VDEC_CODEC_ID_MPEG4:
            inputFormat.fmt.pix_mp.pixelformat       = V4L2_PIX_FMT_MPEG4;
            MTK_V4L2DEVICE_LOGE("[Info] codec type: V4L2_PIX_FMT_MPEG4");
            break;
        case MTK_VDEC_CODEC_ID_VP9:
            inputFormat.fmt.pix_mp.pixelformat       = V4L2_PIX_FMT_VP9;
            MTK_V4L2DEVICE_LOGE("[Info] codec type: V4L2_PIX_FMT_VP9");
            break;
        case MTK_VDEC_CODEC_ID_VPX:
            inputFormat.fmt.pix_mp.pixelformat       = V4L2_PIX_FMT_VP8;
            MTK_V4L2DEVICE_LOGE("[Info] codec type: V4L2_PIX_FMT_VP8");
            break;
        case MTK_VDEC_CODEC_ID_VC1:
            inputFormat.fmt.pix_mp.pixelformat       = V4L2_PIX_FMT_WVC1;
            MTK_V4L2DEVICE_LOGE("[Info] codec type:, V4L2_PIX_FMT_WVC1");
            break;
        case MTK_VDEC_CODEC_ID_MPEG2:
            inputFormat.fmt.pix_mp.pixelformat       = V4L2_PIX_FMT_MPEG2;
            MTK_V4L2DEVICE_LOGE("[Info] codec type: V4L2_PIX_FMT_MPEG2");
            break;
        case MTK_VDEC_CODEC_ID_HEVC:
            inputFormat.fmt.pix_mp.pixelformat       = V4L2_PIX_FMT_H265;
            MTK_V4L2DEVICE_LOGE("[Info] codec type: V4L2_PIX_FMT_H265");
            break;

        default:
            MTK_V4L2DEVICE_LOGE("[Error] Unknown codec type: %d", codecType);
            break;
    }

    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_FMT, &inputFormat);

    EXIT_FUNC

    return 1;
}

int MtkV4L2Device::setFormatFrameBuffer(uint yuvFormat)
{
    ENTER_FUNC

    // Output format has to be setup before streaming starts.
    v4l2_format outputFormat;
    memset(&outputFormat, 0, sizeof(outputFormat));

    outputFormat.type                       = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    switch (yuvFormat)
    {
        case OMX_COLOR_FormatVendorMTKYUV:
            outputFormat.fmt.pix_mp.pixelformat     = V4L2_PIX_FMT_MT21S;
            outputFormat.fmt.pix_mp.num_planes = 1;
            MTK_V4L2DEVICE_LOGE("[Info] yuv format: V4L2_PIX_FMT_MT21S");
            break;
        case OMX_COLOR_FormatYUV420Planar:
            outputFormat.fmt.pix_mp.pixelformat     = V4L2_PIX_FMT_YUV420;
            outputFormat.fmt.pix_mp.num_planes = 2;
            MTK_V4L2DEVICE_LOGE("[Info] yuv format: V4L2_PIX_FMT_YUV420");
            break;
        case OMX_MTK_COLOR_FormatYV12:
            outputFormat.fmt.pix_mp.pixelformat     = V4L2_PIX_FMT_YVU420;
            outputFormat.fmt.pix_mp.num_planes = 2;
            MTK_V4L2DEVICE_LOGE("[Info] yuv format: V4L2_PIX_FMT_YVU420");
            break;
        case OMX_COLOR_FormatVendorMTKYUV_UFO:
            outputFormat.fmt.pix_mp.pixelformat     = V4L2_PIX_FMT_MT21C;
            outputFormat.fmt.pix_mp.num_planes = 2;
            MTK_V4L2DEVICE_LOGE("[Info] yuv format: V4L2_PIX_FMT_MT21C");
            break;
        case OMX_COLOR_FormatYUV420SemiPlanar:
            outputFormat.fmt.pix_mp.pixelformat     = V4L2_PIX_FMT_NV12;
            outputFormat.fmt.pix_mp.num_planes = 2;
            MTK_V4L2DEVICE_LOGE("[Info] yuv format: V4L2_PIX_FMT_NV12");
            break;
        default:
            MTK_V4L2DEVICE_LOGE("[Error] Unknown yuv format: %d", yuvFormat);
            outputFormat.fmt.pix_mp.pixelformat     = V4L2_PIX_FMT_MT21S;
            outputFormat.fmt.pix_mp.num_planes = 1;
            break;
    }

    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_FMT, &outputFormat);

    EXIT_FUNC

    return 1;
}

int MtkV4L2Device::getChipName(uint *chipName)
{
    ENTER_FUNC

    // Need to add a new CID
#if 0
    CHECK_NULL_RETURN_VALUE(chipName, false);

    // Number of output buffers we need.
    struct v4l2_control ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_INTERLACING_FOR_CAPTURE;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_G_CTRL, &ctrl);
    *interlacing = (uint)ctrl.value;
#endif

    // Shoule be removed
    *chipName = VAL_CHIP_NAME_MT6797;

    EXIT_FUNC

    return 1;

}


int MtkV4L2Device::setDecodeMode(VDEC_DRV_SET_DECODE_MODE_T *rtSetDecodeMode)
{
    ENTER_FUNC

    struct v4l2_control ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_MPEG_MTK_DECODE_MODE;
    ctrl.value = rtSetDecodeMode->eDecodeMode;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_CTRL, &ctrl);

    MTK_V4L2DEVICE_LOGE("[Info] set decode mode %d!!\n", rtSetDecodeMode->eDecodeMode);

    EXIT_FUNC

    return 1;
}

int MtkV4L2Device::setMPEG4FrameSize(uint *frameSize)
{
    ENTER_FUNC

    // Need to (1) define a CID (2) passing frameSize to kernel
#if 0
    CHECK_NULL_RETURN_VALUE(frameSize, false);

    // Number of output buffers we need.
    struct v4l2_control ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_DECODE_MODE;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_CTRL, &ctrl);
    (int)ctrl.value;
#endif

    EXIT_FUNC

    return 1;
}

int MtkV4L2Device::setUFOOn()
{
    ENTER_FUNC

    // Need to (1) define a CID
    struct v4l2_control ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_MPEG_MTK_UFO_MODE;
    ctrl.value = 1;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_CTRL, &ctrl);

    EXIT_FUNC

    return 1;
}

V4L2QueueRecorder::V4L2QueueRecorder()
{
	INIT_MUTEX(mMutex);
}

int V4L2QueueRecorder::getSize()
{
	int queuedBufferCnt = 0;

	LOCK(mMutex);
	queuedBufferCnt = mQueueRecorder.size();
	UNLOCK(mMutex);

	return queuedBufferCnt;
}

void V4L2QueueRecorder::putElement(v4l2_buffer toBeRecordedBuffer)
{
	LOCK(mMutex);
	mQueueRecorder.push_back(toBeRecordedBuffer);
	UNLOCK(mMutex);
}

v4l2_buffer V4L2QueueRecorder::getElement(int index)
{
	v4l2_buffer gotElement;

	LOCK(mMutex);
	gotElement = mQueueRecorder[index];
	UNLOCK(mMutex);

	return gotElement;
}

bool V4L2QueueRecorder::eraseElement(int index)
{
	bool isElementErased = false;

	LOCK(mMutex);
    for (Vector<v4l2_buffer>::iterator iter = mQueueRecorder.begin(); iter != mQueueRecorder.end(); ++iter)
    {
        if (iter->index == index)
        {
            mQueueRecorder.erase(iter);
            isElementErased = true;
            break;
        }
    }
	UNLOCK(mMutex);

	return isElementErased;
}



V4L2QueueRecorder::~V4L2QueueRecorder()
{

	DESTROY_MUTEX(mMutex);
}




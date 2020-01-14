
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

#include "vdec_drv_if_public.h"
#include "vdec_drv_if_private.h"

#include "osal_utils.h" // for Vector usage

#ifndef _MTK_V4L2_DEVICE_H_
#define _MTK_V4L2_DEVICE_H_

#define POLL_TIMEOUT 33 // unit: ms. (-1 for infinite)

#define V4L2_CTRL_CLASS_MPEG		                0x00990000	/* MPEG-compression controls */
/*  Mediatek control IDs */
#define V4L2_CID_MPEG_MTK_BASE 				        (V4L2_CTRL_CLASS_MPEG | 0x2000)
#define V4L2_CID_MPEG_MTK_FRAME_INTERVAL 	        (V4L2_CID_MPEG_MTK_BASE+0)
#define V4L2_CID_MPEG_MTK_ERRORMB_MAP 	            (V4L2_CID_MPEG_MTK_BASE+1)

#define V4L2_CID_MPEG_MTK_DECODE_MODE 	            (V4L2_CID_MPEG_MTK_BASE+2)
#define V4L2_CID_MPEG_MTK_FRAME_SIZE 	            (V4L2_CID_MPEG_MTK_BASE+3)
#define V4L2_CID_MPEG_MTK_FIXED_MAX_FRAME_BUFFER 	(V4L2_CID_MPEG_MTK_BASE+4)
#define V4L2_CID_MPEG_MTK_UFO_MODE                  (V4L2_CID_MPEG_MTK_BASE+5)


enum V4L2DeviceType
{
    kDecoder,
    kEncoder,
};

enum V4L2QueueType
{
    kBitstreamQueue,
    kFrameBufferQueue,
};


struct v4l2_aspect_ratio
{
    int aspectRatioWidth;
    int aspectRatioHeight;
};

struct v4l2_bitdepth_info
{
    int bitDepthLuma;
    int bitDepthChroma;
};

struct EFuseBond
{
    VAL_UINT32_T videoBond;
    VAL_UINT32_T divxBond;
    VAL_UINT32_T hevcBond;
};

struct MtkV4L2Device_PROFILE_MAP_ENTRY
{
    VAL_UINT32_T    v4l2Profile;    // V4L2_VIDEO_XXXPROFILETYPE
    VAL_UINT32_T    vdecProfile;    // VDEC_DRV_XXX_VIDEO_PROFILE_T
};

struct MtkV4L2Device_LEVEL_MAP_ENTRY
{
    VAL_UINT32_T    v4l2Level;    // V4L2_VIDEO_XXXLEVELTYPE
    VAL_UINT32_T    vdecLevel;    // VDEC_DRV_VIDEO_LEVEL_T
};



// Internal state of the device.
enum MtkV4L2Device_State
{
    kUninitialized,      // initialize() not yet called.
    kInitialized,        // Initialize() returned true; ready to start decoding.
    kDecoding,           // decoding frames.
    kResetting,          // Presently resetting.
    kAfterReset,         // After Reset(), ready to start decoding again.
    kChangingResolution, // Performing resolution change, all remaining
    // pre-change frames decoded and processed.
    kFlushing,           // ByteUsed 0 was enqueued but not dequeued yet.
    kError,              // Error in kDecoding state.
};

class V4L2QueueRecorder
{

public:

    V4L2QueueRecorder();
    ~V4L2QueueRecorder();

    int getSize();
    void putElement(v4l2_buffer toBeRecordedBuffer);
    v4l2_buffer getElement(int index);
    bool eraseElement(int index);



private:
    pthread_mutex_t mMutex;
    Vector<v4l2_buffer> mQueueRecorder;

};



class MtkV4L2Device
{
        friend class MtkOmxVdec;
        friend class V4L2QueueRecorder;

    public:
        explicit MtkV4L2Device();

        int initialize(V4L2DeviceType type, void *Client);
        void deinitialize();

        int flushFrameBufferQ();
        int flushBitstreamQ();
        void checkFlushDone();

        // V4L2 ioctl implementation.
        int deviceIoctl(int request, void *arg);
        int devicePoll(int *isTherePendingEvent, int timeout = POLL_TIMEOUT);
        int requestBufferBitstream(uint bitstreamBufferCount);
        int requestBufferFrameBuffer(uint frameBufferCount);
        int queueBitstream(int bitstreamIndex, int bitstreamDMAFd, int byteUsed, int maxSize, signed long long timestamp, int flags);
        int queueFrameBuffer(int frameBufferIndex, int frameBufferDMAFd, int byteUsed);
        int setFormatBistream(uint codecType, uint inputBufferSize);
        int setFormatFrameBuffer(uint yuvFormat);
        int getCapFmt(unsigned int *w, unsigned int *h);
        int StreamOnBitstream(void);
        int StreamOnFrameBuffer(void);
        int StreamOffBitstream(void);
        int StreamOffFrameBuffer(void);
        int dequeueBitstream(int *bitstreamIndex, int *isLastBitstream = NULL);
        int dequeueFrameBuffer(int *frameBufferIndex, long long *timestamp, int *isLastFrame);
        int IsStreamOn(V4L2QueueType QueueType);
        void GetStateString(MtkV4L2Device_State state, char *stateString);
        int queuedFrameBufferCount();
        int queuedBitstreamCount();
        int subscribeEvent();
        int dequeueEvent();
        int mFrameBufferSize;

        // Get parameter related
        void checkEFuse(EFuseBond *bonds);
        int getChipName(uint *chipName);
        int getCrop(v4l2_crop *crop_arg);
        int getDPBSize(uint *DPBSize);
        int getFrameInterval(uint *frameInterval);
        int getInterlacing(uint *interlacing);
        int getPicInfo(VDEC_DRV_PICINFO_T *PicInfo);
        int getUFOCapability(int *UFOSupport);
        int getAspectRatio(v4l2_aspect_ratio *aspectRatio);
        int getBitDepthInfo(v4l2_bitdepth_info *bitDepthInfo);
        int getVideoProfileLevel(VAL_UINT32_T videoFormat, EFuseBond bonds, VDEC_DRV_QUERY_VIDEO_FORMAT_T *infoOut);
        bool getPixelFormat(VDEC_DRV_PIXEL_FORMAT_T *pixelFormat);
        int checkVideoFormat(VDEC_DRV_QUERY_VIDEO_FORMAT_T *inputInfo, VDEC_DRV_QUERY_VIDEO_FORMAT_T *outputInfo);

        // Set parameter related
        int setDecodeMode(VDEC_DRV_SET_DECODE_MODE_T *rtSetDecodeMode);
        int setMPEG4FrameSize(uint *frameSize);
        int setUFOOn();

        ~MtkV4L2Device();

        void dumpDebugInfo();
        void ChangeState(MtkV4L2Device_State newState);
        int CheckState(MtkV4L2Device_State toBeCheckState);
        void reset();

        bool needToDumpDebugInfo();
        void updateDebugInfo();

    private:

        // The actual device fd.
        int mDeviceFd;

        // eventfd fd to signal device poll thread when its poll() should be
        // interrupted.
        int mDevicePollInterruptFd;

        V4L2DeviceType mDeviceType ;

        int mBitstreamQStreamOn;
        int mFramebufferQStreamOn;

        MtkV4L2Device_State mState;

        int mMaxBitstreamSize;
        int mBitstreamFinish;
        int mFrameBufferFinish;

        pthread_mutex_t mStateMutex;

        V4L2QueueRecorder mBSQR; // BitStreamQueueRecorder
        V4L2QueueRecorder mFBQR; // FrameBufferQueueRecorder

        __u32 mWidth;
        __u32 mHeight;


        // Debug +
        int mLastBitstreamQStreamOn;
        int mLastFramebufferQStreamOn;
        int mLastFBQR_size;
        int mLastBSQR_size;
        int mLastBitstreamFinish;
        int mLastFrameBufferFinish;
        int mDeadDumpCount;
        // Debug -

        void *mClient;
};


#endif  // _MTK_V4L2_DEVICE_H_

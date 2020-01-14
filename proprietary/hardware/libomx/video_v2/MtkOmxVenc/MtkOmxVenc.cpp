/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein is
 * confidential and proprietary to MediaTek Inc. and/or its licensors. Without
 * the prior written permission of MediaTek inc. and/or its licensors, any
 * reproduction, modification, use or disclosure of MediaTek Software, and
 * information contained herein, in whole or in part, shall be strictly
 * prohibited.
 *
 * MediaTek Inc. (C) 2010. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER
 * ON AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL
 * WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
 * NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH
 * RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY,
 * INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES
 * TO LOOK ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO.
 * RECEIVER EXPRESSLY ACKNOWLEDGES THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO
 * OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES CONTAINED IN MEDIATEK
 * SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE
 * RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S
 * ENTIRE AND CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE
 * RELEASED HEREUNDER WILL BE, AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE
 * MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE
 * CHARGE PAID BY RECEIVER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek
 * Software") have been modified by MediaTek Inc. All revisions are subject to
 * any receiver's applicable license agreements with MediaTek Inc.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *   MtkOmxVenc.cpp
 *
 * Project:
 * --------
 *   MT65xx
 *
 * Description:
 * ------------
 *   MTK OMX Video Encoder component
 *
 * Author:
 * -------
 *   Morris Yang (mtk03147)
 *
 ****************************************************************************/

#include <signal.h>
#include <cutils/log.h>

#include <utils/Trace.h>
#include <utils/AndroidThreads.h>

#include "osal_utils.h"
#include "MtkOmxVenc.h"

#include <cutils/properties.h>
#include <sched.h>
//#include <linux/rtpm_prio.h>

#undef LOG_TAG
#define LOG_TAG "MtkOmxVenc"

#include "OMX_IndexExt.h"

#ifdef ATRACE_TAG
#undef ATRACE_TAG
#define ATRACE_TAG ATRACE_TAG_VIDEO
#endif//ATRACE_TAG

#define MTK_OMX_H263_ENCODER    "OMX.MTK.VIDEO.ENCODER.H263"
#define MTK_OMX_MPEG4_ENCODER   "OMX.MTK.VIDEO.ENCODER.MPEG4"
#define MTK_OMX_AVC_ENCODER     "OMX.MTK.VIDEO.ENCODER.AVC"
#define MTK_OMX_HEVC_ENCODER    "OMX.MTK.VIDEO.ENCODER.HEVC"
#define MTK_OMX_AVC_SEC_ENCODER "OMX.MTK.VIDEO.ENCODER.AVC.secure"
#define MTK_OMX_VP8_ENCODER     "OMX.MTK.VIDEO.ENCODER.VPX"

#define H264_MAX_BS_SIZE    1024*1024
#define HEVC_MAX_BS_SIZE    1024*1024
#define MP4_MAX_BS_SIZE     1024*1024
#define VP8_MAX_BS_SIZE     1024*1024

// Morris Yang 20120214 add for live effect recording [
#ifdef ANDROID_ICS
#include <ui/Rect.h>
//#include <ui/android_native_buffer.h> // for ICS
#include <android/native_window.h> // for JB
// #include <media/stagefright/HardwareAPI.h> // for ICS
#include <HardwareAPI.h> // for JB
//#include <media/stagefright/MetadataBufferType.h> // for ICS
#include <MetadataBufferType.h> // for JB

#include <hardware/gralloc.h>
#include <ui/gralloc_extra.h>
#endif
// ]
#define OMX_CHECK_DUMMY
#include "../../../omx/core/src/MtkOmxCore.h"

#include <poll.h>

#include <utils/Trace.h>
//#include <utils/AndroidThreads.h>

////////for fence in M0/////////////
#include <ui/Fence.h>
#include <media/IOMX.h>
///////////////////end///////////

#define ATRACE_TAG ATRACE_TAG_VIDEO
#define USE_SYSTRACE

#define PROFILING 1

#define IN_FUNC() \
    MTK_OMX_LOGD("+ %s():%d\n", __func__, __LINE__)

#define OUT_FUNC() \
    MTK_OMX_LOGD("- %s():%d\n", __func__, __LINE__)

#define PROP() \
    MTK_OMX_LOGD(" --> %s : %d\n", __func__, __LINE__)

// MtkOmxBufQ [
MtkOmxBufQ::MtkOmxBufQ()
    : mId(MTK_OMX_VENC_BUFQ_INPUT),
      mPendingNum(0)
{
    INIT_MUTEX(mBufQLock);
    mBufQ.clear();
}

MtkOmxBufQ::~MtkOmxBufQ()
{
    DESTROY_MUTEX(mBufQLock);
    mBufQ.clear();
}

int MtkOmxBufQ::DequeueBuffer()
{
    int output_idx = -1;
    LOCK(mBufQLock);

    if (Size() <= 0)
    {
        UNLOCK(mBufQLock);
        return output_idx;
    }
#if CPP_STL_SUPPORT
    output_idx = *(mBufQ.begin());
    mBufQ.erase(mBufQ.begin());
#endif

#if ANDROID
    output_idx = mBufQ[0];
    mBufQ.removeAt(0);
#endif
    UNLOCK(mBufQLock);
    //MTK_OMX_LOGD("q:%d dequeue:%d", (int)mId, output_idx);
    return output_idx;
}

void MtkOmxBufQ::QueueBufferBack(int index)
{
    LOCK(mBufQLock);
#if CPP_STL_SUPPORT
    mBufQ.push_back(index);
#endif

#if ANDROID
    mBufQ.push(index);
#endif
    UNLOCK(mBufQLock);
}

void MtkOmxBufQ::QueueBufferFront(int index)
{
    LOCK(mBufQLock);
#if CPP_STL_SUPPORT
    mBufQ.push_front(index);
#endif

#if ANDROID
    mBufQ.insertAt(index, 0);
#endif
    UNLOCK(mBufQLock);
}

bool MtkOmxBufQ::IsEmpty()
{
#if CPP_STL_SUPPORT
    return (bool)mBufQ.empty();
#endif
#if ANDROID
    return (bool)mBufQ.isEmpty();
#endif
}

void MtkOmxBufQ::Push(int index)
{
#if CPP_STL_SUPPORT
    return mBufQ.push_back(index);
#endif

#if ANDROID
    return mBufQ.push(index);
#endif
}

void MtkOmxBufQ::PushFront(int index)
{
#if CPP_STL_SUPPORT
    mBufQ.push_front(index);
#endif

#if ANDROID
    mBufQ.insertAt(index, 0);
#endif
    return;
}

size_t MtkOmxBufQ::Size()
{
    return mBufQ.size();
}

void MtkOmxBufQ::Clear()
{
    return mBufQ.clear();
}
// ]

void MtkOmxVenc::PriorityAdjustment()
{
    if (1 == mVencAdjustPriority && MTK_VENC_CODEC_ID_AVC == mCodecId)
    {
        //apply this for CTS EncodeVirtualDisplayTest in low-end targets that Venc in RT thread with out of SPEC will
        //occupied CPU resources, ALPS1435942
        VENC_DRV_MRESULT_T mReturn = VENC_DRV_MRESULT_OK;
        OMX_U32 uPriorityAdjustmentType = 0; // todo VENC_DRV_GET_TYPE_PRIORITY_ADJUSTMENT_TYPE v4l2
        //mReturn= eVEncDrvGetParam((VAL_HANDLE_T)NULL, VENC_DRV_GET_TYPE_PRIORITY_ADJUSTMENT_TYPE, (VAL_VOID_T *)&mChipName, (VAL_VOID_T *)&uPriorityAdjustmentType);
        //MTK_OMX_LOGD("uPriorityAdjustmentType %d", uPriorityAdjustmentType);

        switch(uPriorityAdjustmentType)
        {
            case VENC_DRV_PRIORITY_ADJUSTMENT_TYPE_ONE://8127
                if ((1280 == (int)mInputPortDef.format.video.nFrameWidth) &&
                    (720 == (int)mInputPortDef.format.video.nFrameHeight) &&
                    (15 == (mInputPortDef.format.video.xFramerate >> 16)))
                {
                    mVencAdjustPriority = 0;
                    MTK_OMX_LOGD("!!!!!    [MtkOmxVencEncodeThread] sched_setscheduler ok, nice 10");
                    androidSetThreadPriority(0, 20);
                }
                return;
            case VENC_DRV_PRIORITY_ADJUSTMENT_TYPE_TWO://Denali2
                if (OMX_TRUE == mIsMtklog)
                {
                    mVencAdjustPriority = 0;
                    MTK_OMX_LOGD("!!!!!    [MtkOmxVencEncodeThread] sched_setscheduler ok, ANDROID_PRIORITY_DISPLAY");
                    androidSetThreadPriority(0, ANDROID_PRIORITY_DISPLAY);
                }
                return;
            default:
                return;
        }
    }
}

OMX_VIDEO_AVCPROFILETYPE MtkOmxVenc::defaultAvcProfile(VAL_UINT32_T u4ChipName)
{
    VENC_DRV_MRESULT_T mReturn = VENC_DRV_MRESULT_OK;
    OMX_U32 uDefaultAVCProfileType = 2; // todo VENC_DRV_GET_TYPE_DEFAULT_AVC_PROFILE_TYPE v4l2
    //mReturn = eVEncDrvGetParam((VAL_HANDLE_T)NULL, VENC_DRV_GET_TYPE_DEFAULT_AVC_PROFILE_TYPE, (VAL_VOID_T *)&mChipName, (VAL_VOID_T *)&uDefaultAVCProfileType);
    MTK_OMX_LOGD("uDefaultAVCProfileType %d", uDefaultAVCProfileType);

    switch (uDefaultAVCProfileType)
    {
        case VENC_DRV_DEFAULT_AVC_PROFILE_TYPE_ONE://D2 70 80 8167
            return OMX_VIDEO_AVCProfileBaseline;
            break;

        case VENC_DRV_DEFAULT_AVC_PROFILE_TYPE_TWO://8135 8127 7623 8163, D1 D3 55 57 63 39 58 59 71 75 97 99
            return OMX_VIDEO_AVCProfileHigh;
            break;

        default:
            MTK_OMX_LOGE("%s [ERROR] VAL_CHIP_NAME_UNKNOWN", __FUNCTION__);
            return OMX_VIDEO_AVCProfileBaseline;
            break;
    }
    return OMX_VIDEO_AVCProfileBaseline;
}

OMX_VIDEO_AVCLEVELTYPE MtkOmxVenc::defaultAvcLevel(VAL_UINT32_T u4ChipName)
{

    VENC_DRV_MRESULT_T mReturn = VENC_DRV_MRESULT_OK;
    OMX_U32 uDefaultAVCLevelType = 4; // todo VENC_DRV_GET_TYPE_DEFAULT_AVC_LEVEL_TYPE v4l2
    //mReturn = eVEncDrvGetParam((VAL_HANDLE_T)NULL, VENC_DRV_GET_TYPE_DEFAULT_AVC_LEVEL_TYPE, (VAL_VOID_T *)&mChipName, (VAL_VOID_T *)&uDefaultAVCLevelType);
    MTK_OMX_LOGD("uDefaultAVCLevelType %d", uDefaultAVCLevelType);

    switch (uDefaultAVCLevelType)
    {
        case VENC_DRV_DEFAULT_AVC_LEVEL_TYPE_ONE://D2 70 80 8167
            return OMX_VIDEO_AVCLevel21;
            break;

        case VENC_DRV_DEFAULT_AVC_LEVEL_TYPE_TWO://89
            return OMX_VIDEO_AVCLevel31;
            break;
        case VENC_DRV_DEFAULT_AVC_LEVEL_TYPE_THREE://8135 8173 8127 7623, 58 59 97 99
            return OMX_VIDEO_AVCLevel41;
            break;

        case VENC_DRV_DEFAULT_AVC_LEVEL_TYPE_FOUR://6752 8163 D1 D3 55 57 63 39 71 75
            if (OMX_ErrorNone != QueryVideoProfileLevel(VENC_DRV_VIDEO_FORMAT_H264,
                                                        Omx2DriverH264ProfileMap(defaultAvcProfile(mChipName)),
                                                        Omx2DriverH264LevelMap(OMX_VIDEO_AVCLevel41)))
            {
                return OMX_VIDEO_AVCLevel31;
            }
            return OMX_VIDEO_AVCLevel41;
            break;

        default:
            MTK_OMX_LOGE("%s [ERROR] VAL_CHIP_NAME_UNKNOWN", __FUNCTION__);
            return OMX_VIDEO_AVCLevel31;
            break;
    }
    return OMX_VIDEO_AVCLevel31;
}

VENC_DRV_VIDEO_FORMAT_T MtkOmxVenc::GetVencFormat(MTK_VENC_CODEC_ID codecId) {
    switch (codecId) {
        case MTK_VENC_CODEC_ID_MPEG4_SHORT:
            return VENC_DRV_VIDEO_FORMAT_H263;

        case MTK_VENC_CODEC_ID_MPEG4:
            return VENC_DRV_VIDEO_FORMAT_MPEG4;

        case MTK_VENC_CODEC_ID_MPEG4_1080P:
            return VENC_DRV_VIDEO_FORMAT_MPEG4_1080P;

        case MTK_VENC_CODEC_ID_AVC:
            return VENC_DRV_VIDEO_FORMAT_H264;

        case MTK_VENC_CODEC_ID_AVC_VGA:
            return VENC_DRV_VIDEO_FORMAT_H264_VGA;

        default:
            MTK_OMX_LOGE ("Unsupported video format");
            return VENC_DRV_VIDEO_FORMAT_MAX;
    }
}

OMX_VIDEO_HEVCPROFILETYPE MtkOmxVenc::defaultHevcProfile(VAL_UINT32_T u4ChipName)
{
    return OMX_VIDEO_HEVCProfileMain;
}

OMX_VIDEO_HEVCLEVELTYPE MtkOmxVenc::defaultHevcLevel(VAL_UINT32_T u4ChipName)
{
    return OMX_VIDEO_HEVCMainTierLevel1;
}

void MtkOmxVenc::EncodeVideo(OMX_BUFFERHEADERTYPE *pInputBuf, OMX_BUFFERHEADERTYPE *pOutputBuf)
{
    if (NULL != mCoreGlobal)
    {
        ((mtk_omx_core_global *)mCoreGlobal)->video_operation_count++;
    }

    /*if (pInputBuf == NULL) {
        encodeHybridEOS(pOutputBuf);
        return;
    }*/

    if (false == mDoConvertPipeline)
    {
        //check MetaMode input format
        mInputMetaDataFormat = CheckOpaqueFormat(pInputBuf);
        if (0xFFFFFFFF == mGrallocWStride)
        {
            mGrallocWStride = CheckGrallocWStride(pInputBuf);
        }

        /* for DirectLink Meta Mode + */
        if ((OMX_TRUE == DLMetaModeEnable()))
        {
            DLMetaModeEncodeVideo(pInputBuf, pOutputBuf);
            return;
        }
        /* for DirectLink Meta Mode - */
    }

    //init converter buffer here
    if (!mDoConvertPipeline)
    {
        InitConvertBuffer();
    }

    EncodeFunc(pInputBuf, pOutputBuf);
}

void *MtkOmxVencEncodeThread(void *pData)
{
    MtkOmxVenc *pVenc = (MtkOmxVenc *)pData;
    #ifdef OMX_CHECK_DUMMY
    OMX_BOOL bGetDummyIdx = OMX_FALSE;
    #endif
#if ANDROID
    prctl(PR_SET_NAME, (unsigned long) "MtkOmxVencEncodeThread", 0, 0, 0);
#endif

    pVenc->mVencEncThreadTid = gettid();

    ALOGD("[0x%08x] ""MtkOmxVencEncodeThread created pVenc=0x%08X, tid=%d", pVenc, (unsigned int)pVenc, gettid());
    prctl(PR_SET_NAME, (unsigned long)"MtkOmxVencEncodeThread", 0, 0, 0);

    while (1)
    {
        //ALOGD ("[0x%08x] ""## Wait to encode (%d)", get_sem_value(&pVenc->mEncodeSem), pVenc);
        WAIT(pVenc->mEncodeSem);
        #ifdef OMX_CHECK_DUMMY
        bGetDummyIdx = OMX_FALSE;
        #endif
        pVenc->PriorityAdjustment();
        pVenc->watchdogTick = pVenc->getTickCountMs();
        if (OMX_FALSE == pVenc->mIsComponentAlive)
        {
            break;
        }

        if (pVenc->mEncodeStarted == OMX_FALSE)
        {
            ALOGD("[0x%08x] ""Wait for encode start.....", pVenc);
            SLEEP_MS(2);
            continue;
        }

        if (pVenc->mPortReconfigInProgress)
        {
            SLEEP_MS(2);
            ALOGD("[0x%08x] ""MtkOmxVencEncodeThread cannot encode when port re-config is in progress", pVenc);
            continue;
        }

        LOCK(pVenc->mEncodeLock);

        if (pVenc->CheckBufferAvailabilityAdvance(pVenc->mpVencInputBufQ, pVenc->mpVencOutputBufQ) == OMX_FALSE)
        {
            //ALOGD ("[0x%08x] ""No input avail...", pVenc);
            if (pVenc->mWaitPart == 1)
            {
                ALOGE("[0x%08x] ""it should not be here! (%d)", pVenc, pVenc->mPartNum);
                SIGNAL_COND(pVenc->mPartCond);
            }
            UNLOCK(pVenc->mEncodeLock);
            SLEEP_MS(1);
            sched_yield();
            continue;
        }

        // dequeue an input buffer
        int input_idx = pVenc->DequeueBufferAdvance(pVenc->mpVencInputBufQ);

        // dequeue an output buffer
        int output_idx = pVenc->DequeueBufferAdvance(pVenc->mpVencOutputBufQ);
        if (OMX_TRUE == pVenc->CheckNeedOutDummy()) //fix cts issue ALPS03040612 of miss last FBD
        {
            pVenc->mDummyIdx = pVenc->DequeueBufferAdvance(pVenc->mpVencOutputBufQ);
            #ifdef OMX_CHECK_DUMMY
            bGetDummyIdx = OMX_TRUE;
            #endif
        }

        if (!pVenc->allowEncodeVideo(input_idx, output_idx))
        {
            sched_yield();
            if (pVenc->mWaitPart == 1)
            {
                ALOGE("[0x%08x] ""in:%d out:%d , dummy[%d]part:%d", pVenc, input_idx, output_idx, pVenc->mDummyIdx, pVenc->mPartNum);
                SIGNAL_COND(pVenc->mPartCond);
            }
            UNLOCK(pVenc->mEncodeLock);
            continue;
        }
#ifdef OMX_CHECK_DUMMY
        if((bGetDummyIdx == OMX_TRUE) && (pVenc->mDummyIdx < 0))
        {
              sched_yield();
            ALOGE("Error :[0x%08x] ""in:%d out:%d , dummy[%d] God2 dummy Failure", pVenc, input_idx, output_idx, pVenc->mDummyIdx);
            ALOGE("Error :[0x%08x] ""in-size:%d out-size:%d", pVenc->mpVencOutputBufQ->Size(), pVenc->mpVencOutputBufQ->Size());
            if (pVenc->mWaitPart == 1)
            {
                ALOGE("[0x%08x] ""in:%d out:%d , dummy[%d]part:%d", pVenc, input_idx, output_idx, pVenc->mDummyIdx, pVenc->mPartNum);
                SIGNAL_COND(pVenc->mPartCond);
            }
            UNLOCK(pVenc->mEncodeLock);
        }
#endif
        //ALOGD ("[0x%08x] ""Encode [%d, %d] (0x%08X, 0x%08X)", pVenc, input_idx, output_idx,
        //(unsigned int)pVenc->mInputBufferHdrs[input_idx], (unsigned int)pVenc->mOutputBufferHdrs[output_idx]);

        // send the input/output buffers to encoder
        if (pVenc->mDoConvertPipeline)
        {
            pVenc->EncodeVideo(pVenc->mVencInputBufferHdrs[input_idx], pVenc->mOutputBufferHdrs[output_idx]);
        }
        else
        {
            OMX_BUFFERHEADERTYPE *pInputBuf = NULL;
            if (input_idx >= 0) {
                pInputBuf = pVenc->mInputBufferHdrs[input_idx];
            }
            pVenc->EncodeVideo(pInputBuf, pVenc->mOutputBufferHdrs[output_idx]);
        }
        UNLOCK(pVenc->mEncodeLock);
    }

    ALOGD("[0x%08x] ""MtkOmxVencEncodeThread terminated pVenc=0x%08X", pVenc, (unsigned int)pVenc);
    return NULL;
}

//Bruce 20130709 [
void *MtkOmxVencConvertThread(void *pData)
{
    MtkOmxVenc *pVenc = (MtkOmxVenc *)pData;

#if ANDROID
    prctl(PR_SET_NAME, (unsigned long) "MtkOmxVencConvertThread", 0, 0, 0);
#endif

    pVenc->mVencCnvtThreadTid = gettid();

    ALOGD("[0x%08x] ""MtkOmxVencConvertThread created pVenc=0x%08X, tid=%d", pVenc, (unsigned int)pVenc, gettid());
    prctl(PR_SET_NAME, (unsigned long)"MtkOmxVencConvertThread", 0, 0, 0);

    while (1)
    {
        //ALOGD ("[0x%08x] ""## Wait to decode (%d)", pVenc, get_sem_value(&pVenc->mEncodeSem));
        WAIT(pVenc->mConvertSem);

        if (OMX_FALSE == pVenc->mIsComponentAlive)
        {
            break;
        }

        if (pVenc->mEncodeStarted == OMX_FALSE)
        {
            ALOGD("[0x%08x] ""Wait for encode start.....", pVenc);
            SLEEP_MS(2);
            continue;
        }

        if (pVenc->mPortReconfigInProgress)
        {
            SLEEP_MS(2);
            ALOGD("[0x%08x] ""MtkOmxVencConvertThread cannot convert when port re-config is in progress", pVenc);
            continue;
        }

        if (pVenc->mConvertStarted == false)
        {
            SLEEP_MS(2);
            ALOGD("[0x%08x] ""Wait for convert start.....", pVenc);
            continue;
        }

        LOCK(pVenc->mConvertLock);

        if (pVenc->CheckBufferAvailabilityAdvance(pVenc->mpConvertInputBufQ, pVenc->mpConvertOutputBufQ) == OMX_FALSE)
        {
            //ALOGD ("[0x%08x] ""No input avail...", pVenc);
            UNLOCK(pVenc->mConvertLock);
            SLEEP_MS(1);
            sched_yield();
            continue;
        }

        // dequeue an input buffer
        int input_idx = pVenc->DequeueBufferAdvance(pVenc->mpConvertInputBufQ);

        // dequeue an output buffer
        int output_idx = pVenc->DequeueBufferAdvance(pVenc->mpConvertOutputBufQ);

        if ((input_idx < 0) || (output_idx < 0))
        {
            sched_yield();
            UNLOCK(pVenc->mConvertLock);
            continue;
        }

        // send the input/output buffers to encoder
        pVenc->ConvertVideo(pVenc->mInputBufferHdrs[input_idx], pVenc->mConvertOutputBufferHdrs[output_idx]);

        UNLOCK(pVenc->mConvertLock);
    }

    ALOGD("[0x%08x] ""MtkOmxVencConvertThread terminated pVenc=0x%08X", pVenc, (unsigned int)pVenc);


    return NULL;
}
// ]

void *MtkOmxVencThread(void *pData)
{
    MtkOmxVenc *pVenc = (MtkOmxVenc *)pData;

#if ANDROID
    prctl(PR_SET_NAME, (unsigned long) "MtkOmxVencThread", 0, 0, 0);
#endif

    ALOGD("[0x%08x] ""MtkOmxVencThread created pVenc=0x%08X", pVenc, (unsigned int)pVenc);
    prctl(PR_SET_NAME, (unsigned long)"MtkOmxVencThread", 0, 0, 0);

    pVenc->mVencThreadTid = gettid();

    int status;
    ssize_t ret;

    OMX_COMMANDTYPE cmd;
    OMX_U32 cmdCat;
    OMX_U32 nParam1;
    OMX_PTR pCmdData;

    struct pollfd PollFd;
    PollFd.fd = pVenc->mCmdPipe[MTK_OMX_PIPE_ID_READ];
    PollFd.events = POLLIN;
    unsigned int buffer_type;

    while (1)
    {
        status = poll(&PollFd, 1, -1);
        // WaitForSingleObject
        if (-1 == status)
        {
            ALOGE("[0x%08x] ""poll error %d (%s), fd:%d", pVenc, errno, strerror(errno),
                  pVenc->mCmdPipe[MTK_OMX_PIPE_ID_READ]);
            //dump fd
            ALOGE("[0x%08x] ""pipe: %d %d", pVenc, pVenc->mCmdPipe[MTK_OMX_PIPE_ID_READ],
                  pVenc->mCmdPipe[MTK_OMX_PIPE_ID_WRITE]);
            if (errno == 4) // error 4 (Interrupted system call)
            {
            }
            else
            {
                abort();
            }
        }
        else if (0 == status)   // timeout
        {
        }
        else
        {
            if (PollFd.revents & POLLIN)
            {
                READ_PIPE(cmdCat, pVenc->mCmdPipe);
                if (cmdCat == MTK_OMX_GENERAL_COMMAND)
                {
                    READ_PIPE(cmd, pVenc->mCmdPipe);
                    READ_PIPE(nParam1, pVenc->mCmdPipe);
                    ALOGD("[0x%08x] ""# Got general command (%s)", pVenc, CommandToString(cmd));
                    switch (cmd)
                    {
                        case OMX_CommandStateSet:
                            pVenc->HandleStateSet(nParam1);
                            break;

                        case OMX_CommandPortEnable:
                            pVenc->HandlePortEnable(nParam1);
                            break;

                        case OMX_CommandPortDisable:
                            pVenc->HandlePortDisable(nParam1);
                            break;

                        case OMX_CommandFlush:
                            pVenc->HandlePortFlush(nParam1);
                            break;

                        case OMX_CommandMarkBuffer:
                            READ_PIPE(pCmdData, pVenc->mCmdPipe);
                            pVenc->HandleMarkBuffer(nParam1, pCmdData);
                            break;

                        default:
                            ALOGE("[0x%08x] ""Error unhandled command", pVenc);
                            break;
                    }
                }
                else if (cmdCat == MTK_OMX_BUFFER_COMMAND)
                {
                    OMX_BUFFERHEADERTYPE *pBufHead;
                    READ_PIPE(buffer_type, pVenc->mCmdPipe);
                    READ_PIPE(pBufHead, pVenc->mCmdPipe);
                    switch (buffer_type)
                    {
                        case MTK_OMX_EMPTY_THIS_BUFFER_TYPE:
                            //ALOGD ("[0x%08x] ""## EmptyThisBuffer pBufHead(0x%08X)", pVenc, pBufHead);
                            //handle input buffer from IL client
                            pVenc->HandleEmptyThisBuffer(pBufHead);
                            break;
                        case MTK_OMX_FILL_THIS_BUFFER_TYPE:
                            //ALOGD ("[0x%08x] ""## FillThisBuffer pBufHead(0x%08X)", pVenc, pBufHead);
                            // handle output buffer from IL client
                            pVenc->HandleFillThisBuffer(pBufHead);
                            break;
                        default:
                            break;
                    }
                }
                else if (cmdCat == MTK_OMX_STOP_COMMAND)
                {
                    // terminate
                    break;
                }
            }
            else
            {
                ALOGE("[0x%08x] ""Poll get unsupported event:0x%x", pVenc, PollFd.revents);
            }
        }

    }

EXIT:
    ALOGD("[0x%08x] ""MtkOmxVencThread terminated", pVenc);
    return NULL;
}

OMX_BOOL MtkOmxVenc::mEnableMoreLog = OMX_TRUE;

void *MtkOmxVencWatchdogThread(void *pData)
{
    MtkOmxVenc *pVenc = (MtkOmxVenc *)pData;

#if ANDROID
    prctl(PR_SET_NAME, (unsigned long) "MtkOmxVencWatchdogThread", 0, 0, 0);
#endif

    pVenc->mVencCnvtThreadTid = gettid();

    ALOGD("[0x%08x] ""MtkOmxVencWatchdogThread created pVenc=0x%08X, tid=%d",
          pVenc, (unsigned int)pVenc, gettid());
#ifdef CONFIG_MT_ENG_BUILD
        usleep(2000000);
#else
        usleep(500000);
#endif

    while (1)
    {
#ifdef CONFIG_MT_ENG_BUILD
        usleep(pVenc->watchdogTimeout*2);
#else
        usleep(pVenc->watchdogTimeout);
#endif

        if (OMX_FALSE == pVenc->mIsComponentAlive)
        {
            break;
        }
        if (pVenc->mHaveAVCHybridPlatform &&
             pVenc->mIsHybridCodec &&
            (pVenc->getTickCountMs() - pVenc->watchdogTick >= pVenc->watchdogTimeout) &&
            (OMX_FALSE == pVenc->mIsTimeLapseMode))
        {
            LOCK(pVenc->mEncodeLock);
            if (pVenc->mLastFrameBufHdr != NULL && !(pVenc->mLastFrameBufHdr->nFlags & OMX_BUFFERFLAG_TRIGGER_OUTPUT))
            {
                ALOGW("[0x%08x]""Watchdog timeout! %d ms,pVenc->mDoConvertPipeline is %d", pVenc, pVenc->watchdogTimeout,pVenc->mDoConvertPipeline);
                if(!pVenc->mDoConvertPipeline){
                    pVenc->mLastFrameBufHdr->nFlags |= OMX_BUFFERFLAG_TRIGGER_OUTPUT;
                    pVenc->HandleEmptyThisBuffer(pVenc->mLastFrameBufHdr);
                }
            }
            UNLOCK(pVenc->mEncodeLock);
        }else if(pVenc->getTickCountMs() - pVenc->watchdogTick >= pVenc->watchdogTimeout &&
            OMX_FALSE == pVenc->mIsTimeLapseMode)
        {
            LOCK(pVenc->mEncodeLock);
            if (pVenc->mLastFrameBufHdr != NULL)
            {
                ALOGW("[0x%08x] ""Watchdog timeout! %d ms", pVenc, pVenc->watchdogTimeout);
                pVenc->HandleEmptyBufferDone(pVenc->mLastFrameBufHdr);
                pVenc->mLastFrameBufHdr = NULL;
            }
            pVenc->ReturnPendingInputBuffers();
            UNLOCK(pVenc->mEncodeLock);
        }
    }

EXIT:
    ALOGD("[0x%08x] ""MtkOmxVencWatchdogThread terminated", pVenc);
    return NULL;
}

MtkOmxVenc::MtkOmxVenc()
{
    mEnableMoreLog = (OMX_BOOL) MtkVenc::EnableMoreLog();
    MTK_OMX_LOGD_ENG("MtkOmxVenc::MtkOmxVenc this= 0x%08X", (unsigned int)this);
    MTK_OMX_MEMSET(&mCompHandle, 0x00, sizeof(OMX_COMPONENTTYPE));
    mCompHandle.nSize = sizeof(OMX_COMPONENTTYPE);
    mCompHandle.pComponentPrivate = this;
    mState = OMX_StateInvalid;

    mInputBufferHdrs = NULL;
    mOutputBufferHdrs = NULL;
    mInputBufferPopulatedCnt = 0;
    mOutputBufferPopulatedCnt = 0;
    mPendingStatus = 0;
    mEncodeStarted = OMX_FALSE;
    mPortReconfigInProgress = OMX_FALSE;

    mEncoderInitCompleteFlag = OMX_FALSE;
    mDrvHandle = (unsigned int)NULL;

    mHeaderLen = 0;
    mFrameCount = 0;

    /* for DirectLink Meta Mode + */
    mSeqHdrEncoded = OMX_FALSE;
    /* for DirectLink Meta Mode - */

    //#ifndef MT6573_MFV_HW
    mLastFrameBufHdr = NULL;
    mLastBsBufHdr = NULL;
    //#endif

#ifdef ANDROID_ICS
    mForceIFrame = OMX_FALSE;
    mIsTimeLapseMode = OMX_FALSE;
    mIsWhiteboardEffectMode = OMX_FALSE;
    mIsMCIMode = OMX_FALSE;
    // Morris Yang 20120214 add for live effect recording [
    mStoreMetaDataInBuffers = OMX_FALSE;
    mCnvtBuffer = NULL;
    mCnvtBufferSize = 0;
    // ]

    mBitRateUpdated = OMX_FALSE;
    mFrameRateUpdated = OMX_FALSE;

#endif

    mIsClientLocally = OMX_TRUE;

    mInputMVAMgr = new OmxMVAManager("ion");
    mOutputMVAMgr = new OmxMVAManager("ion");
    mCnvtMVAMgr = new OmxMVAManager("ion");

    mLastFrameTimeStamp = 0;

    m3DVideoRecordMode = OMX_VIDEO_H264FPA_NONE;// for MTK S3D SUPPORT

    mCodecId = MTK_VENC_CODEC_ID_INVALID;

    INIT_MUTEX(mCmdQLock);
    INIT_MUTEX(mEncodeLock);

    INIT_SEMAPHORE(mInPortAllocDoneSem);
    INIT_SEMAPHORE(mOutPortAllocDoneSem);
    INIT_SEMAPHORE(mInPortFreeDoneSem);
    INIT_SEMAPHORE(mOutPortFreeDoneSem);
    INIT_SEMAPHORE(mEncodeSem);

    INIT_COND(mPartCond);
    mPartNum = 0;
    mWaitPart = 0;

    //Bruce 20130709 [
    mEmptyThisBufQ.mId = MtkOmxBufQ::MTK_OMX_VENC_BUFQ_INPUT;
    mFillThisBufQ.mId = MtkOmxBufQ::MTK_OMX_VENC_BUFQ_OUTPUT;

    mDoConvertPipeline = false;
    mConvertOutputBufQ.mId = MtkOmxBufQ::MTK_OMX_VENC_BUFQ_CONVERT_OUTPUT;
    mVencInputBufQ.mId = MtkOmxBufQ::MTK_OMX_VENC_BUFQ_VENC_INPUT;
    mpConvertInputBufQ = NULL;
    mpConvertOutputBufQ = NULL;
    mpVencInputBufQ = &mEmptyThisBufQ;
    mpVencOutputBufQ = &mFillThisBufQ;

    mVencInputBufferHdrs = NULL;
    mConvertOutputBufferHdrs = NULL;

    mConvertStarted = false;
    // ]

    mIInterval = 0;
    mSetIInterval = OMX_FALSE;

    mSetWFDMode = OMX_FALSE;
    mWFDMode = OMX_FALSE;
    mSetStreamingMode = OMX_FALSE;

    mScaledWidth = 0;
    mScaledHeight = 0;
    mSkipFrame = 0;
    mDumpFlag = 0;

    {
        mInputScalingMode = (OMX_BOOL) MtkVenc::InputScalingMode();
        mMaxScaledWide = (OMX_U32) MtkVenc::MaxScaledWide("1920");
        mMaxScaledNarrow = (OMX_U32) MtkVenc::MaxScaledNarrow("1088");
        mMaxScaledWide = (mMaxScaledWide > 0) ?  mMaxScaledWide:1920;
        mMaxScaledNarrow = (mMaxScaledNarrow > 0) ?  mMaxScaledNarrow:1088;
    }

    mDrawStripe = (bool) MtkVenc::DrawStripe();
    mDumpInputFrame = (OMX_BOOL) MtkVenc::DumpInputFrame();
    mDumpCts = (OMX_BOOL) MtkVenc::DumpCts();
    mRTDumpInputFrame = (OMX_BOOL) MtkVenc::RTDumpInputFrame("1");
    mDumpColorConvertFrame = (OMX_BOOL) MtkVenc::DumpColorConvertFrame();
    mDumpCCNum = MtkVenc::DumpCCNum("5");

    {
        mEnableDummy = (OMX_BOOL) MtkVenc::EnableDummy("1");
        mDumpDLBS = (OMX_BOOL) MtkVenc::DumpDLBS();
        mIsMtklog = (OMX_BOOL) MtkVenc::IsMtklog();
        watchdogTimeout = MtkVenc::WatchdogTimeout("2000");
    }

    {
        if (MtkVenc::DumpSecureInputFlag())
        {
            mDumpFlag |= DUMP_SECURE_INPUT_Flag;
        }
        if (MtkVenc::DumpSecureTmpInFlag())
        {
            mDumpFlag |= DUMP_SECURE_TMP_IN_Flag;
        }
        if (MtkVenc::DumpSecureOutputFlag())
        {
            mDumpFlag |= DUMP_SECURE_OUTPUT_Flag;
        }
        if (MtkVenc::DumpSecureYv12Flag())
        {
            mDumpFlag |= DUMP_YV12_Flag;
        }
        MTK_OMX_LOGD("dump flag=0x%x", mDumpFlag);
    }

    mCoreGlobal = NULL;
    //Bruce 20130709 [
    INIT_SEMAPHORE(mConvertSem);
    INIT_MUTEX(mConvertLock);

    mInitPipelineBuffer = false;
    // ]
    mDrawBlack = OMX_FALSE;//for Miracast test case SIGMA 5.1.11 workaround


    mSendDummyNAL = false;
    mDummyIdx = -1;

    mIsLivePhoto = false;

    mTmpColorConvertBuf = NULL;
    mTmpColorConvertBufSize = 0;

    mPrependSPSPPSToIDRFrames = OMX_FALSE;
    mPrependSPSPPSToIDRFramesNotify = OMX_FALSE;

    mStoreMetaDataInOutBuffers = OMX_FALSE;

    mInputAllocateBuffer = OMX_FALSE;
    mOutputAllocateBuffer = OMX_FALSE;
    mVencAdjustPriority = 1;

    mIsSecureSrc = OMX_FALSE;
    mIsSecureInst = OMX_FALSE;
    mTestSecInputHandle = 0xffffffff;
    bHasSecTlc = false;

    memset(&mBitStreamBuf, 0, sizeof(mBitStreamBuf));
    memset(&mFrameBuf, 0, sizeof(mFrameBuf));
    mReconfigCount = 0;
    mCnvtPortReconfigInProgress = OMX_FALSE;

    mEnableNonRefP = OMX_FALSE;

    mInputMetaDataFormat = 0;
    mGrallocWStride = 0xFFFFFFFF;

    memset(&mEncDrvSetting, 0, sizeof(mEncDrvSetting));
    memset(&mExtraEncDrvSetting, 0, sizeof(mExtraEncDrvSetting));

    mIsHybridCodec = OMX_FALSE;

    mTeeEncType = NONE_TEE;

    mSetQP = OMX_FALSE;
    mIsMultiSlice = OMX_FALSE;
    mIsViLTE = (OMX_BOOL) MtkVenc::IsViLTE();
    mETBDebug = true;
#ifdef MTK_DUM_SEC_ENC
    pTmp_buf = 0;
    Tmp_bufsz =0;
#endif
    mCmdPipe[0] = -1;
    mCmdPipe[1] = -1;
    mIsCrossMount= false;
    mSetConstantBitrateMode = OMX_FALSE;

    mAVPFEnable = (OMX_BOOL) MtkVenc::AVPFEnable();
    mGotSLI = OMX_FALSE;
    mForceFullIFrame = OMX_FALSE;
    mForceFullIFramePrependHeader = OMX_FALSE;
    mIDRInterval = 0;
    mOperationRate = 0;
    mSetIDRInterval = OMX_FALSE;
    mbYUV420FlexibleMode = OMX_FALSE;

    mPrepareToResolutionChange = OMX_FALSE;
    u4EncodeWidth = 0;
    u4EncodeHeight = 0;
    nFilledLen = 0;
    mLastTimeStamp = 0;
    mSlotBitCount = 0;
    mIDRIntervalinSec = 0;
    mLastIDRTimeStamp = 0;

    mMeetHybridEOS = OMX_FALSE;
    mHaveAVCHybridPlatform = OMX_FALSE;
#ifdef SUPPORT_NATIVE_HANDLE
    mIsAllocateOutputNativeBuffers = OMX_FALSE;
    mIonDevFd = -1;
    mStoreMetaOutNativeHandle.clear();
    mIonBufferInfo.clear();
    mIsChangeBWC4WFD = OMX_FALSE;
    mRecordBitstream  = (OMX_BOOL) MtkVenc::RecordBitstream();
    mWFDLoopbackMode  = (OMX_BOOL) MtkVenc::WFDLoopbackMode();
    MTK_OMX_LOGD_ENG("mRecordBitstream %d, mWFDLoopbackMode %d", mRecordBitstream, mWFDLoopbackMode);
#endif
}

MtkOmxVenc::~MtkOmxVenc()
{
    MTK_OMX_LOGD("~MtkOmxVenc this= 0x%08X", (unsigned int)this);
#ifdef SUPPORT_NATIVE_HANDLE
    for (int i = 0; i < mStoreMetaOutNativeHandle.size(); ++i)
    {
        MTK_OMX_LOGE("[WARNING] Freeing 0x%x in deinit", mStoreMetaOutNativeHandle[i]);
        MTK_OMX_FREE(mStoreMetaOutNativeHandle[i]);
    }
    mStoreMetaOutNativeHandle.clear();
    if (mIonDevFd > 0)
    {
        for (int i = 0; i < mIonBufferInfo.size(); ++i)
        {
            MTK_OMX_LOGE("[WARNING] Freeing ion in deinit fd %d va %p handle %p",
                mIonBufferInfo[i].ion_share_fd, mIonBufferInfo[i].va, mIonBufferInfo[i].ion_handle);
#ifdef COPY_2_CONTIG
            MTK_OMX_LOGE("[WARNING] Freeing ion in deinit fd %d va %p handle %p",
                mIonBufferInfo[i].ion_share_fd_4_enc, mIonBufferInfo[i].va_4_enc, mIonBufferInfo[i].ion_handle_4_enc);
#endif
            if (0 == mIonBufferInfo[i].secure_handle)
            {
                if (NULL != mIonBufferInfo[i].va)
                {
                    ion_munmap(mIonDevFd, mIonBufferInfo[i].va, mIonBufferInfo[i].value[0]);
                }
#ifdef COPY_2_CONTIG
                if (NULL != mIonBufferInfo[i].va_4_enc)
                {
                    ion_munmap(mIonDevFd, mIonBufferInfo[i].va_4_enc, mIonBufferInfo[i].value[0]);
                }
#endif
            }
            ion_share_close(mIonDevFd, mIonBufferInfo[i].ion_share_fd);
            ion_free(mIonDevFd, mIonBufferInfo[i].ion_handle);
#ifdef COPY_2_CONTIG
            ion_share_close(mIonDevFd, mIonBufferInfo[i].ion_share_fd_4_enc);
            ion_free(mIonDevFd, mIonBufferInfo[i].ion_handle_4_enc);
#endif
        }
        mIonBufferInfo.clear();
        close(mIonDevFd);
        mIonDevFd = -1;
    }
#endif

    if (mInputBufferHdrs)
    {
        MTK_OMX_FREE(mInputBufferHdrs);
    }

    if (mOutputBufferHdrs)
    {
        MTK_OMX_FREE(mOutputBufferHdrs);
    }

    //Bruce 20130709 [
    if (mDoConvertPipeline)
    {
        DeinitPipelineBuffer();
    }
    else
    {
        DeinitConvertBuffer();
    }
    // ]

    delete mCnvtMVAMgr;
    delete mOutputMVAMgr;
    delete mInputMVAMgr;

    DESTROY_MUTEX(mEncodeLock);
    DESTROY_MUTEX(mCmdQLock);

    DESTROY_SEMAPHORE(mInPortAllocDoneSem);
    DESTROY_SEMAPHORE(mOutPortAllocDoneSem);
    DESTROY_SEMAPHORE(mInPortFreeDoneSem);
    DESTROY_SEMAPHORE(mOutPortFreeDoneSem);
    DESTROY_SEMAPHORE(mEncodeSem);

    DESTROY_COND(mPartCond);
    //Bruce 20130709 [
    DESTROY_SEMAPHORE(mConvertSem);
    DESTROY_MUTEX(mConvertLock);
    // ]

    if (mTmpColorConvertBuf != NULL)
    {
        free(mTmpColorConvertBuf);
    }
#ifdef MTK_DUM_SEC_ENC
    if(pTmp_buf && Tmp_bufsz)
    {
        MTK_OMX_LOGE("MtkOmxVenc :Free sec dump tmp buffer\n");
        free(pTmp_buf);
        pTmp_buf  =0;
        Tmp_bufsz =0;
    }
#endif
    DeInitSecEncParams();
}

OMX_ERRORTYPE MtkOmxVenc::ComponentInit(OMX_IN OMX_HANDLETYPE hComponent,
                                        OMX_IN OMX_STRING componentName)
{
    IN_FUNC();
    (void)(hComponent);
    OMX_ERRORTYPE err = OMX_ErrorNone;
    MTK_OMX_LOGD("MtkOmxVenc::ComponentInit (%s)", componentName);
    mState = OMX_StateLoaded;
    int ret;

    // query chip name
    mChipName = VAL_CHIP_NAME_MT6797;
    /*if (VENC_DRV_MRESULT_FAIL == eVEncDrvQueryCapability(VENC_DRV_QUERY_TYPE_CHIP_NAME, VAL_NULL, &mChipName))
    {
        MTK_OMX_LOGE("[ERROR] Cannot get encoder property, VENC_DRV_QUERY_TYPE_CHIP_NAME");
        OUT_FUNC();
        return err
    }*/

    InitOMXParams(&mInputPortDef);
    InitOMXParams(&mOutputPortDef);

    if (!strcmp(componentName, MTK_OMX_AVC_ENCODER) ||
        !strcmp(componentName, MTK_OMX_AVC_SEC_ENCODER))
    {
        if (OMX_FALSE == InitAvcEncParams())
        {
            err = OMX_ErrorInsufficientResources;
            MTK_OMX_LOGE("InitAvcEncParams 2 failed ");
            OUT_FUNC();return err;
        }
        mCodecId = MTK_VENC_CODEC_ID_AVC;
        MTK_OMX_LOGD("%s init sec 2 mIsSecureInst %d", __FUNCTION__,mIsSecureInst);
#ifdef SUPPORT_NATIVE_HANDLE
        if (!strcmp(componentName, MTK_OMX_AVC_SEC_ENCODER))
        {
            BWC bwc;
            bwc.Profile_Change(BWCPT_VIDEO_WIFI_DISPLAY, true);
            MTK_OMX_LOGD("enter WFD BWCPT_VIDEO_WIFI_DISPLAY");
            mIsChangeBWC4WFD = OMX_TRUE;
        }
#endif
    }
    else if (!strcmp(componentName, MTK_OMX_HEVC_ENCODER))
    {
        if (OMX_FALSE == InitHevcEncParams())
        {
            err = OMX_ErrorInsufficientResources;
            OUT_FUNC();
            return err;
        }
        mCodecId = MTK_VENC_CODEC_ID_HEVC;
    }
    else if (!strcmp(componentName, MTK_OMX_VP8_ENCODER))
    {
        if (OMX_FALSE == InitVP8EncParams())
        {
            err = OMX_ErrorInsufficientResources;
            OUT_FUNC();
            return err;
        }
        mCodecId = MTK_VENC_CODEC_ID_VP8;
    }
    else if (!strcmp(componentName, MTK_OMX_H263_ENCODER))
    {
        if (OMX_FALSE == InitH263EncParams())
        {
            err = OMX_ErrorInsufficientResources;
            OUT_FUNC();
            return err;
        }
        mCodecId = MTK_VENC_CODEC_ID_MPEG4_SHORT;
    }
    else if (!strcmp(componentName, MTK_OMX_MPEG4_ENCODER))
    {
        if (OMX_FALSE == InitMpeg4EncParams())
        {
            err = OMX_ErrorInsufficientResources;
            OUT_FUNC();
            return err;
        }
        mCodecId = MTK_VENC_CODEC_ID_MPEG4;
    }
    else
    {
        MTK_OMX_LOGE("MtkOmxVenc::ComponentInit ERROR: Unknown component name");
        err = OMX_ErrorBadParameter;
        OUT_FUNC();
        return err;
    }

    {
        VENC_DRV_MRESULT_T mReturn = VENC_DRV_MRESULT_OK;
        OMX_U32 uMpeg4SWPlatform = 0;
        OMX_U32 uAVCHybridPlatform = 0; // to VENC_DRV_GET_TYPE_* v4l2
        //mReturn = eVEncDrvGetParam((VAL_HANDLE_T)NULL, VENC_DRV_GET_TYPE_MPEG4_SW_PLATFORM, (VAL_VOID_T *)&mChipName, (VAL_VOID_T *)&uMpeg4SWPlatform);
        //mReturn = eVEncDrvGetParam((VAL_HANDLE_T)NULL, VENC_DRV_GET_TYPE_AVC_HYBRID_PLATFORM, (VAL_VOID_T *)&mChipName, (VAL_VOID_T *)&uAVCHybridPlatform);
        MTK_OMX_LOGD_ENG("uMpeg4SWPlatform %d, uAVCHybridPlatform %d", uMpeg4SWPlatform, uAVCHybridPlatform);

        if (((mCodecId == MTK_VENC_CODEC_ID_MPEG4 || mCodecId == MTK_VENC_CODEC_ID_MPEG4_SHORT) && uMpeg4SWPlatform)||
            (mCodecId == MTK_VENC_CODEC_ID_AVC && uAVCHybridPlatform))
        {
            mHaveAVCHybridPlatform = (OMX_BOOL)uAVCHybridPlatform;
            mIsHybridCodec = OMX_TRUE;
        }
    }
    // query input color format
    VENC_DRV_YUV_FORMAT_T yuvFormat = VENC_DRV_YUV_FORMAT_YV12;
    //if (VENC_DRV_MRESULT_OK == eVEncDrvGetParam((VAL_HANDLE_T)NULL, VENC_DRV_GET_TYPE_GET_YUV_FORMAT, NULL, &yuvFormat))
    {
        switch (yuvFormat)
        {
            case VENC_DRV_YUV_FORMAT_420:
                mInputPortFormat.eColorFormat = OMX_COLOR_FormatYUV420Planar;
                mInputPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
                break;

            case VENC_DRV_YUV_FORMAT_YV12:
                mInputPortFormat.eColorFormat = OMX_MTK_COLOR_FormatYV12;
                mInputPortDef.format.video.eColorFormat = OMX_MTK_COLOR_FormatYV12;
                break;

            default:
                break;
        }
    }
    //else //compile error
    //{
    //    MTK_OMX_LOGE("ERROR: query VENC_DRV_GET_TYPE_GET_YUV_FORMAT failed");
    //}

    InitOMXParams(&mAvcType);
    InitOMXParams(&mH263Type);
    InitOMXParams(&mMpeg4Type);
    InitOMXParams(&mVp8Type);
    InitOMXParams(&mBitrateType);
    InitOMXParams(&mQuantizationType);
    InitOMXParams(&mVbsmcType);
    InitOMXParams(&mMvType);
    InitOMXParams(&mIntraRefreshType);
    InitOMXParams(&mAvcSliceFMO);
    InitOMXParams(&mErrorCorrectionType);
    InitOMXParams(&mProfileLevelType);
    InitOMXParams(&mFrameRateType);
    InitOMXParams(&mConfigBitrate);
    InitOMXParams(&mConfigIntraRefreshVopType);
    InitOMXParams(&mConfigAVCIntraPeriod);
    InitOMXParams(&mLayerParams);
    InitOMXParams(&mLayerConfig);

    // create command pipe
    ret = pipe(mCmdPipe);
    if (ret)
    {
        MTK_OMX_LOGE("mCmdPipe creation failure");
        err = OMX_ErrorInsufficientResources;
        OUT_FUNC();
        return err;
    }

    mIsComponentAlive = OMX_TRUE;

    //MTK_OMX_LOGD ("mCmdPipe[0] = %d", mCmdPipe[0]);
    // create Venc thread
    ret = pthread_create(&mVencThread, NULL, &MtkOmxVencThread, (void *)this);
    if (ret)
    {
        MTK_OMX_LOGE("MtkOmxVencThread creation failure");
        err = OMX_ErrorInsufficientResources;
        OUT_FUNC();
        return err;
    }

    // create video encoding thread
    ret = pthread_create(&mVencEncodeThread, NULL, &MtkOmxVencEncodeThread, (void *)this);
    if (ret)
    {
        MTK_OMX_LOGE("MtkOmxVencEncodeThread creation failure");
        err = OMX_ErrorInsufficientResources;
        OUT_FUNC();
        return err;
    }

    // create color convert thread
    ret = pthread_create(&mVencConvertThread, NULL, &MtkOmxVencConvertThread, (void *)this);
    if (ret)
    {
        MTK_OMX_LOGE("MtkOmxVencConvertThread creation failure");
        err = OMX_ErrorInsufficientResources;
        OUT_FUNC();
        return err;
    }

    // create watchdog thread
    if (mIsHybridCodec == OMX_TRUE)
    {
        ret = pthread_create(&mVencWatchdogThread, NULL, &MtkOmxVencWatchdogThread, (void *)this);
        if (ret)
        {
            MTK_OMX_LOGE("MtkOmxVencWatchdogThread creation failure");
            err = OMX_ErrorInsufficientResources;
            OUT_FUNC();
            return err;
        }
    }

EXIT:
    OUT_FUNC();
    return err;
}


OMX_ERRORTYPE MtkOmxVenc::ComponentDeInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    (void)(hComponent);
    MTK_OMX_LOGD("+MtkOmxVenc::ComponentDeInit");
    OMX_ERRORTYPE err = OMX_ErrorNone;
    ssize_t ret = 0;
#ifdef SUPPORT_NATIVE_HANDLE
    if (OMX_TRUE == mIsChangeBWC4WFD)
    {
        BWC bwc;
        bwc.Profile_Change(BWCPT_VIDEO_WIFI_DISPLAY, false);
        MTK_OMX_LOGD("leave WFD BWCPT_VIDEO_WIFI_DISPLAY  !");
        mIsChangeBWC4WFD = OMX_FALSE;
    }
#endif

    // terminate decode thread
    mIsComponentAlive = OMX_FALSE;
    SIGNAL(mEncodeSem);

    // terminate convert thread
    SIGNAL(mConvertSem);

    // terminate command thread
    OMX_U32 CmdCat = MTK_OMX_STOP_COMMAND;
    if (mCmdPipe[MTK_OMX_PIPE_ID_WRITE] > -1)
    {
        WRITE_PIPE(CmdCat, mCmdPipe);
    }

    if (IS_PENDING(MTK_OMX_IN_PORT_ENABLE_PENDING))
    {
        SIGNAL(mInPortAllocDoneSem);
    }
    if (IS_PENDING(MTK_OMX_OUT_PORT_ENABLE_PENDING))
    {
        SIGNAL(mOutPortAllocDoneSem);
        MTK_OMX_LOGD("signal mOutPortAllocDoneSem (%d)", get_sem_value(&mOutPortAllocDoneSem));
    }
    if (IS_PENDING(MTK_OMX_IDLE_PENDING))
    {
        SIGNAL(mInPortAllocDoneSem);
        MTK_OMX_LOGD("signal mInPortAllocDoneSem (%d)", get_sem_value(&mInPortAllocDoneSem));
        SIGNAL(mOutPortAllocDoneSem);
        MTK_OMX_LOGD("signal mOutPortAllocDoneSem (%d)", get_sem_value(&mOutPortAllocDoneSem));
    }

    if (!pthread_equal(pthread_self(), mVencConvertThread))
    {
        // wait for mVencConvertThread terminate
        pthread_join(mVencConvertThread, NULL);
    }
    if (mIsHybridCodec == OMX_TRUE)
    {
        if (!pthread_equal(pthread_self(), mVencWatchdogThread))
        {
            // wait for mVencWatchdogThread terminate
            pthread_join(mVencWatchdogThread, NULL);
        }
    }
#if 1
    if (!pthread_equal(pthread_self(), mVencEncodeThread))
    {
        // wait for mVencEncodeThread terminate
        pthread_join(mVencEncodeThread, NULL);
    }

    if (!pthread_equal(pthread_self(), mVencThread))
    {
        // wait for mVencThread terminate
        pthread_join(mVencThread, NULL);
    }
#endif

    if (NULL != mCoreGlobal)
    {
        ((mtk_omx_core_global *)mCoreGlobal)->video_instance_count--;
    }

    if (mCmdPipe[MTK_OMX_PIPE_ID_READ] > -1)
    {
        close(mCmdPipe[MTK_OMX_PIPE_ID_READ]);
    }
    if (mCmdPipe[MTK_OMX_PIPE_ID_WRITE] > -1)
    {
        close(mCmdPipe[MTK_OMX_PIPE_ID_WRITE]);
    }

    MTK_OMX_LOGD("-MtkOmxVenc::ComponentDeInit");

EXIT:
    return err;
}

OMX_ERRORTYPE MtkOmxVenc::GetComponentVersion(OMX_IN OMX_HANDLETYPE hComponent,
                                              OMX_IN OMX_STRING componentName,
                                              OMX_OUT OMX_VERSIONTYPE *componentVersion,
                                              OMX_OUT OMX_VERSIONTYPE *specVersion,
                                              OMX_OUT OMX_UUIDTYPE *componentUUID)

{
    (void)(hComponent);
    (void)(componentName);
    (void)(componentUUID);
    OMX_ERRORTYPE err = OMX_ErrorNone;

    MTK_OMX_LOGD_ENG("MtkOmxVenc::GetComponentVersion");

    componentVersion->s.nVersionMajor = 1;
    componentVersion->s.nVersionMinor = 1;
    componentVersion->s.nRevision = 2;
    componentVersion->s.nStep = 0;
    specVersion->s.nVersionMajor = 1;
    specVersion->s.nVersionMinor = 1;
    specVersion->s.nRevision = 2;
    specVersion->s.nStep = 0;

    return err;
}

OMX_ERRORTYPE MtkOmxVenc::SendCommand(OMX_IN OMX_HANDLETYPE hComponent,
                                      OMX_IN OMX_COMMANDTYPE Cmd,
                                      OMX_IN OMX_U32 nParam1,
                                      OMX_IN OMX_PTR pCmdData)
{
    IN_FUNC();
    (void)(hComponent);
    OMX_ERRORTYPE err = OMX_ErrorNone;

    MTK_OMX_LOGD_ENG("MtkOmxVenc::SendCommand cmd=%s", CommandToString(Cmd));

    OMX_U32 CmdCat = MTK_OMX_GENERAL_COMMAND;

    ssize_t ret = 0;

    LOCK(mCmdQLock);

    if (mState == OMX_StateInvalid)
    {
        UNLOCK(mCmdQLock);
        OUT_FUNC();
        return OMX_ErrorInvalidState;
    }

    switch (Cmd)
    {
        case OMX_CommandStateSet:   // write 8 bytes to pipe [cmd][nParam1]
            if (nParam1 == OMX_StateIdle)
            {
                MTK_OMX_LOGD_ENG("set MTK_OMX_VENC_IDLE_PENDING");
                SET_PENDING(MTK_OMX_IDLE_PENDING);
            }
            else if (nParam1 == OMX_StateLoaded)
            {
                MTK_OMX_LOGD_ENG("set MTK_OMX_VENC_LOADED_PENDING");
                SET_PENDING(MTK_OMX_LOADED_PENDING);
            }
            WRITE_PIPE(CmdCat, mCmdPipe);
            WRITE_PIPE(Cmd, mCmdPipe);
            WRITE_PIPE(nParam1, mCmdPipe);
            break;

        case OMX_CommandPortDisable:
            if ((nParam1 != MTK_OMX_INPUT_PORT) && (nParam1 != MTK_OMX_OUTPUT_PORT) && (nParam1 != MTK_OMX_ALL_PORT))
            {
                UNLOCK(mCmdQLock);
                OUT_FUNC();
                return OMX_ErrorBadParameter;
            }

            // mark the ports to be disabled first, p.84
            if (nParam1 == MTK_OMX_INPUT_PORT || nParam1 == MTK_OMX_ALL_PORT)
            {
                mInputPortDef.bEnabled = OMX_FALSE;
            }

            if (nParam1 == MTK_OMX_OUTPUT_PORT || nParam1 == MTK_OMX_ALL_PORT)
            {
                mOutputPortDef.bEnabled = OMX_FALSE;
            }

            WRITE_PIPE(CmdCat, mCmdPipe);
            WRITE_PIPE(Cmd, mCmdPipe);
            WRITE_PIPE(nParam1, mCmdPipe);
            break;

        case OMX_CommandPortEnable:
            if ((nParam1 != MTK_OMX_INPUT_PORT) && (nParam1 != MTK_OMX_OUTPUT_PORT) && (nParam1 != MTK_OMX_ALL_PORT))
            {
                UNLOCK(mCmdQLock);
                OUT_FUNC();
                return OMX_ErrorBadParameter;
            }

            // mark the ports to be enabled first, p.85
            if (nParam1 == MTK_OMX_INPUT_PORT || nParam1 == MTK_OMX_ALL_PORT)
            {
                mInputPortDef.bEnabled = OMX_TRUE;

                if ((mState != OMX_StateLoaded) && (mInputPortDef.bPopulated == OMX_FALSE))
                {
                    SET_PENDING(MTK_OMX_IN_PORT_ENABLE_PENDING);
                }
            }

            if (nParam1 == MTK_OMX_OUTPUT_PORT || nParam1 == MTK_OMX_ALL_PORT)
            {
                mOutputPortDef.bEnabled = OMX_TRUE;

                if ((mState != OMX_StateLoaded) && (mOutputPortDef.bPopulated == OMX_FALSE))
                {
                    //MTK_OMX_LOGD ("SET_PENDING(MTK_OMX_VENC_OUT_PORT_ENABLE_PENDING) mState(%d)", mState);
                    SET_PENDING(MTK_OMX_OUT_PORT_ENABLE_PENDING);
                }
            }

            WRITE_PIPE(CmdCat, mCmdPipe);
            WRITE_PIPE(Cmd, mCmdPipe);
            WRITE_PIPE(nParam1, mCmdPipe);
            break;

        case OMX_CommandFlush:  // p.84
            if ((nParam1 != MTK_OMX_INPUT_PORT) && (nParam1 != MTK_OMX_OUTPUT_PORT) && (nParam1 != MTK_OMX_ALL_PORT))
            {
                UNLOCK(mCmdQLock);
                OUT_FUNC();
                return OMX_ErrorBadParameter;
            }
            WRITE_PIPE(CmdCat, mCmdPipe);
            WRITE_PIPE(Cmd, mCmdPipe);
            WRITE_PIPE(nParam1, mCmdPipe);
            break;

        case OMX_CommandMarkBuffer:    // write 12 bytes to pipe [cmd][nParam1][pCmdData]
            WRITE_PIPE(CmdCat, mCmdPipe);
            WRITE_PIPE(Cmd, mCmdPipe);
            WRITE_PIPE(nParam1, mCmdPipe);
            WRITE_PIPE(pCmdData, mCmdPipe);
            break;

        default:
            MTK_OMX_LOGE("[ERROR] Unknown command(0x%08X)", Cmd);
            break;
    }

EXIT:
    UNLOCK(mCmdQLock);
    OUT_FUNC();
    return err;
}

OMX_ERRORTYPE MtkOmxVenc::SetCallbacks(OMX_IN OMX_HANDLETYPE hComponent,
                                       OMX_IN OMX_CALLBACKTYPE *pCallBacks,
                                       OMX_IN OMX_PTR pAppData)
{
    IN_FUNC();
    (void)(hComponent);

    MTK_OMX_LOGD_ENG("MtkOmxVenc::SetCallbacks");

    if (NULL == pCallBacks)
    {
        MTK_OMX_LOGE("[ERROR] MtkOmxVenc::SetCallbacks pCallBacks is NULL !!!");
        OUT_FUNC();
        return OMX_ErrorBadParameter;
    }

    mCallback = *pCallBacks;
    mAppData = pAppData;
    mCompHandle.pApplicationPrivate = mAppData;

    OUT_FUNC();
    return OMX_ErrorNone;
}

OMX_ERRORTYPE MtkOmxVenc::SetParameter(OMX_IN OMX_HANDLETYPE hComp,
                                       OMX_IN OMX_INDEXTYPE nParamIndex,
                                       OMX_IN OMX_PTR pCompParam)
{
    IN_FUNC();
    (void)(hComp);
    OMX_ERRORTYPE err = OMX_ErrorNone;

    MTK_OMX_LOGD_ENG("MtkOmxVenc::SetParameter (0x%08X) %s", nParamIndex, indexType(nParamIndex));

    if (mState == OMX_StateInvalid)
    {
        OUT_FUNC();
        return OMX_ErrorIncorrectStateOperation;
    }

    if (NULL == pCompParam)
    {
        OUT_FUNC();
        return OMX_ErrorBadParameter;
    }

    switch (nParamIndex)
    {
        case OMX_IndexParamPortDefinition:
            err = HandleSetPortDefinition((OMX_PARAM_PORTDEFINITIONTYPE *)pCompParam);
            break;
        case OMX_IndexParamVideoPortFormat:
            err = HandleSetVideoPortFormat((OMX_VIDEO_PARAM_PORTFORMATTYPE *)pCompParam);
            break;
        case OMX_IndexParamStandardComponentRole:
            err = HandleSetStandardComponentRole((OMX_PARAM_COMPONENTROLETYPE *)pCompParam);
            break;
        case OMX_IndexParamVideoAvc:
            err = HandleSetVideoAvc((OMX_VIDEO_PARAM_AVCTYPE *)pCompParam);
            break;
        case OMX_IndexParamVideoHevc:
            err = HandleSetVideoHevc((OMX_VIDEO_PARAM_HEVCTYPE *)pCompParam);
            break;
        case OMX_IndexParamVideoH263:
            err = HandleSetVideoH263((OMX_VIDEO_PARAM_H263TYPE *)pCompParam);
            break;
        case OMX_IndexParamVideoMpeg4:
            err = HandleSetVideoMpeg4((OMX_VIDEO_PARAM_MPEG4TYPE *)pCompParam);
            break;
        case OMX_IndexParamVideoVp8:
            err = HandleSetVideoVp8((OMX_VIDEO_PARAM_VP8TYPE *)pCompParam);
            break;
        case OMX_IndexParamVideoBitrate:
            err = HandleSetVideoBitrate((OMX_VIDEO_PARAM_BITRATETYPE *)pCompParam);
            break;
        case OMX_IndexParamVideoQuantization:
            err = HandleSetVideoQuantization((OMX_VIDEO_PARAM_QUANTIZATIONTYPE *)pCompParam);
            break;
        case OMX_IndexParamVideoVBSMC:
            err = HandleSetVideoVBSMC((OMX_VIDEO_PARAM_VBSMCTYPE *)pCompParam);
            break;
        case OMX_IndexParamVideoMotionVector:
            err = HandleSetVideoMotionVector((OMX_VIDEO_PARAM_MOTIONVECTORTYPE *)pCompParam);
            break;
        case OMX_IndexParamVideoIntraRefresh:
            err = HandleSetVideoIntraRefresh((OMX_VIDEO_PARAM_INTRAREFRESHTYPE *)pCompParam);
            break;
        case OMX_IndexParamVideoSliceFMO:
            err = HandleSetVideoSliceFMO((OMX_VIDEO_PARAM_AVCSLICEFMO *)pCompParam);
            break;
        case OMX_IndexParamVideoErrorCorrection:
            err = HandleSetVideoErrorCorrection((OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *)pCompParam);
            break;
        case OMX_IndexVendorMtkOmxVenc3DVideoRecode:    // for MTK S3D SUPPORT
            err = HandleSetVendor3DVideoRecode((OMX_VIDEO_H264FPATYPE *)pCompParam);
            break;
#ifdef ANDROID_ICS
        case OMX_IndexVendorMtkOmxVencSetTimelapseMode:
            err = HandleSetVendorTimelapseMode((OMX_BOOL *)pCompParam);
            break;
        case OMX_IndexVendorMtkOmxVencSetWhiteboardEffectMode:
            err = HandleSetVendorWhiteboardEffectMode((OMX_BOOL *)pCompParam);
            break;
        case OMX_IndexVendorMtkOmxVencSetMCIMode:
            err = HandleSetVendorSetMCIMode((OMX_BOOL *)pCompParam);
            break;
        case OMX_GoogleAndroidIndexStoreMetaDataInBuffers: // Morris Yang 20120214 add for live effect recording
            err = HandleSetGoogleStoreMetaDataInBuffers((StoreMetaDataInBuffersParams *)pCompParam);
            break;
        case OMX_IndexVendorMtkOmxVencSetScenario:
            err = HandleSetVendorSetScenario((OMX_PARAM_U32TYPE *)pCompParam);
            break;
        case OMX_IndexVendorMtkOmxVencPrependSPSPPS:
            err = HandleSetVendorPrependSPSPPS((PrependSPSPPSToIDRFramesParams *)pCompParam);
            break;
        case OMX_IndexVendorMtkOmxVencNonRefPOp:
            err = HandleSetVendorNonRefPOp((OMX_VIDEO_NONREFP *)pCompParam);
            break;
        case OMX_IndexVendorMtkOmxVideoSetClientLocally:
            err = HandleSetVendorSetClientLocally((OMX_CONFIG_BOOLEANTYPE *)pCompParam);
            break;
#endif
        case OMX_GoogleAndroidIndexstoreANWBufferInMetadata:
            err = HandleSetGoogleStoreANWBufferInMetadata((StoreMetaDataInBuffersParams *)pCompParam);
            break;
#ifdef SUPPORT_NATIVE_HANDLE
        case OMX_GoogleAndroidIndexEnableAndroidNativeHandle:
            err = HandleSetGoogleEnableAndroidNativeHandle((AllocateNativeHandleParams *)pCompParam);
            break;
#endif
        case OMX_IndexParamAndroidVideoTemporalLayering:
            err = HandleSetGoogleTemporalLayering((OMX_VIDEO_PARAM_ANDROID_TEMPORALLAYERINGTYPE *)pCompParam);
            break;
        default:
            MTK_OMX_LOGE("MtkOmxVenc::SetParameter unsupported nParamIndex(0x%08X)", nParamIndex);
            err = OMX_ErrorUnsupportedIndex;
            break;
    }

    OUT_FUNC();
    return err;
}

OMX_ERRORTYPE MtkOmxVenc::GetParameter(OMX_IN OMX_HANDLETYPE hComponent,
                                       OMX_IN  OMX_INDEXTYPE nParamIndex,
                                       OMX_INOUT OMX_PTR pCompParam)
{
    IN_FUNC();
    (void)(hComponent);
    OMX_ERRORTYPE err = OMX_ErrorNone;

    MTK_OMX_LOGD_ENG("MtkOmxVenc::GetParameter (0x%08X) %s", nParamIndex, indexType(nParamIndex));

    if (mState == OMX_StateInvalid)
    {
        OUT_FUNC();
        return OMX_ErrorIncorrectStateOperation;
    }

    if (NULL == pCompParam)
    {
        OUT_FUNC();
        return OMX_ErrorBadParameter;
    }

    switch (nParamIndex)
    {
        case OMX_IndexParamPortDefinition:
            err = HandleGetPortDefinition((OMX_PARAM_PORTDEFINITIONTYPE *)pCompParam);
            break;
        case OMX_IndexParamVideoInit:
            err = HandleGetVideoInit((OMX_PORT_PARAM_TYPE *)pCompParam);
            break;
        case OMX_IndexParamAudioInit:
            err = HandleGetAudioInit((OMX_PORT_PARAM_TYPE *)pCompParam);
            break;
        case OMX_IndexParamImageInit:
            err = HandleGetImageInit((OMX_PORT_PARAM_TYPE *)pCompParam);
            break;
        case OMX_IndexParamOtherInit:
            err = HandleGetOtherInit((OMX_PORT_PARAM_TYPE *)pCompParam);
            break;
        case OMX_IndexParamVideoPortFormat:
            err = HandleGetVideoPortFormat((OMX_VIDEO_PARAM_PORTFORMATTYPE *)pCompParam);
            break;
        case OMX_IndexParamStandardComponentRole:
            err = HandleGetStandardComponentRole((OMX_PARAM_COMPONENTROLETYPE *)pCompParam);
            break;
        case OMX_IndexParamVideoAvc:
            err = HandleGetVideoAvc((OMX_VIDEO_PARAM_AVCTYPE *)pCompParam);
            break;
        case OMX_IndexParamVideoHevc:
            err = HandleGetVideoHevc((OMX_VIDEO_PARAM_HEVCTYPE *)pCompParam);
            break;
        case OMX_IndexParamVideoH263:
            err = HandleGetVideoH263((OMX_VIDEO_PARAM_H263TYPE *)pCompParam);
            break;
        case OMX_IndexParamVideoMpeg4:
            err = HandleGetVideoMpeg4((OMX_VIDEO_PARAM_MPEG4TYPE *)pCompParam);
            break;
        case OMX_IndexParamVideoVp8:
            err = HandleGetVideoVp8((OMX_VIDEO_PARAM_VP8TYPE *)pCompParam);
            break;
        case OMX_IndexParamVideoProfileLevelQuerySupported:
            err = HandleGetVideoProfileLevelQuerySupported((OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pCompParam);
            break;
        case OMX_IndexParamVideoBitrate:
            err = HandleGetVideoBitrate((OMX_VIDEO_PARAM_BITRATETYPE *)pCompParam);
            break;
        case OMX_IndexParamVideoQuantization:
            err = HandleGetVideoQuantization((OMX_VIDEO_PARAM_QUANTIZATIONTYPE *)pCompParam);
            break;
        case OMX_IndexParamVideoVBSMC:
            err = HandleGetVideoVBSMC((OMX_VIDEO_PARAM_VBSMCTYPE *)pCompParam);
            break;
        case OMX_IndexParamVideoMotionVector:
            err = HandleGetVideoMotionVector((OMX_VIDEO_PARAM_MOTIONVECTORTYPE *)pCompParam);
            break;
        case OMX_IndexParamVideoIntraRefresh:
            err = HandleGetVideoIntraRefresh((OMX_VIDEO_PARAM_INTRAREFRESHTYPE *)pCompParam);
            break;
        case OMX_IndexParamVideoSliceFMO:
            err = HandleGetVideoSliceFMO((OMX_VIDEO_PARAM_AVCSLICEFMO *)pCompParam);
            break;
        case OMX_IndexParamVideoErrorCorrection:
            err = HandleGetVideoErrorCorrection((OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *)pCompParam);
            break;
        case OMX_IndexVendorMtkOmxVencNonRefPOp:
            err = HandleGetVendorNonRefPOp((OMX_VIDEO_NONREFP *)pCompParam);
            break;
        case OMX_IndexVendorMtkOmxHandle:
            err = HandleGetVendorOmxHandle((OMX_U32 *)pCompParam);
            break;
        case OMX_IndexVendorMtkQueryDriverVersion:
            err = HandleGetVendorQueryDriverVersion((OMX_VIDEO_PARAM_DRIVERVER *)pCompParam);
            break;
        case OMX_IndexVendorMtkOmxVencQueryCodecsSizes:
            err = HandleGetVendorQueryCodecsSizes((OMX_VIDEO_PARAM_SPEC_QUERY*)pCompParam);
            break;
        case OMX_GoogleAndroidIndexDescribeColorFormat:
            err = HandleGetGoogleDescribeColorFormat((DescribeColorFormatParams *)pCompParam);
            break;
        case OMX_GoogleAndroidIndexDescribeColorFormat2:
            err = HandleGetGoogleDescribeColorFormat2((DescribeColorFormat2Params *)pCompParam);
            break;
        case OMX_IndexParamAndroidVideoTemporalLayering:
            err = HandleGetGoogleTemporalLayering((OMX_VIDEO_PARAM_ANDROID_TEMPORALLAYERINGTYPE *)pCompParam);
            break;
        default:
            MTK_OMX_LOGE("MtkOmxVenc::GetParameter unsupported nParamIndex(0x%08X)", nParamIndex);
            err = OMX_ErrorUnsupportedIndex;
            break;
    }

    OUT_FUNC();
    return err;
}

OMX_ERRORTYPE MtkOmxVenc::SetConfig(OMX_IN OMX_HANDLETYPE hComponent,
                                    OMX_IN OMX_INDEXTYPE nConfigIndex,
                                    OMX_IN OMX_PTR pCompConfig)
{
    IN_FUNC();
    (void)(hComponent);
    OMX_ERRORTYPE err = OMX_ErrorNone;

    MTK_OMX_LOGD_ENG("MtkOmxVenc::SetConfig (0x%08X) %s", nConfigIndex, indexType(nConfigIndex));

    switch (nConfigIndex)
    {
        case OMX_IndexConfigVideoFramerate:
            err = HandleSetConfigVideoFramerate((OMX_CONFIG_FRAMERATETYPE *)pCompConfig);
            break;
        case OMX_IndexConfigVideoBitrate:
            err = HandleSetConfigVideoBitrate((OMX_VIDEO_CONFIG_BITRATETYPE *)pCompConfig);
            break;
        case OMX_IndexConfigVideoIntraVOPRefresh:
            err = HandleSetConfigVideoIntraVOPRefresh((OMX_CONFIG_INTRAREFRESHVOPTYPE *)pCompConfig);
            break;
        case OMX_IndexConfigVideoAVCIntraPeriod:
            err = HandleSetConfigVideoAVCIntraPeriod((OMX_VIDEO_CONFIG_AVCINTRAPERIOD *)pCompConfig);
            break;
#ifdef ANDROID_ICS
        case OMX_IndexVendorMtkOmxVencSetForceIframe:
            err = HandleSetConfigVendorSetForceIframe((OMX_PARAM_U32TYPE *)pCompConfig);
            break;
#endif
        case OMX_IndexVendorMtkOmxVencSetIInterval:
            err = HandleSetConfigVendorSetIInterval((OMX_PARAM_U32TYPE *)pCompConfig);
            break;
        case OMX_IndexVendorMtkOmxVencSkipFrame:
            err = HandleSetConfigVendorSkipFrame((OMX_PARAM_U32TYPE *)pCompConfig);
            break;
        case OMX_IndexVendorMtkOmxVencDrawBlack://for Miracast test case SIGMA 5.1.11 workaround
            err = HandleSetConfigVendorDrawBlack((OMX_PARAM_U32TYPE *)pCompConfig);
            break;
        case OMX_IndexVendorMtkConfigQP:
            err = HandleSetConfigVendorConfigQP((OMX_VIDEO_CONFIG_QP *)pCompConfig);
            break;
        case OMX_IndexVendorMtkOmxVencSetForceFullIframe:
            err = HandleSetConfigVendorSetForceFullIframe((OMX_PARAM_U32TYPE *)pCompConfig);
            break;
        case OMX_IndexVendorMtkOmxSliceLossIndication:
            err = HandleSetConfigVendorSliceLossIndication((OMX_CONFIG_SLICE_LOSS_INDICATION *) pCompConfig);
            break;
        case OMX_IndexConfigAndroidIntraRefresh:
            err = HandleSetConfigGoogleIntraRefresh((OMX_VIDEO_CONFIG_ANDROID_INTRAREFRESHTYPE *)pCompConfig);
            break;
        case OMX_IndexConfigOperatingRate:
            err = HandleSetConfigOperatingRate((OMX_PARAM_U32TYPE *)pCompConfig);
            break;
        case OMX_IndexConfigAndroidVideoTemporalLayering:
            err = HandleSetConfigGoogleTemporalLayering((OMX_VIDEO_CONFIG_ANDROID_TEMPORALLAYERINGTYPE *)pCompConfig);
            break;
        case OMX_IndexVendorMtkOmxVencSeResolutionChange:
            err = HandleSetConfigVendorResolutionChange((OMX_VIDEO_PARAM_RESOLUTION *)pCompConfig);
            break;
        case OMX_IndexVendorMtkOmxVencInputScaling:
            err = HandleSetConfigVendorInputScaling((OMX_CONFIG_BOOLEANTYPE *)pCompConfig);
            break;
        default:
            MTK_OMX_LOGE("MtkOmxVenc::GetParameter unsupported nConfigIndex(0x%08X)", nConfigIndex);
            err = OMX_ErrorUnsupportedIndex;
            break;
    }

    OUT_FUNC();
    return err;
}

OMX_ERRORTYPE MtkOmxVenc::GetConfig(OMX_IN OMX_HANDLETYPE hComponent,
                                    OMX_IN OMX_INDEXTYPE nConfigIndex,
                                    OMX_INOUT OMX_PTR pCompConfig)
{
    IN_FUNC();
    (void)(hComponent);
    OMX_ERRORTYPE err = OMX_ErrorNone;

    MTK_OMX_LOGD_ENG("MtkOmxVenc::GetConfig (0x%08X), %s", nConfigIndex, indexType(nConfigIndex));

    switch (nConfigIndex)
    {
        case OMX_IndexConfigVideoFramerate:
            err = HandleGetConfigVideoFramerate((OMX_CONFIG_FRAMERATETYPE *)pCompConfig);
            break;
        case OMX_IndexConfigVideoBitrate:
            err = HandleGetConfigVideoBitrate((OMX_VIDEO_CONFIG_BITRATETYPE *)pCompConfig);
            break;
        case OMX_IndexConfigVideoIntraVOPRefresh:
            err = HandleGetConfigVideoIntraVOPRefresh((OMX_CONFIG_INTRAREFRESHVOPTYPE *)pCompConfig);
            break;
        case OMX_IndexConfigVideoAVCIntraPeriod:
            err = HandleGetConfigVideoAVCIntraPeriod((OMX_VIDEO_CONFIG_AVCINTRAPERIOD *)pCompConfig);
            break;
        case OMX_IndexConfigAndroidIntraRefresh:
            err = HandleGetConfigGoogleIntraRefresh((OMX_VIDEO_CONFIG_ANDROID_INTRAREFRESHTYPE *)pCompConfig);
            break;
        case OMX_IndexConfigAndroidVideoTemporalLayering:
            err = HandleGetConfigGoogleTemporalLayering((OMX_VIDEO_CONFIG_ANDROID_TEMPORALLAYERINGTYPE *)pCompConfig);
            break;
        default:
            MTK_OMX_LOGE("MtkOmxVenc::GetConfig unsupported nConfigIndex(0x%08X)", nConfigIndex);
            err = OMX_ErrorUnsupportedIndex;
            break;
    }

    OUT_FUNC();
    return err;
}

#define MAP_AND_GET_START if(0)
#define MAP_AND_GET(STR, KEY) \
    else if (!strncmp(parameterName,STR,strlen(STR))) {*pIndexType = (OMX_INDEXTYPE) KEY ; return err;}
#define MAP_AND_GET_END else

OMX_ERRORTYPE MtkOmxVenc::GetExtensionIndex(OMX_IN OMX_HANDLETYPE hComponent,
                                            OMX_IN OMX_STRING parameterName,
                                            OMX_OUT OMX_INDEXTYPE *pIndexType)
{
    (void)(hComponent);
    OMX_ERRORTYPE err = OMX_ErrorUnsupportedSetting;
    MTK_OMX_LOGD("MtkOmxVenc::GetExtensionIndex");

#ifdef ANDROID_ICS
    err = OMX_ErrorNone;

    MAP_AND_GET_START;
    MAP_AND_GET("OMX.MTK.index.param.video.EncSetForceIframe", OMX_IndexVendorMtkOmxVencSetForceIframe)
    MAP_AND_GET("OMX.MTK.index.param.video.3DVideoEncode", OMX_IndexVendorMtkOmxVenc3DVideoRecode)

    // Morris Yang 20120214 add for live effect recording [
    MAP_AND_GET("OMX.google.android.index.storeMetaDataInBuffers", OMX_GoogleAndroidIndexStoreMetaDataInBuffers)
    // ]
    MAP_AND_GET("OMX.MTK.VIDEO.index.useIonBuffer", OMX_IndexVendorMtkOmxVideoUseIonBuffer)
    MAP_AND_GET("OMX.MTK.index.param.video.EncSetIFrameRate", OMX_IndexVendorMtkOmxVencSetIInterval)
    MAP_AND_GET("OMX.MTK.index.param.video.EncSetSkipFrame", OMX_IndexVendorMtkOmxVencSkipFrame)
    MAP_AND_GET("OMX.MTK.index.param.video.SetVencScenario", OMX_IndexVendorMtkOmxVencSetScenario)
    MAP_AND_GET("OMX.google.android.index.prependSPSPPSToIDRFrames", OMX_IndexVendorMtkOmxVencPrependSPSPPS)
    MAP_AND_GET("OMX.microsoft.skype.index.driverversion", OMX_IndexVendorMtkQueryDriverVersion)
    MAP_AND_GET("OMX.microsoft.skype.index.qp", OMX_IndexVendorMtkConfigQP)
    else if (!strncmp(parameterName, "OMX.google.android.index.storeGraphicBufferInMetaData",
                strlen("OMX.google.android.index.storeGraphicBufferInMetaData")))
    {
        mInputPortDef.nBufferCountActual = 4;
        err = OMX_ErrorUnsupportedIndex;
        MTK_OMX_LOGD("try to do storeGraphicBufferInMetaData");
    }
    MAP_AND_GET("OMX.MTK.index.param.video.EncInputScaling", OMX_IndexVendorMtkOmxVencInputScaling)
    MAP_AND_GET("OMX.MTK.index.param.video.SlicelossIndication", OMX_IndexVendorMtkOmxSliceLossIndication)
    MAP_AND_GET("OMX.MTK.index.param.video.EncSetForceFullIframe", OMX_IndexVendorMtkOmxVencSetForceFullIframe)
    MAP_AND_GET("OMX.google.android.index.describeColorFormat", OMX_GoogleAndroidIndexDescribeColorFormat)
    MAP_AND_GET("OMX.google.android.index.describeColorFormat2", OMX_GoogleAndroidIndexDescribeColorFormat2)

    MAP_AND_GET("OMX.google.android.index.enableAndroidNativeBuffers", OMX_GoogleAndroidIndexEnableAndroidNativeBuffers)
    MAP_AND_GET("OMX.google.android.index.useAndroidNativeBuffer", OMX_GoogleAndroidIndexUseAndroidNativeBuffer)
    MAP_AND_GET("OMX.google.android.index.getAndroidNativeBufferUsage", OMX_GoogleAndroidIndexGetAndroidNativeBufferUsage)
    MAP_AND_GET("OMX.google.android.index.storeANWBufferInMetadata", OMX_GoogleAndroidIndexstoreANWBufferInMetadata)
    MAP_AND_GET("OMX.google.android.index.prepareForAdaptivePlayback", OMX_GoogleAndroidIndexPrepareForAdaptivePlayback)

#ifdef SUPPORT_NATIVE_HANDLE
    MAP_AND_GET("OMX.google.android.index.allocateNativeHandle", OMX_GoogleAndroidIndexEnableAndroidNativeHandle)
#endif
    MAP_AND_GET_END
    {
        MTK_OMX_LOGE("MtkOmxVenc::GetExtensionIndex Unknown parameter name: %s", parameterName);
        err = OMX_ErrorUnsupportedIndex;
    }
#endif

    return err;
}

OMX_ERRORTYPE MtkOmxVenc::GetState(OMX_IN OMX_HANDLETYPE hComponent,
                                   OMX_INOUT OMX_STATETYPE *pState)
{
    IN_FUNC();
    (void)(hComponent);
    OMX_ERRORTYPE err = OMX_ErrorNone;

    if (NULL == pState)
    {
        MTK_OMX_LOGE("[ERROR] MtkOmxVenc::GetState pState is NULL !!!");
        OUT_FUNC();
        return OMX_ErrorBadParameter;
    }
    *pState = mState;

    MTK_OMX_LOGD_ENG("MtkOmxVenc::GetState (mState=%s)", StateToString(mState));
    OUT_FUNC();
    return err;
}

OMX_ERRORTYPE MtkOmxVenc::AllocateBuffer(OMX_IN OMX_HANDLETYPE hComponent,
                                         OMX_INOUT OMX_BUFFERHEADERTYPE **ppBufferHdr,
                                         OMX_IN OMX_U32 nPortIndex,
                                         OMX_IN OMX_PTR pAppPrivate,
                                         OMX_IN OMX_U32 nSizeBytes)
{
    IN_FUNC();
    (void)(hComponent);
    OMX_ERRORTYPE err = OMX_ErrorNone;

    if (nPortIndex == mInputPortDef.nPortIndex)
    {
        err = CheckInputBufferPortAvailbility();
        if(err != OMX_ErrorNone)
        {
            OUT_FUNC();
            return err;
        }

        InputBufferHeaderAllocate(ppBufferHdr, nPortIndex, pAppPrivate, nSizeBytes);

        EpilogueInputBufferHeaderAllocate();
    }
    else if (nPortIndex == mOutputPortDef.nPortIndex)
    {

        err = CheckOutputBufferPortAvailbility();
        if(err != OMX_ErrorNone)
        {
            OUT_FUNC();
            return err;
        }

        err = OutputBufferHeaderAllocate(ppBufferHdr, nPortIndex, pAppPrivate, nSizeBytes);
        if(err != OMX_ErrorNone)
        {
            OUT_FUNC();
            return err;
        }

        EpilogueOutputBufferHeaderAllocate();
    }
    else //nPortIndex != mInputPortDef.nPortIndex && nPortIndex != mOutputPortDef.nPortIndex
    {
        OUT_FUNC();
        return OMX_ErrorBadPortIndex;
    }

EXIT:
    OUT_FUNC();
    return err;
}

OMX_ERRORTYPE MtkOmxVenc::UseBuffer(OMX_IN OMX_HANDLETYPE hComponent,
                                    OMX_INOUT OMX_BUFFERHEADERTYPE **ppBufferHdr,
                                    OMX_IN OMX_U32 nPortIndex,
                                    OMX_IN OMX_PTR pAppPrivate,
                                    OMX_IN OMX_U32 nSizeBytes,
                                    OMX_IN OMX_U8 *pBuffer)
{
    IN_FUNC();
    (void)(hComponent);
    OMX_ERRORTYPE err = OMX_ErrorNone;

    if (nPortIndex == mInputPortDef.nPortIndex)
    {
        err = CheckInputBufferPortAvailbility();
        if(err != OMX_ErrorNone)
        {
            OUT_FUNC();
            return err;
        }

        InputBufferHeaderUse(ppBufferHdr, nPortIndex, pAppPrivate, nSizeBytes, pBuffer);

        EpilogueInputBufferHeaderUse();
    }
    else if (nPortIndex == mOutputPortDef.nPortIndex)
    {

        err = CheckOutputBufferPortAvailbility();
        if(err != OMX_ErrorNone)
        {
            OUT_FUNC();
            return err;
        }

        OutputBufferHeaderUse(ppBufferHdr, nPortIndex, pAppPrivate, nSizeBytes, pBuffer);

        EpilogueOutputBufferHeaderUse();
    }
    else
    {
        OUT_FUNC();
        return OMX_ErrorBadPortIndex;
    }

EXIT:
    OUT_FUNC();
    return err;
}

OMX_ERRORTYPE MtkOmxVenc::FreeBuffer(OMX_IN OMX_HANDLETYPE hComponent,
                                     OMX_IN OMX_U32 nPortIndex,
                                     OMX_IN OMX_BUFFERHEADERTYPE *pBuffHead)
{
    IN_FUNC();
    (void)(hComponent);
    OMX_ERRORTYPE err = OMX_ErrorNone;
    //MTK_OMX_LOGD ("MtkOmxVenc::FreeBuffer nPortIndex(%d)", nPortIndex);

    if (NULL == pBuffHead)
    {
        MTK_OMX_LOGE("pBuffHead is empty!");
        return OMX_ErrorBadParameter;
    }

    OMX_BOOL bAllowFreeBuffer = AllowToFreeBuffer(nPortIndex, mState);

    //MTK_OMX_LOGD ("@@ mState=%d, Is LOADED PENDING(%d)", mState, IS_PENDING (MTK_OMX_VENC_LOADED_PENDING));
    if (!bAllowFreeBuffer && bufferReadyState(mState))
    {
        mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                               mAppData,
                               OMX_EventError,
                               OMX_ErrorPortUnpopulated,
                               (OMX_U32)NULL,
                               NULL);
        OUT_FUNC();
        return OMX_ErrorPortUnpopulated;
    }

    if (bAllowFreeBuffer && (nPortIndex == MTK_OMX_INPUT_PORT))
    {
        UnmapInputMemory(pBuffHead);
        FreeInputBuffers(pBuffHead);
    }

    if ((nPortIndex == MTK_OMX_OUTPUT_PORT) && bAllowFreeBuffer)
    {
        UnmapOutputMemory(pBuffHead);
        FreeOutputBuffers(pBuffHead);
    }

EXIT:
    OUT_FUNC();
    return err;
}


OMX_ERRORTYPE MtkOmxVenc::EmptyThisBuffer(OMX_IN OMX_HANDLETYPE hComponent,
                                          OMX_IN OMX_BUFFERHEADERTYPE *pBuffHead)
{
    (void)(hComponent);
    OMX_ERRORTYPE err = OMX_ErrorNone;
    if (mETBDebug == true)
    {
        MTK_OMX_LOGD("MtkOmxVenc::EmptyThisBuffer pBuffHead(0x%08X), pBuffer(0x%08X), nFilledLen(%u)",
                     pBuffHead, pBuffHead->pBuffer, pBuffHead->nFilledLen);
    }
    int ret;
    OMX_U32 CmdCat = MTK_OMX_BUFFER_COMMAND;
    OMX_U32 buffer_type = MTK_OMX_EMPTY_THIS_BUFFER_TYPE;
    // write 8 bytes to mEmptyBufferPipe  [buffer_type][pBuffHead]
    LOCK(mCmdQLock);
    WRITE_PIPE(CmdCat, mCmdPipe);
    WRITE_PIPE(buffer_type, mCmdPipe);
    WRITE_PIPE(pBuffHead, mCmdPipe);
    UNLOCK(mCmdQLock);

EXIT:
    return err;
}


OMX_ERRORTYPE MtkOmxVenc::FillThisBuffer(OMX_IN OMX_HANDLETYPE hComponent,
                                         OMX_IN OMX_BUFFERHEADERTYPE *pBuffHead)
{
    (void)(hComponent);
    OMX_ERRORTYPE err = OMX_ErrorNone;
    //MTK_OMX_LOGD ("MtkOmxVenc::FillThisBuffer pBuffHead(0x%08X), pBuffer(0x%08X), nAllocLen(%u)",
    //pBuffHead, pBuffHead->pBuffer, pBuffHead->nAllocLen);
    int ret;
    OMX_U32 CmdCat = MTK_OMX_BUFFER_COMMAND;
    OMX_U32 buffer_type = MTK_OMX_FILL_THIS_BUFFER_TYPE;
    // write 8 bytes to mFillBufferPipe  [bufId][pBuffHead]
    LOCK(mCmdQLock);
    WRITE_PIPE(CmdCat, mCmdPipe);
    WRITE_PIPE(buffer_type, mCmdPipe);
    WRITE_PIPE(pBuffHead, mCmdPipe);
    UNLOCK(mCmdQLock);

EXIT:
    return err;
}


OMX_ERRORTYPE MtkOmxVenc::ComponentRoleEnum(OMX_IN OMX_HANDLETYPE hComponent,
                                            OMX_OUT OMX_U8 *cRole,
                                            OMX_IN OMX_U32 nIndex)
{
    (void)(hComponent);
    OMX_ERRORTYPE err = OMX_ErrorNone;

    if ((0 == nIndex) && (NULL != cRole))
    {
        // Unused callback. enum set to 0
        *cRole = 0;
        MTK_OMX_LOGD("MtkOmxVenc::ComponentRoleEnum: Role[%s]", cRole);
    }
    else
    {
        err = OMX_ErrorNoMore;
    }

    return err;
}

OMX_BOOL MtkOmxVenc::PortBuffersPopulated()
{
    if ((OMX_TRUE == mInputPortDef.bPopulated) && (OMX_TRUE == mOutputPortDef.bPopulated))
    {
        return OMX_TRUE;
    }
    else
    {
        return OMX_FALSE;
    }
}


OMX_ERRORTYPE MtkOmxVenc::HandleStateSet(OMX_U32 nNewState)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    //    MTK_OMX_LOGD ("MtkOmxVenc::HandleStateSet");
    switch (nNewState)
    {
        case OMX_StateIdle:
            if ((mState == OMX_StateLoaded) || (mState == OMX_StateWaitForResources))
            {
                MTK_OMX_LOGD("Request [%s]-> [OMX_StateIdle]", StateToString(mState));

                // wait until input/output buffers allocated
                MTK_OMX_LOGD("wait on mInPortAllocDoneSem(%d), mOutPortAllocDoneSem(%d)!!",
                             get_sem_value(&mInPortAllocDoneSem), get_sem_value(&mOutPortAllocDoneSem));
                WAIT(mInPortAllocDoneSem);
                WAIT(mOutPortAllocDoneSem);

                if ((OMX_TRUE == mInputPortDef.bEnabled) && (OMX_TRUE == mOutputPortDef.bEnabled) &&
                    (OMX_TRUE == PortBuffersPopulated()))
                {
                    mState = OMX_StateIdle;
                    CLEAR_PENDING(MTK_OMX_IDLE_PENDING);
                    mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                           mAppData,
                                           OMX_EventCmdComplete,
                                           OMX_CommandStateSet,
                                           mState,
                                           NULL);
                }
            }
            else if ((mState == OMX_StateExecuting) || (mState == OMX_StatePause))
            {
                MTK_OMX_LOGD("Request [%s]-> [OMX_StateIdle]", StateToString(mState));

                // flush all ports
                // Bruce 20130709 [
                if (mDoConvertPipeline)
                {
                    LOCK(mConvertLock);
                }
                // ]
                LOCK(mEncodeLock);

                if (mPartNum != 0)
                {
                    MTK_OMX_LOGD("Wait Partial Frame Done! now (%d)", mPartNum);
                    mWaitPart = 1;
                    WAIT_COND(mPartCond, mEncodeLock);
                    mWaitPart = 0;
                }
                // Morris Yang 20111223 handle EOS [
                // Morris Yang 20120801 [
                if (mLastBsBufHdr != NULL)
                {
                    mLastBsBufHdr->nFilledLen = 0;
                    mLastBsBufHdr->nFlags |= OMX_BUFFERFLAG_EOS;
                    HandleFillBufferDone(mLastBsBufHdr);
                    MTK_OMX_LOGD("@@ EOS 2-1");
                }
                if (mLastFrameBufHdr != NULL)
                {
                    HandleEmptyBufferDone(mLastFrameBufHdr);
                    mLastFrameBufHdr = NULL;
                    MTK_OMX_LOGD("@@ EOS 2-2");
                }
                // ]
                if (mDoConvertPipeline)
                {
                    ReturnPendingInternalBuffers();
                }
                FlushInputPort();
                FlushOutputPort();
                UNLOCK(mEncodeLock);
                // Bruce 20130709 [
                if (mDoConvertPipeline)
                {
                    UNLOCK(mConvertLock);
                }
                // ]

                // de-initialize decoder
                DeInitVideoEncodeHW();

                mState = OMX_StateIdle;
                CLEAR_PENDING(MTK_OMX_IDLE_PENDING);
                mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                       mAppData,
                                       OMX_EventCmdComplete,
                                       OMX_CommandStateSet,
                                       mState,
                                       NULL);
            }
            else if (mState == OMX_StateIdle)
            {
                MTK_OMX_LOGD("Request [%s]-> [OMX_StateIdle]", StateToString(mState));
                mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                       mAppData,
                                       OMX_EventError,
                                       OMX_ErrorSameState,
                                       (OMX_U32)NULL,
                                       NULL);
            }
            else
            {
                mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                       mAppData,
                                       OMX_EventError,
                                       OMX_ErrorIncorrectStateTransition,
                                       (OMX_U32)NULL,
                                       NULL);
            }

            break;

        case OMX_StateExecuting:
            MTK_OMX_LOGD("Request [%s]-> [OMX_StateExecuting]", StateToString(mState));
            if (mState == OMX_StateIdle || mState == OMX_StatePause)
            {
                // Bruce 20130709 [
                TryTurnOnMDPPipeline();
                if (mDoConvertPipeline)
                {
                    InitPipelineBuffer();
                }
                // ]

                // change state to executing
                mState = OMX_StateExecuting;

                // trigger encode start
                mEncodeStarted = OMX_TRUE;

                // send event complete to IL client
                MTK_OMX_LOGD("state changes to OMX_StateExecuting");
                mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                       mAppData,
                                       OMX_EventCmdComplete,
                                       OMX_CommandStateSet,
                                       mState,
                                       NULL);
            }
            else if (mState == OMX_StateExecuting)
            {
                mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                       mAppData,
                                       OMX_EventError,
                                       OMX_ErrorSameState,
                                       (OMX_U32)NULL,
                                       NULL);
            }
            else
            {
                mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                       mAppData,
                                       OMX_EventError,
                                       OMX_ErrorIncorrectStateTransition,
                                       (OMX_U32)NULL,
                                       NULL);
            }
            break;

        case OMX_StatePause:
            MTK_OMX_LOGD("Request [%s]-> [OMX_StatePause]", StateToString(mState));
            if (mState == OMX_StateIdle || mState == OMX_StateExecuting)
            {
                // TODO: ok
            }
            else if (mState == OMX_StatePause)
            {
                mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                       mAppData,
                                       OMX_EventError,
                                       OMX_ErrorSameState,
                                       (OMX_U32)NULL,
                                       NULL);
            }
            else
            {
                mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                       mAppData,
                                       OMX_EventError,
                                       OMX_ErrorIncorrectStateTransition,
                                       (OMX_U32)NULL,
                                       NULL);
            }
            break;

        case OMX_StateLoaded:
            MTK_OMX_LOGD("Request [%s]-> [OMX_StateLoaded]", StateToString(mState));
            if (mState == OMX_StateIdle)    // IDLE  to LOADED
            {
                if (IS_PENDING(MTK_OMX_LOADED_PENDING))
                {

                    // wait until all input buffers are freed
                    MTK_OMX_LOGD("wait on mInPortFreeDoneSem(%d), mOutPortFreeDoneSem(%d)",
                                 get_sem_value(&mInPortFreeDoneSem), get_sem_value(&mOutPortFreeDoneSem));
                    WAIT(mInPortFreeDoneSem);

                    // wait until all output buffers are freed
                    WAIT(mOutPortFreeDoneSem);

                    mState = OMX_StateLoaded;
                    CLEAR_PENDING(MTK_OMX_LOADED_PENDING);
                    mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                           mAppData,
                                           OMX_EventCmdComplete,
                                           OMX_CommandStateSet,
                                           mState,
                                           NULL);
                }
            }
            else if (mState == OMX_StateLoaded)
            {
                mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                       mAppData,
                                       OMX_EventError,
                                       OMX_ErrorSameState,
                                       (OMX_U32)NULL,
                                       NULL);
            }

        default:
            break;
    }
    return err;
}


OMX_ERRORTYPE MtkOmxVenc::HandlePortEnable(OMX_U32 nPortIndex)
{
    MTK_OMX_LOGD("MtkOmxVenc::HandlePortEnable nPortIndex(0x%X)", (unsigned int)nPortIndex);
    OMX_ERRORTYPE err = OMX_ErrorNone;

    if (nPortIndex == MTK_OMX_INPUT_PORT || nPortIndex == MTK_OMX_ALL_PORT)
    {
        if (IS_PENDING(MTK_OMX_IN_PORT_ENABLE_PENDING))
            // p.86 component is not in LOADED state and the port is not populated
        {
            MTK_OMX_LOGD("Wait on mInPortAllocDoneSem(%d)", get_sem_value(&mInPortAllocDoneSem));
            WAIT(mInPortAllocDoneSem);
            CLEAR_PENDING(MTK_OMX_IN_PORT_ENABLE_PENDING);
        }

        mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                               mAppData,
                               OMX_EventCmdComplete,
                               OMX_CommandPortEnable,
                               MTK_OMX_INPUT_PORT,
                               NULL);
    }

    if (nPortIndex == MTK_OMX_OUTPUT_PORT || nPortIndex == MTK_OMX_ALL_PORT)
    {
        if (IS_PENDING(MTK_OMX_OUT_PORT_ENABLE_PENDING))
        {
            MTK_OMX_LOGD("Wait on mOutPortAllocDoneSem(%d)", get_sem_value(&mOutPortAllocDoneSem));
            WAIT(mOutPortAllocDoneSem);
            CLEAR_PENDING(MTK_OMX_OUT_PORT_ENABLE_PENDING);

            if (mState == OMX_StateExecuting && mPortReconfigInProgress == OMX_TRUE)
            {
                mPortReconfigInProgress = OMX_FALSE;
            }
        }

        mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                               mAppData,
                               OMX_EventCmdComplete,
                               OMX_CommandPortEnable,
                               MTK_OMX_OUTPUT_PORT,
                               NULL);
    }

EXIT:
    return err;
}


OMX_BOOL MtkOmxVenc::CheckBufferAvailability()
{
    if (mEmptyThisBufQ.IsEmpty() || mFillThisBufQ.IsEmpty())
    {
        return OMX_FALSE;
    }
    else
    {
        return OMX_TRUE;
    }
}

void MtkOmxVenc::QueueOutputBuffer(int index)
{
    LOCK(mFillThisBufQ.mBufQLock);

    MTK_OMX_LOGD("@@ QueueOutputBuffer");

    mFillThisBufQ.Push(index);

    UNLOCK(mFillThisBufQ.mBufQLock);
}


void MtkOmxVenc::QueueInputBuffer(int index)
{
    LOCK(mEmptyThisBufQ.mBufQLock);

    MTK_OMX_LOGD("@@ QueueInputBuffer (%d)", index);

    mEmptyThisBufQ.PushFront(index);

    UNLOCK(mEmptyThisBufQ.mBufQLock);
}

OMX_BOOL MtkOmxVenc::QueryDriverFormat(VENC_DRV_QUERY_VIDEO_FORMAT_T *pQinfoOut)
{
    VAL_UINT32_T is_support;
    VENC_DRV_QUERY_VIDEO_FORMAT_T   qinfo;
    VENC_DRV_QUERY_VIDEO_FORMAT_T   *pQinfoIn = &qinfo;

    pQinfoIn->eVideoFormat = GetVencFormat(mCodecId);
    pQinfoIn->eResolution = VENC_DRV_RESOLUTION_SUPPORT_720P;
    pQinfoIn->u4Width = mOutputPortDef.format.video.nFrameWidth;
    pQinfoIn->u4Height = mOutputPortDef.format.video.nFrameHeight;

    switch (mCodecId) {
        case MTK_VENC_CODEC_ID_MPEG4:
        case MTK_VENC_CODEC_ID_MPEG4_SHORT:
            pQinfoIn->eVideoFormat = VENC_DRV_VIDEO_FORMAT_MPEG4;
            pQinfoIn->u4Profile = VENC_DRV_MPEG_VIDEO_PROFILE_MPEG4_SIMPLE;
            pQinfoIn->eLevel = VENC_DRV_VIDEO_LEVEL_3;
            break;
        case MTK_VENC_CODEC_ID_AVC:
            pQinfoIn->eVideoFormat = VENC_DRV_VIDEO_FORMAT_H264;
            pQinfoIn->u4Profile = Omx2DriverH264ProfileMap(mAvcType.eProfile);
            pQinfoIn->eLevel = (VENC_DRV_VIDEO_LEVEL_T)Omx2DriverH264LevelMap(mAvcType.eLevel);
            break;
        default:
            break;
    }

    // query driver property
    /*is_support = eVEncDrvQueryCapability(VENC_DRV_QUERY_TYPE_VIDEO_FORMAT, pQinfoIn, pQinfoOut);
    if(VENC_DRV_MRESULT_FAIL == is_support ) {
        return OMX_FALSE;
    }*/
    return OMX_TRUE;
}

OMX_BOOL MtkOmxVenc::FlushInputPort()
{
    MTK_OMX_LOGD("+FlushInputPort");

    DumpETBQ();
    // return all input buffers currently we have
    ReturnPendingInputBuffers();

    MTK_OMX_LOGD("FlushInputPort -> mNumPendingInput(%d)", (int)mEmptyThisBufQ.mPendingNum);
    int count = 0;
    while (mEmptyThisBufQ.mPendingNum > 0)
    {
        if ((count % 100) == 0)
        {
            MTK_OMX_LOGD("Wait input buffer release....");
        }
        if (count >= 2000)
        {
            MTK_OMX_LOGE("Wait input buffer release...., go to die");
            abort();
        }
        ++count;
        SLEEP_MS(1);
    }

    MTK_OMX_LOGD("-FlushInputPort");
    return OMX_TRUE;
}


OMX_BOOL MtkOmxVenc::FlushOutputPort()
{
    MTK_OMX_LOGD("+FlushOutputPort");

    DumpFTBQ();
    // return all output buffers currently we have
    ReturnPendingOutputBuffers();

    // return all output buffers from decoder
    //  FlushEncoder();

    MTK_OMX_LOGD("-FlushOutputPort -> mNumPendingOutput(%d)", (int)mFillThisBufQ.mPendingNum);

    return OMX_TRUE;
}

OMX_ERRORTYPE MtkOmxVenc::HandlePortDisable(OMX_U32 nPortIndex)
{
    MTK_OMX_LOGD("MtkOmxVenc::HandlePortDisable nPortIndex=0x%X", (unsigned int)nPortIndex);
    OMX_ERRORTYPE err = OMX_ErrorNone;

    // TODO: should we hold mEncodeLock here??

    if (nPortIndex == MTK_OMX_INPUT_PORT || nPortIndex == MTK_OMX_ALL_PORT)
    {

        if ((mState != OMX_StateLoaded) && (mInputPortDef.bPopulated == OMX_TRUE))
        {

            if (mState == OMX_StateExecuting || mState == OMX_StatePause)
            {
                // flush input port
                FlushInputPort();
            }

            // wait until the input buffers are freed
            MTK_OMX_LOGD("@@wait on mInPortFreeDoneSem(%d)", get_sem_value(&mInPortFreeDoneSem));
            WAIT(mInPortFreeDoneSem);
            SIGNAL(mInPortFreeDoneSem);
            MTK_OMX_LOGD("@@wait on mInPortFreeDoneSem(%d)", get_sem_value(&mInPortFreeDoneSem));
        }

        // send OMX_EventCmdComplete back to IL client
        mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                               mAppData,
                               OMX_EventCmdComplete,
                               OMX_CommandPortDisable,
                               MTK_OMX_INPUT_PORT,
                               NULL);
    }

    if (nPortIndex == MTK_OMX_OUTPUT_PORT || nPortIndex == MTK_OMX_ALL_PORT)
    {
        mOutputPortDef.bEnabled = OMX_FALSE;

        if ((mState != OMX_StateLoaded) && (mOutputPortDef.bPopulated == OMX_TRUE))
        {

            if (mState == OMX_StateExecuting || mState == OMX_StatePause)
            {
                // flush output port
                FlushOutputPort();
            }

            // wait until the output buffers are freed
            MTK_OMX_LOGD("@@wait on mOutPortFreeDoneSem(%d)", get_sem_value(&mOutPortFreeDoneSem));
            WAIT(mOutPortFreeDoneSem);
            MTK_OMX_LOGD("@@wait on mOutPortFreeDoneSem(%d)", get_sem_value(&mOutPortFreeDoneSem));
        }

        // send OMX_EventCmdComplete back to IL client
        mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                               mAppData,
                               OMX_EventCmdComplete,
                               OMX_CommandPortDisable,
                               MTK_OMX_OUTPUT_PORT,
                               NULL);
    }


EXIT:
    return err;
}


OMX_ERRORTYPE MtkOmxVenc::HandlePortFlush(OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    MTK_OMX_LOGD("MtkOmxVenc::HandleFlush nPortIndex(0x%X)", (unsigned int)nPortIndex);

    // Bruce 20130709 [
    if (mDoConvertPipeline)
    {
        LOCK(mConvertLock);
        // Because when meta mode we pass handle from convert thread to encode thread, real input
        // will be used in encoding thread. We need LOCK mEncodeLock before do FlushInputPort().
        // If not meta mode we actually only need LOCK mEncodeLock before FlushOutputPort().
        LOCK(mEncodeLock);
    }
    else
    {
        LOCK(mEncodeLock);
    }
    // ]

    if (nPortIndex == MTK_OMX_INPUT_PORT || nPortIndex == MTK_OMX_ALL_PORT)
    {
        if (mDoConvertPipeline)
        {
            ReturnPendingInternalBuffers();
        }

        /*if (mHaveAVCHybridPlatform && (mIsHybridCodec == OMX_TRUE))
        {
            MTK_OMX_LOGD("@@ Check Last frame for 8167\n");

            if (mLastFrameBufHdr != NULL)
            {
                HandleEmptyBufferDone(mLastFrameBufHdr);
                mLastFrameBufHdr = NULL;
                MTK_OMX_LOGD("@@ EOS 2-2");
            }
        }*/

        FlushInputPort();

        mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                               mAppData,
                               OMX_EventCmdComplete,
                               OMX_CommandFlush,
                               MTK_OMX_INPUT_PORT,
                               NULL);
    }

    if (nPortIndex == MTK_OMX_OUTPUT_PORT || nPortIndex == MTK_OMX_ALL_PORT)
    {

        FlushOutputPort();

        mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                               mAppData,
                               OMX_EventCmdComplete,
                               OMX_CommandFlush,
                               MTK_OMX_OUTPUT_PORT,
                               NULL);
    }

    UNLOCK(mEncodeLock);
    // Bruce 20130709 [
    if (mDoConvertPipeline)
    {
        UNLOCK(mConvertLock);
    }
    // ]

EXIT:
    return err;
}


OMX_ERRORTYPE MtkOmxVenc::HandleMarkBuffer(OMX_U32 nParam1, OMX_PTR pCmdData)
{
    (void)(nParam1);
    (void)(pCmdData);
    OMX_ERRORTYPE err = OMX_ErrorNone;
    MTK_OMX_LOGD("MtkOmxVenc::HandleMarkBuffer");

EXIT:
    return err;
}



OMX_ERRORTYPE MtkOmxVenc::HandleEmptyThisBuffer(OMX_BUFFERHEADERTYPE *pBuffHdr)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    //MTK_OMX_LOGD ("MtkOmxVenc::HandleEmptyThisBuffer pBufHead(0x%08X), pBuffer(0x%08X), nFilledLen(%u)",
    //pBuffHdr, pBuffHdr->pBuffer, pBuffHdr->nFilledLen);

    int index = findBufferHeaderIndex(MTK_OMX_INPUT_PORT, pBuffHdr);
    if (index < 0)
    {
        MTK_OMX_LOGE("[ERROR] ETB invalid index(%d)", index);
    }
    //MTK_OMX_LOGD ("ETB idx(%d)", index);

    LOCK(mEmptyThisBufQ.mBufQLock);
    if(!(pBuffHdr->nFlags & OMX_BUFFERFLAG_TRIGGER_OUTPUT)){
        ++mEmptyThisBufQ.mPendingNum;
    }
    mETBDebug = false;
    MTK_OMX_LOGD("%06x ETB (0x%08X) (0x%08X) (%lu), mNumPendingInput(%d), t(%llu)",
                 (unsigned int)this, (unsigned int)pBuffHdr, (unsigned int)pBuffHdr->pBuffer,
                 pBuffHdr->nFilledLen, mEmptyThisBufQ.mPendingNum, pBuffHdr->nTimeStamp);

    mEmptyThisBufQ.Push(index);
    //DumpETBQ();

    UNLOCK(mEmptyThisBufQ.mBufQLock);

    // trigger encode
    // SIGNAL (mEncodeSem);

    // Bruce 20130709 [
    if (mDoConvertPipeline)
    {
        SIGNAL(mConvertSem);

        // Buffer is not empty and convert is not start -> set convert start
        if(!mConvertStarted && CheckBufferAvailability())
        {
            MTK_OMX_LOGD("convert start in ETB");
            mConvertStarted = true;
        }
    }
    else
    {
        SIGNAL(mEncodeSem);
    }
    // ]

EXIT:
    return err;
}

OMX_ERRORTYPE MtkOmxVenc::HandleFillThisBuffer(OMX_BUFFERHEADERTYPE *pBuffHdr)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    //MTK_OMX_LOGD ("MtkOmxVenc::HandleFillThisBuffer pBufHead(0x%08X), pBuffer(0x%08X), nAllocLen(%u)",
    //pBuffHeader, pBuffHeader->pBuffer, pBuffHeader->nAllocLen);

    int index = findBufferHeaderIndex(MTK_OMX_OUTPUT_PORT, pBuffHdr);
    if (index < 0)
    {
        MTK_OMX_LOGE("[ERROR] FTB invalid index(%d)", index);
    }
    //MTK_OMX_LOGD ("FTB idx(%d)", index);

    LOCK(mFillThisBufQ.mBufQLock);

    ++mFillThisBufQ.mPendingNum;
    MTK_OMX_LOGD("%06x FTB (0x%08X) (0x%08X) (%lu), mNumPendingOutput(%d), t(%llu)",
                 (unsigned int)this, (unsigned int)pBuffHdr, (unsigned int)pBuffHdr->pBuffer,
                 pBuffHdr->nAllocLen, mFillThisBufQ.mPendingNum, pBuffHdr->nTimeStamp);

    mFillThisBufQ.Push(index);
    //DumpFTBQ();

    UNLOCK(mFillThisBufQ.mBufQLock);

#ifdef CHECK_OUTPUT_CONSISTENCY
    OMX_U8 *aOutputBuf = pBuffHdr->pBuffer + pBuffHdr->nOffset;
    native_handle_t *handle = (native_handle_t *)(aOutputBuf);
    MTK_OMX_LOGE("[CONSISTENCY]Compare %p and %p, len %d", handle->data[3], handle->data[7], handle->data[2]);
    if(0 != memcmp((void*)handle->data[3], (void*)handle->data[7], handle->data[2]))
    {
        MTK_OMX_LOGE("[Error][CONSISTENCY] and it is not");
    }
    else
    {
        int * temp = (int*)(handle->data[3]);
        MTK_OMX_LOGE("[CONSISTENCY]Clear! first few bytes: %x %x %x %x", *temp, *(temp + 1), *(temp + 2), *(temp + 3));
    }
#endif
    // trigger encode
    SIGNAL(mEncodeSem);

    // Bruce 20130709 [
    if (mDoConvertPipeline && mConvertStarted == false && CheckBufferAvailability() == OMX_TRUE)
    {
        MTK_OMX_LOGD("convert start in FTB");
        mConvertStarted = true;
        SIGNAL(mConvertSem);
    }
    // ]

EXIT:
    return err;
}

OMX_ERRORTYPE MtkOmxVenc::HandleEmptyBufferDone(OMX_BUFFERHEADERTYPE *pBuffHdr)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    //MTK_OMX_LOGD ("MtkOmxVenc::HandleEmptyBufferDone pBufHead(0x%08X), pBuffer(0x%08X)",
    //pBuffHeader, pBuffHeader->pBuffer);

    WaitFence((OMX_U8 *)pBuffHdr->pBuffer, OMX_FALSE);

    LOCK(mEmptyThisBufQ.mBufQLock);
    --mEmptyThisBufQ.mPendingNum;
    UNLOCK(mEmptyThisBufQ.mBufQLock);

    MTK_OMX_LOGD("%06x EBD (0x%08X) (0x%08X), mNumPendingInput(%d), t(%llu)",
                 (unsigned int)this, (unsigned int)pBuffHdr,
                 (unsigned int)pBuffHdr->pBuffer, mEmptyThisBufQ.mPendingNum,
                 pBuffHdr->nTimeStamp);
    mCallback.EmptyBufferDone((OMX_HANDLETYPE)&mCompHandle,
                              mAppData,
                              pBuffHdr);

EXIT:
    return err;
}


OMX_ERRORTYPE MtkOmxVenc::HandleFillBufferDone(OMX_BUFFERHEADERTYPE *pBuffHdr)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    //MTK_OMX_LOGD ("MtkOmxVenc::HandleFillBufferDone pBufHead(0x%08X), pBuffer(0x%08X),
    //nFilledLen(%u)", pBuffHeader, pBuffHeader->pBuffer, pBuffHeader->nFilledLen);

    if (pBuffHdr->nFlags & OMX_BUFFERFLAG_EOS)
    {
        mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                               mAppData,
                               OMX_EventBufferFlag,
                               MTK_OMX_OUTPUT_PORT,
                               pBuffHdr->nFlags,
                               NULL);
    }

    LOCK(mFillThisBufQ.mBufQLock);
    --mFillThisBufQ.mPendingNum;
    UNLOCK(mFillThisBufQ.mBufQLock);

    MTK_OMX_LOGD("%06x FBD (0x%08X) (0x%08X) %lld (%lu), mNumPendingOutput(%d)",
                 (unsigned int)this, (unsigned int)pBuffHdr, (unsigned int)pBuffHdr->pBuffer,
                 pBuffHdr->nTimeStamp, pBuffHdr->nFilledLen, mFillThisBufQ.mPendingNum);
    mCallback.FillBufferDone((OMX_HANDLETYPE)&mCompHandle,
                             mAppData,
                             pBuffHdr);

EXIT:
    return err;
}


void MtkOmxVenc::ReturnPendingInputBuffers()
{
    LOCK(mEmptyThisBufQ.mBufQLock);

    for (size_t i = 0 ; i < mEmptyThisBufQ.Size() ; ++i)
    {
        WaitFence((OMX_U8 *)mInputBufferHdrs[mEmptyThisBufQ.mBufQ[i]]->pBuffer, OMX_FALSE);
        --mEmptyThisBufQ.mPendingNum;
        mCallback.EmptyBufferDone((OMX_HANDLETYPE)&mCompHandle,
                                  mAppData,
                                  mInputBufferHdrs[mEmptyThisBufQ.mBufQ[i]]);
    }
    mEmptyThisBufQ.Clear();

    UNLOCK(mEmptyThisBufQ.mBufQLock);
}


void MtkOmxVenc::ReturnPendingOutputBuffers()
{
    LOCK(mFillThisBufQ.mBufQLock);

    for (size_t i = 0 ; i < mFillThisBufQ.Size() ; ++i)
    {
        --mFillThisBufQ.mPendingNum;
        mCallback.FillBufferDone((OMX_HANDLETYPE)&mCompHandle,
                                 mAppData,
                                 mOutputBufferHdrs[mFillThisBufQ.mBufQ[i]]);
    }
    mFillThisBufQ.Clear();

    UNLOCK(mFillThisBufQ.mBufQLock);
}


void MtkOmxVenc::DumpETBQ()
{
    MTK_OMX_LOGD("--- ETBQ: ");

    for (size_t i = 0 ; i < mEmptyThisBufQ.Size() ; ++i)
    {
        MTK_OMX_LOGD("[%d] - pBuffHead(0x%08X)", mEmptyThisBufQ.mBufQ[i],
                     (unsigned int)mInputBufferHdrs[mEmptyThisBufQ.mBufQ[i]]);
    }
}

void MtkOmxVenc::DumpFTBQ()
{
    MTK_OMX_LOGD("--- FTBQ size: %d \n",mFillThisBufQ.Size());

    for (size_t i = 0 ; i < mFillThisBufQ.Size() ; ++i)
    {
        MTK_OMX_LOGD("[%d] - pBuffHead(0x%08X)", mFillThisBufQ.mBufQ[i],
                     (unsigned int)mOutputBufferHdrs[mFillThisBufQ.mBufQ[i]]);
    }
}

OMX_ERRORTYPE MtkOmxVenc::EncHandleEmptyBufferDone(OMX_BUFFERHEADERTYPE *pBuffHdr)
{
    IN_FUNC();
    OMX_ERRORTYPE err = OMX_ErrorNone;
    if (MtkOmxBufQ::MTK_OMX_VENC_BUFQ_VENC_INPUT == mpVencInputBufQ->mId)//convert pipeline output
    {
        LOCK(mpVencInputBufQ->mBufQLock);
        --mpVencInputBufQ->mPendingNum;
        UNLOCK(mpVencInputBufQ->mBufQLock);

        MTK_OMX_LOGD_ENG("%06x VENC_p EBD (0x%08X) (0x%08X), mNumPendingInput(%d)",
                     (unsigned int)this, (unsigned int)pBuffHdr,
                     (unsigned int)pBuffHdr->pBuffer, mpVencInputBufQ->mPendingNum);
        //MTK_OMX_LOGD ("MtkOmxVenc::HandleFillThisBuffer pBufHead(0x%08X), pBuffer(0x%08X), nAllocLen(%u)",
        //pBuffHeader, pBuffHeader->pBuffer, pBuffHeader->nAllocLen);
        int index = findBufferHeaderIndexAdvance(MtkOmxBufQ::MTK_OMX_VENC_BUFQ_CONVERT_OUTPUT,
                                                 MtkOmxBufQ::MTK_OMX_VENC_BUFQ_VENC_INPUT, pBuffHdr);
        if (index < 0)
        {
            MTK_OMX_LOGE("[ERROR] CNVT_p FTB invalid index(%d)", index);
        }
        //MTK_OMX_LOGD ("FTB idx(%d)", index);

        LOCK(mpConvertOutputBufQ->mBufQLock);

        ++mpConvertOutputBufQ->mPendingNum;
        MTK_OMX_LOGE("%06x CNVT_p FTB (0x%08X) (0x%08X) (%lu), mNumPendingOutput(%d)",
                     (unsigned int)this, (unsigned int)pBuffHdr, (unsigned int)pBuffHdr->pBuffer,
                     pBuffHdr->nAllocLen, mpConvertOutputBufQ->mPendingNum);

        mpConvertOutputBufQ->Push(index);
        UNLOCK(mpConvertOutputBufQ->mBufQLock);

        // trigger convert
        SIGNAL(mConvertSem);
    }
    else if (MtkOmxBufQ::MTK_OMX_VENC_BUFQ_INPUT == mpVencInputBufQ->mId)//input
    {
        OUT_FUNC();
        return HandleEmptyBufferDone(pBuffHdr);
    }
    OUT_FUNC();
    return err;
}

void MtkOmxVenc::TryTurnOnMDPPipeline()
{
    if (mStoreMetaDataInBuffers == OMX_TRUE &&
        (mInputScalingMode == OMX_TRUE || mSetWFDMode == OMX_TRUE || mPrependSPSPPSToIDRFrames == OMX_TRUE)) // check Prepend when basic AOSP WFD
    {
        // all chip turn on MDP pipeline
        mDoConvertPipeline = true;
        /*VENC_DRV_MRESULT_T mReturn = VENC_DRV_MRESULT_OK;
        OMX_U32 uGetMDPPipeLineEnableType = 0;
        mReturn = eVEncDrvGetParam((VAL_HANDLE_T)NULL, VENC_DRV_GET_TYPE_MDPPIPELINE_ENABLE_TYPE, (VAL_VOID_T *)&mChipName, (VAL_VOID_T *)&uGetMDPPipeLineEnableType);
        MTK_OMX_LOGD("uGetMDPPipeLineEnableType%d", uGetMDPPipeLineEnableType);

        //if (mChipName == VAL_CHIP_NAME_MT8135 && mInputPortDef.format.video.nFrameWidth >= 1920)
        if ((uGetMDPPipeLineEnableType == VENC_DRV_MDP_PIPELINE_TYPE_ONE) && mInputPortDef.format.video.nFrameWidth >= 1920)
        {
            mDoConvertPipeline = true;
        }
        //else if ((mChipName == VAL_CHIP_NAME_ROME || mChipName == VAL_CHIP_NAME_MT8173 || mChipName == VAL_CHIP_NAME_MT6795) && OMX_FALSE == mIsSecureInst)
        else if ((uGetMDPPipeLineEnableType == VENC_DRV_MDP_PIPELINE_TYPE_TWO) && OMX_FALSE == mIsSecureInst)
        {
            //1080p or 720p@60 do pipeline, because they say (ME2?) sometimes input is RGBA format...
            if (mInputPortDef.format.video.nFrameWidth >= 1920 ||
                (mInputPortDef.format.video.nFrameWidth >= 1280 && 60 == (mInputPortDef.format.video.xFramerate >> 16)))
            {
                mDoConvertPipeline = true;
            }
        }
        else
        {
            mDoConvertPipeline = false;
        }*/
    }
    if (mDoConvertPipeline)
    {
        mpConvertInputBufQ  = &mEmptyThisBufQ;
        mpConvertOutputBufQ = &mConvertOutputBufQ;
        mpVencInputBufQ     = &mVencInputBufQ;
        mpVencOutputBufQ    = &mFillThisBufQ;
    }
    else
    {
        mpConvertInputBufQ  = NULL;
        mpConvertOutputBufQ = NULL;
        mpVencInputBufQ     = &mEmptyThisBufQ;
        mpVencOutputBufQ    = &mFillThisBufQ;
    }
}

OMX_ERRORTYPE MtkOmxVenc::QueryVideoProfileLevel(VENC_DRV_VIDEO_FORMAT_T eVideoFormat,
                                                 VAL_UINT32_T u4Profile, VAL_UINT32_T eLevel)
{
    VENC_DRV_QUERY_VIDEO_FORMAT_T qInfo;
    memset(&qInfo, 0, sizeof(qInfo));
    // Query driver to see if supported
    qInfo.eVideoFormat = eVideoFormat;
    qInfo.u4Profile = u4Profile;
    qInfo.eLevel = (VENC_DRV_VIDEO_LEVEL_T)eLevel;
    VENC_DRV_MRESULT_T nDrvRet = VENC_DRV_MRESULT_OK;//eVEncDrvQueryCapability(VENC_DRV_QUERY_TYPE_VIDEO_FORMAT, &qInfo, 0);

    if (VENC_DRV_MRESULT_OK != nDrvRet)
    {
        MTK_OMX_LOGE("QueryVideoProfileLevel(%d) fail, profile(%d)/level(%d)", eVideoFormat, qInfo.u4Profile,
                     qInfo.eLevel);
        return OMX_ErrorNoMore;
    }
    return OMX_ErrorNone;
}

OMX_U32 MtkOmxVenc::getHWLimitSize(OMX_U32 bufferSize)
{
    // for AVC HW VENC Solution
    // xlmtc * ylmt * 64 + (( xlmtc % 8 == 0 ) ? 0 : (( 8 - ( xlmtc % 8 )) * 64 ))
    // worse case is 8 * 64 = 512 bytes

    // don't care platform, always add 512
    return bufferSize + 512;
}

OMX_BOOL MtkOmxVenc::CheckNeedOutDummy(void)
{
  if(mWFDMode == OMX_TRUE && mEnableDummy == OMX_TRUE ||
    (mHaveAVCHybridPlatform && (mIsHybridCodec))) //fix cts issue ALPS03040612 of miss last FBD
  {
      if (OMX_FALSE == mIsSecureSrc)
      {
         return OMX_TRUE;
      }
   }
   return OMX_FALSE;
}

//Codecs on each platform may be different kinds of solution.
//That's why we check codec ID in thie function.
bool MtkOmxVenc::isHWSolution(void)
{
    switch (mCodecId)
    {
        case MTK_VENC_CODEC_ID_AVC:
            {
                // !(70 80 D2 8167)
                VENC_DRV_MRESULT_T mReturn = VENC_DRV_MRESULT_OK;
                OMX_U32 uGetIsNoTHWSolution = 0; //todo VENC_DRV_GET_TYPE_SW_SOLUTION
                //mReturn = eVEncDrvGetParam((VAL_HANDLE_T)NULL, VENC_DRV_GET_TYPE_SW_SOLUTION, (VAL_VOID_T *)&mChipName, (VAL_VOID_T *)&uGetIsNoTHWSolution);
                MTK_OMX_LOGD("uGetIsNoTHWSolution%d", uGetIsNoTHWSolution);

                if(!uGetIsNoTHWSolution)
                {
                    return true;
                }
                else
                {
                    return false;
                }
            }
        default:
            return false;
            break;
    }
    return false;
}

OMX_BOOL MtkOmxVenc::WaitFence(OMX_U8 *mBufHdr, OMX_BOOL mWaitFence)
{
    if (OMX_TRUE == mStoreMetaDataInBuffers)
    {
        VideoNativeMetadata &nativeMeta = *(VideoNativeMetadata *)(mBufHdr);
        //MTK_OMX_LOGD(" nativeMeta.eType %d, fd: %x", nativeMeta.eType, nativeMeta.nFenceFd);
        if(kMetadataBufferTypeANWBuffer == nativeMeta.eType)
        {
            if( 0 <= nativeMeta.nFenceFd )
            {
                MTK_OMX_LOGD_ENG(" %s for fence %d", (OMX_TRUE == mWaitFence?"wait":"noWait"), nativeMeta.nFenceFd);

                //OMX_FLASE for flush and other FBD without getFrmBuffer case
                //should close FD directly
                if(OMX_TRUE == mWaitFence)
                {
                    // Construct a new Fence object to manage a given fence file descriptor.
                    // When the new Fence object is destructed the file descriptor will be
                    // closed.
                    // from: frameworks\native\include\ui\Fence.h
                    sp<Fence> fence = new Fence(nativeMeta.nFenceFd);
                    int64_t startTime = getTickCountUs();
                    status_t ret = fence->wait(IOMX::kFenceTimeoutMs);
                    int64_t duration = getTickCountUs() - startTime;
                    //Log waning on long duration. 10ms is an empirical value.
                    if (duration >= 10000){
                        MTK_OMX_LOGD("ret %x, wait fence %d took %lld us", ret, nativeMeta.nFenceFd, (long long)duration);
                    }
                }
                else
                {
                    //Fence::~Fence() would close fd automatically so encoder should not close
                    close(nativeMeta.nFenceFd);
                }
                //client need close and set -1 after waiting fence
                nativeMeta.nFenceFd = -1;
            }
        }
    }
    return OMX_TRUE;
}

bool MtkOmxVenc::allowEncodeVideo(int inputIdx, int outputIdx)
{
    //allow encode do one frame
    return ((inputIdx >= 0 && outputIdx >= 0) ||
            (mIsHybridCodec && !mDoConvertPipeline &&
             (mMeetHybridEOS && inputIdx < 0) && outputIdx >= 0));
}

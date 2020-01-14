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
 *   MtkOmxVdecEx.cpp
 *
 * Project:
 * --------
 *   MT65xx
 *
 * Description:
 * ------------
 *   MTK OMX Video Decoder component
 *
 * Author:
 * -------
 *   Morris Yang (mtk03147)
 *
 ****************************************************************************/

#include "stdio.h"
#include "string.h"
#include <cutils/log.h>
#include <signal.h>
#include <sys/mman.h>
//#include "ColorConverter.h"
//#include <media/stagefright/MetaData.h>
#include <ui/Rect.h>
#include "MtkOmxVdecEx.h"
#include "OMX_Index.h"

#include <media/stagefright/foundation/ADebug.h>
#include "DpBlitStream.h"
//#include "../MtkOmxVenc/MtkOmxMVAMgr.h"
#include "MtkOmxMVAMgr.h"

#if 1 // for VAL_CHIP_NAME_MT6755 || VAL_CHIP_NAME_DENALI_3
#include <linux/svp_region.h>
#endif

#ifdef HAVE_AEE_FEATURE
#include "aee.h"
#endif

#if (ANDROID_VER >= ANDROID_ICS)
#include <android/native_window.h>
#include <HardwareAPI.h>
//#include <gralloc_priv.h>
#include <ui/GraphicBufferMapper.h>
#include <ui/gralloc_extra.h>
#include <ion.h>
#include <linux/mtk_ion.h>
#include "graphics_mtk_defs.h"
#include <utils/threads.h>
#include <poll.h>
#include "gralloc_mtk_defs.h"  //for GRALLOC_USAGE_SECURE
#endif
#include <system/window.h>

pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

/* this segment add for logd enalbe control*/
char OmxVdecLogValue[PROPERTY_VALUE_MAX];
char OmxVdecPerfLogValue[PROPERTY_VALUE_MAX];
OMX_BOOL bOmxVdecLogEnable = OMX_FALSE;
OMX_BOOL bOmxVdecPerfLogEnable = OMX_FALSE;

#define MTK_SW_RESOLUTION_CHANGE_SUPPORT
#define MTK_OMX_H263_DECODER "OMX.MTK.VIDEO.DECODER.H263"
#define MTK_OMX_MPEG4_DECODER  "OMX.MTK.VIDEO.DECODER.MPEG4"
#define MTK_OMX_AVC_DECODER "OMX.MTK.VIDEO.DECODER.AVC"
#define MTK_OMX_AVC_SEC_DECODER "OMX.MTK.VIDEO.DECODER.AVC.secure"
#define MTK_OMX_RV_DECODER "OMX.MTK.VIDEO.DECODER.RV"
#define MTK_OMX_VC1_DECODER "OMX.MTK.VIDEO.DECODER.VC1"
#define MTK_OMX_VPX_DECODER "OMX.MTK.VIDEO.DECODER.VPX"
#define MTK_OMX_VP9_DECODER "OMX.MTK.VIDEO.DECODER.VP9"
#define MTK_OMX_MPEG2_DECODER "OMX.MTK.VIDEO.DECODER.MPEG2"
#define MTK_OMX_DIVX_DECODER "OMX.MTK.VIDEO.DECODER.DIVX"
#define MTK_OMX_DIVX3_DECODER "OMX.MTK.VIDEO.DECODER.DIVX3"
#define MTK_OMX_XVID_DECODER "OMX.MTK.VIDEO.DECODER.XVID"
#define MTK_OMX_S263_DECODER "OMX.MTK.VIDEO.DECODER.S263"
#define MTK_OMX_HEVC_DECODER "OMX.MTK.VIDEO.DECODER.HEVC"
#define MTK_OMX_HEVC_SEC_DECODER "OMX.MTK.VIDEO.DECODER.HEVC.secure"  //HEVC.SEC.M0
#define MTK_OMX_MJPEG_DECODER "OMX.MTK.VIDEO.DECODER.MJPEG"

#define CACHE_LINE_SIZE 64 // LCM of all supported cache line size


const uint32_t FPS_PROFILE_COUNT = 30;

#if 1
//#if PROFILING
//static FILE *fpVdoProfiling;

int64_t getTickCountMs()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)(tv.tv_sec * 1000LL + tv.tv_usec / 1000);
}

static int64_t getTickCountUs()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)(tv.tv_sec * 1000000LL + tv.tv_usec);
}

void show_uptime()
{
    char cmdline[1024];
    int r;

    //static fd = open("/proc/uptime", 0);  //O_RDONLY=0
    static int fd = 0;
    if (fd == 0)
    {
        r = 0;
        fd = open("/proc/uptime", 0);
    }
    else
    {
        lseek(fd, 0, SEEK_SET);
        r = read(fd, cmdline, 1023);
        //close(fd);
        if (r < 0) { r = 0; }
    }
    cmdline[r] = 0;
    ALOGD("uptime - %s", cmdline);
}
#endif

void __setBufParameter(buffer_handle_t hnd, int32_t mask, int32_t value)
{
    gralloc_extra_ion_sf_info_t info;
    gralloc_extra_query(hnd, GRALLOC_EXTRA_GET_IOCTL_ION_SF_INFO, &info);
    gralloc_extra_sf_set_status(&info, mask, value);
    gralloc_extra_perform(hnd, GRALLOC_EXTRA_SET_IOCTL_ION_SF_INFO, &info);
}

void writeBufferToFile
(
    const char *filename,
    void       *buffer,
    uint32_t    length
)
{
    FILE *fp;
    ALOGD("writeBufferToFile: filename=%s, buffer=0x%p, length=%d bytes", filename, buffer, length);
    fp = fopen(filename, "ab");
    if (fp)
    {
        fwrite(buffer, 1, length, fp);
        fclose(fp);
    }
    else
    {
        ALOGE("writeBufferToFile: fopen failed.");
    }
}

// TODO: remove
void FakeVdecDrvDecode(MtkOmxVdec *pVdec, OMX_BUFFERHEADERTYPE *pInputBuf, OMX_BUFFERHEADERTYPE *pOutputBuf)
{
    ALOGD("[0x%08x] FakeVdecDrvDecode TS=%lld", pVdec, pInputBuf->nTimeStamp);

    pOutputBuf->nFilledLen = (176 * 144 * 3 >> 1);
    pOutputBuf->nTimeStamp = pInputBuf->nTimeStamp;
    pOutputBuf->nOffset = 0;
    MTK_OMX_MEMSET(pOutputBuf->pBuffer, 0xFF, pOutputBuf->nFilledLen);

    SLEEP_MS(30); // fake decode 30ms

    pVdec->HandleEmptyBufferDone(pInputBuf);
    pVdec->HandleFillBufferDone(pOutputBuf, pVdec->mRealCallBackFillBufferDone);
}


CmdThreadRequestHandler::CmdThreadRequestHandler()
{
    INIT_MUTEX(mMutex);
    mRequest = 0;
}

CmdThreadRequestHandler::~CmdThreadRequestHandler()
{
    DESTROY_MUTEX(mMutex);
}

void CmdThreadRequestHandler::setRequest(OMX_U32 req)
{
    LOCK(mMutex);
    mRequest |= req;
    UNLOCK(mMutex);
}

OMX_U32 CmdThreadRequestHandler::getRequest()
{
    return mRequest;
}

void CmdThreadRequestHandler::clearRequest(OMX_U32 req)
{
    LOCK(mMutex);
    mRequest &= ~req;
    UNLOCK(mMutex);
}


#ifdef DYNAMIC_PRIORITY_ADJUSTMENT
void MtkOmxVdec::PriorityAdjustment()
{
    if (!(mPropFlags & MTK_OMX_VDEC_ENABLE_PRIORITY_ADJUSTMENT))
    {
        return;
    }

    if ((mChipName != VAL_CHIP_NAME_MT6572) &&
        (mChipName != VAL_CHIP_NAME_MT6582) &&
        (mChipName != VAL_CHIP_NAME_MT6592))
    {
        return;
    }

    // for UI response improvement
#if 1
    mRRSlidingWindowCnt--;
    if (mRRSlidingWindowCnt == 0)   // check cpu loading
    {
        VAL_VCODEC_CPU_LOADING_INFO_T loadingInfo;
        loadingInfo._cpu_idle_time = 0;
        loadingInfo._thread_cpu_time = 0;
        loadingInfo._sched_clock = 0;
        loadingInfo._inst_count = 0;

        mRRSlidingWindowCnt = mRRSlidingWindowLength;
        mRRCntCurWindow = 0;
    }
    else
    {
        // check mRRSlidingWindowLimit to see if we need to force NORMAL priority
    }
#endif
    // ]

    switch (mCodecId)
    {
        case MTK_VDEC_CODEC_ID_HEVC:
        case MTK_VDEC_CODEC_ID_AVC:
        case MTK_VDEC_CODEC_ID_MPEG4:
        case MTK_VDEC_CODEC_ID_H263:
        case MTK_VDEC_CODEC_ID_VC1:
        case MTK_VDEC_CODEC_ID_MPEG2:
        case MTK_VDEC_CODEC_ID_VPX:
        case MTK_VDEC_CODEC_ID_VP9:
        case MTK_VDEC_CODEC_ID_MJPEG:
        {
            int64_t currTimeStamp;
            currTimeStamp = mAVSyncTime;

            //if ((get_sem_value(&mDecodeSem)) > 0 && (mNumPendingOutput < mPendingOutputThreshold)) {
            if ((currTimeStamp < mllLastDispTime - mTimeThreshold && mllLastDispTime > 1000000) || mErrorCount >= 90)
            {
                mErrorCount = 0;
                // trying to yield CPU
                EnableRRPriority(OMX_FALSE);
                //usleep(1);
                sched_yield();
            }
            else
            {
                // Change the scheduling policy to SCHED_RR
                EnableRRPriority(OMX_TRUE);
            }
            break;
        }

        case MTK_VDEC_CODEC_ID_RV:
        {
            // Morris Yang 20111229 [
            //if (((get_sem_value(&mDecodeSem)) > 0 && (mNumPendingOutput < mPendingOutputThreshold)) || mErrorCount >= 90) {
            if (((mllLastDispTime > 1000000) && ((mNumAllDispAvailOutput - mNumNotDispAvailOutput) >= 3)) || mErrorCount >= 90)
            {
                // ]
                mErrorCount = 0;
                // trying to yield CPU
                EnableRRPriority(OMX_FALSE);
                //usleep(1);
                sched_yield();
            }
            else
            {
                // Change the scheduling policy to SCHED_RR
                EnableRRPriority(OMX_TRUE);
            }
            break;
        }
        default:
            MTK_OMX_LOGE("MtkOmxVdec::PriorityAdjustment invalid codec id (%d)", mCodecId);
            break;
    }
}
#endif

void MtkOmxVdec::EnableRRPriority(OMX_BOOL bEnable)
{
    struct sched_param sched_p;
    sched_getparam(0, &sched_p);
    sched_p.sched_priority = 0;

    int priority = 0;

    if ((mChipName != VAL_CHIP_NAME_MT6572) &&
        (mChipName != VAL_CHIP_NAME_MT6582) &&
        (mChipName != VAL_CHIP_NAME_MT6592))
    {
        androidSetThreadPriority(0, ANDROID_PRIORITY_FOREGROUND);
        return;
    }

    // for UI response improvement
#if 1 // Morris Yang 20120112 mark temporarily
    if (bEnable)
    {
        if (mRRCntCurWindow > mRRSlidingWindowLimit)
        {
            MTK_OMX_LOGD("@@ exceed RR limit");
            bEnable = OMX_FALSE;
        }
        mRRCntCurWindow++;
    }
#endif

    if (bEnable)
    {
        if (mCurrentSchedPolicy != SCHED_RR)
        {
            mCurrentSchedPolicy = SCHED_RR;
            sched_p.sched_priority = RT_THREAD_PRI_OMX_VIDEO; //RTPM_PRIO_OMX_VIDEO_DECODE; // RT Priority
        }
        else
        {
            //MTK_OMX_LOGD ("!!!!! already RR priority");
            return;
        }
    }
    else
    {
        if (mCurrentSchedPolicy != SCHED_NORMAL)
        {
            mCurrentSchedPolicy = SCHED_NORMAL;
            sched_p.sched_priority = 0;
        }
        else
        {
            //MTK_OMX_LOGD ("!!!!! already NORMAL priority");
            return;
        }
    }

    if (0 != sched_setscheduler(0, mCurrentSchedPolicy, &sched_p))
    {
        MTK_OMX_LOGE("[%s] failed, errno: %d", __func__, errno);
    }
    else
    {
        sched_p.sched_priority = -1;
        sched_getparam(0, &sched_p);

        if (bEnable)
        {
            MTK_OMX_LOGD("%06x !!!!! to RR %d", this, sched_p.sched_priority);
        }
        else
        {
            MTK_OMX_LOGD("%06x !!!!! to NOR %d", this, sched_p.sched_priority);
        }
    }

    if (OMX_TRUE == mCodecTidInitialized)
    {
        for (unsigned int i = 0 ; i < mNumCodecThreads ; i++)
        {
            //struct sched_param sched_p;
            if (0 != sched_getparam(mCodecTids[i], &sched_p))
            {
                MTK_OMX_LOGE("1 [%s] failed, errno: %d", __func__, errno);
            }
            if (mCurrentSchedPolicy == SCHED_RR)
            {
                sched_p.sched_priority = RT_THREAD_PRI_OMX_VIDEO; //RTPM_PRIO_OMX_VIDEO_DECODE; // RT Priority
            }
            else   //SCHED_NORMAL
            {
                sched_p.sched_priority = 0;
            }

            if (0 != sched_setscheduler(mCodecTids[i], mCurrentSchedPolicy, &sched_p))
            {
                MTK_OMX_LOGE("2 [%s] failed, errno: %d", __func__, errno);
            }
        }
    }
}


#if (ANDROID_VER >= ANDROID_KK)
OMX_BOOL MtkOmxVdec::SetupMetaIonHandle(OMX_BUFFERHEADERTYPE *pBufHdr)
{
    OMX_U32 graphicBufHandle = 0;

    if (OMX_FALSE == GetMetaHandleFromOmxHeader(pBufHdr, &graphicBufHandle))
    {
        MTK_OMX_LOGE("SetupMetaIonHandle failed, LINE:%d", __LINE__);
        return OMX_FALSE;
    }

    if (0 == graphicBufHandle)
    {
        MTK_OMX_LOGE("SetupMetaIonHandle failed, LINE:%d", __LINE__);
        return OMX_FALSE;
    }

    VBufInfo info;
    int ret = mOutputMVAMgr->getOmxInfoFromHndl((void *)graphicBufHandle, &info);
    if (ret >= 0)
    {
        // found handle
        //MTK_OMX_LOGD("SetupMetaIonHandle found handle, u4BuffHdr(0x%08x)", pBufHdr);
        return OMX_TRUE;
    }
    else
    {
        // cannot found handle, create a new entry
        //MTK_OMX_LOGD("SetupMetaIonHandle cannot find handle, create a new entry,LINE:%d", __LINE__);
    }

    int count = 0;

    ret = mOutputMVAMgr->newOmxMVAwithHndl((void *)graphicBufHandle, (void *)pBufHdr);
    if (ret < 0)
    {
        MTK_OMX_LOGE("[ERROR]newOmxMVAwithHndl() failed");
    }

    OMX_U32 buffer, bufferSize = 0;
    VBufInfo BufInfo;
    if (mOutputMVAMgr->getOmxInfoFromHndl((void *)graphicBufHandle, &BufInfo) < 0)
    {
        MTK_OMX_LOGE("[ERROR][ION][Output]Can't find Frm in mOutputMVAMgr,LINE:%d", __LINE__);
        return OMX_FALSE;
    }

    buffer = BufInfo.u4VA;
    bufferSize = BufInfo.u4BuffSize;;

    gralloc_extra_ion_sf_info_t sf_info;
    memset(&sf_info, 0, sizeof(gralloc_extra_ion_sf_info_t));
    gralloc_extra_query((buffer_handle_t)graphicBufHandle, GRALLOC_EXTRA_GET_IOCTL_ION_SF_INFO, &sf_info);

    if ((sf_info.status & GRALLOC_EXTRA_MASK_TYPE) != GRALLOC_EXTRA_BIT_TYPE_VIDEO)
    {
        OMX_U32 u4PicAllocSize = mOutputStrideBeforeReconfig * mOutputSliceHeightBeforeReconfig;
        MTK_OMX_LOGD("First allocated buffer memset to black u4PicAllocSize %d, buffSize %d", u4PicAllocSize, bufferSize);
        //set default color to black
        memset((void *)(buffer + u4PicAllocSize), 128, u4PicAllocSize / 2);
        memset((void *)(buffer), 0x10, u4PicAllocSize);
    }


    buffer_handle_t _handle = NULL;
    _handle = (buffer_handle_t)graphicBufHandle;
    //ret = mOutputMVAMgr->getMapHndlFromIndex(count, &_handle);

    if (_handle != NULL)
    {
        gralloc_extra_ion_sf_info_t sf_info;
        //MTK_OMX_LOGD ("gralloc_extra_query");
        memset(&sf_info, 0, sizeof(gralloc_extra_ion_sf_info_t));

        gralloc_extra_query(_handle, GRALLOC_EXTRA_GET_IOCTL_ION_SF_INFO, &sf_info);

        sf_info.pool_id = (int32_t)this; //  for PQ to identify bitstream instance.

        gralloc_extra_sf_set_status(&sf_info,
                                    GRALLOC_EXTRA_MASK_TYPE | GRALLOC_EXTRA_MASK_DIRTY,
                                    GRALLOC_EXTRA_BIT_TYPE_VIDEO | GRALLOC_EXTRA_BIT_DIRTY);

        gralloc_extra_perform(_handle, GRALLOC_EXTRA_SET_IOCTL_ION_SF_INFO, &sf_info);
        //MTK_OMX_LOGD("gralloc_extra_perform");
    }
    else
    {
        MTK_OMX_LOGE("SetupMetaIonHandle pool id not set , DC failed,graphicBufHandle value %d\n",graphicBufHandle);
    }
//
    mIonOutputBufferCount++;
    MTK_OMX_LOGD("pBuffer(0x%08x),VA(0x%08X), PA(0x%08X), size(%d) mIonOutputBufferCount(%d)", pBufHdr, BufInfo.u4VA, BufInfo.u4PA, bufferSize, mIonOutputBufferCount);

    if (mIonOutputBufferCount >= mOutputPortDef.nBufferCountActual)
    {
        //MTK_OMX_LOGD("SetupMetaIonHandle ERROR: Cannot found empty entry");
        //MTK_OMX_LOGE("SetupMetaIonHandle Warning: mIonOutputBufferCount %d,u4BuffHdr(0x%08x),graphicBufHandle(0x%08x)" ,mIonOutputBufferCount, pBufHdr,graphicBufHandle );
        //return OMX_FALSE;
    }
    return OMX_TRUE;


    MTK_OMX_LOGD("SetupMetaIonHandle ERROR: LINE:%d", __LINE__);
    return OMX_FALSE;
}

OMX_BOOL MtkOmxVdec::SetupMetaIonHandleAndGetFrame(VDEC_DRV_FRAMEBUF_T *aFrame, OMX_U8 *pBuffer)
{
    // check if we had this handle first
    OMX_U32 graphicBufHandle = 0;
    if (OMX_FALSE == GetMetaHandleFromBufferPtr(pBuffer, &graphicBufHandle))
    {
        MTK_OMX_LOGE("SetupMetaIonHandleAndGetFrame failed, LINE:%d", __LINE__);
        return OMX_FALSE;
    }

    if (0 == graphicBufHandle)
    {
        MTK_OMX_LOGE("SetupMetaIonHandleAndGetFrame failed, LINE:%d", __LINE__);
        return OMX_FALSE;
    }

    OMX_U32 buffer, bufferSize = 0;
    VBufInfo BufInfo;
    int ret = 0;
    if (mOutputMVAMgr->getOmxInfoFromHndl((void *)graphicBufHandle, &BufInfo) < 0)
    {
        MTK_OMX_LOGE("[ERROR][ION][Output]Can't find Frm in mOutputMVAMgr,LINE:%d", __LINE__);
        MTK_OMX_LOGE("SetupMetaIonHandleAndGetFrame failed, cannot found handle, LINE:%d", __LINE__);
        // Add new entry
        ret = mOutputMVAMgr->newOmxMVAwithHndl((void *)graphicBufHandle, NULL);

        if (ret < 0)
        {
            MTK_OMX_LOGE("[ERROR]newOmxMVAwithHndl() failed");
        }

        if (mOutputMVAMgr->getOmxInfoFromHndl((void *)graphicBufHandle, &BufInfo) < 0)
        {
            MTK_OMX_LOGE("[ERROR][ION][Output]Can't find Frm in mOutputMVAMgr,LINE:%d", __LINE__);
            return OMX_FALSE;
        }
    }

    aFrame->rBaseAddr.u4VA = BufInfo.u4VA;
    aFrame->rBaseAddr.u4PA = BufInfo.u4PA;

    // TODO: FREE ION related resource

    return OMX_TRUE;
}
#endif

OMX_BOOL MtkOmxVdec::GetM4UFrameandBitstreamBuffer(VDEC_DRV_FRAMEBUF_T *aFrame, OMX_U8 *aInputBuf, OMX_U32 aInputSize, OMX_U8 *aOutputBuf)
{
    MTK_OMX_LOGD("deprecated");
    return OMX_TRUE;
}

OMX_BOOL MtkOmxVdec::GetBitstreamBuffer(OMX_U8 *aInputBuf, OMX_U32 aInputSize)
{
    //MTK_OMX_LOGD("[M4U] mM4UBufferCount = %d\n", mM4UBufferCount);

    VAL_UINT32_T u4x, u4y;

    if (OMX_TRUE == mIsSecureInst)
    {
        mRingbuf.rSecMemHandle = (unsigned long)aInputBuf;
        mRingbuf.rBase.u4Size = aInputSize;
        mRingbuf.rBase.u4VA = 0;
        MTK_OMX_LOGD("[INFO] GetM4UFrameandBitstreamBuffer mRingbuf.rSecMemHandle(0x%08X), mRingbuf.rBase.u4Size(%d)", mRingbuf.rSecMemHandle, mRingbuf.rBase.u4Size);
    }
    else
    {
        VBufInfo bufInfo;
        int ret = mInputMVAMgr->getOmxInfoFromVA((void *)aInputBuf, &bufInfo);
        if (ret < 0)
        {
            MTK_OMX_LOGE("[ERROR][ION][Input][VideoDecode],line %d\n", __LINE__);
            return OMX_FALSE;
        }
        mRingbuf.rBase.u4VA = bufInfo.u4VA;
        mRingbuf.rBase.u4PA = bufInfo.u4PA;
        mRingbuf.rBase.u4Size = aInputSize;
        mRingbuf.u4Read = bufInfo.u4VA;
        mRingbuf.u4Write = bufInfo.u4VA + aInputSize;
    }

    /*
        MTK_OMX_LOGD("[M4U] mRingbuf.rBase.u4VA = 0x%x, mRingbuf.rBase.u4PA = 0x%x, mRingbuf.rBase.u4Size = %d, mRingbuf.u4Read = 0x%x, mRingbuf.u4Write = 0x%x",
            mRingbuf.rBase.u4VA, mRingbuf.rBase.u4PA, mRingbuf.rBase.u4Size, mRingbuf.u4Read, mRingbuf.u4Write);
        */
    return OMX_TRUE;
}

//HEVC.SEC.M0
OMX_BOOL MtkOmxVdec::SetupMetaSecureHandleAndGetFrame(VDEC_DRV_FRAMEBUF_T *aFrame, OMX_U8 *pBuffer)
{
    // check if we had this handle first
    OMX_U32 graphicBufHandle = 0;
    if (OMX_FALSE == GetMetaHandleFromBufferPtr(pBuffer, &graphicBufHandle))
    {
        MTK_OMX_LOGE("SetupMetaSecureHandleAndGetFrame failed, LINE:%d", __LINE__);
        return OMX_FALSE;
    }

    if (0 == graphicBufHandle)
    {
        MTK_OMX_LOGE("SetupMetaSecureHandleAndGetFrame failed, LINE:%d", __LINE__);
        return OMX_FALSE;
    }

    int i = 0;
    for (i = 0 ; i < mOutputPortDef.nBufferCountActual ; i++)
    {
        if ((void *)graphicBufHandle == mSecFrmBufInfo[i].pNativeHandle)
        {
            // found handle, setup VA/PA from table
            aFrame->rBaseAddr.u4VA = 0x200;
            aFrame->rBaseAddr.u4PA = 0x200;
            aFrame->rSecMemHandle = mSecFrmBufInfo[i].u4SecHandle;
            //MTK_OMX_LOGE("SetupMetaSecureHandleAndGetFrame@@ aFrame->rSecMemHandle(0x%08X)", aFrame->rSecMemHandle);
            return OMX_TRUE;
        }
    }

    if (i == mOutputPortDef.nBufferCountActual)
    {
        MTK_OMX_LOGE("SetupMetaSecureHandleAndGetFrame failed, cannot found handle, LINE:%d", __LINE__);
        return OMX_FALSE;
    }

    return OMX_TRUE;

}

OMX_BOOL MtkOmxVdec::GetM4UFrame(VDEC_DRV_FRAMEBUF_T *aFrame, OMX_U8 *aOutputBuf)
{
    VAL_UINT32_T u4x, u4y;

#if (ANDROID_VER >= ANDROID_KK)
    if (OMX_TRUE == mStoreMetaDataInBuffers)
    {
        // TODO: Extract buffer VA/PA from graphic buffer handle
        if (mIsSecureInst == OMX_TRUE) //HEVC.SEC.M0
        {
            return SetupMetaSecureHandleAndGetFrame(aFrame, aOutputBuf);
        }
        else
        {
            return SetupMetaIonHandleAndGetFrame(aFrame, aOutputBuf);
        }
    }
#endif

    if (aFrame == NULL)
    {
        return OMX_FALSE;
    }

    if (OMX_TRUE == mOutputUseION)
    {
        VBufInfo bufInfo;
        int ret = mOutputMVAMgr->getOmxInfoFromVA((void *)aOutputBuf, &bufInfo);
        if (ret < 0)
        {
            MTK_OMX_LOGE("[ERROR][ION][Output][VideoDecode]Can't find Frm in MVAMgr,line %d\n", __LINE__);
            return OMX_FALSE;

        }
        aFrame->rBaseAddr.u4VA = bufInfo.u4VA;
        aFrame->rBaseAddr.u4PA = bufInfo.u4PA;

        if (OMX_TRUE == mIsSecureInst)
        {
            aFrame->rSecMemHandle = bufInfo.secure_handle;
            //aFrame->rFrameBufVaShareHandle = mIonBufferInfo[u4y].va_share_handle;
            aFrame->rFrameBufVaShareHandle = 0;
            MTK_OMX_LOGE("@@ aFrame->rSecMemHandle(0x%08X), aFrame->rFrameBufVaShareHandle(0x%08X)", aFrame->rSecMemHandle, aFrame->rFrameBufVaShareHandle);
        }

        // MTK_OMX_LOGD("[ION] frame->rBaseAddr.u4VA = 0x%x, frame->rBaseAddr.u4PA = 0x%x", aFrame->rBaseAddr.u4VA, aFrame->rBaseAddr.u4PA);
    }
    else if (OMX_TRUE == mIsSecureInst)
    {
        for (u4y = 0; u4y < mSecFrmBufCount; u4y++)
        {
            //MTK_OMX_LOGE ("@@ aOutputBuf(0x%08X), mSecFrmBufInfo[%d].u4BuffId(0x%08X)", aOutputBuf, u4y, mSecFrmBufInfo[u4y].u4BuffId);
            if (mSecFrmBufInfo[u4y].u4BuffId == (VAL_UINT32_T)aOutputBuf)
            {
                aFrame->rBaseAddr.u4VA = 0x200;
                aFrame->rBaseAddr.u4PA = 0x200;
                aFrame->rSecMemHandle = mSecFrmBufInfo[u4y].u4SecHandle;
                MTK_OMX_LOGE("@@ aFrame->rSecMemHandle(0x%08X)", aFrame->rSecMemHandle);
                break;
            }
        }

        if (u4y == mSecFrmBufCount)
        {
            MTK_OMX_LOGE("[ERROR][SECURE][output][VideoDecode]\n");
            return OMX_FALSE;
        }
    }
    else  // M4U
    {
        VBufInfo bufInfo;
        int ret = mOutputMVAMgr->getOmxInfoFromVA((void *)aOutputBuf, &bufInfo);
        if (ret < 0)
        {
            MTK_OMX_LOGE("[ERROR][M4U][Output][VideoDecode]Can't find Frm in MVAMgr,line %d\n", __LINE__);
            return OMX_FALSE;
        }
        aFrame->rBaseAddr.u4VA = bufInfo.u4VA;
        aFrame->rBaseAddr.u4PA = bufInfo.u4PA;
    }

    return OMX_TRUE;
}


OMX_BOOL MtkOmxVdec::ConfigIonBuffer(int ion_fd, int handle)
{

    struct ion_mm_data mm_data;
    mm_data.mm_cmd = ION_MM_CONFIG_BUFFER;
    mm_data.config_buffer_param.handle = handle;
    mm_data.config_buffer_param.eModuleID = eVideoGetM4UModuleID(VAL_MEM_CODEC_FOR_VDEC);
    mm_data.config_buffer_param.security = 0;
    mm_data.config_buffer_param.coherent = 0;

    if (ion_custom_ioctl(ion_fd, ION_CMD_MULTIMEDIA, &mm_data))
    {
        MTK_OMX_LOGE("[ERROR] cannot configure buffer");
        return OMX_FALSE;
    }

    return OMX_TRUE;
}

VAL_UINT32_T MtkOmxVdec::GetIonPhysicalAddress(int ion_fd, int handle)
{
    // query physical address
    struct ion_sys_data sys_data;
    sys_data.sys_cmd = ION_SYS_GET_PHYS;
    sys_data.get_phys_param.handle = handle;
    if (ion_custom_ioctl(ion_fd, ION_CMD_SYSTEM, &sys_data))
    {
        MTK_OMX_LOGE("[ERROR] cannot get buffer physical address");
        return 0;
    }
    return (VAL_UINT32_T)sys_data.get_phys_param.phy_addr;
}

void MtkOmxVdec::dequeueBitstreamBuffer()
{
    //if (1 == mMtkV4L2Device.IsStreamOn(true))
    {
        int bitstreamIndex = -1;
        OMX_BUFFERHEADERTYPE *pBuffHdr = NULL;
        OMX_BOOL bFlushAll = OMX_FALSE;
        int isLastBitstream = 0;

        do
        {
            mMtkV4L2Device.dequeueBitstream(&bitstreamIndex, &isLastBitstream);

            if (-1 != bitstreamIndex)
            {
                if (bitstreamIndex == mInputPortDef.nBufferCountActual)
                {
                    MTK_OMX_LOGD("Got flush buffer(idx:%d) @ dequeueBitstreamBuffer()", bitstreamIndex);
                }
                else
                {
                    pBuffHdr = mInputBufferHdrs[bitstreamIndex];
                    GetFreeInputBuffer(bFlushAll, pBuffHdr);
                }
            }

            if ((1 == isLastBitstream) && mInputFlushALL)
            {
                MTK_OMX_LOGD("Signal mFlushBitstreamBufferDoneSem");
                SIGNAL(mFlushBitstreamBufferDoneSem);
            }
        }
        while (-1 != bitstreamIndex);
    }
}

void MtkOmxVdec::dequeueFrameBuffer()
{
    //if (1 == mMtkV4L2Device.IsStreamOn(false))
    {
        OMX_BUFFERHEADERTYPE *ipOutBuf = NULL;
        VDEC_DRV_FRAMEBUF_T *FrameBuf  = NULL;
        OMX_TICKS      timestamp       = -1;
        OMX_S32 isLastFrame            = 0;

        do
        {
            ipOutBuf = GetDisplayBuffer((mFixedMaxBuffer == OMX_TRUE) ? OMX_TRUE : OMX_FALSE, &FrameBuf, &timestamp, &isLastFrame);

            if (NULL != ipOutBuf)
            {
                ValidateAndRemovePTS(timestamp);

                if (OMX_FALSE == mEOSFound)
                {
                    ipOutBuf->nTimeStamp = timestamp; // use V4L2 dequeued timestamp
                }
                else
                {
                    // These are the frames we don't wanna display (After EOS frame)
                    ipOutBuf->nTimeStamp = -1;
                    ipOutBuf->nFilledLen = 0;
                }

                OMX_BOOL bEOSTS_Frame = (mEOSQueued == OMX_TRUE && mEOSTS == timestamp && mEOSTS != -1) ? OMX_TRUE : OMX_FALSE;
                OMX_BOOL bEOSFlag_Frame = (isLastFrame == 2) ? OMX_TRUE : OMX_FALSE;
                OMX_BOOL bV4L2LastFrame = (isLastFrame == 1) ? OMX_TRUE : OMX_FALSE;

                if (OMX_FALSE == mEOSFound && (bEOSTS_Frame || bEOSFlag_Frame))
                {
                    ipOutBuf->nFlags |= OMX_BUFFERFLAG_EOS;
                    mEOSFound = OMX_TRUE;
                    MTK_OMX_LOGD("EOS Frame was dequeued. bEOSTS_Frame(%d), bV4L2LastFrame(%d), isLastFrame(%d), mEOSTS(%lld), tempTS(%lld)",
                                                                          bEOSTS_Frame, bV4L2LastFrame, isLastFrame, mEOSTS, timestamp);
                }
                else
                {
                    //MTK_OMX_LOGD("EOS Frame was NOT dequeued. isLastFrame(%d), mEOSTS(%lld), tempTS(%lld)", isLastFrame, mEOSTS, timestamp);
                }

                mllLastUpdateTime = timestamp;
                mllLastDispTime = ipOutBuf->nTimeStamp;


                MTK_OMX_LOGD("%06x dequeueBuffers. frame (0x%08X) (0x%08X) %lld (%u) GET_DISP i(%d) frm_buf(0x%08X), flags(0x%08x)",
                             this, ipOutBuf, ipOutBuf->pBuffer, ipOutBuf->nTimeStamp, ipOutBuf->nFilledLen,
                             mGET_DISP_i, mGET_DISP_tmp_frame_addr, ipOutBuf->nFlags);

                CheckFreeBuffer(OMX_FALSE);
                MTK_OMX_LOGD("mFillThisBufQ size %d, mBufColorConvertDstQ %d, mBufColorConvertSrcQ %d, queuedFrameBufferCount %d",
                             mFillThisBufQ.size(), mBufColorConvertDstQ.size(),
                             mBufColorConvertSrcQ.size(), mMtkV4L2Device.queuedFrameBufferCount());


                HandleFillBufferDone(ipOutBuf, mRealCallBackFillBufferDone);

                if (OMX_TRUE == mEOSFound || OMX_TRUE == bV4L2LastFrame)
                {
                    int queuedFrameBufferCount = mMtkV4L2Device.queuedFrameBufferCount();
                    if (queuedFrameBufferCount == 0 && mFlushInProcess)
                    {
                        MTK_OMX_LOGD("Signal mFlushFrameBufferDoneSem(%d). isLastFrame(%d), queuedFrameBufferCount(%d)", get_sem_value(&mFlushFrameBufferDoneSem), isLastFrame, queuedFrameBufferCount);
                        SIGNAL(mFlushFrameBufferDoneSem);
                    }
                    else
                    {
                        //MTK_OMX_LOGD("Still need to DQ YUV(%d)", queuedFrameBufferCount);
                    }
                }



            }
        }
        while (NULL != ipOutBuf);

#if 0
        if (OMX_TRUE == mEOSFound || OMX_TRUE == isLastFrame)
        {
            int queuedFrameBufferCount = mMtkV4L2Device.queuedFrameBufferCount();
            if (queuedFrameBufferCount == 0)
            {
                MTK_OMX_LOGD("Signal mFlushFrameBufferDoneSem. isLastFrame(%d), queuedFrameBufferCount(%d)", isLastFrame, queuedFrameBufferCount);
                SIGNAL(mFlushFrameBufferDoneSem);
            }
            else
            {
                MTK_OMX_LOGD("Still need to DQ YUV(%d)", queuedFrameBufferCount);
            }
        }
#endif
    }
}

void MtkOmxVdec::dequeueBuffers()
{
    // Try to DQ bitstream buffer
    dequeueBitstreamBuffer();

    // Try to DQ YUV buffer
    dequeueFrameBuffer();

}

OMX_BOOL MtkOmxVdec::queueBitstreamBuffer(int *input_idx, OMX_BOOL *needContinue)
{
	OMX_BUFFERHEADERTYPE *toBeQueuedBuffer = NULL;
    mInputBuffInuse = OMX_TRUE;
    *needContinue = OMX_FALSE;

    //*input_idx = DequeueInputBuffer();
    int status = TRY_LOCK(mDecodeLock);
    if (status == 0)
    {
        *input_idx = GetInputBufferFromETBQ();

        if ((*input_idx < 0))
        {
            //SLEEP_MS(2); // v4l2 todo...
            //sched_yield();
            //ALOGD("MtkOmxVdecDecodeThread No input buffer.....Let's continue");
            //UNLOCK(mDecodeLock);
            *needContinue = OMX_TRUE;
            UNLOCK(mDecodeLock);
            goto QUEUEBITSTREAMBUFFER_EXIT;
        }

        // v4l2 todo: this is a workaround. It should be checked later

        toBeQueuedBuffer = mInputBufferHdrs[*input_idx];
        int inputIonFd;
        getIonFdByHeaderIndex(*input_idx, &inputIonFd, -1, NULL);


        //mpCurrInput = mInputBufferHdrs[*input_idx];
        //ALOGD("MtkOmxVdecDecodeThread mpCurrInput->nTimeStamp = %lld", mpCurrInput->nTimeStamp);


        //
        // Prepare to StreamOnBitstream and queue bitstream
        //
        // V4L2 todo: need to queue YUV buffer at another proper location
        if (0 == mMtkV4L2Device.IsStreamOn(kBitstreamQueue))
        {
            //
            // bitstream part
            //
            mMtkV4L2Device.requestBufferBitstream(mInputPortDef.nBufferCountActual);

            if (0 == mMtkV4L2Device.queueBitstream(*input_idx, inputIonFd, toBeQueuedBuffer->nFilledLen, mInputPortDef.nBufferSize, toBeQueuedBuffer->nTimeStamp, toBeQueuedBuffer->nFlags))
            {
                //HandleEmptyBufferDone(mpCurrInput);
                MTK_OMX_LOGD("queueBitstream idx(%d) TS(%lld) Len(%d) Fail. Try later", *input_idx, toBeQueuedBuffer->nTimeStamp, toBeQueuedBuffer->nFilledLen);
            }
            else
            {
                // queueBitstream successfully.
                //   insert and sort time stamp
                {
                    mpCurrInput = toBeQueuedBuffer;
                    ALOGD("MtkOmxVdecDecodeThread mpCurrInput->nTimeStamp = %lld", mpCurrInput->nTimeStamp);
                    RemoveInputBufferFromETBQ();
                    preInputIdx = *input_idx;

                    if (0 != toBeQueuedBuffer->nFilledLen && 0 != mpCurrInput->nTimeStamp)
                    {
                        if (InsertionSortForInputPTS(mpCurrInput->nTimeStamp) == OMX_FALSE)
                        {
                            ALOGE("Insert PTS error");
                            HandleEmptyBufferDone(mpCurrInput);

                            mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                                   mAppData,
                                                   OMX_EventError,
                                                   OMX_ErrorOverflow,
                                                   NULL,
                                                   NULL);

                            //return OMX_FALSE;
                            ALOGD("(Break) Insert PTS fail! mpCurrInput->nTimeStamp(%lld)", mpCurrInput->nTimeStamp);
                            UNLOCK(mDecodeLock);
                            goto QUEUEBITSTREAMBUFFER_ERR;
                        }
                    }
                }

                mMtkV4L2Device.StreamOnBitstream();

            }

            if (checkIfNeedPortReconfig() == OMX_TRUE)
            {
                handleResolutionChange();
                *needContinue = OMX_TRUE;
            }
        }
        else
        {
            // We have already SteamOn Bitstream

            // v4l2 todo : queue all bitstream buffer first for efficiency
            // queue bistream

            if (0 == mMtkV4L2Device.queueBitstream(*input_idx, inputIonFd, toBeQueuedBuffer->nFilledLen, mInputPortDef.nBufferSize,  toBeQueuedBuffer->nTimeStamp, toBeQueuedBuffer->nFlags))
            {
                //HandleEmptyBufferDone(mpCurrInput);
                MTK_OMX_LOGD("queueBitstream idx(%d) TS(%lld) Len(%d) Fail. Try Later", *input_idx, toBeQueuedBuffer->nTimeStamp, toBeQueuedBuffer->nFilledLen);
            }
            else
            {
                mpCurrInput = toBeQueuedBuffer;
                ALOGD("MtkOmxVdecDecodeThread mpCurrInput->nTimeStamp = %lld, flag(0x%08x)", mpCurrInput->nTimeStamp, toBeQueuedBuffer->nFlags);
                RemoveInputBufferFromETBQ();

                // insert and sort time stamp
                {
                    if (InsertionSortForInputPTS(mpCurrInput->nTimeStamp) == OMX_FALSE)
                    {
                        ALOGE("Insert PTS error");
                        HandleEmptyBufferDone(mpCurrInput);

                        mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                               mAppData,
                                               OMX_EventError,
                                               OMX_ErrorOverflow,
                                               NULL,
                                               NULL);

                        //return OMX_FALSE;
                        ALOGD("(Break) Insert PTS fail! mpCurrInput->nTimeStamp(%lld)", mpCurrInput->nTimeStamp);
                        UNLOCK(mDecodeLock);
                        goto QUEUEBITSTREAMBUFFER_ERR;
                    }
                }
            }
        }

        UNLOCK(mDecodeLock);
    }
    else
    {
        *needContinue = OMX_TRUE;
        goto QUEUEBITSTREAMBUFFER_EXIT;
    }


QUEUEBITSTREAMBUFFER_EXIT:
    return OMX_TRUE;

QUEUEBITSTREAMBUFFER_ERR:
    return OMX_FALSE;
}

void MtkOmxVdec::StreamOnFrameBufferQueue()
{
    MTK_OMX_LOGD("StreamOnFrameBufferQueue\n");

    //
    // (12) Prepare to StreamOnFrameBuffer and queue framebuffer
    //
    if (0 == mMtkV4L2Device.IsStreamOn(kFrameBufferQueue))
    {
        //
        // frmaebuffer part
        //
        mMtkV4L2Device.requestBufferFrameBuffer(mOutputPortDef.nBufferCountActual);

        VDEC_DRV_FRAMEBUF_T *pFrameOutput = NULL;
        int ionFD, frameBufferIndex;

        OMXGetOutputBufferFd(&pFrameOutput, &frameBufferIndex, &ionFD, 0, VAL_TRUE, NULL);

        // v4l2 todo: need to modify byteUsed as the correct value
        if (-1 != frameBufferIndex)
        {
            if (false == mMtkV4L2Device.queueFrameBuffer(frameBufferIndex, ionFD, mOutputPortDef.format.video.nFrameWidth*mOutputPortDef.format.video.nFrameHeight))
            {
                OMX_BUFFERHEADERTYPE *ipOutputBuffer = mOutputBufferHdrs[frameBufferIndex];
				ipOutputBuffer->nTimeStamp = -1;
                ipOutputBuffer->nFilledLen = 0;
                HandleFillBufferDone(ipOutputBuffer, OMX_TRUE);
                MTK_OMX_LOGD("(StreamOn FB) Failed to queueFrameBuffer idx(%d), BufferHeader(0x%08x). FBD directly", frameBufferIndex, ipOutputBuffer);
            }
            else
            {
                mMtkV4L2Device.StreamOnFrameBuffer();
                mEverCallback = OMX_TRUE;
            }
        }
    }
}

void MtkOmxVdec::queueFrameBuffer()
{
    if (1 == mMtkV4L2Device.IsStreamOn(kFrameBufferQueue))
    {
        // queue frame buffer
        VDEC_DRV_FRAMEBUF_T *pFrameOutput = NULL;
        int outputIonFD, frameBufferIndex;

        int status = TRY_LOCK(mDecodeLock);
        if (status == 0)
        {
            OMXGetOutputBufferFd(&pFrameOutput, &frameBufferIndex, &outputIonFD, 0, VAL_TRUE, NULL);

            // v4l2 todo: need to modify byteUsed as the correct value
            if (-1 != frameBufferIndex)
            {
            if (0 == mMtkV4L2Device.queueFrameBuffer(frameBufferIndex, outputIonFD, mOutputPortDef.format.video.nFrameWidth*mOutputPortDef.format.video.nFrameHeight))
                {
                    OMX_BUFFERHEADERTYPE *ipOutputBuffer = mOutputBufferHdrs[frameBufferIndex];
                    ipOutputBuffer->nTimeStamp = -1;
                    ipOutputBuffer->nFilledLen = 0;
                    HandleFillBufferDone(ipOutputBuffer, OMX_TRUE);
                    MTK_OMX_LOGD("(StreamOn FB) Failed to queueFrameBuffer idx(%d), BufferHeader(0x%08x). FBD directly", frameBufferIndex, ipOutputBuffer);
                }
                else
                {
                    mEverCallback = OMX_TRUE;
                }
            }

            UNLOCK(mDecodeLock);
        }
    }
}

OMX_BOOL MtkOmxVdec::queueBuffers(int *input_idx, OMX_BOOL *needContinue)
{
    MTK_OMX_LOGD("queueBuffers\n");

    *needContinue = OMX_FALSE;

    if (mPortReconfigInProgress)
    {
        *needContinue = OMX_TRUE;
        goto QUEUEBUFFER_EXIT;
    }

    queueFrameBuffer();

    //
    // prepare an input buffer
    //
    if (OMX_FALSE == queueBitstreamBuffer(input_idx, needContinue))
    {
        goto QUEUEBUFFER_ERR;
    }
    else
    {
        if (*input_idx < 0 && preInputIdx != -1 &&
            mMtkV4L2Device.IsStreamOn(kBitstreamQueue) > 0 &&
            mMtkV4L2Device.queuedBitstreamCount() > 0 &&
            mDecoderInitCompleteFlag == OMX_FALSE)
        {
            *input_idx = preInputIdx;
            *needContinue = OMX_FALSE;
            goto QUEUEBUFFER_EXIT;
        }
        else if (OMX_TRUE == *needContinue)
        {
            goto QUEUEBUFFER_EXIT;
        }
    }

    StreamOnFrameBufferQueue();

QUEUEBUFFER_EXIT:
    return OMX_TRUE;

QUEUEBUFFER_ERR:
    *needContinue = OMX_FALSE;
    return OMX_FALSE;
}

OMX_BOOL MtkOmxVdec::checkIfNeedPortReconfig()
{
    if (mThumbnailMode == OMX_TRUE)
        return OMX_FALSE;

    // v4l2 todo: need to refine these code.
    //
    // Get sequence info
    //
    unsigned int formatWidth, formatHeight;
    while (mMtkV4L2Device.getCapFmt(&formatWidth, &formatHeight) == 1)
    {
        ALOGD("[Info] MtkOmxVdecDecodeThread GetCapFmt not ready. Try again...\n");
        //Todo: should we add a time-out
    }
    mSeqInfo.u4Width = formatWidth;
    mSeqInfo.u4Height = formatHeight;
    ALOGD("[Info] MtkOmxVdecDecodeThread GetCapFmt is ready. width(%d), height(%d)..\n", formatWidth, formatHeight);

    //
    // Get color format
    //
    VDEC_DRV_PIXEL_FORMAT_T ePixelFormat;
    OMX_COLOR_FORMATTYPE colorFormat;
    if (0 == mMtkV4L2Device.getPixelFormat(&ePixelFormat))
    {
        MTK_OMX_LOGE("[ERROR] Cannot get color format");
    }

    switch (ePixelFormat)
    {
        case VDEC_DRV_PIXEL_FORMAT_YUV_420_PLANER:
            colorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Planar;
            break;

        case VDEC_DRV_PIXEL_FORMAT_YUV_420_PLANER_MTK:
            colorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatVendorMTKYUV;
            break;

        case VDEC_DRV_PIXEL_FORMAT_YUV_YV12:
            colorFormat = (OMX_COLOR_FORMATTYPE)OMX_MTK_COLOR_FormatYV12;
            break;
        case VDEC_DRV_PIXEL_FORMAT_YUV_420_PLANER_UFO:
            colorFormat = OMX_COLOR_FormatVendorMTKYUV_UFO;
            break;
        case VDEC_DRV_PIXEL_FORMAT_YUV_NV12:
            colorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
            break;
        default:
            MTK_OMX_LOGE("[ERROR] Cannot get color format %d", colorFormat);
            break;
    }

    //
    // Get buffer count
    //
    unsigned int u4ChagnedDPBSize, u4CheckBufferCount;
    if (OMX_FALSE == mIsSecureInst)
    {
        if (1 != mMtkV4L2Device.getDPBSize(&u4ChagnedDPBSize))
        {
            MTK_OMX_LOGE("[ERROR] Cannot get param: VDEC_DRV_GET_TYPE_QUERY_VIDEO_DPB_SIZE");
        }
    }
    else
    {
        MTK_OMX_LOGE("[Info][2] Secure Video, mDPBSize = 16");
        u4ChagnedDPBSize = 16;
    }

    MTK_OMX_LOGE("[Info] mDPBSize = %d", u4ChagnedDPBSize);

    if(u4ChagnedDPBSize>16)
    {
        u4ChagnedDPBSize = 16;
    }

    mDPBSize = u4ChagnedDPBSize;

    {
        u4CheckBufferCount = u4ChagnedDPBSize + mMinUndequeuedBufs + FRAMEWORK_OVERHEAD + MAX_COLORCONVERT_OUTPUTBUFFER_COUNT;
    }

    mSeqInfoCompleteFlag = OMX_TRUE;

    if (colorFormat != mOutputPortDef.format.video.eColorFormat ||
        u4CheckBufferCount > mOutputPortDef.nBufferCountActual ||
        mOutputPortDef.format.video.nFrameWidth != formatWidth ||
        mOutputPortDef.format.video.nFrameHeight != formatHeight ||
        mOutputPortDef.format.video.nStride != VDEC_ROUND_N(formatWidth, mQInfoOut.u4StrideAlign) ||
        mOutputPortDef.format.video.nSliceHeight != VDEC_ROUND_N(formatHeight, mQInfoOut.u4SliceHeightAlign))
    {
        MTK_OMX_LOGE("[Info] NeedPortReconfig eColorFormat(%d->%d), BufCount(%d->%d), width(%d->%d), Height(%d->%d), Stride(%d->%d), SliceHeight(%d->%d)",
        mOutputPortDef.format.video.eColorFormat, colorFormat,
        mOutputPortDef.nBufferCountActual,u4CheckBufferCount,mOutputPortDef.format.video.nFrameWidth,formatWidth,
        mOutputPortDef.format.video.nFrameHeight, formatHeight, mOutputPortDef.format.video.nStride, VDEC_ROUND_N(formatWidth, mQInfoOut.u4StrideAlign),
        mOutputPortDef.format.video.nSliceHeight, VDEC_ROUND_N(formatHeight, mQInfoOut.u4SliceHeightAlign));
        return OMX_TRUE;
    }

    return OMX_FALSE;
}

OMX_BOOL MtkOmxVdec::getReconfigOutputPortSetting()
{
    VDEC_DRV_PICINFO_T rPicInfo;

    unsigned int formatWidth, formatHeight;
    VDEC_DRV_PIXEL_FORMAT_T ePixelFormat;
    v4l2_crop temp_ccop_info;
    v4l2_bitdepth_info bitDepthInfo;
    VAL_UINT32_T u4ChagnedDPBSize = 0;
    VAL_UINT32_T u4CheckBufferCount = 0;
    OMX_COLOR_FORMATTYPE colorFormat;

    //
    // 1. get frame width height colorformat
    //
    if (0 != mMtkV4L2Device.getCapFmt(&formatWidth, &formatHeight))
    {
        MTK_OMX_LOGE("[ERROR] Cannot get capture format");
        goto GET_RECONFIG_SETTING_FAIL;
    }

    if (0 == mMtkV4L2Device.getPixelFormat(&ePixelFormat))
    {
        MTK_OMX_LOGE("[ERROR] Cannot get color format");
        goto GET_RECONFIG_SETTING_FAIL;
    }

    switch (ePixelFormat)
    {
        case VDEC_DRV_PIXEL_FORMAT_YUV_420_PLANER:
            colorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Planar;
            break;

        case VDEC_DRV_PIXEL_FORMAT_YUV_420_PLANER_MTK:
            colorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatVendorMTKYUV;
            break;

        case VDEC_DRV_PIXEL_FORMAT_YUV_YV12:
            colorFormat = (OMX_COLOR_FORMATTYPE)OMX_MTK_COLOR_FormatYV12;
            break;
        case VDEC_DRV_PIXEL_FORMAT_YUV_420_PLANER_UFO:
            colorFormat = OMX_COLOR_FormatVendorMTKYUV_UFO;
            break;
        case VDEC_DRV_PIXEL_FORMAT_YUV_NV12:
            colorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
            break;
        default:
            MTK_OMX_LOGE("[ERROR] Cannot get color format %d", colorFormat);
            break;
    }

    if (1 == mMtkV4L2Device.getCrop(&temp_ccop_info))
    {
        mCropLeft = temp_ccop_info.c.left;
        mCropTop = temp_ccop_info.c.top;
        mCropWidth = temp_ccop_info.c.width;
        mCropHeight = temp_ccop_info.c.height;
    }
    else
    {
        mCropLeft = 0;
        mCropTop = 0;
        mCropWidth = formatWidth - 1;
        mCropHeight = formatHeight - 1;
    }
    mReconfigOutputPortSettings.u4Width = formatWidth;
    mReconfigOutputPortSettings.u4Height = formatHeight;
    mReconfigOutputPortSettings.u4RealWidth = mCropWidth;
    mReconfigOutputPortSettings.u4RealHeight = mCropHeight;
    mReconfigOutputPortColorFormat = colorFormat;

    MTK_OMX_LOGD("getReconfigOutputPortSetting() mCropLeft %d, mCropTop %d, mCropWidth %d, mCropHeight %d\n",
                 mCropLeft, mCropTop, mCropWidth, mCropHeight);

    //
    // 2. get buffer count
    //
    if (OMX_FALSE == mIsSecureInst)
    {
        if (1 != mMtkV4L2Device.getDPBSize(&u4ChagnedDPBSize))
        {
            MTK_OMX_LOGE("[ERROR] Cannot get param: VDEC_DRV_GET_TYPE_QUERY_VIDEO_DPB_SIZE");
            goto GET_RECONFIG_SETTING_FAIL;
        }
    }
    else
    {
        MTK_OMX_LOGE("[Info][2] Secure Video, mDPBSize = 16");
        u4ChagnedDPBSize = 16;
    }

    {
        u4CheckBufferCount = u4ChagnedDPBSize + mMinUndequeuedBufs + FRAMEWORK_OVERHEAD + MAX_COLORCONVERT_OUTPUTBUFFER_COUNT;
    }

    if (mThumbnailMode == OMX_FALSE && u4CheckBufferCount > mOutputPortDef.nBufferCountActual)
    {
        mReconfigOutputPortBufferCount = u4CheckBufferCount;
        mOutputPortDef.nBufferCountMin = u4CheckBufferCount - mMinUndequeuedBufs;
    }
    else
    {
        mReconfigOutputPortBufferCount = mOutputPortDef.nBufferCountActual;
        mOutputPortDef.nBufferCountMin = mOutputPortDef.nBufferCountActual - mMinUndequeuedBufs;
    }
    mDPBSize = u4ChagnedDPBSize;

    //
    // 3. get buffer size
    //
    if (0 == mMtkV4L2Device.getBitDepthInfo(&bitDepthInfo))
    {
        MTK_OMX_LOGE("[ERROR] Cannot get bit depth info");
        goto GET_RECONFIG_SETTING_FAIL;
    }
    if ((bitDepthInfo.bitDepthLuma == 10 || bitDepthInfo.bitDepthChroma == 10) && OMX_FALSE == mbIs10Bit)
    {
        mbIs10Bit = OMX_TRUE;
        mIsHorizontalScaninLSB = rPicInfo.bIsHorizontalScaninLSB;
    }

    if (meDecodeType != VDEC_DRV_DECODER_MTK_SOFTWARE)
    {
        mReconfigOutputPortBufferSize = (mReconfigOutputPortSettings.u4RealWidth * mReconfigOutputPortSettings.u4RealHeight * 3 >> 1) + 16;
    }
    else
    {
        mReconfigOutputPortBufferSize = (mReconfigOutputPortSettings.u4RealWidth * (mReconfigOutputPortSettings.u4RealHeight + 1) * 3) >> 1;
    }

    if (OMX_TRUE == mbIs10Bit)
    {
        mReconfigOutputPortBufferSize *= 1.25;
    }


    MTK_OMX_LOGD("--- getReconfigOutputPortSetting --- (%d %d %d %d %d %d %d) -> (%d %d %d %d %d %d %d)",
                     mOutputPortDef.format.video.nFrameWidth, mOutputPortDef.format.video.nFrameHeight,
                     mOutputPortDef.format.video.nStride, mOutputPortDef.format.video.nSliceHeight,
                     mOutputPortDef.nBufferCountActual, mOutputPortDef.nBufferSize, mOutputPortDef.format.video.eColorFormat,
                     mReconfigOutputPortSettings.u4Width, mReconfigOutputPortSettings.u4Height,
                     mReconfigOutputPortSettings.u4RealWidth, mReconfigOutputPortSettings.u4RealHeight,
                     mReconfigOutputPortBufferCount, mReconfigOutputPortBufferSize, mReconfigOutputPortColorFormat);

    return OMX_TRUE;

GET_RECONFIG_SETTING_FAIL:
    return OMX_FALSE;

}

void MtkOmxVdec::handleResolutionChange()
{
    MTK_OMX_LOGD("MtkOmxVdec::handleResolutionChange");

	mPortReconfigInProgress = OMX_TRUE;

    mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                               mAppData,
                               OMX_EventPortSettingsChanged,
                               MTK_OMX_OUTPUT_PORT,
                               NULL,
                               NULL);
}

void MtkOmxVdec::handlePendingEvent()
{
    int resolutionChangeEvent = 0;

    resolutionChangeEvent = mMtkV4L2Device.dequeueEvent();

    if (resolutionChangeEvent)
    {
        handleResolutionChange();
    }
}

OMX_BOOL MtkOmxVdec::isThereCmdThreadRequest()
{
    OMX_U32 request = mCmdThreadRequestHandler.getRequest();

    if (request == 0)
    {
        return OMX_FALSE;
    }
    else if (request & MTK_CMD_REQUEST_FLUSH_INPUT_PORT)
    {
        MTK_OMX_LOGD("isThereCmdThreadRequest(): got flush input port request");
        if ((mMtkV4L2Device.flushBitstreamQ() == 0) && mInputFlushALL)
        {
            // no need to flush bitstream buffer
            SIGNAL(mFlushBitstreamBufferDoneSem);
            MTK_OMX_LOGD("isThereCmdThreadRequest(): mFlushBitstreamBufferDoneSem signal");
        }
        mCmdThreadRequestHandler.clearRequest(MTK_CMD_REQUEST_FLUSH_INPUT_PORT);
    }
    else if (request & MTK_CMD_REQUEST_FLUSH_OUTPUT_PORT)
    {
        MTK_OMX_LOGD("isThereCmdThreadRequest(): got flush output port request");
        if (mMtkV4L2Device.flushFrameBufferQ() == 0 && mFlushInProcess)
        {
            // no need to flush frame buffer
            SIGNAL(mFlushFrameBufferDoneSem);
            MTK_OMX_LOGD("isThereCmdThreadRequest(): mFlushFrameBufferDoneSem(%d) signal", get_sem_value(&mFlushFrameBufferDoneSem));
        }
        mCmdThreadRequestHandler.clearRequest(MTK_CMD_REQUEST_FLUSH_OUTPUT_PORT);
    }
    else
    {
        MTK_OMX_LOGE("isThereCmdThreadRequest(): got unknown request");
        return OMX_FALSE;
    }

    return OMX_TRUE;
}

void thread_exit_handler(int sig)
{
    ALOGE("@@ this signal is %d, tid=%d", sig, gettid());
    pthread_exit(0);
}


void *MtkOmxVdecDecodeThread(void *pData)
{
    MtkOmxVdec *pVdec = (MtkOmxVdec *)pData;
    OMX_BOOL needContinue = OMX_FALSE;
    OMX_BOOL ret          = OMX_TRUE;
    int input_idx = -1;

#if ANDROID
    prctl(PR_SET_NAME, (unsigned long) "MtkOmxVdecDecodeThread", 0, 0, 0);
#endif

    // register signal handler
    struct sigaction actions;
    memset(&actions, 0, sizeof(actions));
    sigemptyset(&actions.sa_mask);
    actions.sa_flags = 0;
    actions.sa_handler = thread_exit_handler;
    sigaction(SIGUSR1, &actions, NULL);

    pVdec->EnableRRPriority(OMX_TRUE);

    ALOGD("[0x%08x] MtkOmxVdecDecodeThread created pVdec=0x%08X, tid=%d", pVdec, (unsigned int)pVdec, gettid());

    pVdec->mVdecDecThreadTid = gettid();

    timespec now;

    while (1)
    {
        int retVal = TRY_WAIT(pVdec->mDecodeSem);
        if (0 != retVal)
        {
            // ETB and FTB are both no new buffers.
            //   Let's take a break to slowdonw this thread.
            pthread_mutex_lock(&mut);
            clock_gettime(CLOCK_REALTIME, &now);
            now.tv_nsec += 8000000;
            int err = pthread_cond_timedwait(&cond, &mut, &now);
            if (err == ETIMEDOUT)
            {
                 //ALOGD("- time out %d: dispatch something.../n");
            }
            pthread_mutex_unlock(&mut);
        }

        //ALOGD("## 0x%08x MtkOmxVdecDecodeThread Wait to decode (input: %d, output: %d) ", pVdec,
        //      pVdec->mEmptyThisBufQ.size(),
        //      pVdec->mFillThisBufQ.size());
        pVdec->mInputBuffInuse = OMX_FALSE;
        pVdec->mEverCallback = OMX_FALSE;

        //
        // (1) Check request from cmd thread
        //
        if (OMX_TRUE == pVdec->isThereCmdThreadRequest())
        {
            ALOGD("[0x%08x] MtkOmxVdecDecodeThread got some request from cmd thread.....Let's continue", pVdec);
            continue;
        }

        //
        // (2) Check Alive.
        //       mIsComponentAlive will be set as false when ComponentDeinit()
        if (OMX_FALSE == pVdec->mIsComponentAlive)
        {
            ALOGD("MtkOmxVdecDecodeThread pVdec->mIsComponentAlive = FALSE. break");
            break;
        }

        //
        // (3) Check DecodeStarted
        //       mDecodeStarted will be set as true when entering executing state
        if (pVdec->mDecodeStarted == OMX_FALSE)
        {
            ALOGD("[0x%08x] MtkOmxVdecDecodeThread Wait for decode start.....Let's continue", pVdec);
            SLEEP_MS(2);
            continue;
        }

        // v4l2 todo...
        // (4) poll first
        //       16ms : for 60 fps
        // v4l2 todo: (2) check pending event. NULL --> don't care pending events
        int isTherePendingEvent = 0;
        pVdec->mMtkV4L2Device.devicePoll(&isTherePendingEvent, 16 /*ms*/);

        // v4l2 todo...
        // (5) Handle event, i.e. resolution change
        //
        if (isTherePendingEvent)
        {
            pVdec->handlePendingEvent();
        }

        //
        // (6) Check are there free bitstream/YUV buffers from V4L2.
        //
        pVdec->dequeueBuffers();

        //
        // (7) Check are there available YUV buffers in ETBQ/FTBQ. If yes, queue it.
        //
        ret = pVdec->queueBuffers(&input_idx, &needContinue);
        if (OMX_FALSE == ret)
        {
            break;
        }
        else
        {
            if (OMX_TRUE == needContinue)
            {
                continue;
            }
        }

        // send the input/output buffers to decoder
        if (pVdec->DecodeVideoEx(pVdec->mInputBufferHdrs[input_idx]) == OMX_FALSE)
        {
            pVdec->mErrorInDecoding++;
            ALOGE("[0x%08x] DecodeVideoEx() something wrong when decoding....", pVdec);
            break;
        }

#ifdef DYNAMIC_PRIORITY_ADJUSTMENT
        pVdec->PriorityAdjustment();
#endif

    }

    pVdec->mInputBuffInuse = OMX_FALSE;
    ALOGD("[0x%08x] MtkOmxVdecDecodeThread terminated pVdec=0x%08X", pVdec, (unsigned int)pVdec);
    pVdec->mVdecDecodeThread = NULL;
    return NULL;
}


void *MtkOmxVdecThread(void *pData)
{
    MtkOmxVdec *pVdec = (MtkOmxVdec *)pData;
    ALOGD("[0x%08x] MtkOmxVdecThread created pVdec=0x%08X, tid=%d", pVdec, (unsigned int)pVdec, gettid());

    pVdec->mVdecThreadTid = gettid();

    int status;
    ssize_t ret;

    OMX_COMMANDTYPE cmd;
    OMX_U32 cmdCat;
    OMX_U32 nParam1;
    OMX_PTR pCmdData;

    unsigned int buffer_type;

    struct pollfd PollFd;
    PollFd.fd = pVdec->mCmdPipe[MTK_OMX_PIPE_ID_READ];
    PollFd.events = POLLIN;

    while (1)
    {
        status = poll(&PollFd, 1, -1);
        // WaitForSingleObject
        if (-1 == status)
        {
            ALOGE("[0x%08x] poll error %d (%s), fd:%d", pVdec, errno, strerror(errno), pVdec->mCmdPipe[MTK_OMX_PIPE_ID_READ]);
            //dump fd
            ALOGE("[0x%08x] pipe: %d %d", pVdec, pVdec->mCmdPipe[MTK_OMX_PIPE_ID_READ], pVdec->mCmdPipe[MTK_OMX_PIPE_ID_WRITE]);
            pVdec->FdDebugDump();
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
                READ_PIPE(cmdCat, pVdec->mCmdPipe);
                if (cmdCat == MTK_OMX_GENERAL_COMMAND)
                {
                    READ_PIPE(cmd, pVdec->mCmdPipe);
                    READ_PIPE(nParam1, pVdec->mCmdPipe);
                    ALOGD("[0x%08x] # Got general command (%s)", pVdec, CommandToString(cmd));
                    switch (cmd)
                    {
                        case OMX_CommandStateSet:
                            pVdec->HandleStateSet(nParam1);
                            break;

                        case OMX_CommandPortEnable:
                            pVdec->HandlePortEnable(nParam1);
                            break;

                        case OMX_CommandPortDisable:
                            pVdec->HandlePortDisable(nParam1);
                            break;

                        case OMX_CommandFlush:
                            pVdec->HandlePortFlush(nParam1);
                            break;

                        case OMX_CommandMarkBuffer:
                            READ_PIPE(pCmdData, pVdec->mCmdPipe);
                            pVdec->HandleMarkBuffer(nParam1, pCmdData);

                        default:
                            ALOGE("[0x%08x] Error unhandled command", pVdec);
                            break;
                    }
                }
                else if (cmdCat == MTK_OMX_BUFFER_COMMAND)
                {
                    OMX_BUFFERHEADERTYPE *pBufHead;
                    READ_PIPE(buffer_type, pVdec->mCmdPipe);
                    READ_PIPE(pBufHead, pVdec->mCmdPipe);
                    switch (buffer_type)
                    {
                        case MTK_OMX_EMPTY_THIS_BUFFER_TYPE:
                            //MTK_OMX_LOGD ("## EmptyThisBuffer pBufHead(0x%08X)", pBufHead);
                            //handle input buffer from IL client
                            pVdec->HandleEmptyThisBuffer(pBufHead);
                            break;
                        case MTK_OMX_FILL_THIS_BUFFER_TYPE:
                            //MTK_OMX_LOGD ("## FillThisBuffer pBufHead(0x%08X)", pBufHead);
                            // handle output buffer from IL client
                            pVdec->HandleFillThisBuffer(pBufHead);
                            break;
#if 0
                        case MTK_OMX_EMPTY_BUFFER_DONE_TYPE:
                            //MTK_OMX_LOGD ("## EmptyBufferDone pBufHead(0x%08X)", pBufHead);
                            // TODO: handle return input buffer
                            pVdec->HandleEmptyBufferDone(pBufHead);
                            break;
                        case MTK_OMX _FILL_BUFFER_DONE_TYPE:
                            //MTK_OMX_LOGD ("## FillBufferDone pBufHead(0x%08X)", pBufHead);
                            // TODO: handle return output buffer
                            pVdec->HandleFillBufferDone(pBufHead);
                            break;
#endif
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
                ALOGE("[0x%08x] FD: %d %d, Poll get unsupported event:0x%x", pVdec, pVdec->mCmdPipe[0], pVdec->mCmdPipe[1], PollFd.revents);
                abort();
            }
        }

    }

EXIT:
    ALOGD("[0x%08x] MtkOmxVdecThread terminated", pVdec);
    pVdec->mVdecThread = NULL;
    return NULL;
}

void *MtkOmxVdecConvertThread(void *pData)
{
    MtkOmxVdec *pVdec = (MtkOmxVdec *)pData;
    ALOGD("[0x%08x] MtkOmxVdecConvertThread created pVdec=0x%08X, tid=%d", pVdec, (unsigned int)pVdec, gettid());

    pVdec->mVdecConvertThreadTid = gettid();

    int status;
    ssize_t ret;

    OMX_COMMANDTYPE cmd;
    OMX_U32 cmdCat;
    OMX_U32 nParam1;
    OMX_PTR pCmdData;

    unsigned int buffer_type;

    struct pollfd PollFd;
    PollFd.fd = pVdec->mConvertCmdPipe[MTK_OMX_PIPE_ID_READ];
    PollFd.events = POLLIN;

    while (1)
    {
        status = poll(&PollFd, 1, -1);
        // WaitForSingleObject
        if (-1 == status)
        {
            ALOGE("[0x%08x] poll error %d (%s), fd:%d", pVdec, errno, strerror(errno), pVdec->mConvertCmdPipe[MTK_OMX_PIPE_ID_READ]);
            //dump fd
            ALOGE("[0x%08x] pipe: %d %d", pVdec, pVdec->mConvertCmdPipe[MTK_OMX_PIPE_ID_READ], pVdec->mConvertCmdPipe[MTK_OMX_PIPE_ID_WRITE]);
            pVdec->FdDebugDump();
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
                READ_PIPE(cmdCat, pVdec->mConvertCmdPipe);
                //pVdec->mCountPIPEREAD++;
                //if( (pVdec->mCountPIPEMaxDiff >= 0) && ( pVdec->mCountPIPEMaxDiff < (pVdec->mCountPIPEWRITE - pVdec->mCountPIPEREAD)) )
                //{
                //    pVdec->mCountPIPEMaxDiff = pVdec->mCountPIPEWRITE - pVdec->mCountPIPEREAD;
                //}

                //ALOGD("## MtkOmxVdecConvertThread cmdCat %d", cmdCat);

                if (cmdCat == MTK_OMX_GENERAL_COMMAND)
                {
                    READ_PIPE(cmd, pVdec->mConvertCmdPipe);
                    READ_PIPE(nParam1, pVdec->mConvertCmdPipe);
                    ALOGD("[0x%08x] # Got general command (%s)", pVdec, CommandToString(cmd));
                    switch (cmd)
                    {
                        case OMX_CommandMarkBuffer:
                            READ_PIPE(pCmdData, pVdec->mConvertCmdPipe);
                            pVdec->HandleMarkBuffer(nParam1, pCmdData);
                            break;
                        default:
                            ALOGE("[0x%08x] Error unhandled command %d", pVdec, cmd);
                            break;
                    }
                }
                else if (cmdCat == MTK_OMX_BUFFER_COMMAND)
                {
                    OMX_BUFFERHEADERTYPE *pBufHead;
                    READ_PIPE(buffer_type, pVdec->mConvertCmdPipe);
                    READ_PIPE(pBufHead, pVdec->mConvertCmdPipe);
                    //ALOGD("## buffer_type %d", buffer_type);
                    switch (buffer_type)
                    {
                        case MTK_OMX_FILL_CONVERTED_BUFFER_DONE_TYPE:
                            READ_PIPE(nParam1, pVdec->mConvertCmdPipe);
                            //pVdec->mCountPIPEREADFBD++;
                            //if( (pVdec->mCountPIPEMaxFBDDiff >= 0) && ( pVdec->mCountPIPEMaxFBDDiff < (pVdec->mCountPIPEWRITEFBD - pVdec->mCountPIPEREADFBD)) )
                            //{
                            //    pVdec->mCountPIPEMaxFBDDiff = pVdec->mCountPIPEWRITEFBD - pVdec->mCountPIPEREADFBD;
                            //}
                            //ALOGD("## MtkOmxVdecConvertThread 2 mFlushInProcess %d, (r %d / w %d) diff %d/%d",
                            //pVdec->mFlushInProcess, pVdec->mCountPIPEREADFBD, pVdec->mCountPIPEWRITEFBD, pVdec->mCountPIPEWRITEFBD - pVdec->mCountPIPEREADFBD, pVdec->mCountPIPEMaxFBDDiff);
                            if (OMX_FALSE == pVdec->mFlushInProcess)
                            {
                                pVdec->mFlushInConvertProcess = 1;
                                pVdec->HandleColorConvertForFillBufferDone(nParam1, OMX_FALSE);
                                pVdec->mFlushInConvertProcess = 0;
                            }
                            break;
                        default:
                            ALOGE("[0x%08x] Error unhandled buffer_type %d", pVdec, buffer_type);
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
                ALOGE("[0x%08x] FD: %d %d, Poll get unsupported event:0x%x", pVdec, pVdec->mConvertCmdPipe[0], pVdec->mConvertCmdPipe[1], PollFd.revents);
                abort();
            }
        }

    }

EXIT:
    ALOGD("[0x%08x] MtkOmxVdecConvertThread terminated", pVdec);
    pVdec->mVdecConvertThread = NULL;
    return NULL;
}

void MtkOmxVdec::getChipName(OMX_U32 &chipName)
{
    mMtkV4L2Device.getChipName(&chipName);

    switch (chipName)
    {
        case VAL_CHIP_NAME_MT6516:
            MTK_OMX_LOGE("[MtkOmxVdec]VAL_CHIP_NAME_MT6516");
            break;
        case VAL_CHIP_NAME_MT6571:
            MTK_OMX_LOGE("[MtkOmxVdec]VAL_CHIP_NAME_MT6571");
            break;
        case VAL_CHIP_NAME_MT6572:
            MTK_OMX_LOGE("[MtkOmxVdec]VAL_CHIP_NAME_MT6572");
            break;
        case VAL_CHIP_NAME_MT6573:
            MTK_OMX_LOGE("[MtkOmxVdec]VAL_CHIP_NAME_MT6573");
            break;
        case VAL_CHIP_NAME_MT6575:
            MTK_OMX_LOGE("[MtkOmxVdec]VAL_CHIP_NAME_MT6575");
            break;
        case VAL_CHIP_NAME_MT6577:
            MTK_OMX_LOGE("[MtkOmxVdec]VAL_CHIP_NAME_MT6577");
            break;
        case VAL_CHIP_NAME_MT6589:
            MTK_OMX_LOGE("[MtkOmxVdec]VAL_CHIP_NAME_MT6589");
            break;
        case VAL_CHIP_NAME_MT6582:
            MTK_OMX_LOGE("[MtkOmxVdec]VAL_CHIP_NAME_MT6582");
            break;
        case VAL_CHIP_NAME_MT8135:
            MTK_OMX_LOGE("[MtkOmxVdec]VAL_CHIP_NAME_MT8135");
            break;
        case VAL_CHIP_NAME_ROME:
            MTK_OMX_LOGE("[MtkOmxVdec]VAL_CHIP_NAME_ROME");
            break;
        case VAL_CHIP_NAME_MT6592:
            MTK_OMX_LOGE("[MtkOmxVdec]VAL_CHIP_NAME_MT6592");
            break;
        case VAL_CHIP_NAME_MT8127:
            MTK_OMX_LOGE("[MtkOmxVdec]VAL_CHIP_NAME_MT8127");
            break;
        case VAL_CHIP_NAME_MT6752:
            MTK_OMX_LOGE("[MtkOmxVdec]VAL_CHIP_NAME_MT6752");
            break;
        case VAL_CHIP_NAME_MT6795:
            MTK_OMX_LOGE("[MtkOmxVdec]VAL_CHIP_NAME_MT6795");
            break;
        case VAL_CHIP_NAME_DENALI_1:
            MTK_OMX_LOGE("[MtkOmxVdec]VAL_CHIP_NAME_DENALI_1");
            break;
        case VAL_CHIP_NAME_DENALI_2:
            MTK_OMX_LOGE("[MtkOmxVdec]VAL_CHIP_NAME_DENALI_2");
            break;
        case VAL_CHIP_NAME_DENALI_3:
            MTK_OMX_LOGE("[MtkOmxVdec]VAL_CHIP_NAME_DENALI_3");
            break;
        case VAL_CHIP_NAME_MT8163:
            MTK_OMX_LOGE("[MtkOmxVdec]VAL_CHIP_NAME_MT8163");
            break;
        case VAL_CHIP_NAME_MT6580:
            MTK_OMX_LOGE("[MtkOmxVdec]VAL_CHIP_NAME_MT6580");
            break;
        case VAL_CHIP_NAME_MT6755:
            MTK_OMX_LOGE("[MtkOmxVdec]VAL_CHIP_NAME_MT6755");
            break;
        case VAL_CHIP_NAME_MT6797:
            MTK_OMX_LOGE("[MtkOmxVdec]VAL_CHIP_NAME_MT6797");
            break;
        case VAL_CHIP_NAME_MT8173:
            MTK_OMX_LOGE("[MtkOmxVdec]VAL_CHIP_NAME_MT8173");
            break;
        default:
            MTK_OMX_LOGE("[MtkOmxVdec][ERROR] VAL_CHIP_NAME_UNKNOWN");
            break;
    }

}


MtkOmxVdec::MtkOmxVdec()
{
    MTK_OMX_LOGD("MtkOmxVdec::MtkOmxVdec this= 0x%08X", (unsigned int)this);

    CheckLogEnable();

    getChipName(mChipName);

    MTK_OMX_MEMSET(&mCompHandle, 0x00, sizeof(OMX_COMPONENTTYPE));
    mCompHandle.nSize = sizeof(OMX_COMPONENTTYPE);
    mCompHandle.pComponentPrivate = this;
    mState = OMX_StateInvalid;

    mInputBufferHdrs = NULL;
    mOutputBufferHdrs = NULL;
    mInputBufferPopulatedCnt = 0;
    mOutputBufferPopulatedCnt = 0;
    mPendingStatus = 0;
    mDecodeStarted = OMX_FALSE;
    mPortReconfigInProgress = OMX_FALSE;

    mNumPendingInput = 0;
    mNumPendingOutput = 0;

    mErrorInDecoding = 0;

    mTotalDecodeTime = 0;

    mCodecId = MTK_VDEC_CODEC_ID_INVALID;
    mCurrentSchedPolicy = SCHED_OTHER;

    INIT_MUTEX(mCmdQLock);

    INIT_MUTEX(mConvertCmdQLock);
    INIT_MUTEX(mFillThisConvertBufQLock);
    INIT_MUTEX(mEmptyThisBufQLock);
    INIT_MUTEX(mFillThisBufQLock);
    INIT_MUTEX(mDecodeLock);
    INIT_MUTEX(mWaitDecSemLock);
    INIT_MUTEX(mFillUsedLock);

    INIT_SEMAPHORE(mInPortAllocDoneSem);
    INIT_SEMAPHORE(mOutPortAllocDoneSem);
    INIT_SEMAPHORE(mInPortFreeDoneSem);
    INIT_SEMAPHORE(mOutPortFreeDoneSem);
    INIT_SEMAPHORE(mDecodeSem);
    INIT_SEMAPHORE(mOutputBufferSem);
    INIT_SEMAPHORE(mFlushFrameBufferDoneSem);
    INIT_SEMAPHORE(mFlushBitstreamBufferDoneSem);

    mDecoderInitCompleteFlag = OMX_FALSE;
    mBitStreamBufferAllocated = OMX_FALSE;
    mBitStreamBufferVa = 0;
    mBitStreamBufferPa = 0;
    mFrameBuf = NULL;
    mFrameBufSize = 0;
    mInputBuf = NULL;
    mNumFreeAvailOutput = 0;
    mNumAllDispAvailOutput = 0;
    mNumNotDispAvailOutput = 0;
    mInterlaceChkComplete = OMX_FALSE;
    mIsInterlacing = OMX_FALSE;

    for (mColorConvertDstBufferCount = 0; mColorConvertDstBufferCount < MAX_COLORCONVERT_OUTPUTBUFFER_COUNT; mColorConvertDstBufferCount++)
    {
        mColorConvertDstBufferHdr[mColorConvertDstBufferCount] = 0xffffffff;
    }
    mColorConvertDstBufferCount = 0;

    //default is 1, 2 for crossMount that camera HAL return buffer N after receive N+1
    mMaxColorConvertOutputBufferCnt = MAX_COLORCONVERT_OUTPUTBUFFER_COUNT;
    mCrossMountSupportOn = OMX_FALSE;
    VAL_CHAR_T mMCCValue[PROPERTY_VALUE_MAX];
    if (property_get("ro.mtk_crossmount_support", mMCCValue, NULL))
    {
        int mCCvalue = atoi(mMCCValue);

        if (mCCvalue)
        {
            VAL_CHAR_T mMCCMaxValue[PROPERTY_VALUE_MAX];
            //mCrossMountSupportOn = OMX_TRUE;  //enable via setParameter now
            if (property_get("ro.mtk_crossmount.maxcount", mMCCMaxValue, NULL))
            {
                int mCCMaxvalue = atoi(mMCCMaxValue);
                mMaxColorConvertOutputBufferCnt = mCCMaxvalue;
                if (mMaxColorConvertOutputBufferCnt > (MTK_VDEC_AVC_DEFAULT_OUTPUT_BUFFER_COUNT / 2))
                {
                    mMaxColorConvertOutputBufferCnt = MTK_VDEC_AVC_DEFAULT_OUTPUT_BUFFER_COUNT;
                }
            }
            else
            {
                mMaxColorConvertOutputBufferCnt = MAX_COLORCONVERT_OUTPUTBUFFER_COUNT;
            }
            MTK_OMX_LOGD("[CM] enable CrossMunt and config MaxCC buffer count in OMX component : %d", mMaxColorConvertOutputBufferCnt);
        }
        else
        {
            MTK_OMX_LOGD("keep orignal buffer count : %d", mMaxColorConvertOutputBufferCnt);
        }
    }
    else
    {
        MTK_OMX_LOGD("keep orignal buffer count : %d", mMaxColorConvertOutputBufferCnt);
    }

    VAL_CHAR_T mDIValue[PROPERTY_VALUE_MAX];

    property_get("ro.mtk_deinterlace_support", mDIValue, "0");
    mDeInterlaceEnable = OMX_FALSE;//(VAL_BOOL_T) atoi(mDIValue);

    mFlushInProcess = OMX_FALSE;
#if (ANDROID_VER >= ANDROID_KK)
    mAdaptivePlayback = VAL_TRUE;

    VAL_CHAR_T mAdaptivePlaybackValue[PROPERTY_VALUE_MAX];

    if (mChipName == VAL_CHIP_NAME_MT6572)
    {
        property_get("mtk.omxvdec.ad.vp", mAdaptivePlaybackValue, "0");
    }
    else
    {
        property_get("mtk.omxvdec.ad.vp", mAdaptivePlaybackValue, "1");
    }

    mAdaptivePlayback = (VAL_BOOL_T) atoi(mAdaptivePlaybackValue);
/*
    if (mAdaptivePlayback)
    {
        MTK_OMX_LOGU("Adaptive Playback Enable!");
    }
    else
    {
        MTK_OMX_LOGU("Adaptive Playback Disable!");
    }
*/
#endif

    mRealCallBackFillBufferDone = OMX_TRUE;

    //mRealCallBackFillBufferDone = OMX_TRUE;

    mM4UBufferHandle = VAL_NULL;
    mM4UBufferCount = 0;

    mDPBSize = 0;
    mSeqInfoCompleteFlag = OMX_FALSE;

    eVideoInitMVA((VAL_VOID_T **)&mM4UBufferHandle);
    OMX_U32 i;

    mIonInputBufferCount = 0;
    mIonOutputBufferCount = 0;
    mInputUseION = OMX_FALSE;
    mOutputUseION = OMX_FALSE;
    mIonDevFd = -1;

    mIsClientLocally = OMX_TRUE;
    mIsFromGralloc = OMX_FALSE;

    for (mSecFrmBufCount = 0; mSecFrmBufCount < VIDEO_ION_MAX_BUFFER; mSecFrmBufCount++)
    {
        mSecFrmBufInfo[mSecFrmBufCount].u4BuffId = 0xffffffff;
        mSecFrmBufInfo[mSecFrmBufCount].u4BuffHdr = 0xffffffff;
        mSecFrmBufInfo[mSecFrmBufCount].u4BuffSize = 0xffffffff;
        mSecFrmBufInfo[mSecFrmBufCount].u4SecHandle = 0xffffffff;
        mSecFrmBufInfo[mSecFrmBufCount].pNativeHandle = (void *)0xffffffff;
    }
    mSecFrmBufCount = 0;

    mPropFlags = 0;

#if PROFILING
    /*
        fpVdoProfiling = fopen("//data//VIDEO_DECODE_PROFILING.txt", "ab");

        if (fpVdoProfiling == VAL_NULL)
        {
            MTK_OMX_LOGE("[ERROR] cannot open VIDEO_PROFILING.txt\n");
        }
    */
#endif

#if defined(DYNAMIC_PRIORITY_ADJUSTMENT)
    mllLastDispTime = 0;
#endif
    mllLastUpdateTime = 0;

    //#ifdef MT6577
    mCodecTidInitialized = OMX_FALSE;
    mNumCodecThreads = 0;
    MTK_OMX_MEMSET(mCodecTids, 0x00, sizeof(pid_t) * 8);
    //#endif

#ifdef DYNAMIC_PRIORITY_ADJUSTMENT
    char value[PROPERTY_VALUE_MAX];
    char *delimiter = " ";
    property_get("mtk.omxvdec.enable.priadj", value, "1");
    char *pch = strtok(value, delimiter);
    bool _enable = atoi(pch);
    if (_enable)
    {
        mPropFlags |= MTK_OMX_VDEC_ENABLE_PRIORITY_ADJUSTMENT;
        pch = strtok(NULL, delimiter);
        if (pch == NULL)
        {
            mPendingOutputThreshold = 5;
        }
        else
        {
            mPendingOutputThreshold = atoi(pch);
        }

        MTK_OMX_LOGD("Priority Adjustment enabled (mPendingOutputThreshold=%d)!!!", mPendingOutputThreshold);
    }
    property_get("mtk.omxvdec.enable.priadjts", value, "1");
    pch = strtok(value, delimiter);
    _enable = atoi(pch);
    if (_enable)
    {
        mPropFlags |= MTK_OMX_VDEC_ENABLE_PRIORITY_ADJUSTMENT;
        pch = strtok(NULL, delimiter);
        if (pch == NULL)
        {
            mTimeThreshold = 133000;
        }
        else
        {
            mTimeThreshold = atoi(pch);
        }

        MTK_OMX_LOGD("Priority Adjustment enabled (mTimeThreshold=%lld)!!!", mTimeThreshold);
    }
    mErrorCount = 0;
    EnableRRPriority(OMX_TRUE);
#else
    EnableRRPriority(OMX_TRUE);
#endif
    char value2[PROPERTY_VALUE_MAX];

    property_get("mtk.omxvdec.dump", value2, "0");
    mDumpOutputFrame = (OMX_BOOL) atoi(value2);

    property_get("mtk.omxvdec.dumpProfiling", value2, "0");
    mDumpOutputProfling = (OMX_BOOL) atoi(value2);

    property_get("mtk.omxvdec.output.buf.count", value2, "0");
    mForceOutputBufferCount = atoi(value2);

    // for VC1
    mFrameTsInterval = 0;
    mCurrentFrameTs = 0;
    mFirstFrameRetrieved = OMX_FALSE;
    mResetFirstFrameTs = OMX_FALSE;
    mCorrectTsFromOMX = OMX_FALSE;

    // for H.264
    iTSIn = 0;
    DisplayTSArray[0] = 0;

#if (ANDROID_VER >= ANDROID_ICS)
    mIsUsingNativeBuffers = OMX_FALSE;
#endif

#if (ANDROID_VER >= ANDROID_KK)
    mStoreMetaDataInBuffers = OMX_FALSE;
    mEnableAdaptivePlayback = OMX_FALSE;
    mMaxWidth = 0;
    mMaxHeight = 0;
    mCropLeft = 0;
    mCropTop = 0;
    mCropWidth = 0;
    mCropHeight = 0;
    mEarlyEOS = OMX_FALSE;
#endif

    // for UI response improvement
    mRRSlidingWindowLength = 15;
    mRRSlidingWindowCnt = mRRSlidingWindowLength;
    mRRSlidingWindowLimit = mRRSlidingWindowLength;
    mRRCntCurWindow = 0;
    mLastCpuIdleTime = 0L;
    mLastSchedClock = 0L;

#if CPP_STL_SUPPORT
    mEmptyThisBufQ.clear();
    mFillThisBufQ.clear();
#endif

#if ANDROID
    mEmptyThisBufQ.clear();
    mFillThisBufQ.clear();
#endif
    FNum = 0;

    mBufColorConvertDstQ.clear();
    mBufColorConvertSrcQ.clear();
    mFlushInConvertProcess = 0;

    mMinUndequeuedBufs = MIN_UNDEQUEUED_BUFS;
    mMinUndequeuedBufsDiff = 0;
    mMinUndequeuedBufsFlag = OMX_FALSE;
    mStarvationSize = 0;

    mThumbnailMode = OMX_FALSE;
    mSeekTargetTime = 0;
    mSeekMode = OMX_FALSE;
    mPrepareSeek = OMX_FALSE;

    mbH263InMPEG4 = OMX_FALSE;
    mEOSFound = OMX_FALSE;
    mEOSQueued = OMX_FALSE;
    mEOSTS = -1;
    mFATALError = OMX_FALSE;

    mFailInitCounter = 0;
    mInputZero = OMX_FALSE;
    mIs4kSwAvc = OMX_FALSE;
    mIsVp9Sw4k = OMX_FALSE;

    mCoreGlobal = NULL;
    mStreamingMode = OMX_FALSE;
    mACodecColorConvertMode = OMX_FALSE;
    m3DStereoMode = OMX_VIDEO_H264FPA_2D;
#if 0//def MTK S3D SUPPORT
    mFramesDisplay = 0;
    m3DStereoMode = OMX_VIDEO_H264FPA_NONE;
    s3dAsvd = NULL;
    asvdWorkingBuffer = NULL;
#endif
    mAspectRatioWidth = 1;
    mAspectRatioHeight = 1;

    mFrameInterval = 0;
    meDecodeType = VDEC_DRV_DECODER_MTK_HARDWARE;
    mFixedMaxBuffer = OMX_FALSE; // v4l2 todo

#if 0 //// FIXME
    mGlobalInstData = NULL;
#endif

    mNoReorderMode = OMX_FALSE;

    mIsSecureInst = OMX_FALSE;
    mbShareableMemoryEnabled = OMX_FALSE;
    mTeeType = NONE_TEE;

    mSkipReferenceCheckMode = OMX_FALSE;
    mLowLatencyDecodeMode = OMX_FALSE;
    mFlushDecoderDoneInPortSettingChange = OMX_TRUE;

    mH264SecVdecTlcLib = NULL;
    mTlcHandle = NULL;

    mH264SecVdecInHouseLib = NULL;
    mHEVCSecVdecInHouseLib = NULL; //HEVC.SEC.M0

    mInputAllocateBuffer = OMX_FALSE;
    mOutputAllocateBuffer = OMX_FALSE;

    mAVSyncTime = -1;
    mResetCurrTime = false;

    mGET_DISP_i = 0;
    mGET_DISP_tmp_frame_addr = 0;

    mbYUV420FlexibleMode = OMX_FALSE;

    mInputFlushALL = OMX_FALSE;

    mInputMVAMgr        = new OmxMVAManager("ion", "MtkOmxVdec1");
    mOutputMVAMgr       = new OmxMVAManager("ion", "MtkOmxVdec2");
    mIgnoreGUI = OMX_FALSE;

    mbIs10Bit = VAL_FALSE;
    mIsHorizontalScaninLSB = VAL_FALSE;

    mCmdPipe[0] = -1;
    mCmdPipe[1] = -1;
    mConvertCmdPipe[0] = -1;
    mConvertCmdPipe[1] = -1;

    mFullSpeedOn = false;
    mViLTESupportOn = OMX_FALSE;

    preInputIdx = -1;
}


MtkOmxVdec::~MtkOmxVdec()
{
    MTK_OMX_LOGD("~MtkOmxVdec this= 0x%08X", (unsigned int)this);

    eVideoDeInitMVA(mM4UBufferHandle);

#if PROFILING
    //fclose(fpVdoProfiling);
#endif

    if (mInputBufferHdrs)
    {
        MTK_OMX_FREE(mInputBufferHdrs);
    }

    if (mOutputBufferHdrs)
    {
        MTK_OMX_FREE(mOutputBufferHdrs);
    }

    if (mFrameBuf)
    {
        MTK_OMX_FREE(mFrameBuf);
    }

    mFrameBufSize = 0;
    delete mOutputMVAMgr;
    delete mInputMVAMgr;

    if (mInputBuf)
    {
        MTK_OMX_FREE(mInputBuf);
    }

    if (mBitStreamBufferVa)
    {
        mBitStreamBufferVa = 0;
    }

#if 0//def MTK S3D SUPPORT
    if (s3dAsvd)
    {
        s3dAsvd->AsvdReset();
        s3dAsvd->destroyInstance();
        MTK_OMX_FREE(asvdWorkingBuffer);
        DestroyMutex();
    }
#endif

    if (-1 != mIonDevFd)
    {
        close(mIonDevFd);
    }

    DESTROY_MUTEX(mEmptyThisBufQLock);
    DESTROY_MUTEX(mFillThisBufQLock);
    DESTROY_MUTEX(mDecodeLock);
    DESTROY_MUTEX(mWaitDecSemLock);
    DESTROY_MUTEX(mCmdQLock);
    DESTROY_MUTEX(mFillUsedLock);

    DESTROY_MUTEX(mConvertCmdQLock);
    DESTROY_MUTEX(mFillThisConvertBufQLock);

    DESTROY_SEMAPHORE(mInPortAllocDoneSem);
    DESTROY_SEMAPHORE(mOutPortAllocDoneSem);
    DESTROY_SEMAPHORE(mInPortFreeDoneSem);
    DESTROY_SEMAPHORE(mOutPortFreeDoneSem);
    DESTROY_SEMAPHORE(mDecodeSem);
    DESTROY_SEMAPHORE(mOutputBufferSem);
    DESTROY_SEMAPHORE(mFlushFrameBufferDoneSem);
    DESTROY_SEMAPHORE(mFlushBitstreamBufferDoneSem);

    //#ifdef MTK_SEC_VIDEO_PATH_SUPPORT
    //#ifdef TRUSTONIC_TEE_SUPPORT
#if 0
    if (NULL != mTlcHandle)
    {
        MtkH264SecVdec_tlcClose_Ptr *pfnMtkH264SecVdec_tlcClose = (MtkH264SecVdec_tlcClose_Ptr *) dlsym(mH264SecVdecTlcLib, MTK_H264_SEC_VDEC_TLC_CLOSE_NAME);
        if (NULL == pfnMtkH264SecVdec_tlcClose)
        {
            MTK_OMX_LOGE("cannot find MtkH264SecVdec_tlcClose, LINE: %d", __LINE__);
        }
        else
        {
            pfnMtkH264SecVdec_tlcClose(mTlcHandle);
            mTlcHandle = NULL;
        }
    }
#endif
    if (NULL != mH264SecVdecTlcLib)
    {
        dlclose(mH264SecVdecTlcLib);
    }
    if (NULL != mH264SecVdecInHouseLib)
    {
        dlclose(mH264SecVdecInHouseLib);
    }
    if (NULL != mHEVCSecVdecInHouseLib) //HEVC.SEC.M0
    {
        dlclose(mHEVCSecVdecInHouseLib);
    }
    //#endif
    //#endif
}

OMX_BOOL MtkOmxVdec::initCodecParam(OMX_STRING componentName)
{
    if (!strcmp(componentName, MTK_OMX_H263_DECODER))
    {
        if (OMX_FALSE == InitH263Params())
        {
            goto INIT_FAIL;
        }
        mCodecId = MTK_VDEC_CODEC_ID_H263;
    }
    else if (!strcmp(componentName, MTK_OMX_MPEG4_DECODER))
    {
        if (OMX_FALSE == InitMpeg4Params())
        {
            goto INIT_FAIL;
        }
        mCodecId = MTK_VDEC_CODEC_ID_MPEG4;
    }
    else if (!strcmp(componentName, MTK_OMX_AVC_DECODER))
    {
        if (OMX_FALSE == InitAvcParams())
        {
            goto INIT_FAIL;
        }
        mCodecId = MTK_VDEC_CODEC_ID_AVC;
    }
    else if (!strcmp(componentName, MTK_OMX_RV_DECODER))
    {
        if (OMX_FALSE == InitRvParams())
        {
            goto INIT_FAIL;
        }
        mCodecId = MTK_VDEC_CODEC_ID_RV;
    }
    else if (!strcmp(componentName, MTK_OMX_VC1_DECODER))
    {
        if (OMX_FALSE == InitVc1Params())
        {
            goto INIT_FAIL;
        }
        mCodecId = MTK_VDEC_CODEC_ID_VC1;
    }
    else if (!strcmp(componentName, MTK_OMX_VPX_DECODER))
    {
        if (OMX_FALSE == InitVpxParams())
        {
            goto INIT_FAIL;
        }
        mCodecId = MTK_VDEC_CODEC_ID_VPX;
    }
    else if (!strcmp(componentName, MTK_OMX_VP9_DECODER))
    {
        if (OMX_FALSE == InitVp9Params())
        {
            goto INIT_FAIL;
        }
        mCodecId = MTK_VDEC_CODEC_ID_VP9;
    }
    else if (!strcmp(componentName, MTK_OMX_MPEG2_DECODER))
    {
        if (OMX_FALSE == InitMpeg2Params())
        {
            goto INIT_FAIL;
        }
        mCodecId = MTK_VDEC_CODEC_ID_MPEG2;
    }
    else if (!strcmp(componentName, MTK_OMX_DIVX_DECODER))
    {
        if (OMX_FALSE == InitDivxParams())
        {
            goto INIT_FAIL;
        }
        mCodecId = MTK_VDEC_CODEC_ID_DIVX;
    }
    else if (!strcmp(componentName, MTK_OMX_DIVX3_DECODER))
    {
        if (OMX_FALSE == InitDivx3Params())
        {
            goto INIT_FAIL;
        }
        mCodecId = MTK_VDEC_CODEC_ID_DIVX3;
    }
    else if (!strcmp(componentName, MTK_OMX_XVID_DECODER))
    {
        if (OMX_FALSE == InitXvidParams())
        {
            goto INIT_FAIL;
        }
        mCodecId = MTK_VDEC_CODEC_ID_XVID;
    }
    else if (!strcmp(componentName, MTK_OMX_S263_DECODER))
    {
        if (OMX_FALSE == InitS263Params())
        {
            goto INIT_FAIL;
        }
        mCodecId = MTK_VDEC_CODEC_ID_S263;
    }
    else if (!strcmp(componentName, MTK_OMX_MJPEG_DECODER))
    {
        //if (VAL_CHIP_NAME_MT6572 != mChipName) {//not 6572
        //MTK_OMX_LOGE ("MtkOmxVdec::ComponentInit ERROR: Don't support MJPEG");
        //err = OMX_ErrorBadParameter;
        //goto EXIT;
        //}
        if (OMX_FALSE == InitMJpegParams())
        {
            goto INIT_FAIL;
        }
        mCodecId = MTK_VDEC_CODEC_ID_MJPEG;
    }
    else if (!strcmp(componentName, MTK_OMX_HEVC_DECODER))
    {
        if (OMX_FALSE == InitHEVCParams())
        {
            goto INIT_FAIL;
        }
        mCodecId = MTK_VDEC_CODEC_ID_HEVC;
    }
    else if (!strcmp(componentName, MTK_OMX_HEVC_SEC_DECODER)) //HEVC.SEC.M0
    {
        if (OMX_FALSE == InitHEVCSecParams())
        {
            goto INIT_FAIL;
        }
        mCodecId = MTK_VDEC_CODEC_ID_HEVC; // USE the same codec id
    }
    else if (!strcmp(componentName, MTK_OMX_AVC_SEC_DECODER))
    {

        if (OMX_FALSE == InitAvcSecParams())
        {
            goto INIT_FAIL;
        }
        mCodecId = MTK_VDEC_CODEC_ID_AVC;
    }
    else
    {
        MTK_OMX_LOGE("MtkOmxVdec::ComponentInit ERROR: Unknown component name");
        goto INIT_FAIL;
    }

    return OMX_TRUE;

INIT_FAIL:
    return OMX_FALSE;
}

OMX_ERRORTYPE MtkOmxVdec::ComponentInit(OMX_IN OMX_HANDLETYPE hComponent,
                                        OMX_IN OMX_STRING componentName)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    MTK_OMX_LOGE("MtkOmxVdec::ComponentInit (%s)", componentName);
    mState = OMX_StateLoaded;
    int ret;
    size_t arraySize = 0;

    mVdecThread = pthread_self();
    mVdecDecodeThread = pthread_self();
    mVdecConvertThread = pthread_self();

    mVdecThreadCreated = false;
    mVdecDecodeThreadCreated = false;
    mVdecConvertThreadCreated = false;

    pthread_attr_t attr;

    if (OMX_FALSE == initCodecParam(componentName))
    {
        err = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    VDEC_DRV_MRESULT_T eResult;

    if (!strcmp(componentName, MTK_OMX_MJPEG_DECODER)) //is MJPEG
    {
        //don't create driver when MJPEG
        mOutputPortFormat.eColorFormat = OMX_COLOR_Format32bitARGB8888;
        mOutputPortDef.format.video.eColorFormat = OMX_COLOR_Format32bitARGB8888;
        mOutputPortDef.format.video.nStride = VDEC_ROUND_32(mOutputPortDef.format.video.nFrameWidth);
        mOutputPortDef.format.video.nSliceHeight = VDEC_ROUND_16(mOutputPortDef.format.video.nSliceHeight);
    }
    else
    {

        if (0 == mMtkV4L2Device.initialize(V4L2DeviceType::kDecoder, (void *)this))
        {
            MTK_OMX_LOGE("Error!! Cannot create driver");
            err = OMX_ErrorInsufficientResources;
            goto EXIT;
        }
		mMtkV4L2Device.subscribeEvent();

        //VDEC_DRV_SET_DECODE_MODE_T rtSetDecodeMode;
        //MTK_OMX_MEMSET(&rtSetDecodeMode, 0, sizeof(VDEC_DRV_SET_DECODE_MODE_T));
        //rtSetDecodeMode.eDecodeMode = VDEC_DRV_DECODE_MODE_NO_REORDER;
        //mMtkV4L2Device.setDecodeMode(&rtSetDecodeMode);


        // query output color format and stride and sliceheigt
        MTK_OMX_MEMSET(&mQInfoOut, 0, sizeof(VDEC_DRV_QUERY_VIDEO_FORMAT_T));
        if (OMX_TRUE == QueryDriverFormat(&mQInfoOut))
        {
            switch (mQInfoOut.ePixelFormat)
            {
                case VDEC_DRV_PIXEL_FORMAT_YUV_420_PLANER:
                    mOutputPortFormat.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Planar;
                    mOutputPortDef.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Planar;
                    break;

                case VDEC_DRV_PIXEL_FORMAT_YUV_420_PLANER_MTK:
                    mOutputPortFormat.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatVendorMTKYUV;
                    mOutputPortDef.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatVendorMTKYUV;
                    break;

                case VDEC_DRV_PIXEL_FORMAT_YUV_YV12:
                    mOutputPortFormat.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_MTK_COLOR_FormatYV12;
                    mOutputPortDef.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_MTK_COLOR_FormatYV12;
                    break;
                case VDEC_DRV_PIXEL_FORMAT_YUV_420_PLANER_UFO:
                    mOutputPortFormat.eColorFormat = OMX_COLOR_FormatVendorMTKYUV_UFO;
                    mOutputPortDef.format.video.eColorFormat = OMX_COLOR_FormatVendorMTKYUV_UFO;
                    break;
                case VDEC_DRV_PIXEL_FORMAT_YUV_NV12:
                    mOutputPortFormat.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
                    mOutputPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
                    break;
                default:
                    mOutputPortFormat.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatVendorMTKYUV;
                    mOutputPortDef.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatVendorMTKYUV;
                    MTK_OMX_LOGE("[Error] Unknown color format(%d), set to OMX_COLOR_FormatVendorMTKYUV.", mQInfoOut.ePixelFormat);
                    break;
            }
            if (mFixedMaxBuffer == OMX_TRUE)
            {
                mOutputPortDef.format.video.nFrameWidth = mQInfoOut.u4Width;
                mOutputPortDef.format.video.nFrameHeight = mQInfoOut.u4Height;

            }
            mOutputPortDef.format.video.nStride = VDEC_ROUND_N(mOutputPortDef.format.video.nFrameWidth, mQInfoOut.u4StrideAlign);
            mOutputPortDef.format.video.nSliceHeight = VDEC_ROUND_N(mOutputPortDef.format.video.nFrameHeight, mQInfoOut.u4SliceHeightAlign);
            meDecodeType = mQInfoOut.eDecodeType;
            if (meDecodeType == VDEC_DRV_DECODER_MTK_HARDWARE)
            {
                mPropFlags &= ~MTK_OMX_VDEC_ENABLE_PRIORITY_ADJUSTMENT;
                MTK_OMX_LOGD("MtkOmxVdec::SetConfig -> disable priority adjustment");
            }

            mMtkV4L2Device.setFormatBistream(mCodecId, mInputPortDef.nBufferSize);
            mMtkV4L2Device.setFormatFrameBuffer(mOutputPortFormat.eColorFormat);
        }
        else
        {
            MTK_OMX_LOGE("ERROR: query driver format failed");
        }
    }

    // allocate input buffer headers address array
    mInputBufferHdrs = (OMX_BUFFERHEADERTYPE **)MTK_OMX_ALLOC(sizeof(OMX_BUFFERHEADERTYPE *) * MAX_TOTAL_BUFFER_CNT);
    MTK_OMX_MEMSET(mInputBufferHdrs, 0x00, sizeof(OMX_BUFFERHEADERTYPE *) * MAX_TOTAL_BUFFER_CNT);

    // allocate output buffer headers address array
    {

        mOutputBufferHdrs = (OMX_BUFFERHEADERTYPE **)MTK_OMX_ALLOC(sizeof(OMX_BUFFERHEADERTYPE *) * MAX_TOTAL_BUFFER_CNT);
        MTK_OMX_MEMSET(mOutputBufferHdrs, 0x00, sizeof(OMX_BUFFERHEADERTYPE *) * MAX_TOTAL_BUFFER_CNT);
        mOutputBufferHdrsCnt = MAX_TOTAL_BUFFER_CNT;
    }


    // allocate mFrameBuf
    arraySize = sizeof(FrmBufStruct) * MAX_TOTAL_BUFFER_CNT;
    mFrameBuf = (FrmBufStruct *)MTK_OMX_ALLOC(arraySize);
    MTK_OMX_MEMSET(mFrameBuf, 0x00, arraySize);

    MTK_OMX_LOGD("allocate mFrameBuf: 0x%08x mOutputPortDef.nBufferCountActual:%d MAX_TOTAL_BUFFER_CNT:%d sizeof(FrmBufStruct):%d", mFrameBuf, mOutputPortDef.nBufferCountActual, MAX_TOTAL_BUFFER_CNT, sizeof(FrmBufStruct));

    // allocate mInputBuf
    mInputBuf = (InputBufStruct *)MTK_OMX_ALLOC(sizeof(InputBufStruct) * MAX_TOTAL_BUFFER_CNT);
    MTK_OMX_MEMSET(mInputBuf, 0x00, sizeof(InputBufStruct) * MAX_TOTAL_BUFFER_CNT);

    // create command pipe
    ret = pipe(mCmdPipe);
    if (ret)
    {
        MTK_OMX_LOGE("mCmdPipe creation failure err(%d)", ret);
        err = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    else
    {
        MTK_OMX_LOGD("MtkOmxVdec 0x%x: pipe : %d, %d", (unsigned int)this, mCmdPipe[0], mCmdPipe[1]);
    }

    ret = pipe(mConvertCmdPipe);
    if (ret)
    {
        MTK_OMX_LOGE("mCmdPipe creation failure err(%d)", ret);
        err = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    else
    {
        MTK_OMX_LOGD("MtkOmxVdec 0x%x: Convert pipe : %d, %d", (unsigned int)this, mConvertCmdPipe[0], mConvertCmdPipe[1]);
    }

    mIsComponentAlive = OMX_TRUE;

    //MTK_OMX_LOGD ("mCmdPipe[0] = %d", mCmdPipe[0]);
    // create vdec thread

    pthread_attr_init(&attr);

    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    ret = pthread_create(&mVdecThread, &attr, &MtkOmxVdecThread, (void *)this);

    pthread_attr_destroy(&attr);

    //ret = pthread_create(&mVdecThread, NULL, &MtkOmxVdecThread, (void *)this);
    if (ret)
    {
        MTK_OMX_LOGE("MtkOmxVdecThread creation failure err(%d)", ret);
        err = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    else
    {
        mVdecThreadCreated = true;
    }

    // create video decoding thread

    pthread_attr_init(&attr);

    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    ret = pthread_create(&mVdecDecodeThread, &attr, &MtkOmxVdecDecodeThread, (void *)this);

    pthread_attr_destroy(&attr);

    //ret = pthread_create(&mVdecDecodeThread, NULL, &MtkOmxVdecDecodeThread, (void *)this);
    if (ret)
    {
        MTK_OMX_LOGE("MtkOmxVdecDecodeThread creation failure err(%d)", ret);
        err = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    else
    {
        mVdecDecodeThreadCreated = true;
    }

    // create vdec convert thread
    pthread_attr_init(&attr);

    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    ret = pthread_create(&mVdecConvertThread, &attr, &MtkOmxVdecConvertThread, (void *)this);

    pthread_attr_destroy(&attr);

    //ret = pthread_create(&mVdecConvertThread, NULL, &MtkOmxVdecConvertThread, (void *)this);
    if (ret)
    {
        MTK_OMX_LOGE("mVdecConvertThread creation failure err(%d)", ret);
        err = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    else
    {
        mVdecConvertThreadCreated = true;
    }


    pthread_mutex_init(&mut,NULL);
    pthread_cond_init(&cond,NULL);

EXIT:
    return err;
}


OMX_ERRORTYPE MtkOmxVdec::ComponentDeInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    MTK_OMX_LOGD("+MtkOmxVdec::ComponentDeInit");
    OMX_ERRORTYPE err = OMX_ErrorNone;
    ssize_t ret = 0;

    if (MTK_VDEC_CODEC_ID_MJPEG != mCodecId)//handlesetstate DeInitVideoDecodeHW() fail will  cause mDrvHandle = NULL
    {
        if (mDecoderInitCompleteFlag == OMX_TRUE)
        {
            MTK_OMX_LOGE("Warning!! ComponentDeInit before DeInitVideoDecodeHW! De-Init video driver..");

            mDecoderInitCompleteFlag = OMX_FALSE;
        }

        //mMtkV4L2Device.deinitialize();
    }

#if 1 // for VAL_CHIP_NAME_MT6755 || VAL_CHIP_NAME_DENALI_3
    //for SVP shareable memory, Tehsin
    if (mIsSecureInst && mbShareableMemoryEnabled && (mChipName == VAL_CHIP_NAME_MT6797 || mChipName == VAL_CHIP_NAME_MT6755 || mChipName == VAL_CHIP_NAME_DENALI_3))
    {
        // adb shell echo 1 > /dev/svp_region
        int fd = -1;
        fd = open("/proc/svp_region", O_RDONLY);
        if (fd == -1)
        {
            MTK_OMX_LOGE("[Info] fail to open /proc/svp_region");
            fsync(1);
        }
        else
        {
            //kernel would query TEE sec api to release mem from sec world
            char *share_mem_enable = "1";
            int vRet;
            int vRet2;

            vRet2 = ioctl(fd, SVP_REGION_IOC_ONLINE, &vRet);

            MTK_OMX_LOGD("Should Release Sec Memory %d %d %d", vRet, vRet2, errno);
            close(fd);
            mbShareableMemoryEnabled = OMX_FALSE;
        }
    }
#endif
    // terminate decode thread
    mIsComponentAlive = OMX_FALSE;
    //SIGNAL(mOutputBufferSem);
    //SIGNAL(mDecodeSem);
    // terminate MJC thread
    // For Scaler ClearMotion +
    // terminate command thread
    OMX_U32 CmdCat = MTK_OMX_STOP_COMMAND;
    if (mCmdPipe[MTK_OMX_PIPE_ID_WRITE] > -1)
    {
        WRITE_PIPE(CmdCat, mCmdPipe);
    }
    if (mConvertCmdPipe[MTK_OMX_PIPE_ID_WRITE] > -1)
    {
        WRITE_PIPE(CmdCat, mConvertCmdPipe);
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
#if 1
    // ALPS02355777 pthread_join -> pthread_detach

    static int64_t _in_time_1 = 0;
    static int64_t _in_time_2 = 0;
    static int64_t _out_time = 0;
    _in_time_1 = getTickCountMs();

    while (1)
    {
        if (mVdecConvertThread != NULL && mVdecConvertThreadCreated)
        {
            _in_time_2 = getTickCountMs();
            _out_time = _in_time_2 - _in_time_1;
            if (_out_time > 5000)
            {
                MTK_OMX_LOGE("timeout wait for mVdecConvertThread terminated");
#ifdef HAVE_AEE_FEATURE
                aee_system_warning("CRDISPATCH_KEY:OMX video decode issue", NULL, DB_OPT_FTRACE, "\nOmx timeout wait for mVdecConvertThread terminated!");
#endif //HAVE_AEE_FEATURE
                break;
            }
            else
            {
                SLEEP_MS(10);
            }
        }
        else
        {
            break;
        }
    }
    mVdecConvertThreadCreated = false;

    _in_time_1 = getTickCountMs();
    while (1)
    {
        if (mVdecDecodeThread != NULL && mVdecDecodeThreadCreated)
        {
            _in_time_2 = getTickCountMs();
            _out_time = _in_time_2 - _in_time_1;
            if (_out_time > 5000)
            {
                MTK_OMX_LOGE("timeout wait for mVdecDecodeThread terminated");
#ifdef HAVE_AEE_FEATURE
                aee_system_warning("CRDISPATCH_KEY:OMX video decode issue", NULL, DB_OPT_FTRACE, "\nOmx timeout wait for mVdecDecodeThread terminated!");
#endif //HAVE_AEE_FEATURE
                break;
            }
            else
            {
                SLEEP_MS(10);
            }
        }
        else
        {
            break;
        }
    }
    mVdecDecodeThreadCreated = false;
    _in_time_1 = getTickCountMs();

    while (1)
    {
        if (mVdecThread != NULL && mVdecThreadCreated)
        {
            _in_time_2 = getTickCountMs();
            _out_time = _in_time_2 - _in_time_1;
            if (_out_time > 5000)
            {
                MTK_OMX_LOGE("timeout wait for mVdecThread terminated");
#ifdef HAVE_AEE_FEATURE
                aee_system_warning("CRDISPATCH_KEY:OMX video decode issue", NULL, DB_OPT_FTRACE, "\nOmx timeout wait for mVdecThread terminated!");
#endif //HAVE_AEE_FEATURE
                break;
            }
            else
            {
                SLEEP_MS(10);
            }
        }
        else
        {
            break;
        }
    }
    mVdecThreadCreated = false;
#endif

    if (NULL != mCoreGlobal)
    {
        ((mtk_omx_core_global *)mCoreGlobal)->video_instance_count--;
#if 0 // FIXME
        for (int i = 0 ; i < ((mtk_omx_core_global *)mCoreGlobal)->gInstanceList.size() ; i++)
        {
            const mtk_omx_instance_data *pInstanceData = ((mtk_omx_core_global *)mCoreGlobal)->gInstanceList.itemAt(i);
            if (pInstanceData == mGlobalInstData)
            {
                MTK_OMX_LOGE("@@ Remove instance op_thread(%d)", pInstanceData->op_thread);
                ((mtk_omx_core_global *)mCoreGlobal)->gInstanceList.removeAt(i);
            }
        }
#endif
    }

	mMtkV4L2Device.deinitialize();

    if (mCmdPipe[MTK_OMX_PIPE_ID_READ] > -1)
    {
        close(mCmdPipe[MTK_OMX_PIPE_ID_READ]);
    }
    if (mCmdPipe[MTK_OMX_PIPE_ID_WRITE] > -1)
    {
        close(mCmdPipe[MTK_OMX_PIPE_ID_WRITE]);
    }

    if (mConvertCmdPipe[MTK_OMX_PIPE_ID_READ] > -1)
    {
        close(mConvertCmdPipe[MTK_OMX_PIPE_ID_READ]);
    }
    if (mConvertCmdPipe[MTK_OMX_PIPE_ID_WRITE] > -1)
    {
        close(mConvertCmdPipe[MTK_OMX_PIPE_ID_WRITE]);
    }

    MTK_OMX_LOGD("-MtkOmxVdec::ComponentDeInit");

EXIT:
    return err;
}


OMX_ERRORTYPE MtkOmxVdec::GetComponentVersion(OMX_IN OMX_HANDLETYPE hComponent,
                                              OMX_IN OMX_STRING componentName,
                                              OMX_OUT OMX_VERSIONTYPE *componentVersion,
                                              OMX_OUT OMX_VERSIONTYPE *specVersion,
                                              OMX_OUT OMX_UUIDTYPE *componentUUID)

{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    MTK_OMX_LOGD("MtkOmxVdec::GetComponentVersion");
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


OMX_ERRORTYPE MtkOmxVdec::SendCommand(OMX_IN OMX_HANDLETYPE hComponent,
                                      OMX_IN OMX_COMMANDTYPE Cmd,
                                      OMX_IN OMX_U32 nParam1,
                                      OMX_IN OMX_PTR pCmdData)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    MTK_OMX_LOGD("MtkOmxVdec::SendCommand cmd=%s", CommandToString(Cmd));

    OMX_U32 CmdCat = MTK_OMX_GENERAL_COMMAND;

    ssize_t ret = 0;

    LOCK(mCmdQLock);

    if (mState == OMX_StateInvalid)
    {
        err = OMX_ErrorInvalidState;
        goto EXIT;
    }

    switch (Cmd)
    {
        case OMX_CommandStateSet:   // write 8 bytes to pipe [cmd][nParam1]
            if (nParam1 == OMX_StateIdle)
            {
                MTK_OMX_LOGD("set MTK_OMX_VDEC_IDLE_PENDING");
                SET_PENDING(MTK_OMX_IDLE_PENDING);
            }
            else if (nParam1 == OMX_StateLoaded)
            {
                MTK_OMX_LOGD("set MTK_OMX_VDEC_LOADED_PENDING");
                SET_PENDING(MTK_OMX_LOADED_PENDING);
            }
            WRITE_PIPE(CmdCat, mCmdPipe);
            WRITE_PIPE(Cmd, mCmdPipe);
            WRITE_PIPE(nParam1, mCmdPipe);
            break;

        case OMX_CommandPortDisable:
            if ((nParam1 != MTK_OMX_INPUT_PORT) && (nParam1 != MTK_OMX_OUTPUT_PORT) && (nParam1 != MTK_OMX_ALL_PORT))
            {
                err = OMX_ErrorBadParameter;
                goto EXIT;
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
                err = OMX_ErrorBadParameter;
                goto EXIT;
            }

            // mark the ports to be enabled first, p.85
            if (nParam1 == MTK_OMX_INPUT_PORT || nParam1 == MTK_OMX_ALL_PORT)
            {
                mInputPortDef.bEnabled = OMX_TRUE;

                if ((mState != OMX_StateLoaded) && (mInputPortDef.bPopulated == OMX_FALSE))
                {
                    SET_PENDING(MTK_OMX_IN_PORT_ENABLE_PENDING);
                }

                if ((mState == OMX_StateLoaded) && (mInputPortDef.bPopulated == OMX_FALSE))   // component is idle pending and port is not populated
                {
                    SET_PENDING(MTK_OMX_IN_PORT_ENABLE_PENDING);
                }
                if ((mState == OMX_StateLoaded) && (mOutputPortDef.bPopulated == OMX_FALSE))   // component is idle pending and port is not populated
                {
                    SET_PENDING(MTK_OMX_OUT_PORT_ENABLE_PENDING);
                }
            }

            if (nParam1 == MTK_OMX_OUTPUT_PORT || nParam1 == MTK_OMX_ALL_PORT)
            {
                mOutputPortDef.bEnabled = OMX_TRUE;

                if ((mState != OMX_StateLoaded) && (mOutputPortDef.bPopulated == OMX_FALSE))
                {
                    //MTK_OMX_LOGD ("SET_PENDING(MTK_OMX_VDEC_OUT_PORT_ENABLE_PENDING) mState(%d)", mState);
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
                err = OMX_ErrorBadParameter;
                goto EXIT;
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
    return err;
}


OMX_ERRORTYPE MtkOmxVdec::SetCallbacks(OMX_IN OMX_HANDLETYPE hComponent,
                                       OMX_IN OMX_CALLBACKTYPE *pCallBacks,
                                       OMX_IN OMX_PTR pAppData)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    //MTK_OMX_LOGD ("MtkOmxVdec::SetCallbacks");
    if (NULL == pCallBacks)
    {
        MTK_OMX_LOGE("[ERROR] MtkOmxVdec::SetCallbacks pCallBacks is NULL !!!");
        err = OMX_ErrorBadParameter;
        goto EXIT;
    }
    mCallback = *pCallBacks;
    mAppData = pAppData;
    mCompHandle.pApplicationPrivate = mAppData;

EXIT:
    return err;
}

#define CHECK_ERRORNONE_N_EXIT(err)\
	if (OMX_ErrorNone != err)\
	{\
		goto EXIT;\
	}\

#define CHECK_BEEN_HANDLED(err)\
	if (OMX_ErrorNotImplemented != err)\
	{\
		goto EXIT;\
	}\
	else\
    {\
        err = OMX_ErrorNone;\
    }\


OMX_ERRORTYPE MtkOmxVdec::SetParameter(OMX_IN OMX_HANDLETYPE hComp,
                                       OMX_IN OMX_INDEXTYPE nParamIndex,
                                       OMX_IN OMX_PTR pCompParam)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    MTK_OMX_LOGD("SP (%s) mState %x", GetParameterSrting(nParamIndex), mState);

    err = CheckSetParamState();
    CHECK_ERRORNONE_N_EXIT(err);

    if (NULL == pCompParam)
    {
        err = OMX_ErrorBadParameter;
        goto EXIT;
    }

    err = CheckICSLaterSetParameters(hComp, nParamIndex,  pCompParam);
    CHECK_BEEN_HANDLED(err);

    err = CheckKKLaterSetParameters(nParamIndex,  pCompParam);
    CHECK_BEEN_HANDLED(err);

    err = CheckMLaterSetParameters(nParamIndex, pCompParam);
    CHECK_BEEN_HANDLED(err);

    switch (nParamIndex)
    {
        case OMX_IndexParamPortDefinition:
    	{
            err = HandleSetPortDefinition((OMX_PARAM_PORTDEFINITIONTYPE*)pCompParam);
			CHECK_ERRORNONE_N_EXIT(err);
    	}
		break;

        case OMX_IndexParamVideoPortFormat:
        {
            err = HandleSetVideoPortFormat((OMX_PARAM_PORTDEFINITIONTYPE*)pCompParam);
			CHECK_ERRORNONE_N_EXIT(err);
        }
		break;

        case OMX_IndexParamStandardComponentRole:
        {
            OMX_PARAM_COMPONENTROLETYPE *pRoleParams = (OMX_PARAM_COMPONENTROLETYPE *)pCompParam;
            strcpy((char *)mCompRole, (char *)pRoleParams->cRole);
        }
		break;

        case OMX_IndexParamVideoRv:
        {
            OMX_VIDEO_PARAM_RVTYPE *pRvType = (OMX_VIDEO_PARAM_RVTYPE *)pCompParam;
            MTK_OMX_LOGD("Set pRvType nPortIndex=%d", pRvType->nPortIndex);
            memcpy(&mRvType, pCompParam, sizeof(OMX_VIDEO_PARAM_RVTYPE));
        }
		break;

        case OMX_IndexVendorMtkOmxVdecThumbnailMode:
        {
            HandleSetMtkOmxVdecThumbnailMode();
        }
		break;

        case OMX_IndexVendorMtkOmxVdecUseClearMotion:
    	{
			HandleSetMtkOmxVdecUseClearMotion();
    	}
		break;

        case OMX_IndexVendorMtkOmxVdecGetMinUndequeuedBufs:
        {
            HandleMinUndequeuedBufs((VAL_UINT32_T*)pCompParam);
        }
		break;

#if 0//def MTK S3D SUPPORT
        case OMX_IndexVendorMtkOmxVdec3DVideoPlayback:
        {
            m3DStereoMode = *(OMX_VIDEO_H264FPATYPE *)pCompParam;
            MTK_OMX_LOGD("3D mode from parser, %d", m3DStereoMode);
            break;
        }
#endif

        case OMX_IndexVendorMtkOmxVdecStreamingMode:
        {
            mStreamingMode = *(OMX_BOOL *)pCompParam;
        }
		break;

        case OMX_IndexVendorMtkOmxVdecACodecColorConvertMode:
        {
            mACodecColorConvertMode = *(OMX_U32 *)pCompParam;
            MTK_OMX_LOGD("OMX_IndexVendorMtkOmxVdecACodecColorConvertMode mACodecColorConvertMode: %d", mACodecColorConvertMode);
        }
		break;

        case OMX_IndexVendorMtkOmxVdecFixedMaxBuffer:
        {
            mFixedMaxBuffer = OMX_TRUE;
            MTK_OMX_LOGD("all output buffer will be set to the MAX support frame size");
        }
		break;

        case OMX_IndexVendorMtkOmxVideoUseIonBuffer:
        {
            UseIonBufferParams *pUseIonBufferParams = (UseIonBufferParams *)pCompParam;
			err = HandleUseIonBuffer(hComp, pUseIonBufferParams);
			CHECK_ERRORNONE_N_EXIT(err);
		}
		break;

        case OMX_IndexVendorMtkOmxVideoSetClientLocally:
        {
            mIsClientLocally = *((OMX_BOOL *)pCompParam);
            MTK_OMX_LOGD("@@ mIsClientLocally(%d)", mIsClientLocally);
            break;
        }

        case OMX_IndexVendorMtkOmxVdecNoReorderMode:
        {
            MTK_OMX_LOGD("Set No Reorder mode enable");
            mNoReorderMode = OMX_TRUE;
            break;
        }

        case OMX_IndexVendorMtkOmxVdecSkipReferenceCheckMode:
        {
            MTK_OMX_LOGD("Sets skip reference check mode!");
            mSkipReferenceCheckMode = OMX_TRUE;
            break;
        }

        case OMX_IndexVendorMtkOmxVdecLowLatencyDecode:
        {
            MTK_OMX_LOGD("Sets low latency decode mode!");
            mLowLatencyDecodeMode = OMX_TRUE;
            break;
        }

        case OMX_IndexVendorMtkOmxVdecUse16xSlowMotion:
        {
            MTK_OMX_LOGD("Set 16x slowmotion mode");
            mb16xSlowMotionMode = OMX_TRUE;
        }
        break;

        case OMX_IndexVendorMtkOmxVdecSetScenario:
        {
            OMX_U32 *tmpVal = ((OMX_U32 *)pCompParam);

            if (*tmpVal == 5)
			{
				mViLTESupportOn = OMX_TRUE;
			}
            else if (*tmpVal == 6)
			{
				mCrossMountSupportOn = true;
			}

            MTK_OMX_LOGD("@@ set vdec scenario %lu, mViLTESupportOn %d, mCrossMountSupportOn %d", *tmpVal, mViLTESupportOn, mCrossMountSupportOn);
        }
		break;

        default:
        {
            MTK_OMX_LOGE("MtkOmxVdec::SetParameter unsupported nParamIndex");
            err = OMX_ErrorUnsupportedIndex;
        }
		break;
    }

EXIT:
    return err;
}

OMX_ERRORTYPE MtkOmxVdec::GetParameter(OMX_IN OMX_HANDLETYPE hComponent,
                                       OMX_IN  OMX_INDEXTYPE nParamIndex,
                                       OMX_INOUT OMX_PTR pCompParam)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    MTK_OMX_LOGD("GP (%s)", GetParameterSrting(nParamIndex));

    err = CheckGetParamState();
	CHECK_ERRORNONE_N_EXIT(err);

    if (NULL == pCompParam)
    {
        err = OMX_ErrorBadParameter;
        goto EXIT;
    }

    err = CheckICSLaterGetParameter(nParamIndex, pCompParam);
	CHECK_BEEN_HANDLED(err);

    switch (nParamIndex)
    {
        case OMX_IndexParamPortDefinition:
    	{
			err = HandleGetPortDefinition(pCompParam);
    	}
		break;

        case OMX_IndexParamVideoInit:
		case OMX_IndexParamAudioInit:
		case OMX_IndexParamImageInit:
		case OMX_IndexParamOtherInit:
		{
			HandleAllInit(nParamIndex, pCompParam);
		}
		break;

        case OMX_IndexParamVideoPortFormat:
    	{
			err = HandleGetPortFormat(pCompParam);
    	}
		break;

        case OMX_IndexParamStandardComponentRole:
        {
            OMX_PARAM_COMPONENTROLETYPE *pRoleParams = (OMX_PARAM_COMPONENTROLETYPE *)pCompParam;
            strcpy((char *)pRoleParams->cRole, (char *)mCompRole);
            break;
        }

        case OMX_IndexParamVideoRv:
        {
            OMX_VIDEO_PARAM_RVTYPE *pRvType = (OMX_VIDEO_PARAM_RVTYPE *)pCompParam;
            MTK_OMX_LOGD("Get pRvType nPortIndex=%d", pRvType->nPortIndex);
            memcpy(pCompParam, &mRvType, sizeof(OMX_VIDEO_PARAM_RVTYPE));
            break;
        }

        case OMX_IndexParamVideoProfileLevelQuerySupported:
        {
			err = HandleGetVideoProfileLevelQuerySupported(pCompParam);
        }
		break;

        case OMX_IndexVendorMtkOmxVdecVideoSpecQuerySupported:
		{
			err = HandleGetVdecVideoSpecQuerySupported(pCompParam);
    	}
		break;

        case OMX_IndexVendorMtkOmxPartialFrameQuerySupported:
        {
            OMX_BOOL *pSupportPartialFrame = (OMX_BOOL *)pCompParam;
            (*pSupportPartialFrame) = OMX_FALSE;
        }
        break;

        case OMX_IndexVendorMtkOmxVdecGetColorFormat:
        {
            OMX_COLOR_FORMATTYPE *colorFormat = (OMX_COLOR_FORMATTYPE *)pCompParam;
            *colorFormat = mOutputPortFormat.eColorFormat;
            //MTK_OMX_LOGD("colorFormat %lx",*colorFormat);
        }
		break;

        //alps\frameworks\base\media\jni\android_media_ImageReader.cpp
        //ImageReader_imageSetup() need YV12
        case OMX_GoogleAndroidIndexDescribeColorFormat:
        {
			MTK_OMX_LOGE("++++OMX_COLOR_Format24bitRGB888:%d", OMX_COLOR_Format24bitRGB888);
			err = HandleGetDescribeColorFormat(pCompParam);
			MTK_OMX_LOGE("----, err:%d", err);
		}
		break;

        case OMX_IndexVendorMtkOmxHandle:
        {
            OMX_U32 *pHandle = (OMX_U32 *)pCompParam;
            *pHandle = (OMX_U32)this;
            break;
        }
        default:
        {
            MTK_OMX_LOGE("MtkOmxVdec::GetParameter unsupported nParamIndex");
            err = OMX_ErrorUnsupportedIndex;
            break;
        }
    }

EXIT:
    return err;
}


OMX_ERRORTYPE MtkOmxVdec::SetConfig(OMX_IN OMX_HANDLETYPE hComponent,
                                    OMX_IN OMX_INDEXTYPE nConfigIndex,
                                    OMX_IN OMX_PTR ComponentConfigStructure)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    //MTK_OMX_LOGD ("MtkOmxVdec::SetConfig");

    if (mState == OMX_StateInvalid)
    {
        err = OMX_ErrorInvalidState;
        goto EXIT;
    }

    switch (nConfigIndex)
    {
#ifdef DYNAMIC_PRIORITY_ADJUSTMENT
        case OMX_IndexVendorMtkOmxVdecPriorityAdjustment:
        {
            if (*((OMX_BOOL *)ComponentConfigStructure) == OMX_TRUE)
            {
                mPropFlags |= MTK_OMX_VDEC_ENABLE_PRIORITY_ADJUSTMENT;
                MTK_OMX_LOGD("MtkOmxVdec::SetConfig -> enable priority adjustment");
            }
            else
            {
                mPropFlags &= ~MTK_OMX_VDEC_ENABLE_PRIORITY_ADJUSTMENT;
                MTK_OMX_LOGD("MtkOmxVdec::SetConfig -> disable priority adjustment");
            }
            break;
        }
#endif
        case OMX_IndexVendorMtkOmxVdecSeekMode:
        {
            mSeekTargetTime = *(OMX_TICKS *)ComponentConfigStructure;
            MTK_OMX_LOGD("Set seek mode enable, %lld", mSeekTargetTime);
            if (mStreamingMode == OMX_FALSE && mSeekTargetTime > 0 && mDecoderInitCompleteFlag == OMX_TRUE)
            {
                mPrepareSeek = OMX_TRUE;
                mllLastUpdateTime = mSeekTargetTime;
            }
            else
            {
                mSeekMode = OMX_FALSE;

                if (mSeekTargetTime == 0 &&
                    mChipName == VAL_CHIP_NAME_MT6580 && mCodecId == MTK_VDEC_CODEC_ID_AVC)
                {
                    //for 6580 AVC, reset current time when loop (seek back to 0). (avoid speedy mode)
                    mAVSyncTime = mSeekTargetTime;
                    mResetCurrTime = true;
                }

                if (mStreamingMode == OMX_TRUE)
                {
                    mSeekTargetTime = 0;
                }
            }
            break;
        }

        case OMX_IndexVendorMtkOmxVdecAVSyncTime:
        {
            int64_t time = *(int64_t *)ComponentConfigStructure;
            //MTK_OMX_LOGD("MtkOmxVdec::SetConfig set avsync time %lld", time);
            mAVSyncTime = time;
            break;
        }

        //#ifdef MTK_16X_SLOWMOTION_VIDEO_SUPPORT
        case OMX_IndexVendorMtkOmxVdecSlowMotionSpeed:
        {
            // Todo: Pass the slowmotion speed to MJC
            unsigned int param = *(OMX_U32 *)ComponentConfigStructure;
            MTK_OMX_LOGD("Set 16x slowmotion speed(%d)", param);
            break;
        }

        case OMX_IndexVendorMtkOmxVdecSlowMotionSection:
        {
            // Todo: Pass the slowmotion speed to MJC
            OMX_MTK_SLOWMOTION_SECTION *pParam = (OMX_MTK_SLOWMOTION_SECTION *)ComponentConfigStructure;
            MTK_OMX_LOGD("Sets 16x slowmotion section(%lld ~ %lld)", pParam->StartTime, pParam->EndTime);
            break;
        }
        //#endif

        default:
            MTK_OMX_LOGE("MtkOmxVdec::SetConfig Unknown config index: 0x%08X", nConfigIndex);
            err = OMX_ErrorUnsupportedIndex;
            break;
    }

EXIT:
    return err;
}


OMX_ERRORTYPE MtkOmxVdec::GetConfig(OMX_IN OMX_HANDLETYPE hComponent,
                                    OMX_IN OMX_INDEXTYPE nConfigIndex,
                                    OMX_INOUT OMX_PTR ComponentConfigStructure)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    //MTK_OMX_LOGD("+ MtkOmxVdec::GetConfig");
    MTK_OMX_LOGD("GetConfig (0x%08X) mState %x", nConfigIndex, mState);

    if (mState == OMX_StateInvalid)
    {
        err = OMX_ErrorInvalidState;
        goto EXIT;
    }

    switch (nConfigIndex)
    {
#ifdef DYNAMIC_PRIORITY_ADJUSTMENT
        case OMX_IndexVendorMtkOmxVdecPriorityAdjustment:
        {
            OMX_BOOL *pEnableState = (OMX_BOOL *)ComponentConfigStructure;
            *pEnableState = (mPropFlags & MTK_OMX_VDEC_ENABLE_PRIORITY_ADJUSTMENT ? OMX_TRUE : OMX_FALSE);
            break;
        }
#endif

        case OMX_IndexVendorMtkOmxVdecGetAspectRatio:
        {
            OMX_S32 *pAspectRatio = (OMX_S32 *)ComponentConfigStructure;
            *pAspectRatio = (mAspectRatioWidth << 16) | mAspectRatioHeight;
            break;
        }

        case OMX_IndexVendorMtkOmxVdecGetCropInfo:
        {
            OMX_CONFIG_RECTTYPE *pCropInfo = (OMX_CONFIG_RECTTYPE *)ComponentConfigStructure;
            {

                struct v4l2_crop crop_arg;
                memset(&crop_arg, 0, sizeof(crop_arg));

                if (1 == mMtkV4L2Device.getCrop(&crop_arg))
                {
                    pCropInfo->nLeft    = crop_arg.c.left;
                    pCropInfo->nTop     = crop_arg.c.top;
                    pCropInfo->nWidth   = crop_arg.c.width;
                    pCropInfo->nHeight  = crop_arg.c.height;
                    mCropLeft = crop_arg.c.left;
                    mCropTop = crop_arg.c.top;
                    mCropWidth = crop_arg.c.width;
                    mCropHeight = crop_arg.c.height;
                }

                MTK_OMX_LOGD("OMX_IndexVendorMtkOmxVdecGetCropInfo: from Codec, u4CropLeft %d, u4CropTop %d, u4CropWidth %d, u4CropHeight %d\n",
                             crop_arg.c.left, crop_arg.c.top, crop_arg.c.width, crop_arg.c.height);
            }
            break;
        }

#if (ANDROID_VER >= ANDROID_KK)
        case OMX_IndexConfigCommonOutputCrop:
        {
            OMX_CONFIG_RECTTYPE *rectParams = (OMX_CONFIG_RECTTYPE *)ComponentConfigStructure;

            if (rectParams->nPortIndex != mOutputPortDef.nPortIndex)
            {
                return OMX_ErrorUndefined;
            }
            if (mCropLeft == 0 && mCropTop == 0 && mCropWidth == 0 && mCropHeight == 0)
            {
                MTK_OMX_LOGD("mCropWidth : %d , mCropHeight : %d", mCropWidth, mCropHeight);
                return OMX_ErrorUndefined;
            }
            rectParams->nLeft = mCropLeft;
            rectParams->nTop = mCropTop;
            rectParams->nWidth = mCropWidth;
            rectParams->nHeight = mCropHeight;
            //MTK_OMX_LOGD("mCropWidth : %d , mCropHeight : %d", mCropWidth, mCropHeight);
            MTK_OMX_LOGD("crop info (%d)(%d)(%d)(%d)", rectParams->nLeft, rectParams->nTop, rectParams->nWidth, rectParams->nHeight);
            break;
        }
#endif

        case OMX_IndexVendorMtkOmxVdecComponentColorConvert:
        {
            OMX_BOOL *pEnableState = (OMX_BOOL *)ComponentConfigStructure;

            if (OMX_TRUE == mOutputAllocateBuffer ||OMX_TRUE == needColorConvertWithNativeWindow())
            {
                *pEnableState = OMX_TRUE;
            }
            else
            {
                *pEnableState = OMX_FALSE;
            }

            MTK_OMX_LOGD("ComponentColorConvert %d", *pEnableState);
            break;
        }

        default:
            MTK_OMX_LOGE("MtkOmxVdec::GetConfig Unknown config index: 0x%08X", nConfigIndex);
            err = OMX_ErrorUnsupportedIndex;
            break;
    }

    //MTK_OMX_LOGD("- MtkOmxVdec::GetConfig");

EXIT:
    return err;
}


OMX_ERRORTYPE MtkOmxVdec::GetExtensionIndex(OMX_IN OMX_HANDLETYPE hComponent,
                                            OMX_IN OMX_STRING parameterName,
                                            OMX_OUT OMX_INDEXTYPE *pIndexType)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    //MTK_OMX_LOGD ("MtkOmxVdec::GetExtensionIndex");

    if (mState == OMX_StateInvalid)
    {
        err = OMX_ErrorInvalidState;
        goto EXIT;
    }

    if (!strncmp(parameterName, MTK_OMXVDEC_EXTENSION_INDEX_PARAM_PRIORITY_ADJUSTMENT, strlen(MTK_OMXVDEC_EXTENSION_INDEX_PARAM_PRIORITY_ADJUSTMENT)))
    {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexVendorMtkOmxVdecPriorityAdjustment;
    }
#if 0//def MTK S3D SUPPORT
    else if (!strncmp(parameterName, MTK_OMXVDEC_EXTENSION_INDEX_PARAM_3D_STEREO_PLAYBACK, strlen(MTK_OMXVDEC_EXTENSION_INDEX_PARAM_3D_STEREO_PLAYBACK)))
    {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexVendorMtkOmxVdec3DVideoPlayback;
    }
#endif
#if (ANDROID_VER >= ANDROID_ICS)
    else if (!strncmp(parameterName, "OMX.google.android.index.enableAndroidNativeBuffers", strlen("OMX.google.android.index.enableAndroidNativeBuffers")))
    {
        *pIndexType = (OMX_INDEXTYPE)OMX_GoogleAndroidIndexEnableAndroidNativeBuffers;
    }
    else if (!strncmp(parameterName, "OMX.google.android.index.useAndroidNativeBuffer", strlen(parameterName)))
    {
        *pIndexType = (OMX_INDEXTYPE)OMX_GoogleAndroidIndexUseAndroidNativeBuffer;
    }
    else if (!strncmp(parameterName, "OMX.google.android.index.getAndroidNativeBufferUsage", strlen("OMX.google.android.index.getAndroidNativeBufferUsage")))
    {
        *pIndexType = (OMX_INDEXTYPE)OMX_GoogleAndroidIndexGetAndroidNativeBufferUsage;
    }
    else if (!strncmp(parameterName, MTK_OMXVDEC_EXTENSION_INDEX_PARAM_STREAMING_MODE, strlen(MTK_OMXVDEC_EXTENSION_INDEX_PARAM_STREAMING_MODE)))
    {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexVendorMtkOmxVdecStreamingMode;
    }
    else if (!strncmp(parameterName, "OMX.google.android.index.describeColorFormat", strlen("OMX.google.android.index.describeColorFormat")))
    {
        *pIndexType = (OMX_INDEXTYPE) OMX_GoogleAndroidIndexDescribeColorFormat;
    }
#endif
    else if (!strncmp(parameterName, MTK_OMX_EXTENSION_INDEX_PARTIAL_FRAME_QUERY_SUPPORTED, strlen(MTK_OMX_EXTENSION_INDEX_PARTIAL_FRAME_QUERY_SUPPORTED)))
    {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexVendorMtkOmxPartialFrameQuerySupported;
    }
    else if (!strncmp(parameterName, MTK_OMXVDEC_EXTENSION_INDEX_PARAM_SWITCH_BW_TVOUT, strlen(MTK_OMXVDEC_EXTENSION_INDEX_PARAM_SWITCH_BW_TVOUT)))
    {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexVendorMtkOmxVdecSwitchBwTVout;
    }
    else if (!strncmp(parameterName, MTK_OMXVDEC_EXTENSION_INDEX_PARAM_NO_REORDER_MODE, strlen(MTK_OMXVDEC_EXTENSION_INDEX_PARAM_NO_REORDER_MODE)))
    {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexVendorMtkOmxVdecNoReorderMode;
    }
    else if (!strncmp(parameterName, "OMX.MTK.VIDEO.index.useIonBuffer", strlen("OMX.MTK.VIDEO.index.useIonBuffer")))
    {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexVendorMtkOmxVideoUseIonBuffer;  // Morris Yang 20130709 ION
    }
    else if (!strncmp(parameterName, MTK_OMXVDEC_EXTENSION_INDEX_PARAM_FIXED_MAX_BUFFER, strlen(MTK_OMXVDEC_EXTENSION_INDEX_PARAM_FIXED_MAX_BUFFER)))
    {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexVendorMtkOmxVdecFixedMaxBuffer;
    }
    else if (!strncmp(parameterName, MTK_OMXVDEC_EXTENSION_INDEX_PARAM_SKIP_REFERENCE_CHECK_MODE, strlen(MTK_OMXVDEC_EXTENSION_INDEX_PARAM_SKIP_REFERENCE_CHECK_MODE)))
    {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexVendorMtkOmxVdecSkipReferenceCheckMode;
    }
    else if (!strncmp(parameterName, "OMX.google.android.index.prepareForAdaptivePlayback", strlen("OMX.google.android.index.prepareForAdaptivePlayback")))
    {
        *pIndexType = (OMX_INDEXTYPE) OMX_GoogleAndroidIndexPrepareForAdaptivePlayback;
    }
    else if (!strncmp(parameterName, "OMX.google.android.index.storeMetaDataInBuffers", strlen("OMX.google.android.index.storeMetaDataInBuffers")))
    {
        *pIndexType = (OMX_INDEXTYPE)OMX_GoogleAndroidIndexStoreMetaDataInBuffers;
        // Mark for AdaptivePlayback +
        if (mAdaptivePlayback)
        {

        }
        else
        {
            err = OMX_ErrorUnsupportedIndex;
        }
        // Mark for AdaptivePlayback -
    }
#if (ANDROID_VER >= ANDROID_M)
    else if (!strncmp(parameterName, "OMX.google.android.index.storeANWBufferInMetadata", strlen("OMX.google.android.index.storeANWBufferInMetadata")))
    {
        *pIndexType = (OMX_INDEXTYPE)OMX_GoogleAndroidIndexstoreANWBufferInMetadata;
        // Mark for AdaptivePlayback +
        if (mAdaptivePlayback)
        {

        }
        else
        {
            err = OMX_ErrorUnsupportedIndex;
        }
        // Mark for AdaptivePlayback -
    }
#endif
    else if (!strncmp(parameterName, MTK_OMXVDEC_EXTENSION_INDEX_PARAM_LOW_LATENCY_DECODE, strlen(MTK_OMXVDEC_EXTENSION_INDEX_PARAM_LOW_LATENCY_DECODE)))
    {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexVendorMtkOmxVdecLowLatencyDecode;
    }
    else if (!strncmp(parameterName, "OMX.MTK.index.param.video.SetVdecScenario",
                      strlen("OMX.MTK.index.param.video.SetVdecScenario")))
    {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexVendorMtkOmxVdecSetScenario;
    }
    else
    {
        MTK_OMX_LOGE("MtkOmxVdec::GetExtensionIndex Unknown parameter name: %s", parameterName);
        err = OMX_ErrorUnsupportedIndex;
    }

EXIT:
    return err;
}

OMX_ERRORTYPE MtkOmxVdec::GetState(OMX_IN OMX_HANDLETYPE hComponent,
                                   OMX_INOUT OMX_STATETYPE *pState)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;

    if (NULL == pState)
    {
        MTK_OMX_LOGE("[ERROR] MtkOmxVdec::GetState pState is NULL !!!");
        err = OMX_ErrorBadParameter;
        goto EXIT;
    }
    *pState = mState;

    MTK_OMX_LOGD("MtkOmxVdec::GetState (mState=%s)", StateToString(mState));

EXIT:
    return err;
}


OMX_ERRORTYPE MtkOmxVdec::AllocateBuffer(OMX_IN OMX_HANDLETYPE hComponent,
                                         OMX_INOUT OMX_BUFFERHEADERTYPE **ppBufferHdr,
                                         OMX_IN OMX_U32 nPortIndex,
                                         OMX_IN OMX_PTR pAppPrivate,
                                         OMX_IN OMX_U32 nSizeBytes)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    int ret = 0;
#if 1
    if (OMX_TRUE == mIsSecureInst && OMX_FALSE == mbShareableMemoryEnabled)
    {
        if (mTeeType == TRUSTONIC_TEE)
        {
            // for chips need shareable memory (enable shareable memory)

            // adb shell echo 0 > /proc/svp_region
            if (mChipName == VAL_CHIP_NAME_MT6797 || mChipName == VAL_CHIP_NAME_MT6755 || mChipName == VAL_CHIP_NAME_DENALI_3)
            {
                int fd = -1;
                fd = open("/proc/svp_region", O_RDONLY);

                if (fd == -1)
                {
                    MTK_OMX_LOGE("[Info] fail to open /proc/svp_region");
                    fsync(1);
                }
                else
                {
                    //kernel reserve mem for sec. and tell TEE the size and address(Tehsin.Lin)
                    char *share_mem_enable = "0";
                    int vRet;
                    int vRet2;

                    vRet2 = ioctl(fd, SVP_REGION_IOC_OFFLINE, &vRet);
                    MTK_OMX_LOGD("Need Sec Memory %d %d %d", vRet, vRet2, errno);
                    close(fd);
                    mbShareableMemoryEnabled = OMX_TRUE;
                }
            }
        }
    }
#endif

    if (nPortIndex == mInputPortDef.nPortIndex)
    {

        if (OMX_FALSE == mInputPortDef.bEnabled)
        {
            err = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }

        if (OMX_TRUE == mInputPortDef.bPopulated)
        {
            MTK_OMX_LOGE("Errorin MtkOmxVdec::AllocateBuffer, input port already populated, LINE:%d", __LINE__);
            err = OMX_ErrorBadParameter;
            goto EXIT;
        }

        mInputAllocateBuffer = OMX_TRUE;

        if (mIsSecureInst == OMX_FALSE)
        {
            mInputUseION = OMX_TRUE;
        }

        *ppBufferHdr = mInputBufferHdrs[mInputBufferPopulatedCnt] = (OMX_BUFFERHEADERTYPE *)MTK_OMX_ALLOC(sizeof(OMX_BUFFERHEADERTYPE));

        if (OMX_TRUE == mIsSecureInst)
        {
            if (INHOUSE_TEE == mTeeType)
            {
                //#if defined(MTK_SEC_VIDEO_PATH_SUPPORT) && defined(MTK_IN_HOUSE_TEE_SUPPORT)
                MtkVideoAllocateSecureBuffer_Ptr *pfnMtkVideoAllocateSecureBuffer_Ptr = (MtkVideoAllocateSecureBuffer_Ptr *) dlsym(mH264SecVdecInHouseLib, MTK_H264_SEC_VDEC_IN_HOUSE_ALLOCATE_SECURE_BUFFER);
                if (NULL == pfnMtkVideoAllocateSecureBuffer_Ptr)
                {
                    MTK_OMX_LOGE("[ERROR] cannot find MtkVideoAllocateSecureBuffer, LINE: %d", __LINE__);
                    err = OMX_ErrorUndefined;
                    goto EXIT;
                }

                (*ppBufferHdr)->pBuffer = (OMX_U8 *)pfnMtkVideoAllocateSecureBuffer_Ptr(nSizeBytes, 512);  // allocate secure buffer from TEE
                //#endif
            }
            else
            {
                if (NULL != mH264SecVdecTlcLib)
                {
                    MtkH264SecVdec_secMemAllocateTBL_Ptr *pfnMtkH264SecVdec_secMemAllocateTBL = (MtkH264SecVdec_secMemAllocateTBL_Ptr *) dlsym(mH264SecVdecTlcLib, MTK_H264_SEC_VDEC_SEC_MEM_ALLOCATE_TBL_NAME);
                    if (NULL != pfnMtkH264SecVdec_secMemAllocateTBL)
                    {
                        (*ppBufferHdr)->pBuffer = (OMX_U8 *)pfnMtkH264SecVdec_secMemAllocateTBL(1024, nSizeBytes); // for t-play
                    }
                    else
                    {
                        MTK_OMX_LOGE("[ERROR] cannot find MtkH264SecVdec_secMemAllocateTBL, LINE: %d", __LINE__);
                        err = OMX_ErrorUndefined;
                        goto EXIT;
                    }
                }
                else
                {
                    MTK_OMX_LOGE("[ERROR] mH264SecVdecTlcLib is NULL, LINE: %d", __LINE__);
                    err = OMX_ErrorUndefined;
                    goto EXIT;
                }
            }
            MTK_OMX_LOGD("AllocateBuffer hSecureHandle = 0x%08X", (*ppBufferHdr)->pBuffer);
        }
        else
        {
            if (OMX_FALSE == mInputUseION)
            {
                (*ppBufferHdr)->pBuffer = (OMX_U8 *)MTK_OMX_MEMALIGN(MEM_ALIGN_64, nSizeBytes); //(OMX_U8*)MTK_OMX_ALLOC(nSizeBytes);  // allocate input from dram
            }
        }

        (*ppBufferHdr)->nAllocLen = nSizeBytes;
        (*ppBufferHdr)->pAppPrivate = pAppPrivate;
        (*ppBufferHdr)->pMarkData = NULL;
        (*ppBufferHdr)->nInputPortIndex  = MTK_OMX_INPUT_PORT;
        (*ppBufferHdr)->nOutputPortIndex = MTK_OMX_INVALID_PORT;
        //for ACodec color convert
        (*ppBufferHdr)->pPlatformPrivate = NULL;
        //(*ppBufferHdr)->pInputPortPrivate = NULL; // TBD

        if (OMX_FALSE == mIsSecureInst)
        {
            if (OMX_TRUE == mInputUseION)
            {
                ret = mInputMVAMgr->newOmxMVAandVA(MEM_ALIGN_512, (int)nSizeBytes, (void *)(*ppBufferHdr), (void **)(&(*ppBufferHdr)->pBuffer));
                if (ret < 0)
                {
                    MTK_OMX_LOGE("[ERROR] Allocate ION Buffer failed (%d), LINE:%d", ret, __LINE__);
                    err = OMX_ErrorUndefined;
                    goto EXIT;
                }
                mIonInputBufferCount++;
            }
            else    // M4U
            {
                if (strncmp("m4u", mInputMVAMgr->getType(), strlen("m4u")))
                {
                    //if not m4u map
                    delete mInputMVAMgr;
                    mInputMVAMgr = new OmxMVAManager("m4u", "MtkOmxVdec1");
                }
                ret = mInputMVAMgr->newOmxMVAandVA(MEM_ALIGN_512, (int)nSizeBytes,
                                                   (void *)(*ppBufferHdr), (void **)(&(*ppBufferHdr)->pBuffer));
                if (ret < 0)
                {
                    MTK_OMX_LOGE("[ERROR] Allocate M4U failed (%d), LINE:%d", ret, __LINE__);
                    err = OMX_ErrorUndefined;
                    goto EXIT;
                }
                //MTK_OMX_LOGD("[M4U][Input][UseBuffer] mM4UBufferVa = 0x%x, mM4UBufferPa = 0x%x, mM4UBufferSize = 0x%x, mM4UBufferCount = %d\n",
                //      mM4UBufferVa[mM4UBufferCount], mM4UBufferPa[mM4UBufferCount], mM4UBufferSize[mM4UBufferCount], mM4UBufferCount);
                mM4UBufferCount++;
            }

        }

        MTK_OMX_LOGD("MtkOmxVdec::AllocateBuffer In port_idx(0x%X), idx[%d], pBuffHead(0x%08X), pBuffer(0x%08X), mInputUseION(%d)", (unsigned int)nPortIndex, (int)mInputBufferPopulatedCnt, (unsigned int)mInputBufferHdrs[mInputBufferPopulatedCnt], (unsigned int)((*ppBufferHdr)->pBuffer), mInputUseION);


        InsertInputBuf(*ppBufferHdr);

        mInputBufferPopulatedCnt++;

        if (mInputBufferPopulatedCnt == mInputPortDef.nBufferCountActual)
        {
            mInputPortDef.bPopulated = OMX_TRUE;

            if (IS_PENDING(MTK_OMX_IDLE_PENDING))
            {
                SIGNAL(mInPortAllocDoneSem);
                MTK_OMX_LOGD("signal mInPortAllocDoneSem (%d)", get_sem_value(&mInPortAllocDoneSem));
            }

            if (IS_PENDING(MTK_OMX_IN_PORT_ENABLE_PENDING))
            {
                SIGNAL(mInPortAllocDoneSem);
                MTK_OMX_LOGD("signal mInPortAllocDoneSem (%d)", get_sem_value(&mInPortAllocDoneSem));
            }

            MTK_OMX_LOGD("AllocateBuffer:: input port populated");
        }
    }
    else if (nPortIndex == mOutputPortDef.nPortIndex)
    {

        if (OMX_FALSE == mOutputPortDef.bEnabled)
        {
            err = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }

        if (OMX_TRUE == mOutputPortDef.bPopulated)
        {
            MTK_OMX_LOGE("Errorin MtkOmxVdec::AllocateBuffer, output port already populated, LINE:%d", __LINE__);
            err = OMX_ErrorBadParameter;
            goto EXIT;
        }

        mOutputAllocateBuffer = OMX_TRUE;
        MTK_OMX_LOGD("AllocateBuffer:: mOutputAllocateBuffer = OMX_TRUE");

        if (mIsSecureInst == OMX_FALSE)
        {
            mOutputUseION = OMX_TRUE;
        }

        if (0 == mOutputBufferPopulatedCnt)
        {
            size_t arraySize = sizeof(FrmBufStruct) * MAX_TOTAL_BUFFER_CNT;
            MTK_OMX_MEMSET(mFrameBuf, 0x00, arraySize);
            MTK_OMX_LOGD("AllocateBuffer:: clear mFrameBuf");
        }

        *ppBufferHdr = mOutputBufferHdrs[mOutputBufferPopulatedCnt] = (OMX_BUFFERHEADERTYPE *)MTK_OMX_ALLOC(sizeof(OMX_BUFFERHEADERTYPE));

        OMX_U32 u4BufferVa;
        OMX_U32 u4BufferPa;

        if (OMX_TRUE == mIsSecureInst)
        {

            if (INHOUSE_TEE == mTeeType)
            {
                //#if defined(MTK_SEC_VIDEO_PATH_SUPPORT) && defined(MTK_IN_HOUSE_TEE_SUPPORT)
                MtkVideoAllocateSecureBuffer_Ptr *pfnMtkVideoAllocateSecureBuffer_Ptr = (MtkVideoAllocateSecureBuffer_Ptr *) dlsym(mH264SecVdecInHouseLib, MTK_H264_SEC_VDEC_IN_HOUSE_ALLOCATE_SECURE_BUFFER);
                if (NULL == pfnMtkVideoAllocateSecureBuffer_Ptr)
                {
                    MTK_OMX_LOGE("[ERROR] cannot find MtkVideoAllocateSecureBuffer, LINE: %d", __LINE__);
                    err = OMX_ErrorUndefined;
                    goto EXIT;
                }

                (*ppBufferHdr)->pBuffer = (OMX_U8 *)pfnMtkVideoAllocateSecureBuffer_Ptr(nSizeBytes, 512);  // allocate secure buffer from TEE
                //#endif
            }
            else
            {
                if (NULL != mH264SecVdecTlcLib)
                {
                    MtkH264SecVdec_secMemAllocate_Ptr *pfnMtkH264SecVdec_secMemAllocate = (MtkH264SecVdec_secMemAllocate_Ptr *) dlsym(mH264SecVdecTlcLib, MTK_H264_SEC_VDEC_SEC_MEM_ALLOCATE_NAME);
                    if (NULL != pfnMtkH264SecVdec_secMemAllocate)
                    {
                        (*ppBufferHdr)->pBuffer = (OMX_U8 *)pfnMtkH264SecVdec_secMemAllocate(1024, nSizeBytes); //MtkVdecAllocateSecureFrameBuffer(nSizeBytes, 512);   // allocate secure buffer from TEE
                    }
                    else
                    {
                        MTK_OMX_LOGE("[ERROR] cannot find MtkH264SecVdec_secMemAllocate, LINE: %d", __LINE__);
                        err = OMX_ErrorUndefined;
                        goto EXIT;
                    }
                }
                else
                {
                    MTK_OMX_LOGE("[ERROR] mH264SecVdecTlcLib is NULL, LINE: %d", __LINE__);
                    err = OMX_ErrorUndefined;
                    goto EXIT;
                }
            }

            MTK_OMX_LOGD("AllocateBuffer hSecureHandle = 0x%08X", (*ppBufferHdr)->pBuffer);
        }
        else
        {

            if (OMX_TRUE == mStoreMetaDataInBuffers)
            {
                u4BufferVa = (OMX_U32)MTK_OMX_MEMALIGN(MEM_ALIGN_512, nSizeBytes);//(OMX_U32)MTK_OMX_ALLOC(nSizeBytes);  // allocate input from dram
            }

        }

        (*ppBufferHdr)->pBuffer = (OMX_U8 *)u4BufferVa;
        (*ppBufferHdr)->nAllocLen = nSizeBytes;
        (*ppBufferHdr)->pAppPrivate = pAppPrivate;
        (*ppBufferHdr)->pMarkData = NULL;
        (*ppBufferHdr)->nInputPortIndex  = MTK_OMX_INVALID_PORT;
        (*ppBufferHdr)->nOutputPortIndex = MTK_OMX_OUTPUT_PORT;
        //for ACodec color convert
        (*ppBufferHdr)->pPlatformPrivate = NULL;
        //(*ppBufferHdr)->pOutputPortPrivate = NULL; // TBD

        if (mOutputBufferPopulatedCnt < MAX_COLORCONVERT_OUTPUTBUFFER_COUNT)
        {
            mColorConvertDstBufferHdr[mColorConvertDstBufferCount] = (VAL_UINT32_T)(*ppBufferHdr);
            MTK_OMX_LOGD("[AllocateBuffer][Color Convert BUFFER][%d] Hdr = 0x%08X",
                                             mColorConvertDstBufferCount,
                                             mColorConvertDstBufferHdr[mColorConvertDstBufferCount]);
            mColorConvertDstBufferCount++;
        }

#if (ANDROID_VER >= ANDROID_KK)
        if (OMX_FALSE == mStoreMetaDataInBuffers)
        {
#endif
            if (OMX_TRUE == mOutputUseION)
            {

                ret = mOutputMVAMgr->newOmxMVAandVA(MEM_ALIGN_512, (int)nSizeBytes,
                                                    (void *)*ppBufferHdr, (void **)(&(*ppBufferHdr)->pBuffer));
                if (ret < 0)
                {
                    MTK_OMX_LOGE("[ERROR] Allocate ION Buffer failed (%d), LINE:%d", ret, __LINE__);
                    err = OMX_ErrorUndefined;
                    goto EXIT;
                }
                mIonOutputBufferCount++;
                MTK_OMX_LOGD("[AllocateBuffer] mIonOutputBufferCount (%d), u4BuffHdr(0x%08x), LINE:%d", mIonOutputBufferCount, (*ppBufferHdr), __LINE__);
            }
            else // M4U
            {
                if (strncmp("m4u", mOutputMVAMgr->getType(), strlen("m4u")))
                {
                    //if not m4u map
                    delete mOutputMVAMgr;
                    mOutputMVAMgr = new OmxMVAManager("m4u", "MtkOmxVdec1");
                }
                ret = mOutputMVAMgr->newOmxMVAandVA(MEM_ALIGN_512, (int)nSizeBytes,
                                                    (void *)(*ppBufferHdr), (void **)(&(*ppBufferHdr)->pBuffer));

                if (ret < 0)
                {
                    MTK_OMX_LOGE("[ERROR] Allocate M4U failed (%d), LINE:%d", ret, __LINE__);
                    err = OMX_ErrorBadParameter;
                    goto EXIT;
                }
                mM4UBufferCount++;
            }

            MTK_OMX_LOGD("MtkOmxVdec::AllocateBuffer Out port_idx(0x%X), idx[%d], pBuffHead(0x%08X), pBuffer(0x%08X), mOutputUseION(%d)",
                         (unsigned int)nPortIndex, (int)mOutputBufferPopulatedCnt, (unsigned int)mOutputBufferHdrs[mOutputBufferPopulatedCnt], (unsigned int)((*ppBufferHdr)->pBuffer), mOutputUseION);

            InsertFrmBuf(*ppBufferHdr);


            // reset all buffer to black
            if (mOutputUseION == OMX_FALSE)
            {
                OMX_U32 u4PicAllocSize = mOutputPortDef.format.video.nSliceHeight * mOutputPortDef.format.video.nStride;
                //MTK_OMX_LOGD ("mOutputUseION:false, u4PicAllocSize %d", u4PicAllocSize);
                memset((*ppBufferHdr)->pBuffer + u4PicAllocSize, 128, u4PicAllocSize / 2);
                memset((*ppBufferHdr)->pBuffer, 0x10, u4PicAllocSize);
            }
            else
            {
                OMX_U32 u4PicAllocSize = mOutputPortDef.format.video.nSliceHeight * mOutputPortDef.format.video.nStride;
                //MTK_OMX_LOGD ("mOutputUseION:true, u4PicAllocSize %d", u4PicAllocSize);
                memset((*ppBufferHdr)->pBuffer + u4PicAllocSize, 128, u4PicAllocSize / 2);
                memset((*ppBufferHdr)->pBuffer, 0x10, u4PicAllocSize);
            }


#if (ANDROID_VER >= ANDROID_KK)
        }
        else
        {
            InsertFrmBuf(*ppBufferHdr);
        }
#endif

        mOutputBufferPopulatedCnt++;
        MTK_OMX_LOGD("AllocateBuffer:: mOutputBufferPopulatedCnt(%d)output port", mOutputBufferPopulatedCnt);

        if (mOutputBufferPopulatedCnt == mOutputPortDef.nBufferCountActual)
        {
            //CLEAR_SEMAPHORE(mOutPortFreeDoneSem);
            mOutputStrideBeforeReconfig = mOutputPortDef.format.video.nStride;
            mOutputSliceHeightBeforeReconfig = mOutputPortDef.format.video.nSliceHeight;
            mOutputPortDef.bPopulated = OMX_TRUE;

            if (IS_PENDING(MTK_OMX_IDLE_PENDING))
            {
                SIGNAL(mOutPortAllocDoneSem);
                MTK_OMX_LOGD("signal mOutPortAllocDoneSem (%d)", get_sem_value(&mOutPortAllocDoneSem));
            }

            if (IS_PENDING(MTK_OMX_OUT_PORT_ENABLE_PENDING))
            {
                SIGNAL(mOutPortAllocDoneSem);
                MTK_OMX_LOGD("signal mOutPortAllocDoneSem (%d)", get_sem_value(&mOutPortAllocDoneSem));
            }

            MTK_OMX_LOGE("AllocateBuffer:: output port populated");
        }

    }
    else
    {
        err = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

EXIT:
    return err;
}


OMX_ERRORTYPE MtkOmxVdec::UseBuffer(OMX_IN OMX_HANDLETYPE hComponent,
                                    OMX_INOUT OMX_BUFFERHEADERTYPE **ppBufferHdr,
                                    OMX_IN OMX_U32 nPortIndex,
                                    OMX_IN OMX_PTR pAppPrivate,
                                    OMX_IN OMX_U32 nSizeBytes,
                                    OMX_IN OMX_U8 *pBuffer)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    int ret = 0;

    if (nPortIndex == mInputPortDef.nPortIndex)
    {

        if (OMX_FALSE == mInputPortDef.bEnabled)
        {
            err = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }

        if (OMX_TRUE == mInputPortDef.bPopulated)
        {
            MTK_OMX_LOGE("Errorin MtkOmxVdec::UseBuffer, input port already populated, LINE:%d", __LINE__);
            err = OMX_ErrorBadParameter;
            goto EXIT;
        }

        *ppBufferHdr = mInputBufferHdrs[mInputBufferPopulatedCnt] = (OMX_BUFFERHEADERTYPE *)MTK_OMX_ALLOC(sizeof(OMX_BUFFERHEADERTYPE));
        (*ppBufferHdr)->pBuffer = pBuffer;
        (*ppBufferHdr)->nAllocLen = nSizeBytes;
        (*ppBufferHdr)->pAppPrivate = pAppPrivate;
        (*ppBufferHdr)->pMarkData = NULL;
        (*ppBufferHdr)->nInputPortIndex  = MTK_OMX_INPUT_PORT;
        (*ppBufferHdr)->nOutputPortIndex = MTK_OMX_INVALID_PORT;
        //for ACodec color convert
        (*ppBufferHdr)->pPlatformPrivate = NULL;
        //(*ppBufferHdr)->pInputPortPrivate = NULL; // TBD

        if (OMX_TRUE == mInputUseION)
        {
            mIonInputBufferCount++;
        }

        MTK_OMX_LOGD("UB port (0x%X), idx[%d] (0x%08X)(0x%08X), mInputUseION(%d)", (unsigned int)nPortIndex, (int)mInputBufferPopulatedCnt, (unsigned int)mInputBufferHdrs[mInputBufferPopulatedCnt], (unsigned int)pBuffer, mInputUseION);

        InsertInputBuf(*ppBufferHdr);

        mInputBufferPopulatedCnt++;
        if (mInputBufferPopulatedCnt == mInputPortDef.nBufferCountActual)
        {
            mInputPortDef.bPopulated = OMX_TRUE;

            if (IS_PENDING(MTK_OMX_IDLE_PENDING))
            {
                SIGNAL(mInPortAllocDoneSem);
                MTK_OMX_LOGD("signal mInPortAllocDoneSem (%d)", get_sem_value(&mInPortAllocDoneSem));
            }

            if (IS_PENDING(MTK_OMX_IN_PORT_ENABLE_PENDING))
            {
                SIGNAL(mInPortAllocDoneSem);
                MTK_OMX_LOGD("signal mInPortAllocDoneSem (%d)", get_sem_value(&mInPortAllocDoneSem));
            }

            MTK_OMX_LOGD("input port populated");
        }
    }
    else if (nPortIndex == mOutputPortDef.nPortIndex)
    {

        if (OMX_FALSE == mOutputPortDef.bEnabled)
        {
            err = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }

        if (OMX_TRUE == mOutputPortDef.bPopulated)
        {
            MTK_OMX_LOGE("Errorin MtkOmxVdec::UseBuffer, output port already populated, LINE:%d", __LINE__);
            err = OMX_ErrorBadParameter;
            goto EXIT;
        }

        if (0 == mOutputBufferPopulatedCnt)
        {
            size_t arraySize = sizeof(FrmBufStruct) * MAX_TOTAL_BUFFER_CNT;
            MTK_OMX_MEMSET(mFrameBuf, 0x00, arraySize);
            MTK_OMX_LOGD("UseBuffer:: clear mFrameBuf");
        }
        *ppBufferHdr = mOutputBufferHdrs[mOutputBufferPopulatedCnt] = (OMX_BUFFERHEADERTYPE *)MTK_OMX_ALLOC(sizeof(OMX_BUFFERHEADERTYPE));
        (*ppBufferHdr)->pBuffer = pBuffer;
        (*ppBufferHdr)->nAllocLen = nSizeBytes;
        (*ppBufferHdr)->pAppPrivate = pAppPrivate;
        (*ppBufferHdr)->pMarkData = NULL;
        (*ppBufferHdr)->nInputPortIndex  = MTK_OMX_INVALID_PORT;
        (*ppBufferHdr)->nOutputPortIndex = MTK_OMX_OUTPUT_PORT;
        (*ppBufferHdr)->pPlatformPrivate = NULL;

        if (mOutputBufferPopulatedCnt < MAX_COLORCONVERT_OUTPUTBUFFER_COUNT)
        {
            mColorConvertDstBufferHdr[mColorConvertDstBufferCount] = (VAL_UINT32_T)(*ppBufferHdr);
            MTK_OMX_LOGD("[UseBuffer][Color Convert BUFFER][%d] Hdr = 0x%08X",
                                             mColorConvertDstBufferCount,
                                             mColorConvertDstBufferHdr[mColorConvertDstBufferCount]);
            mColorConvertDstBufferCount++;
        }

#if (ANDROID_VER >= ANDROID_KK)
        if (OMX_FALSE == mStoreMetaDataInBuffers)
        {
#endif
            if (OMX_TRUE == mOutputUseION)
            {
                mIonOutputBufferCount++;
                MTK_OMX_LOGE("[UseBuffer] mIonOutputBufferCount (%d), u4BuffHdr(0x%08x), LINE:%d", mIonOutputBufferCount, (*ppBufferHdr), __LINE__);
            }
            else if (OMX_TRUE == mIsSecureInst)
            {
                mSecFrmBufInfo[mSecFrmBufCount].u4BuffHdr = (VAL_UINT32_T)(*ppBufferHdr);
                mSecFrmBufCount++;
            }

            MTK_OMX_LOGD("UB port (0x%X), idx[%d] (0x%08X)(0x%08X), mOutputUseION(%d)", (unsigned int)nPortIndex, (int)mOutputBufferPopulatedCnt, (unsigned int)mOutputBufferHdrs[mOutputBufferPopulatedCnt], (unsigned int)pBuffer, mOutputUseION);

            InsertFrmBuf(*ppBufferHdr);

            //suggest checking the picSize and nSizeBytes first
            if (mIsSecureInst == OMX_TRUE)    // don't do this
            {
            }
            else
            {
                OMX_U32 u4PicAllocSize = mOutputPortDef.format.video.nSliceHeight * mOutputPortDef.format.video.nStride;
                MTK_OMX_LOGD("mOutputUseION: %d, u4PicAllocSize %d", mOutputUseION, u4PicAllocSize);
                memset((*ppBufferHdr)->pBuffer + u4PicAllocSize, 128, u4PicAllocSize / 2);
                memset((*ppBufferHdr)->pBuffer, 0x10, u4PicAllocSize);
            }


#if (ANDROID_VER >= ANDROID_KK)
        }
        else
        {
            InsertFrmBuf(*ppBufferHdr);
        }
#endif

        mOutputBufferPopulatedCnt++;
        MTK_OMX_LOGE("UseBuffer: mOutputBufferPopulatedCnt(%d)output port", mOutputBufferPopulatedCnt);

        {
            buffer_handle_t _handle = NULL;
            //ret = mOutputMVAMgr->getMapHndlFromIndex(mOutputBufferPopulatedCnt - 1, &_handle);
            VBufInfo bufInfo;
            ret = mOutputMVAMgr->getOmxInfoFromVA(pBuffer, &bufInfo);
            _handle = (buffer_handle_t)bufInfo.pNativeHandle;

            if ((ret > 0) && (_handle != NULL))
            {
                gralloc_extra_ion_sf_info_t sf_info;
                //MTK_OMX_LOGU ("gralloc_extra_query");
                memset(&sf_info, 0, sizeof(gralloc_extra_ion_sf_info_t));

                gralloc_extra_query(_handle, GRALLOC_EXTRA_GET_IOCTL_ION_SF_INFO, &sf_info);

                sf_info.pool_id = (int32_t)this;  //  for PQ to identify bitstream instance.

                gralloc_extra_sf_set_status(&sf_info,
                                            GRALLOC_EXTRA_MASK_TYPE | GRALLOC_EXTRA_MASK_DIRTY,
                                            GRALLOC_EXTRA_BIT_TYPE_VIDEO | GRALLOC_EXTRA_BIT_DIRTY);

                gralloc_extra_perform(_handle, GRALLOC_EXTRA_SET_IOCTL_ION_SF_INFO, &sf_info);
                //MTK_OMX_LOGU ("gralloc_extra_perform");
            }
        }

        if (mOutputBufferPopulatedCnt == mOutputPortDef.nBufferCountActual)
        {
            //CLEAR_SEMAPHORE(mOutPortFreeDoneSem);
            mOutputStrideBeforeReconfig = mOutputPortDef.format.video.nStride;
            mOutputSliceHeightBeforeReconfig = mOutputPortDef.format.video.nSliceHeight;
            mOutputPortDef.bPopulated = OMX_TRUE;

            if (IS_PENDING(MTK_OMX_IDLE_PENDING))
            {
                SIGNAL(mOutPortAllocDoneSem);
                MTK_OMX_LOGD("signal mOutPortAllocDoneSem (%d)", get_sem_value(&mOutPortAllocDoneSem));
            }

            if (IS_PENDING(MTK_OMX_OUT_PORT_ENABLE_PENDING))
            {
                SIGNAL(mOutPortAllocDoneSem);
                MTK_OMX_LOGD("signal mOutPortAllocDoneSem (%d)", get_sem_value(&mOutPortAllocDoneSem));
            }

            MTK_OMX_LOGD("output port populated");
        }
    }
    else
    {
        err = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    return err;

EXIT:
    MTK_OMX_LOGD("UseBuffer return err %x", err);
    return err;
}


OMX_ERRORTYPE MtkOmxVdec::FreeBuffer(OMX_IN OMX_HANDLETYPE hComponent,
                                     OMX_IN OMX_U32 nPortIndex,
                                     OMX_IN OMX_BUFFERHEADERTYPE *pBuffHead)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    //MTK_OMX_LOGD ("MtkOmxVdec::FreeBuffer nPortIndex(%d)", nPortIndex);
    OMX_BOOL bAllowFreeBuffer = OMX_FALSE;

    //MTK_OMX_LOGD ("@@ mState=%d, Is LOADED PENDING(%d), Is IDLE PENDING(%d)", mState, IS_PENDING (MTK_OMX_LOADED_PENDING), IS_PENDING (MTK_OMX_IDLE_PENDING));
    if (mState == OMX_StateExecuting || mState == OMX_StateIdle || mState == OMX_StatePause)
    {
        if (((nPortIndex == MTK_OMX_INPUT_PORT) && (mInputPortDef.bEnabled == OMX_FALSE)) ||
            ((nPortIndex == MTK_OMX_OUTPUT_PORT) && (mOutputPortDef.bEnabled == OMX_FALSE)))      // in port disabled case, p.99
        {
            bAllowFreeBuffer = OMX_TRUE;
        }
        else if ((mState == OMX_StateIdle) && (IS_PENDING(MTK_OMX_LOADED_PENDING)))        // de-initialization, p.128
        {
            bAllowFreeBuffer = OMX_TRUE;
        }
        else
        {
            mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                   mAppData,
                                   OMX_EventError,
                                   OMX_ErrorPortUnpopulated,
                                   NULL,
                                   NULL);
            err = OMX_ErrorPortUnpopulated;
            goto EXIT;
        }
    }
    else if ((mState == OMX_StateLoaded) && IS_PENDING(MTK_OMX_IDLE_PENDING))
    {
        bAllowFreeBuffer = OMX_TRUE;
    }

    if ((nPortIndex == MTK_OMX_INPUT_PORT) && bAllowFreeBuffer)
    {

        if (OMX_TRUE == mIsSecureInst)
        {
            MTK_OMX_LOGD("FreeBuffer: hSecureHandle(0x%08X)", pBuffHead->pBuffer);
            if (INHOUSE_TEE == mTeeType)
            {
                //#if defined(MTK_SEC_VIDEO_PATH_SUPPORT) && defined(MTK_IN_HOUSE_TEE_SUPPORT)
                MtkVideoFreeSecureBuffer_Ptr *pfnMtkVideoFreeSecureBuffer_Ptr = (MtkVideoFreeSecureBuffer_Ptr *) dlsym(mH264SecVdecInHouseLib, MTK_H264_SEC_VDEC_IN_HOUSE_FREE_SECURE_BUFFER);
                if (NULL == pfnMtkVideoFreeSecureBuffer_Ptr)
                {
                    MTK_OMX_LOGE("[ERROR] cannot find MtkVideoAllocateSecureBuffer, LINE: %d", __LINE__);
                    err = OMX_ErrorUndefined;
                    goto EXIT;
                }

                if (MTK_SECURE_AL_SUCCESS != pfnMtkVideoFreeSecureBuffer_Ptr((OMX_U32)(pBuffHead->pBuffer)))
                {
                    MTK_OMX_LOGE("MtkVideoFreeSecureBuffer failed, line:%d\n", __LINE__);
                }
                //#endif
            }
            else
            {

                if (NULL != mH264SecVdecTlcLib)
                {
                    MtkH264SecVdec_secMemFreeTBL_Ptr *pfnMtkH264SecVdec_secMemFreeTBL = (MtkH264SecVdec_secMemFreeTBL_Ptr *) dlsym(mH264SecVdecTlcLib, MTK_H264_SEC_VDEC_SEC_MEM_FREE_TBL_NAME);
                    if (NULL != pfnMtkH264SecVdec_secMemFreeTBL)
                    {
                        if (pfnMtkH264SecVdec_secMemFreeTBL((OMX_U32)(pBuffHead->pBuffer)) < 0)      // for t-play
                        {
                            MTK_OMX_LOGE("MtkH264SecVdec_secMemFreeTBL failed, line:%d\n", __LINE__);
                        }
                    }
                    else
                    {
                        MTK_OMX_LOGE("[ERROR] cannot find MtkH264SecVdec_secMemFreeTBL, LINE: %d", __LINE__);
                        err = OMX_ErrorUndefined;
                        goto EXIT;
                    }
                }
                else
                {
                    MTK_OMX_LOGE("[ERROR] mH264SecVdecTlcLib is NULL, LINE: %d", __LINE__);
                    err = OMX_ErrorUndefined;
                    goto EXIT;
                }

            }
        }
        else
        {
			MTK_OMX_LOGD("[FreeBuffer] 0x%08x", pBuffHead->pBuffer);
            int ret = mInputMVAMgr->freeOmxMVAByVa((void *)pBuffHead->pBuffer);
            if (ret < 0)
            {
                MTK_OMX_LOGE("[ERROR][Input][FreeBuffer]0x%08x\n", pBuffHead->pBuffer);
            }
        }

        RemoveInputBuf(pBuffHead);
        // free input buffers
        for (OMX_U32 i = 0 ; i < mInputPortDef.nBufferCountActual ; i++)
        {
            if (pBuffHead == mInputBufferHdrs[i])
            {
                MTK_OMX_LOGD("FB in (0x%08X)", (unsigned int)pBuffHead);
                MTK_OMX_FREE(mInputBufferHdrs[i]);
                mInputBufferHdrs[i] = NULL;
                mInputBufferPopulatedCnt--;
            }
        }

        if (mInputBufferPopulatedCnt == 0)       // all input buffers have been freed
        {
            if ((OMX_TRUE == mInputUseION))
            {
                mIonInputBufferCount = 0;
            }
            mInputPortDef.bPopulated = OMX_FALSE;
            SIGNAL(mInPortFreeDoneSem);
            MTK_OMX_LOGD("MtkOmxVdec::FreeBuffer all input buffers have been freed!!! signal mInPortFreeDoneSem(%d)", get_sem_value(&mInPortFreeDoneSem));
        }

        if ((mInputPortDef.bEnabled == OMX_TRUE) && (mState == OMX_StateLoaded) && IS_PENDING(MTK_OMX_IDLE_PENDING))
        {
            mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                   mAppData,
                                   OMX_EventError,
                                   OMX_ErrorPortUnpopulated,
                                   NULL,
                                   NULL);
        }
    }

    if ((nPortIndex == MTK_OMX_OUTPUT_PORT) && bAllowFreeBuffer)
    {

#if (ANDROID_VER >= ANDROID_KK)
        if (OMX_FALSE == mStoreMetaDataInBuffers)
        {
#endif

            //int ret = mOutputMVAMgr->freeOmxMVAByBufHdr((void*)pBuffHead);
            int ret = mOutputMVAMgr->freeOmxMVAByVa((void *)pBuffHead->pBuffer);
            if (ret < 0)
            {
                MTK_OMX_LOGE("[ERROR][Output][FreeBuffer], LINE: %d\n", __LINE__);
            }

#if (ANDROID_VER >= ANDROID_KK)
        }
        else
        {
            // mStoreMetaDataInBuffers is OMX_TRUE
            OMX_U32 graphicBufHandle = 0;

            GetMetaHandleFromOmxHeader(pBuffHead, &graphicBufHandle);

            VAL_UINT32_T u4Idx;

            if (OMX_TRUE == mPortReconfigInProgress)
            {
                int count = 0;
                while (OMX_FALSE == mFlushDecoderDoneInPortSettingChange)
                {
                    MTK_OMX_LOGD("waiting flush decoder done...");
                    SLEEP_MS(5);
                    count++;
                    if (count == 100)
                    {
                        break;
                    }
                }
            }

            int ret = mOutputMVAMgr->freeOmxMVAByHndl((void *)graphicBufHandle);
            if (ret < 0)
            {
                MTK_OMX_LOGE("[ERROR][Output][FreeBuffer], LINE: %d\n", __LINE__);
            }
        }
#endif
        RemoveFrmBuf(pBuffHead);

        // free color convert buffers
        for (VAL_UINT32_T u4I = 0; u4I < MAX_COLORCONVERT_OUTPUTBUFFER_COUNT; u4I++)
        {
            if (mColorConvertDstBufferHdr[u4I] == (VAL_UINT32_T)pBuffHead)
            {
                MTK_OMX_LOGD("[FreeBuffer][Color Convert BUFFER][%d] Hdr = 0x%08X",
                             u4I, mColorConvertDstBufferHdr[u4I]);
                mColorConvertDstBufferHdr[u4I] = 0xffffffff;
                mColorConvertDstBufferCount--;
            }
        }

        // free output buffers
        for (OMX_U32 i = 0 ; i < mOutputBufferHdrsCnt ; i++)
        {
            if (pBuffHead == mOutputBufferHdrs[i])
            {
                MTK_OMX_FREE(mOutputBufferHdrs[i]);
                mOutputBufferHdrs[i] = NULL;
                mOutputBufferPopulatedCnt--;
                MTK_OMX_LOGE("FB out (0x%08X) mOutputBufferPopulatedCnt(%d)", (unsigned int)pBuffHead, mOutputBufferPopulatedCnt);
            }
        }

        if (mOutputBufferPopulatedCnt == 0)      // all output buffers have been freed
        {
            if (OMX_TRUE == mOutputUseION || OMX_TRUE == mStoreMetaDataInBuffers)
            {
                mIonOutputBufferCount = 0;
                mM4UBufferCount = mInputBufferPopulatedCnt;
            }

            else if (OMX_TRUE == mIsSecureInst)
            {
                mSecFrmBufCount = 0;
            }

            else
            {
                mM4UBufferCount = mInputBufferPopulatedCnt;
            }
            mOutputPortDef.bPopulated = OMX_FALSE;
            SIGNAL(mOutPortFreeDoneSem);
            MTK_OMX_LOGE("MtkOmxVdec::FreeBuffer all output buffers have been freed!!! signal mOutPortFreeDoneSem(%d)", get_sem_value(&mOutPortFreeDoneSem));

            if (mOutputMVAMgr != NULL)
            {
                mOutputMVAMgr->freeOmxMVAAll();
            }
        }

        if ((mOutputPortDef.bEnabled == OMX_TRUE) && (mState == OMX_StateLoaded) && IS_PENDING(MTK_OMX_IDLE_PENDING))
        {
            mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                   mAppData,
                                   OMX_EventError,
                                   OMX_ErrorPortUnpopulated,
                                   NULL,
                                   NULL);
        }
    }

    // TODO: free memory for AllocateBuffer case

EXIT:
    return err;
}


OMX_ERRORTYPE MtkOmxVdec::EmptyThisBuffer(OMX_IN OMX_HANDLETYPE hComponent,
                                          OMX_IN OMX_BUFFERHEADERTYPE *pBuffHead)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    //MTK_OMX_LOGD ("MtkOmxVdec::EmptyThisBuffer pBuffHead(0x%08X), pBuffer(0x%08X), nFilledLen(%u)", pBuffHead, pBuffHead->pBuffer, pBuffHead->nFilledLen);
    int ret;
    OMX_U32 CmdCat = MTK_OMX_BUFFER_COMMAND;
    OMX_U32 buffer_type = MTK_OMX_EMPTY_THIS_BUFFER_TYPE;
    // write 8 bytes to mEmptyBufferPipe  [buffer_type][pBuffHead]
    LOCK(mCmdQLock);
    WRITE_PIPE(CmdCat, mCmdPipe);
    WRITE_PIPE(buffer_type, mCmdPipe);
    WRITE_PIPE(pBuffHead, mCmdPipe);
EXIT:
    UNLOCK(mCmdQLock);
    return err;
}


OMX_ERRORTYPE MtkOmxVdec::FillThisBuffer(OMX_IN OMX_HANDLETYPE hComponent,
                                         OMX_IN OMX_BUFFERHEADERTYPE *pBuffHead)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    //MTK_OMX_LOGD ("MtkOmxVdec::FillThisBuffer pBuffHead(0x%08X), pBuffer(0x%08X), nAllocLen(%u)", pBuffHead, pBuffHead->pBuffer, pBuffHead->nAllocLen);
    int ret;
    OMX_U32 CmdCat = MTK_OMX_BUFFER_COMMAND;
    OMX_U32 buffer_type = MTK_OMX_FILL_THIS_BUFFER_TYPE;
    // write 8 bytes to mFillBufferPipe  [bufId][pBuffHead]
    LOCK(mCmdQLock);
    WRITE_PIPE(CmdCat, mCmdPipe);
    WRITE_PIPE(buffer_type, mCmdPipe);
    WRITE_PIPE(pBuffHead, mCmdPipe);
EXIT:
    UNLOCK(mCmdQLock);
    return err;
}


OMX_ERRORTYPE MtkOmxVdec::ComponentRoleEnum(OMX_IN OMX_HANDLETYPE hComponent,
                                            OMX_OUT OMX_U8 *cRole,
                                            OMX_IN OMX_U32 nIndex)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;

    if ((0 == nIndex) && (NULL != cRole))
    {
        strcpy((char *)cRole, (char *)mCompRole);
        MTK_OMX_LOGD("MtkOmxVdec::ComponentRoleEnum: Role[%s]", cRole);
    }
    else
    {
        err = OMX_ErrorNoMore;
    }

    return err;
}

OMX_BOOL MtkOmxVdec::PortBuffersPopulated()
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


OMX_ERRORTYPE MtkOmxVdec::HandleStateSet(OMX_U32 nNewState)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    //    MTK_OMX_LOGD ("MtkOmxVdec::HandleStateSet");
    switch (nNewState)
    {
        case OMX_StateIdle:
            if ((mState == OMX_StateLoaded) || (mState == OMX_StateWaitForResources))
            {
                MTK_OMX_LOGD("Request [%s]-> [OMX_StateIdle]", StateToString(mState));

                if ((OMX_FALSE == mInputPortDef.bEnabled) || (OMX_FALSE == mOutputPortDef.bEnabled))
                {
                    break; // leave the flow to port enable
                }

                // wait until input/output buffers allocated
                MTK_OMX_LOGD("wait on mInPortAllocDoneSem(%d), mOutPortAllocDoneSem(%d)!!", get_sem_value(&mInPortAllocDoneSem), get_sem_value(&mOutPortAllocDoneSem));
                WAIT(mInPortAllocDoneSem);
                WAIT(mOutPortAllocDoneSem);

                if ((OMX_TRUE == mInputPortDef.bEnabled) && (OMX_TRUE == mOutputPortDef.bEnabled) && (OMX_TRUE == PortBuffersPopulated()))
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
                mInputFlushALL = OMX_TRUE;
                SIGNAL(mOutputBufferSem);

                LOCK(mDecodeLock);
                FlushInputPort();
                FlushOutputPort();
                if (mPortReconfigInProgress == OMX_TRUE)//Bruce, for setting IDLE when port reconfig
                {
                    MTK_OMX_LOGE("Set state when PortReconfigInProgress");
                    mPortReconfigInProgress = OMX_FALSE;
                    mIgnoreGUI = OMX_FALSE;
                }
                UNLOCK(mDecodeLock);

                // de-initialize decoder
                DeInitVideoDecodeHW();

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
                                       NULL,
                                       NULL);
            }
            else
            {
                mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                       mAppData,
                                       OMX_EventError,
                                       OMX_ErrorIncorrectStateTransition,
                                       NULL,
                                       NULL);
            }

            break;

        case OMX_StateExecuting:
            MTK_OMX_LOGD("Request [%s]-> [OMX_StateExecuting]", StateToString(mState));
            if (mState == OMX_StateIdle || mState == OMX_StatePause)
            {
                mInputFlushALL = OMX_FALSE;
                // change state to executing
                mState = OMX_StateExecuting;

                // trigger decode start
                mDecodeStarted = OMX_TRUE;

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
                                       NULL,
                                       NULL);
            }
            else
            {
                mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                       mAppData,
                                       OMX_EventError,
                                       OMX_ErrorIncorrectStateTransition,
                                       NULL,
                                       NULL);
            }
            break;

        case OMX_StatePause:
            MTK_OMX_LOGD("Request [%s]-> [OMX_StatePause]", StateToString(mState));
            if (mState == OMX_StateIdle || mState == OMX_StateExecuting)
            {
                mState = OMX_StatePause;
                mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                       mAppData,
                                       OMX_EventCmdComplete,
                                       OMX_CommandStateSet,
                                       mState,
                                       NULL);
            }
            else if (mState == OMX_StatePause)
            {
                mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                       mAppData,
                                       OMX_EventError,
                                       OMX_ErrorSameState,
                                       NULL,
                                       NULL);
            }
            else
            {
                mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                       mAppData,
                                       OMX_EventError,
                                       OMX_ErrorIncorrectStateTransition,
                                       NULL,
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
                    MTK_OMX_LOGD("wait on mInPortFreeDoneSem(%d) %d, mOutPortFreeDoneSem(%d) %d ", get_sem_value(&mInPortFreeDoneSem), mOutputBufferPopulatedCnt, get_sem_value(&mOutPortFreeDoneSem), mOutputBufferPopulatedCnt);
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
            else if (mState == OMX_StateWaitForResources)
            {
                mState = OMX_StateLoaded;
                mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                       mAppData,
                                       OMX_EventCmdComplete,
                                       OMX_CommandStateSet,
                                       mState,
                                       NULL);
            }
            else if (mState == OMX_StateLoaded)
            {
                mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                       mAppData,
                                       OMX_EventError,
                                       OMX_ErrorSameState,
                                       NULL,
                                       NULL);
            }
            else
            {
                mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                       mAppData,
                                       OMX_EventError,
                                       OMX_ErrorIncorrectStateTransition,
                                       NULL,
                                       NULL);
            }
            break;

        case OMX_StateWaitForResources:
            MTK_OMX_LOGD("Request [%s]-> [OMX_StateWaitForResources]", StateToString(mState));
            if (mState == OMX_StateLoaded)
            {
                mState = OMX_StateWaitForResources;
                mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                       mAppData,
                                       OMX_EventCmdComplete,
                                       OMX_CommandStateSet,
                                       mState,
                                       NULL);
            }
            else if (mState == OMX_StateWaitForResources)
            {
                mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                       mAppData,
                                       OMX_EventError,
                                       OMX_ErrorSameState,
                                       NULL,
                                       NULL);
            }
            else
            {
                mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                       mAppData,
                                       OMX_EventError,
                                       OMX_ErrorIncorrectStateTransition,
                                       NULL,
                                       NULL);
            }
            break;

        case OMX_StateInvalid:
            MTK_OMX_LOGD("Request [%s]-> [OMX_StateInvalid]", StateToString(mState));
            if (mState == OMX_StateInvalid)
            {
                mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                       mAppData,
                                       OMX_EventError,
                                       OMX_ErrorSameState,
                                       NULL,
                                       NULL);
            }
            else
            {
                mState = OMX_StateInvalid;

                // for conformance test <2,7> loaded -> invalid
                mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                       mAppData,
                                       OMX_EventError,
                                       OMX_ErrorInvalidState,
                                       NULL,
                                       NULL);
            }
            break;

        default:
            break;
    }
    return err;
}


OMX_ERRORTYPE MtkOmxVdec::HandlePortEnable(OMX_U32 nPortIndex)
{
    MTK_OMX_LOGD("MtkOmxVdec::HandlePortEnable nPortIndex(0x%X)", (unsigned int)nPortIndex);
    OMX_ERRORTYPE err = OMX_ErrorNone;

    if (nPortIndex == MTK_OMX_INPUT_PORT || nPortIndex == MTK_OMX_ALL_PORT)
    {
        if (IS_PENDING(MTK_OMX_IN_PORT_ENABLE_PENDING))       // p.86 component is not in LOADED state and the port is not populated
        {
            MTK_OMX_LOGD("Wait on mInPortAllocDoneSem(%d)", get_sem_value(&mInPortAllocDoneSem));
            WAIT(mInPortAllocDoneSem);
            CLEAR_PENDING(MTK_OMX_IN_PORT_ENABLE_PENDING);
        }
        mInputFlushALL = OMX_FALSE;

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
                MTK_OMX_LOGD("mPortReconfigInProgress as FALSE");
                mPortReconfigInProgress = OMX_FALSE;
                mIgnoreGUI = OMX_FALSE;

                //mMtkV4L2Device.StreamOnBitstream();
                mMtkV4L2Device.requestBufferFrameBuffer(0);
                mMtkV4L2Device.requestBufferFrameBuffer(mOutputPortDef.nBufferCountActual);
                mMtkV4L2Device.StreamOnFrameBuffer();

            }
        }

        mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                               mAppData,
                               OMX_EventCmdComplete,
                               OMX_CommandPortEnable,
                               MTK_OMX_OUTPUT_PORT,
                               NULL);
    }

    if (IS_PENDING(MTK_OMX_IDLE_PENDING))
    {
        if ((mState == OMX_StateLoaded) || (mState == OMX_StateWaitForResources))
        {
            if ((OMX_TRUE == mInputPortDef.bEnabled) && (OMX_TRUE == mOutputPortDef.bEnabled) && (OMX_TRUE == PortBuffersPopulated()))
            {
                MTK_OMX_LOGD("@@ Change to IDLE in HandlePortEnable()");
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
    }

    return err;
}

int MtkOmxVdec::DequeueInputBuffer()
{
    int input_idx = -1;
    LOCK(mEmptyThisBufQLock);

#if CPP_STL_SUPPORT
    if (mEmptyThisBufQ.size() > 0)
    {
        input_idx = *(mEmptyThisBufQ.begin());
        mEmptyThisBufQ.erase(mEmptyThisBufQ.begin());
    }
#endif

#if ANDROID
    if (mEmptyThisBufQ.size() > 0)
    {
        input_idx = mEmptyThisBufQ[0];
        mEmptyThisBufQ.removeAt(0);
    }
#endif

    UNLOCK(mEmptyThisBufQLock);

    return input_idx;
}


int MtkOmxVdec::GetInputBufferFromETBQ()
{
    int input_idx = -1;
    LOCK(mEmptyThisBufQLock);

#if CPP_STL_SUPPORT
    if (mEmptyThisBufQ.size() > 0)
    {
        input_idx = *(mEmptyThisBufQ.begin());
        mEmptyThisBufQ.erase(mEmptyThisBufQ.begin());
    }
#endif

#if ANDROID
    if (mEmptyThisBufQ.size() > 0)
    {
        input_idx = mEmptyThisBufQ[0];

		// We would remove the bitstream buffer from ETBQ after succeed to queueBitstream.
        //mEmptyThisBufQ.removeAt(0);
    }
#endif

    UNLOCK(mEmptyThisBufQLock);

    return input_idx;
}



void MtkOmxVdec::RemoveInputBufferFromETBQ()
{
#if ANDROID
    int input_idx = -1;
    LOCK(mEmptyThisBufQLock);


    if (mEmptyThisBufQ.size() > 0)
    {
        mEmptyThisBufQ.removeAt(0);
    }
	else
	{
		MTK_OMX_LOGE("[Err] Remove input buffer from the empty ETBQ");
	}
    UNLOCK(mEmptyThisBufQLock);
#endif
}




void MtkOmxVdec::CheckOutputBuffer()
{
    unsigned int i;
    int index;

    LOCK(mFillThisBufQLock);
    for (i = 0; i < mFillThisBufQ.size(); i++)
    {
        index = mFillThisBufQ[i];
        if (OMX_FALSE == IsFreeBuffer(mOutputBufferHdrs[index]))
        {
            MTK_OMX_LOGD("Output[%d] [0x%08X] is not free. pFrameBufArray", index, mOutputBufferHdrs[index]);
        }
        else
        {
            MTK_OMX_LOGD("Output[%d] [0x%08X] is free. pFrameBufArray", index, mOutputBufferHdrs[index]);
        }
    }
    UNLOCK(mFillThisBufQLock);
}

int MtkOmxVdec::DequeueOutputBuffer()
{
    int output_idx = -1, i;
    LOCK(mFillThisBufQLock);

    //MTK_OMX_LOGD("DequeueOutputBuffer -> mFillThisBufQ.size():%d, ", mFillThisBufQ.size());

#if CPP_STL_SUPPORT
    output_idx = *(mFillThisBufQ.begin());
    mFillThisBufQ.erase(mFillThisBufQ.begin());
#endif

#if ANDROID
    for (i = 0; i < mFillThisBufQ.size(); i++)
    {
        output_idx = mFillThisBufQ[i];
        if (OMX_FALSE == IsFreeBuffer(mOutputBufferHdrs[output_idx]))
        {
            MTK_OMX_LOGD("DequeueOutputBuffer(), mOutputBufferHdrs[%d] 0x%08x is not free", output_idx, mOutputBufferHdrs[output_idx]);
        }
        else
        {
            MTK_OMX_LOGE("DequeueOutputBuffer(), mOutputBufferHdrs[%d] 0x%08x is free", output_idx, mOutputBufferHdrs[output_idx]);


            break;
        }
    }

    if (0 == mFillThisBufQ.size())
    {
        //MTK_OMX_LOGD("DequeueOutputBuffer(), mFillThisBufQ.size() is 0, return original idx %d", output_idx);
        UNLOCK(mFillThisBufQLock);
        output_idx = -1;
#ifdef HAVE_AEE_FEATURE
        //aee_system_warning("CRDISPATCH_KEY:OMX video decode issue", NULL, DB_OPT_DEFAULT, "\nOmxVdec should no be here will push dummy output buffer!");
#endif //HAVE_AEE_FEATURE
        return output_idx;
    }

    if (i == mFillThisBufQ.size())
    {
        output_idx = -1;
    }
    else
    {
        output_idx = mFillThisBufQ[i];
        mFillThisBufQ.removeAt(i);
    }
#endif

    UNLOCK(mFillThisBufQLock);

    return output_idx;
}

int MtkOmxVdec::FindQueueOutputBuffer(OMX_BUFFERHEADERTYPE *pBuffHdr)
{
    // ATTENTION, please lock this from caller function.
    int output_idx = -1, i;
    //LOCK(mFillThisBufQLock);

#if ANDROID
    for (i = 0; i < mFillThisBufQ.size(); i++)
    {
        output_idx = mFillThisBufQ[i];
        if (pBuffHdr == mOutputBufferHdrs[output_idx])
        {
            MTK_OMX_LOGD("FQOB %d (%d,%d)", output_idx, i, mFillThisBufQ.size());
            break;
        }
    }
    if (i == mFillThisBufQ.size())
    {
        MTK_OMX_LOGE("FindQueueOutputBuffer not found, 0x%08X", pBuffHdr);
        i = -1;
    }
#endif

    //UNLOCK(mFillThisBufQLock);

    return i;
}

void MtkOmxVdec::QueueInputBuffer(int index)
{
    LOCK(mEmptyThisBufQLock);

    //MTK_OMX_LOGD ("@@ QueueInputBuffer (%d)", index);

#if CPP_STL_SUPPORT
    //mEmptyThisBufQ.push_front(index);
#endif

#if ANDROID
    mEmptyThisBufQ.insertAt(index, 0);
#endif

    UNLOCK(mEmptyThisBufQLock);
}

void MtkOmxVdec::QueueOutputBuffer(int index)
{
    // Caller is responsible for lock protection
    //LOCK(mFillThisBufQLock);

    //MTK_OMX_LOGD ("@@ QueueOutputBuffer");
#if CPP_STL_SUPPORT
    mFillThisBufQ.push_back(index);
#endif

#if ANDROID
    mFillThisBufQ.push(index);
#endif

    //UNLOCK(mFillThisBufQLock);

    if (mInputZero == OMX_TRUE)
    {
        MTK_OMX_LOGD("@@ mInputZero, QueueOutputBuffer SIGNAL (mDecodeSem)");
        mInputZero = OMX_FALSE;
        SIGNAL(mDecodeSem);
    }

}


OMX_BOOL MtkOmxVdec::FlushInputPort()
{
    // in this function ,  ******* mDecodeLock is LOCKED ********
    int cnt = 0;
    MTK_OMX_LOGD("+FlushInputPort");
    mInputFlushALL = OMX_TRUE;
    SIGNAL(mOutputBufferSem);

    DumpETBQ();

    // return all input buffers currently we have
    //ReturnPendingInputBuffers();
	//mMtkV4L2Device.StreamOffBitstream();
    //FlushDecoder(OMX_FALSE);

    // return all input buffers from decoder
    if (mDecoderInitCompleteFlag == OMX_TRUE || mMtkV4L2Device.queuedBitstreamCount() > 0)
    {
        mCmdThreadRequestHandler.setRequest(MTK_CMD_REQUEST_FLUSH_INPUT_PORT);
        MTK_OMX_LOGD("+ Wait BS flush done, Wait mFlushBitstreamBufferDoneSem");
        WAIT_T(mFlushBitstreamBufferDoneSem);
        MTK_OMX_LOGD("- Wait BS flush done");

        iTSIn = 0;
        DisplayTSArray[0] = 0;
    }
    // return all input buffers currently we have
    ReturnPendingInputBuffers();

    MTK_OMX_LOGD("FlushInputPort -> mNumPendingInput(%d), ETBQ size(%d)", (int)mNumPendingInput, mEmptyThisBufQ.size());

#if 0
    while (mNumPendingInput > 0)
    {
        MTK_OMX_LOGD("Wait input buffer release....%d, ", mNumPendingInput, mEmptyThisBufQ.size());
        SLEEP_MS(1);
        cnt ++;
        if (cnt > 2000)
        {
            MTK_OMX_LOGE("Wait input buffer release timeout mNumPendingInput %d", mNumPendingInput);
            abort();
            break;
        }
    }
#endif

    MTK_OMX_LOGD("-FlushInputPort");
    return OMX_TRUE;
}


OMX_BOOL MtkOmxVdec::FlushOutputPort()
{
    // in this function ,  ******* mDecodeLock is LOCKED ********
    MTK_OMX_LOGD("+FlushOutputPort");
    mFlushInProcess = OMX_TRUE;
    while (1 == mFlushInConvertProcess)
    {
        MTK_OMX_LOGD("wait mFlushInConvertProcess");
        usleep(10000);
    }

    VAL_UINT32_T u4SemCount = get_sem_value(&mOutputBufferSem);

    while (u4SemCount > 0 && get_sem_value(&mOutputBufferSem) > 0)
    {
        WAIT(mOutputBufferSem);
        u4SemCount--;
    }

    DumpFTBQ();

    // return all output buffers currently we have
    //ReturnPendingOutputBuffers();

    // return all output buffers from decoder
    //mMtkV4L2Device.StreamOffFrameBuffer();
    //FlushDecoder(OMX_FALSE);

    // return all output buffers from decoder
    if (mDecoderInitCompleteFlag == OMX_TRUE || mMtkV4L2Device.queuedFrameBufferCount() > 0)
    {
        mCmdThreadRequestHandler.setRequest(MTK_CMD_REQUEST_FLUSH_OUTPUT_PORT);
        MTK_OMX_LOGD("+ Wait FB flush done, wait mFlushFrameBufferDoneSem, queuedFrameBufferCount(%d)", mMtkV4L2Device.queuedFrameBufferCount());
        WAIT_T(mFlushFrameBufferDoneSem);
        MTK_OMX_LOGD("- Wait FB flush done, mFlushFrameBufferDoneSem %d", get_sem_value(&mFlushFrameBufferDoneSem));

        iTSIn = 0;
        DisplayTSArray[0] = 0;
    }

    // return all output buffers currently we have
    ReturnPendingOutputBuffers();

    mNumFreeAvailOutput = 0;
    mNumAllDispAvailOutput = 0;
    mNumNotDispAvailOutput = 0;
    mEOSFound = OMX_FALSE;
    mEOSQueued = OMX_FALSE;
    mFlushInProcess = OMX_FALSE;
    MTK_OMX_LOGD("-FlushOutputPort -> mNumPendingOutput(%d)", (int)mNumPendingOutput);

    return OMX_TRUE;
}

OMX_ERRORTYPE MtkOmxVdec::HandlePortDisable(OMX_U32 nPortIndex)
{
    MTK_OMX_LOGD("MtkOmxVdec::HandlePortDisable nPortIndex=0x%X", (unsigned int)nPortIndex);
    OMX_ERRORTYPE err = OMX_ErrorNone;

    // TODO: should we hold mDecodeLock here??

    if (nPortIndex == MTK_OMX_INPUT_PORT || nPortIndex == MTK_OMX_ALL_PORT)
    {

        if (mInputPortDef.bPopulated == OMX_TRUE)
        {
            //if ((mState != OMX_StateLoaded) && (mInputPortDef.bPopulated == OMX_TRUE)) {

            if (mState == OMX_StateExecuting || mState == OMX_StatePause)
            {
                mInputFlushALL = OMX_TRUE;
                SIGNAL(mOutputBufferSem);

                LOCK(mDecodeLock);
                FlushInputPort();
                UNLOCK(mDecodeLock);
            }

            // wait until the input buffers are freed
            WAIT(mInPortFreeDoneSem);
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

        MTK_OMX_LOGD("MtkOmxVdec::HandlePortDisable mOutputPortDef.bPopulated(%d)", mOutputPortDef.bPopulated);

        if (mOutputPortDef.bPopulated == OMX_TRUE)
        {
            //if ((mState != OMX_StateLoaded) && (mOutputPortDef.bPopulated == OMX_TRUE)) {

            if (mState == OMX_StateExecuting || mState == OMX_StatePause)
            {
                // flush output port
                LOCK(mDecodeLock);
                FlushOutputPort();
                UNLOCK(mDecodeLock);

                mFlushDecoderDoneInPortSettingChange = OMX_TRUE;
            }

            // wait until the output buffers are freed
            WAIT(mOutPortFreeDoneSem);
        }
        else
        {
            if (get_sem_value(&mOutPortFreeDoneSem) > 0)
            {
                MTK_OMX_LOGD("@@ OutSem ++");
                int retVal = TRY_WAIT(mOutPortFreeDoneSem);
                if (0 == retVal)
                {
                    MTK_OMX_LOGD("@@ OutSem -- (OK)");
                }
                else if (EAGAIN == retVal)
                {
                    MTK_OMX_LOGD("@@ OutSem -- (EAGAIN)");
                }
            }
        }

        if (OMX_TRUE == mPortReconfigInProgress)
        {
            // update output port def
            getReconfigOutputPortSetting();

            mOutputPortDef.format.video.nFrameWidth		= mReconfigOutputPortSettings.u4Width;
            mOutputPortDef.format.video.nFrameHeight 	= mReconfigOutputPortSettings.u4Height;
            mOutputPortDef.format.video.nStride 		= mReconfigOutputPortSettings.u4RealWidth;
            mOutputPortDef.format.video.nSliceHeight 	= mReconfigOutputPortSettings.u4RealHeight;
            mOutputPortDef.nBufferCountActual 			= mReconfigOutputPortBufferCount;
            mOutputPortDef.nBufferSize 					= mReconfigOutputPortBufferSize;
            mOutputPortDef.format.video.eColorFormat    = mReconfigOutputPortColorFormat;
            mOutputPortFormat.eColorFormat              = mReconfigOutputPortColorFormat;
            MTK_OMX_LOGE("MtkOmxVdec::HandlePortDisable update port definition");

            if ((OMX_TRUE == mCrossMountSupportOn))
            {
                mMaxColorConvertOutputBufferCnt = (mOutputPortDef.nBufferCountActual / 2);
                mReconfigOutputPortBufferCount += mMaxColorConvertOutputBufferCnt;
                mOutputPortDef.nBufferCountActual = mReconfigOutputPortBufferCount;
                MTK_OMX_LOGD("during OMX_EventPortSettingsChanged nBufferCountActual after adjust = %d(+%d) ",
                             mOutputPortDef.nBufferCountActual, mMaxColorConvertOutputBufferCnt);
            }
        }

        // send OMX_EventCmdComplete back to IL client
        mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                               mAppData,
                               OMX_EventCmdComplete,
                               OMX_CommandPortDisable,
                               MTK_OMX_OUTPUT_PORT,
                               NULL);
    }

    return err;
}


OMX_ERRORTYPE MtkOmxVdec::HandlePortFlush(OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    MTK_OMX_LOGD("MtkOmxVdec::HandleFlush nPortIndex(0x%X)", (unsigned int)nPortIndex);

    if (nPortIndex == MTK_OMX_INPUT_PORT || nPortIndex == MTK_OMX_ALL_PORT)
    {
        mInputFlushALL = OMX_TRUE;
        SIGNAL(mOutputBufferSem); // one for the driver callback to get output buffer
        SIGNAL(mOutputBufferSem); // the other one is if one of input buffer is EOS and no output buffer is available

        MTK_OMX_LOGD("get lock before FlushInputPort\n");
        LOCK(mDecodeLock);
        FlushInputPort();
        mInputFlushALL = OMX_FALSE;
        UNLOCK(mDecodeLock);

        MTK_OMX_LOGD("Before callback BS flush done. ETBQ(%d)", mEmptyThisBufQ.size());
        mMtkV4L2Device.dumpDebugInfo();

        if (mEmptyThisBufQ.size() > 0)
        {
            MTK_OMX_LOGD("BS flush not complete.");
            abort();
        }

        mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                               mAppData,
                               OMX_EventCmdComplete,
                               OMX_CommandFlush,
                               MTK_OMX_INPUT_PORT,
                               NULL);

        MTK_OMX_LOGD("After callback BS flush done. ETBQ(%d)", mEmptyThisBufQ.size());
        mMtkV4L2Device.dumpDebugInfo();
    }

    if (nPortIndex == MTK_OMX_OUTPUT_PORT || nPortIndex == MTK_OMX_ALL_PORT)
    {
        MTK_OMX_LOGD("get lock before FlushOutputPort\n");
        LOCK(mDecodeLock);
        FlushOutputPort();
        UNLOCK(mDecodeLock);
        if (mCodecId == MTK_VDEC_CODEC_ID_HEVC && OMX_TRUE == mPortReconfigInProgress)
        {
            mOutputPortDef.format.video.nFrameWidth = mReconfigOutputPortSettings.u4Width;
            mOutputPortDef.format.video.nFrameHeight = mReconfigOutputPortSettings.u4Height;
            mOutputPortDef.format.video.nStride = mReconfigOutputPortSettings.u4RealWidth;
            mOutputPortDef.format.video.nSliceHeight = mReconfigOutputPortSettings.u4RealHeight;
            mOutputPortDef.nBufferCountActual = mReconfigOutputPortBufferCount;
            mOutputPortDef.nBufferSize = mReconfigOutputPortBufferSize;

            if ((OMX_TRUE == mCrossMountSupportOn))
            {
                mMaxColorConvertOutputBufferCnt = (mOutputPortDef.nBufferCountActual / 2);
                mReconfigOutputPortBufferCount += mMaxColorConvertOutputBufferCnt;
                mOutputPortDef.nBufferCountActual = mReconfigOutputPortBufferCount;
                MTK_OMX_LOGD("during OMX_EventPortSettingsChanged nBufferCountActual after adjust = %d(+%d) ",
                             mOutputPortDef.nBufferCountActual, mMaxColorConvertOutputBufferCnt);
            }
        }

        //mMtkV4L2Device.dumpDebugInfo();

        MTK_OMX_LOGD("Before callback FB flush done. FTBQ(%d)", mFillThisBufQ.size());
        mMtkV4L2Device.dumpDebugInfo();

        if (mFillThisBufQ.size() > 0)
        {
            MTK_OMX_LOGD("FB flush not complete.");
            abort();
        }

        mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                               mAppData,
                               OMX_EventCmdComplete,
                               OMX_CommandFlush,
                               MTK_OMX_OUTPUT_PORT,
                               NULL);

        //MTK_OMX_LOGD("Callback Flush Done");

        MTK_OMX_LOGD("After callback FB flush done. FTBQ(%d)", mFillThisBufQ.size());
        mMtkV4L2Device.dumpDebugInfo();


    }

    mMtkV4L2Device.StreamOnBitstream();
    mMtkV4L2Device.StreamOnFrameBuffer();


    return err;
}


OMX_ERRORTYPE MtkOmxVdec::HandleMarkBuffer(OMX_U32 nParam1, OMX_PTR pCmdData)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    MTK_OMX_LOGD("MtkOmxVdec::HandleMarkBuffer");

    return err;
}

OMX_U64 MtkOmxVdec::GetInputBufferCheckSum(OMX_BUFFERHEADERTYPE *pBuffHdr)
{
    OMX_U64 InputCheckSum = 0;
    OMX_U32 i = 0;
    OMX_U8 *InputBufferPointer = NULL;

    InputBufferPointer = pBuffHdr->pBuffer;
    for (i = 0; i < pBuffHdr->nFilledLen; i++)
    {
        InputCheckSum = InputCheckSum + *InputBufferPointer;
        InputBufferPointer ++;
    }

    return InputCheckSum;
}


OMX_ERRORTYPE MtkOmxVdec::HandleEmptyThisBuffer(OMX_BUFFERHEADERTYPE *pBuffHdr)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    MTK_OMX_LOGD("ETB (0x%08X) (0x%08X) (%u) %lld (%d %d) AVSyncTime(%lld)",
                 pBuffHdr, pBuffHdr->pBuffer, pBuffHdr->nFilledLen, pBuffHdr->nTimeStamp, mNumPendingInput, mEmptyThisBufQ.size(), mAVSyncTime);

    int index = findBufferHeaderIndex(MTK_OMX_INPUT_PORT, pBuffHdr);
    if (index < 0)
    {
        MTK_OMX_LOGE("[ERROR] ETB invalid index(%d)", index);
    }
    //MTK_OMX_LOGD ("ETB idx(%d)", index);

    LOCK(mEmptyThisBufQLock);
    mNumPendingInput++;

#if CPP_STL_SUPPORT
    mEmptyThisBufQ.push_back(index);
    DumpETBQ();
#endif

#if ANDROID
    mEmptyThisBufQ.push(index);
    //DumpETBQ();
#endif
    UNLOCK(mEmptyThisBufQLock);

    // for INPUT_DRIVEN
    SIGNAL(mDecodeSem);
    pthread_cond_signal(&cond);

#if 0
    FILE *fp_output;
    OMX_U32 size_fp_output;
    char ucStringyuv[100];
    char *ptemp_buff = (char *)pBuffHdr->pBuffer;
    sprintf(ucStringyuv, "//sdcard//Vdec%4d.bin",  gettid());
    fp_output = fopen(ucStringyuv, "ab");
    if (fp_output != NULL)
    {
        char header[4] = { 0, 0, 0, 1};
        fwrite(header, 1, 4, fp_output);
        size_fp_output = pBuffHdr->nFilledLen;
        MTK_OMX_LOGD("input write size = %d\n", size_fp_output);
        size_fp_output = fwrite(ptemp_buff, 1, size_fp_output, fp_output);
        MTK_OMX_LOGD("input real write size = %d\n", size_fp_output);
        fclose(fp_output);
    }
    else
    {
        LOGE("sdcard/mfv_264.out file create error\n");
    }
#endif

    return err;
}


OMX_ERRORTYPE MtkOmxVdec::HandleFillThisBuffer(OMX_BUFFERHEADERTYPE *pBuffHdr)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    OMX_BOOL bDecodeBuffer = OMX_FALSE;
    MTK_OMX_LOGD("FTB (0x%08X) (0x%08X) (%u) ts(%lld) AVSyncTime(%lld)", pBuffHdr, pBuffHdr->pBuffer, pBuffHdr->nAllocLen, pBuffHdr->nTimeStamp, mAVSyncTime);

    int index = findBufferHeaderIndex(MTK_OMX_OUTPUT_PORT, pBuffHdr);

    int i;
    OMX_BOOL bFound = OMX_FALSE;
    if (index < 0)
    {
        MTK_OMX_LOGE("[ERROR] FTB invalid index(%d)", index);
    }
    //MTK_OMX_LOGD ("FTB idx(%d)", index);


    LOCK(mFillThisBufQLock);

#if (ANDROID_VER >= ANDROID_KK)
    if (OMX_TRUE == mStoreMetaDataInBuffers)
    {
        SetupMetaIonHandle(pBuffHdr);

        int ret = 0;
        int dstIdx = -1;
        VBufInfo info;
        OMX_U32 graphicBufHandle = 0;
        GetMetaHandleFromOmxHeader(pBuffHdr, &graphicBufHandle);
        ret = mOutputMVAMgr->getOmxInfoFromHndl((void *)graphicBufHandle, &info);

        if (1 == ret)
        {
            for (i = 0; i < mOutputPortDef.nBufferCountActual; i++)
            {
                //MTK_OMX_LOGD("xxxxx %d, 0x%08x (0x%08x)", i, mFrameBuf[i].ipOutputBuffer, mOutputBufferHdrs[i]);
                if (mFrameBuf[i].ipOutputBuffer == pBuffHdr)
                {
                    dstIdx = i;
                    break;
                }
            }

            if (dstIdx > -1)
            {
                // Not in OMX's control
                //MTK_OMX_LOGE("[TEST] HandleFillThisBuffer Case 2, new mFrameBuf[%d] hdr = 0x%x, ionHandle = %d, va = 0x%x", dstIdx, pBuffHdr, info.ionBufHndl, info.u4VA);
                mFrameBuf[dstIdx].bDisplay 				= OMX_FALSE;
                mFrameBuf[dstIdx].bNonRealDisplay 		= OMX_FALSE;
                mFrameBuf[dstIdx].bFillThis 			= OMX_TRUE;
                mFrameBuf[dstIdx].iTimestamp 			= 0;
                mFrameBuf[dstIdx].bGraphicBufHandle 	= graphicBufHandle;
                mFrameBuf[dstIdx].ionBufHandle 			= info.ionBufHndl;
                mFrameBuf[dstIdx].refCount 				= 0;

                memset(&mFrameBuf[dstIdx].frame_buffer, 0, sizeof(mFrameBuf[dstIdx].frame_buffer));
                mFrameBuf[dstIdx].frame_buffer.rBaseAddr.u4VA = info.u4VA;
                mFrameBuf[dstIdx].frame_buffer.rBaseAddr.u4PA = info.u4PA;
            }
            else
            {
                MTK_OMX_LOGE("[ERROR] FTB OMX Buffer header not exist 0x%x", pBuffHdr);
            }
        }
        else
        {
            MTK_OMX_LOGE("[ERROR] HandleFillThisBuffer failed to map buffer");
        }
    }
#endif

    mNumPendingOutput++;

    // wake up decode thread
    //SIGNAL(mDecodeSem);
    //pthread_cond_signal(&cond);


    MTK_OMX_LOGE("[@#@] FTB idx %d, mNumPendingOutput(%d) / ( %d / %d )",
                 index, mNumPendingOutput, mBufColorConvertDstQ.size(), mBufColorConvertSrcQ.size());

#if CPP_STL_SUPPORT
    mFillThisBufQ.push_back(index);
    //DumpFTBQ();
#endif

#if ANDROID

    if (OMX_TRUE == IsColorConvertBuffer(pBuffHdr))
    {
        if (PrepareAvaliableColorConvertBuffer(index, OMX_FALSE) >= 0)
        {
            MTK_OMX_LOGD("index:%d is color convert buffer mBufColorConvertDstQ.size():%d", index, mBufColorConvertDstQ.size());
        }
        else
        {
            //MTK_OMX_LOGD("Color Convert Off, push to FillThisBufQ");
            bDecodeBuffer = OMX_TRUE;
            mFillThisBufQ.push(index);
        }
    }
    else
    {
        //#else
        bDecodeBuffer = OMX_TRUE;
        mFillThisBufQ.push(index);
    }
    //DumpFTBQ();
#endif //Android


    /*int mReturnIndex = -1;

    if (bDecodeBuffer == OMX_TRUE)
    {
        mReturnIndex = PrepareAvaliableColorConvertBuffer(index, OMX_FALSE);

        if (0 > mReturnIndex)
        {
            //ALOGE("PrepareAvaliableColorConvertBuffer retry");
        }
    }*/

    // wake up decode thread
    SIGNAL(mDecodeSem);
    pthread_cond_signal(&cond);


    for (i = 0; i < mOutputPortDef.nBufferCountActual; i++)
    {
        //MTK_OMX_LOGD("xxxxx %d, 0x%08x (0x%08x)", i, mFrameBuf[i].ipOutputBuffer, mOutputBufferHdrs[i]);
        if (mFrameBuf[i].ipOutputBuffer == pBuffHdr && bDecodeBuffer == OMX_TRUE)
        {
            mFrameBuf[i].bFillThis = OMX_TRUE;
            SIGNAL(mOutputBufferSem);
            // always signal for racing issue
            //SIGNAL(mDecodeSem);
            bFound = OMX_TRUE;
        }
    }

    UNLOCK(mFillThisBufQLock);

    if (bFound == OMX_FALSE && bDecodeBuffer == OMX_TRUE)
    {
        mNumFreeAvailOutput++;
        //MTK_OMX_LOGD("0x%08x SIGNAL mDecodeSem from HandleFillThisBuffer() 2", this);
        //SIGNAL(mDecodeSem);
    }

    if (mNumAllDispAvailOutput > 0)
    {
        mNumAllDispAvailOutput--;
    }


    return err;
}


OMX_ERRORTYPE MtkOmxVdec::HandleEmptyBufferDone(OMX_BUFFERHEADERTYPE *pBuffHdr)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    MTK_OMX_LOGD("EBD (0x%08X) (0x%08X) (%d) (%d %d)", pBuffHdr, pBuffHdr->pBuffer, mNumPendingInput, mNumPendingInput, mEmptyThisBufQ.size());

    LOCK(mEmptyThisBufQLock);
    if (mNumPendingInput > 0)
    {
        mNumPendingInput--;
    }
    else
    {
        MTK_OMX_LOGE("[ERROR] mNumPendingInput == 0 and want to --");
    }
    UNLOCK(mEmptyThisBufQLock);

    mCallback.EmptyBufferDone((OMX_HANDLETYPE)&mCompHandle,
                              mAppData,
                              pBuffHdr);

    return err;
}


void MtkOmxVdec::HandleFillBufferDone_DI_SetColorFormat(OMX_BUFFERHEADERTYPE *pBuffHdr, OMX_BOOL mRealCallBackFillBufferDone)
{
    if (mDeInterlaceEnable && (pBuffHdr->nFilledLen > 0))
    {
        if ((mInterlaceChkComplete == OMX_FALSE) && (mThumbnailMode != OMX_TRUE)
            && (meDecodeType != VDEC_DRV_DECODER_MTK_SOFTWARE))
        {
            VAL_UINT32_T u32VideoInteraceing  = 0;
            // V4L2 todo: if (VDEC_DRV_MRESULT_OK != eVDecDrvGetParam(mDrvHandle, VDEC_DRV_GET_TYPE_QUERY_VIDEO_INTERLACING, NULL, &u32VideoInteraceing))
            if (0 == mMtkV4L2Device.getInterlacing(&u32VideoInteraceing))
            {
                MTK_OMX_LOGE("VDEC_DRV_GET_TYPE_QUERY_VIDEO_INTERLACING not support");
                u32VideoInteraceing = 0;
            }
            mMtkV4L2Device.getInterlacing(&u32VideoInteraceing);
            mIsInterlacing = (VAL_BOOL_T)u32VideoInteraceing;
            MTK_OMX_LOGD("mIsInterlacing %d", mIsInterlacing);
            if (mIsInterlacing == OMX_TRUE)
            {
                mOutputPortFormat.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatVendorMTKYUV_FCM;
                mOutputPortDef.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatVendorMTKYUV_FCM;

                mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                       mAppData,
                                       OMX_EventPortSettingsChanged,
                                       MTK_OMX_OUTPUT_PORT,
                                       OMX_IndexVendMtkOmxUpdateColorFormat,
                                       NULL);

                mInterlaceChkComplete = OMX_TRUE;
            }
        }
    }
}

void MtkOmxVdec::HandleFillBufferDone_FlushCache(OMX_BUFFERHEADERTYPE *pBuffHdr, OMX_BOOL mRealCallBackFillBufferDone)
{
#if 0
    if (OMX_TRUE == mMJCEnable && OMX_FALSE == mRealCallBackFillBufferDone)
    {
        if ((OMX_TRUE == mOutputAllocateBuffer && (OMX_FALSE == mThumbnailMode)) ||
            ((OMX_TRUE == mStoreMetaDataInBuffers) && (OMX_TRUE == mbYUV420FlexibleMode)))
        {
            for (OMX_U32 i = 0; i < mOutputPortDef.nBufferCountActual; i++)
            {
                if (mFrameBuf[i].ipOutputBuffer == pBuffHdr)
                {
                    if (1)//if (OMX_TRUE == mFrameBuf[i].bUsed) // v4l2 todo. Need to check this when MJC is on
                    {
                        MTK_OMX_LOGD("@@ GetFrmStructure frm=0x%x, omx=0x%x, i=%d, color= %x, type= %x", &mFrameBuf[i].frame_buffer, pBuffHdr, i,
                                     mOutputPortFormat.eColorFormat, mInputPortFormat.eCompressionFormat);
                        //todo: cancel?
                        if ((meDecodeType == VDEC_DRV_DECODER_MTK_SOFTWARE && mCodecId == MTK_VDEC_CODEC_ID_HEVC) ||
                            (meDecodeType == VDEC_DRV_DECODER_MTK_SOFTWARE && mCodecId == MTK_VDEC_CODEC_ID_VPX))
                        {
                            MTK_OMX_LOGD("@Flush Cache Before MDP");
                            eVideoFlushCache(NULL, 0, 0);
                            sched_yield();
                            usleep(1000);       //For CTS checksum fail to make sure flush cache to dram
                        }
                    }
                    else
                    {
                        MTK_OMX_LOGD("GetFrmStructure is not in used for convert, flag %x", mFrameBuf[i].ipOutputBuffer->nFlags);
                    }
                }
            }
        }
    }
#endif
}

void MtkOmxVdec::HandleFillBufferDone_FillBufferToPostProcess(OMX_BUFFERHEADERTYPE *pBuffHdr, OMX_BOOL mRealCallBackFillBufferDone)
{
#if 0
    //MTK_OMX_LOGD("pBuffHdr->nTimeStamp -> %lld, length -> %d", pBuffHdr->nTimeStamp, pBuffHdr->nFilledLen);
    if (mUseClearMotion == OMX_TRUE && mMJCBufferCount == 0 && mScalerBufferCount == 0 && mDecoderInitCompleteFlag == OMX_TRUE && pBuffHdr->nFilledLen != 0)
    {
        //Bypass MJC if MJC/Scaler output buffers are not marked
        MJC_MODE mMode;
        mUseClearMotion = OMX_FALSE;
        mMode = MJC_MODE_BYPASS;
        m_fnMJCSetParam(mpMJC, MJC_PARAM_RUNTIME_DISABLE, &mMode);
        //mpMJC->mScaler.SetParameter(MJCScaler_PARAM_MODE, &mMode);
        ALOGD("No MJC buffer or Scaler buffer (%d)(%d)", mMJCBufferCount, mScalerBufferCount);
    }

    // For Scaler ClearMotion +
    SIGNAL(mpMJC->mMJCFrameworkSem);
    // For Scaler ClearMotion -
#endif
}

void MtkOmxVdec::HandleFillBufferDone_FillBufferToFramework(OMX_BUFFERHEADERTYPE *pBuffHdr, OMX_BOOL mRealCallBackFillBufferDone)
{
    //MTK_OMX_LOGD("FBD (0x%08X) (0x%08X) %lld (%u) GET_DISP i(%d) frm_buf(0x%08X), flags(0x%08x) mFlushInProcess %d",
    //             pBuffHdr, pBuffHdr->pBuffer, pBuffHdr->nTimeStamp, pBuffHdr->nFilledLen,
    //             mGET_DISP_i, mGET_DISP_tmp_frame_addr, pBuffHdr->nFlags, mFlushInProcess);

    if ((OMX_TRUE == mStoreMetaDataInBuffers) && (pBuffHdr->nFilledLen != 0))
    {
        OMX_U32 bufferType = *((OMX_U32 *)pBuffHdr->pBuffer);
        //MTK_OMX_LOGD("bufferType %d, %d, %d", bufferType, sizeof(VideoGrallocMetadata),
        //    sizeof(VideoNativeMetadata));
        // check buffer type
        if (kMetadataBufferTypeGrallocSource == bufferType)
        {
            pBuffHdr->nFilledLen = sizeof(VideoGrallocMetadata);//8
        }
        else if (kMetadataBufferTypeANWBuffer == bufferType)
        {
            pBuffHdr->nFilledLen = sizeof(VideoNativeMetadata);//12 in 32 bit
        }
    }

    HandleGrallocExtra(pBuffHdr);

#if (ANDROID_VER >= ANDROID_M)
    WaitFence(pBuffHdr, OMX_FALSE);
#endif

    if (OMX_TRUE == mOutputAllocateBuffer || OMX_TRUE == needColorConvertWithNativeWindow())
    {
        OMX_U32 i = 0;
        for (i = 0; i < mOutputPortDef.nBufferCountActual; i++)
        {
            if (mFrameBuf[i].ipOutputBuffer == pBuffHdr)
            {
                MTK_OMX_LOGD("@@ QueueOutputColorConvertSrcBuffer frm=0x%x, omx=0x%x, i=%d, pBuffHdr->nFlags = %x, mFlushInProcess %d, EOS %d, t: %lld",
                             &mFrameBuf[i].frame_buffer, pBuffHdr, i,
                             pBuffHdr->nFlags, mFlushInProcess, mEOSFound, pBuffHdr->nTimeStamp);

                QueueOutputColorConvertSrcBuffer(i);

                break;
            }
            else
            {
                //MTK_OMX_LOGE(" %d/%d, __LINE__ %d", i, mOutputPortDef.nBufferCountActual, __LINE__);
            }
        }
        if (mOutputPortDef.nBufferCountActual == i)
        {
            MTK_OMX_LOGD(" QueueOutputColorConvertSrcBuffer out of range %d", i);
        }

    }
    else
    {
        if (OMX_TRUE == mCrossMountSupportOn)
        {
            //VBufInfo info; //mBufInfo
            int ret = 0;
            buffer_handle_t _handle;
            if (OMX_TRUE == mStoreMetaDataInBuffers)
            {
                OMX_U32 graphicBufHandle = 0;

                GetMetaHandleFromOmxHeader(pBuffHdr, &graphicBufHandle);

                ret = mOutputMVAMgr->getOmxInfoFromHndl((void *)graphicBufHandle, &mBufInfo);

            }
            else
            {
                ret = mOutputMVAMgr->getOmxInfoFromVA((void *)pBuffHdr->pBuffer, &mBufInfo);
            }

            if (ret < 0)
            {
                MTK_OMX_LOGD("HandleGrallocExtra cannot find buffer info, LINE: %d", __LINE__);
            }
            else
            {
                MTK_OMX_LOGD("mBufInfo u4VA %x, u4PA %x, iIonFd %d", mBufInfo.u4VA, mBufInfo.u4PA, mBufInfo.iIonFd);
                pBuffHdr->pPlatformPrivate = (OMX_U8 *)&mBufInfo;
            }

            pBuffHdr->nFlags |= OMX_BUFFERFLAG_VDEC_OUTPRIVATE;
        }

        LOCK(mFillThisBufQLock);
        mNumPendingOutput--;
        //MTK_OMX_LOGD ("FBD mNumPendingOutput(%d)", mNumPendingOutput);
        if (OMX_TRUE == mStoreMetaDataInBuffers)
        {
            int srcIdx = -1;
            int idx = 0;

            for (idx = 0; idx < mOutputPortDef.nBufferCountActual; idx++)
            {
                if (mFrameBuf[idx].ipOutputBuffer == pBuffHdr)
                {
                    srcIdx = idx;
                    break;
                }
            }

            if (srcIdx > -1)
            {
				mOutputMVAMgr->freeOmxMVAByVa((void *)mFrameBuf[srcIdx].frame_buffer.rBaseAddr.u4VA);

                mFrameBuf[srcIdx].bDisplay 				= OMX_FALSE;
                mFrameBuf[srcIdx].bNonRealDisplay 		= OMX_FALSE;
                mFrameBuf[srcIdx].bFillThis 			= OMX_FALSE;
                mFrameBuf[srcIdx].iTimestamp 			= 0;
                mFrameBuf[srcIdx].bGraphicBufHandle 	= 0;
                mFrameBuf[srcIdx].ionBufHandle 			= 0;
                mFrameBuf[srcIdx].refCount 				= 0;
				memset(&mFrameBuf[srcIdx].frame_buffer, 0, sizeof(VDEC_DRV_FRAMEBUF_T));
            }
            else
            {
                MTK_OMX_LOGE("[ERROR] frame buffer array out of bound - src %d", srcIdx);
            }
        }
        UNLOCK(mFillThisBufQLock);
        mCallback.FillBufferDone((OMX_HANDLETYPE)&mCompHandle,
                                 mAppData,
                                 pBuffHdr);

        if (pBuffHdr->nFlags & OMX_BUFFERFLAG_EOS)
        {
            MTK_OMX_LOGD("[Info] %s callback event OMX_EventBufferFlag %d %d %d", __func__, mOutputAllocateBuffer, mThumbnailMode, needColorConvertWithNativeWindow());
            mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                   mAppData,
                                   OMX_EventBufferFlag,
                                   MTK_OMX_OUTPUT_PORT,
                                   pBuffHdr->nFlags,
                                   NULL);
        }

	    MTK_OMX_LOGD("FBD (0x%08X) (0x%08X) %lld (%u) GET_DISP i(%d) frm_buf(0x%08X), flags(0x%08x) mFlushInProcess %d",
	                 pBuffHdr, pBuffHdr->pBuffer, pBuffHdr->nTimeStamp, pBuffHdr->nFilledLen,
	                 mGET_DISP_i, mGET_DISP_tmp_frame_addr, pBuffHdr->nFlags, mFlushInProcess);

	    mGET_DISP_i = 0;
	    mGET_DISP_tmp_frame_addr = 0;
    }
}

OMX_ERRORTYPE MtkOmxVdec::HandleFillBufferDone(OMX_BUFFERHEADERTYPE *pBuffHdr, OMX_BOOL mRealCallBackFillBufferDone)
{
    //MTK_OMX_LOGE("### mDeInterlaceEnable = %d mInterlaceChkComplete = %d mThumbnailMode = %d\n", mDeInterlaceEnable, mInterlaceChkComplete, mThumbnailMode);

    HandleFillBufferDone_DI_SetColorFormat(pBuffHdr, mRealCallBackFillBufferDone);

    HandleFillBufferDone_FlushCache(pBuffHdr, mRealCallBackFillBufferDone);
    {
        HandleFillBufferDone_FillBufferToFramework(pBuffHdr, mRealCallBackFillBufferDone);
    }

    return OMX_ErrorNone;
}

OMX_BOOL MtkOmxVdec::GrallocExtraSetBufParameter(buffer_handle_t _handle,
                                                 VAL_UINT32_T gralloc_masks, VAL_UINT32_T gralloc_bits, OMX_TICKS nTimeStamp,
                                                 VAL_BOOL_T bIsMJCOutputBuffer, VAL_BOOL_T bIsScalerOutputBuffer)
{
    GRALLOC_EXTRA_RESULT err = GRALLOC_EXTRA_OK;
    gralloc_extra_ion_sf_info_t sf_info;
    VAL_UINT32_T uYAlign = 0;
    VAL_UINT32_T uCbCrAlign = 0;
    VAL_UINT32_T uHeightAlign = 0;

    err = gralloc_extra_query(_handle, GRALLOC_EXTRA_GET_IOCTL_ION_SF_INFO, &sf_info);
    if (GRALLOC_EXTRA_OK != err)
    {
        VAL_UINT32_T u4I;
        //for (u4I = 0; u4I < mIonOutputBufferCount; u4I++)
        //{
        //    MTK_OMX_LOGE("mIonOutputBufferInfo[%d].pNativeHandle:0x%x (%d)", u4I, mIonOutputBufferInfo[u4I].pNativeHandle, mOutputPortDef.nBufferCountActual);
        //}
        MTK_OMX_LOGE("GrallocExtraSetBufParameter(), gralloc_extra_query error:0x%x", err);
        return OMX_FALSE;
    }

    // buffer parameter
    gralloc_extra_sf_set_status(&sf_info, gralloc_masks, gralloc_bits);
    sf_info.videobuffer_status = 0;

    uYAlign = mQInfoOut.u4StrideAlign;
    uCbCrAlign = mQInfoOut.u4StrideAlign / 2;
    uHeightAlign = mQInfoOut.u4SliceHeightAlign;

    // Flexible YUV format, need to specify layout
    // I420 is always used for flexible yuv output with native window, no need to specify
    if (!bIsMJCOutputBuffer && (OMX_COLOR_FormatYUV420Planar == mOutputPortFormat.eColorFormat || OMX_MTK_COLOR_FormatYV12 == mOutputPortFormat.eColorFormat) && VAL_FALSE == mbYUV420FlexibleMode)
    {
        if (OMX_COLOR_FormatYUV420Planar == mOutputPortFormat.eColorFormat)
        {
            gralloc_extra_sf_set_status(&sf_info, GRALLOC_EXTRA_MASK_CM, GRALLOC_EXTRA_BIT_CM_I420);
        }
        else if (OMX_MTK_COLOR_FormatYV12 == mOutputPortFormat.eColorFormat)
        {
            gralloc_extra_sf_set_status(&sf_info, GRALLOC_EXTRA_MASK_CM, GRALLOC_EXTRA_BIT_CM_YV12);
        }
    }

    if (bIsMJCOutputBuffer)
    {
        // MTK BLK alignment for MJC processed buffer
        uYAlign = 16;
        uCbCrAlign = 8;
        uHeightAlign = 32;
    }
    else if (bIsScalerOutputBuffer)
    {
        uYAlign = 16;
        uCbCrAlign = 16;
        uHeightAlign = 16;
    }
    else
    {
        // Android YV12 has 16/16/16 align, different from other YUV planar format
        if (OMX_MTK_COLOR_FormatYV12 == mOutputPortFormat.eColorFormat && uCbCrAlign < 16)
        {
            uCbCrAlign = 16;
        }
        else if (VAL_TRUE == mbYUV420FlexibleMode) // Internal color converted to fit standard I420
        {
            uYAlign = 2;
            uCbCrAlign = 1;
            uHeightAlign = 2;
        }

        if (mOutputPortFormat.eColorFormat == OMX_COLOR_FormatVendorMTKYUV_UFO)
        {
            if (mCodecId == MTK_VDEC_CODEC_ID_HEVC)
            {
                if ((mOutputPortDef.format.video.nFrameWidth / 64) == (mOutputPortDef.format.video.nStride / 64) &&
                    (mOutputPortDef.format.video.nFrameWidth / 64) == (mOutputPortDef.format.video.nSliceHeight / 64))
                {
                    MTK_OMX_LOGD("@@ UFO HandleGrallocExtra 64x64(0x%08X)", _handle);
                    uYAlign = 64;
                    uCbCrAlign = 32;
                    uHeightAlign = 64;
                }
                else if ((mOutputPortDef.format.video.nFrameWidth / 32) == (mOutputPortDef.format.video.nStride / 32) &&
                         (mOutputPortDef.format.video.nFrameWidth / 32) == (mOutputPortDef.format.video.nSliceHeight / 32))
                {
                    MTK_OMX_LOGD("@@ UFO HandleGrallocExtra 32x32(0x%08X)", _handle);
                    uYAlign = 32;
                    uCbCrAlign = 16;
                    uHeightAlign = 32;
                }
                else
                {
                    MTK_OMX_LOGD("@@ UFO HandleGrallocExtra 16x32(0x%08X)", _handle);
                }
            }
            else
            {
                MTK_OMX_LOGD("@@ UFO HandleGrallocExtra 16x32(0x%08X)", _handle);
            }
        }
    }

    //MTK_OMX_LOGD("@@ Video buffer status Y/Cb/Cr alignment =  %u/%u/%u, Height alignment = %u, Deinterlace = %u",
    //             uYAlign, uCbCrAlign, uCbCrAlign, uHeightAlign, (1u & mIsInterlacing));
    sf_info.videobuffer_status = (1 << 31) |
                                 ((uYAlign >> 1) << 25) |
                                 ((uCbCrAlign >> 1) << 19) |
                                 ((uHeightAlign >> 1) << 13) |
                                 ((mIsInterlacing & 1) << 12);
    //MTK_OMX_LOGD("@@ Video buffer status 0x%x", sf_info.videobuffer_status);

    // timestamp, u32
    sf_info.timestamp = nTimeStamp / 1000;
    //MTK_OMX_LOGD("GrallocExtraSetBufParameter(), timestamp: %d", sf_info.timestamp);


    err = gralloc_extra_perform(_handle, GRALLOC_EXTRA_SET_IOCTL_ION_SF_INFO, &sf_info);
    if (GRALLOC_EXTRA_OK != err)
    {
        MTK_OMX_LOGE("GrallocExtraSetBufParameter(), gralloc_extra_perform error:0x%x", err);
        return OMX_FALSE;
    }
    return OMX_TRUE;
}

OMX_BOOL MtkOmxVdec::HandleGrallocExtra(OMX_BUFFERHEADERTYPE *pBuffHdr)
{
    VAL_UINT32_T u4I;

    if (OMX_TRUE == mOutputUseION)
    {
        VBufInfo info;
        int ret = 0;
        buffer_handle_t _handle;
        if (OMX_TRUE == mStoreMetaDataInBuffers)
        {
            OMX_U32 graphicBufHandle = 0;

            GetMetaHandleFromOmxHeader(pBuffHdr, &graphicBufHandle);

            ret = mOutputMVAMgr->getOmxInfoFromHndl((void *)graphicBufHandle, &info);

        }
        else
        {
            ret = mOutputMVAMgr->getOmxInfoFromVA((void *)pBuffHdr->pBuffer, &info);
        }

        if (ret < 0)
        {
            MTK_OMX_LOGD("HandleGrallocExtra cannot find buffer info, LINE: %d", __LINE__);
        }

        _handle = (buffer_handle_t)info.pNativeHandle;

        //MTK_OMX_LOGD ("@@ HandleGrallocExtra (0x%08X)", _handle);
        VAL_UINT32_T gralloc_masks = GRALLOC_EXTRA_MASK_TYPE | GRALLOC_EXTRA_MASK_DIRTY;
        VAL_UINT32_T gralloc_bits = GRALLOC_EXTRA_BIT_TYPE_VIDEO;

        if ((pBuffHdr->nFlags & OMX_BUFFERFLAG_MJC_DUMMY_OUTPUT_BUFFER) != OMX_BUFFERFLAG_MJC_DUMMY_OUTPUT_BUFFER)
        {
            gralloc_bits |= GRALLOC_EXTRA_BIT_DIRTY;
        }

        if (mOutputPortDef.format.video.eColorFormat == OMX_COLOR_FormatVendorMTKYUV_FCM)
        {
            gralloc_masks |= GRALLOC_EXTRA_MASK_CM;
            gralloc_bits |= GRALLOC_EXTRA_BIT_CM_NV12_BLK_FCM;
        }

        gralloc_masks |= GRALLOC_EXTRA_MASK_YUV_COLORSPACE;
        gralloc_bits |= GRALLOC_EXTRA_BIT_YUV_BT601_NARROW;

        if (meDecodeType == VDEC_DRV_DECODER_MTK_HARDWARE)
        {
            gralloc_masks |= GRALLOC_EXTRA_MASK_FLUSH;
            gralloc_bits |= GRALLOC_EXTRA_BIT_NOFLUSH;
        }
        if (m3DStereoMode == OMX_VIDEO_H264FPA_SIDEBYSIDE)
        {
            gralloc_masks |= GRALLOC_EXTRA_MASK_S3D;
            gralloc_bits |= GRALLOC_EXTRA_BIT_S3D_SBS;
        }
        else if (m3DStereoMode == OMX_VIDEO_H264FPA_TOPANDBOTTOM)
        {
            gralloc_masks |= GRALLOC_EXTRA_MASK_S3D;
            gralloc_bits |= GRALLOC_EXTRA_BIT_S3D_TAB;
        }
        else
        {
            gralloc_masks |= GRALLOC_EXTRA_MASK_S3D;
            gralloc_bits |= GRALLOC_EXTRA_BIT_S3D_2D;
        }


        VAL_BOOL_T bIsMJCOutputBuffer = OMX_FALSE;
        VAL_BOOL_T bIsScalerOutputBuffer = OMX_FALSE;
        if (NULL != _handle)
        {
            GrallocExtraSetBufParameter(_handle, gralloc_masks, gralloc_bits, pBuffHdr->nTimeStamp, bIsMJCOutputBuffer, bIsScalerOutputBuffer);
        }
        else
        {
            //it should be handle the NULL case in non-meta mode
            MTK_OMX_LOGD("GrallocExtraSetBufParameter handle is null, skip once");
        }
        //__setBufParameter(_handle, gralloc_masks, gralloc_bits);
}

if (OMX_TRUE == mIsSecureInst)
{
    for (u4I = 0; u4I < mSecFrmBufCount; u4I++)
    {
        if (((VAL_UINT32_T)mSecFrmBufInfo[u4I].u4BuffHdr == (VAL_UINT32_T)pBuffHdr) && (((VAL_UINT32_T)0xffffffff) != (VAL_UINT32_T)mSecFrmBufInfo[u4I].pNativeHandle))
        {
            buffer_handle_t _handle = (buffer_handle_t)mSecFrmBufInfo[u4I].pNativeHandle;
            MTK_OMX_LOGD("@@ HandleGrallocExtra(secure) -  (0x%08X)", _handle);
            GrallocExtraSetBufParameter(_handle,
                                        GRALLOC_EXTRA_MASK_TYPE | GRALLOC_EXTRA_MASK_DIRTY,
                                        GRALLOC_EXTRA_BIT_TYPE_VIDEO | GRALLOC_EXTRA_BIT_DIRTY,
                                        pBuffHdr->nTimeStamp, VAL_FALSE, VAL_FALSE);
            __setBufParameter(_handle, GRALLOC_EXTRA_MASK_CM, GRALLOC_EXTRA_BIT_CM_NV12_BLK); // Use MTK BLK format
            //__setBufParameter(_handle, GRALLOC_EXTRA_MASK_TYPE | GRALLOC_EXTRA_MASK_DIRTY, GRALLOC_EXTRA_BIT_TYPE_VIDEO | GRALLOC_EXTRA_BIT_DIRTY);
            break;
        }
    }
}

if (OMX_TRUE == mStoreMetaDataInBuffers && (NULL != pBuffHdr))
{
    OMX_U32 graphicBufHandle = 0;

    GetMetaHandleFromOmxHeader(pBuffHdr, &graphicBufHandle);


    VAL_UINT32_T gralloc_masks = GRALLOC_EXTRA_MASK_TYPE | GRALLOC_EXTRA_MASK_DIRTY;
    VAL_UINT32_T gralloc_bits = GRALLOC_EXTRA_BIT_TYPE_VIDEO;

    if ((pBuffHdr->nFlags & OMX_BUFFERFLAG_MJC_DUMMY_OUTPUT_BUFFER) != OMX_BUFFERFLAG_MJC_DUMMY_OUTPUT_BUFFER)
    {
        gralloc_bits |= GRALLOC_EXTRA_BIT_DIRTY;
    }

    if (mOutputPortDef.format.video.eColorFormat == OMX_COLOR_FormatVendorMTKYUV_FCM)
    {
        gralloc_masks |= GRALLOC_EXTRA_MASK_CM;
        gralloc_bits |=  GRALLOC_EXTRA_BIT_CM_NV12_BLK_FCM;
    }

    gralloc_masks |= GRALLOC_EXTRA_MASK_YUV_COLORSPACE;
    gralloc_bits |= GRALLOC_EXTRA_BIT_YUV_BT601_NARROW;

    if (meDecodeType == VDEC_DRV_DECODER_MTK_HARDWARE)
    {
        gralloc_masks |= GRALLOC_EXTRA_MASK_FLUSH;
        gralloc_bits |= GRALLOC_EXTRA_BIT_NOFLUSH;
    }
    if (m3DStereoMode == OMX_VIDEO_H264FPA_SIDEBYSIDE)
    {
        gralloc_masks |= GRALLOC_EXTRA_MASK_S3D;
        gralloc_bits |= GRALLOC_EXTRA_BIT_S3D_SBS;
    }
    else if (m3DStereoMode == OMX_VIDEO_H264FPA_TOPANDBOTTOM)
    {
        gralloc_masks |= GRALLOC_EXTRA_MASK_S3D;
        gralloc_bits |= GRALLOC_EXTRA_BIT_S3D_TAB;
    }
    else
    {
        gralloc_masks |= GRALLOC_EXTRA_MASK_S3D;
        gralloc_bits |= GRALLOC_EXTRA_BIT_S3D_2D;
    }

    VAL_BOOL_T bIsMJCOutputBuffer = OMX_FALSE;
    VAL_BOOL_T bIsScalerOutputBuffer = OMX_FALSE;
    GrallocExtraSetBufParameter((buffer_handle_t)graphicBufHandle, gralloc_masks, gralloc_bits, pBuffHdr->nTimeStamp, bIsMJCOutputBuffer, bIsScalerOutputBuffer);
    //__setBufParameter((buffer_handle_t)graphicBufHandle, GRALLOC_EXTRA_MASK_TYPE | GRALLOC_EXTRA_MASK_DIRTY, GRALLOC_EXTRA_BIT_TYPE_VIDEO | GRALLOC_EXTRA_BIT_DIRTY);
}

return OMX_TRUE;
}

void MtkOmxVdec::ReturnPendingInputBuffers()
{
    LOCK(mEmptyThisBufQLock);

#if CPP_STL_SUPPORT
    vector<int>::const_iterator iter;
    for (iter = mEmptyThisBufQ.begin(); iter != mEmptyThisBufQ.end(); iter++)
    {
        int input_idx = (*iter);
        if (mNumPendingInput > 0)
        {
            mNumPendingInput--;
        }
        else
        {
            MTK_OMX_LOGE("[ERROR] mNumPendingInput == 0 and want to --");
        }
        mCallback.EmptyBufferDone((OMX_HANDLETYPE)&mCompHandle,
                                  mAppData,
                                  mInputBufferHdrs[mEmptyThisBufQ[input_idx]]);
    }
    mEmptyThisBufQ.clear();
#endif

#if ANDROID
    for (size_t i = 0 ; i < mEmptyThisBufQ.size() ; i++)
    {
        if (mNumPendingInput > 0)
        {
            mNumPendingInput--;
        }
        else
        {
            MTK_OMX_LOGE("[ERROR] mNumPendingInput == 0 (%d)(0x%08X)", i, (unsigned int)mInputBufferHdrs[mEmptyThisBufQ[i]]);
        }
        mCallback.EmptyBufferDone((OMX_HANDLETYPE)&mCompHandle,
                                  mAppData,
                                  mInputBufferHdrs[mEmptyThisBufQ[i]]);
    }
    mEmptyThisBufQ.clear();
#endif

    UNLOCK(mEmptyThisBufQLock);
}


void MtkOmxVdec::ReturnPendingOutputBuffers()
{
    LOCK(mFillThisBufQLock);

#if CPP_STL_SUPPORT
    vector<int>::const_iterator iter;
    for (iter = mFillThisBufQ.begin(); iter != mFillThisBufQ.end(); iter++)
    {
        int output_idx = (*iter);
        mNumPendingOutput--;
        mCallback.FillBufferDone((OMX_HANDLETYPE)&mCompHandle,
                                 mAppData,
                                 mOutputBufferHdrs[mFillThisBufQ[output_idx]]);
    }
    mFillThisBufQ.clear();
#endif

#if ANDROID
    OMX_ERRORTYPE err = OMX_ErrorNone;

    for (size_t i = 0 ; i < mBufColorConvertDstQ.size() ; i++)
    {
        MTK_OMX_LOGD("%s@%d. return CCDQ(%d)", __FUNCTION__, __LINE__, mBufColorConvertDstQ.size());

#if (ANDROID_VER >= ANDROID_M)
        WaitFence(mOutputBufferHdrs[mBufColorConvertDstQ[i]], OMX_FALSE);
#endif
        mNumPendingOutput--;
        mOutputBufferHdrs[mBufColorConvertDstQ[i]]->nFilledLen = 0;
        mOutputBufferHdrs[mBufColorConvertDstQ[i]]->nTimeStamp = -1;

        mCallback.FillBufferDone((OMX_HANDLETYPE)&mCompHandle,
                                 mAppData,
                                 mOutputBufferHdrs[mBufColorConvertDstQ[i]]);

	    MTK_OMX_LOGD("FBD of DstQ (0x%08X) (0x%08X) %lld (%u) GET_DISP i(%d), flags(0x%08x) mFlushInProcess %d",
	                 mOutputBufferHdrs[mBufColorConvertDstQ[i]],
	                 mOutputBufferHdrs[mBufColorConvertDstQ[i]]->pBuffer,
	                 mOutputBufferHdrs[mBufColorConvertDstQ[i]]->nTimeStamp,
	                 mOutputBufferHdrs[mBufColorConvertDstQ[i]]->nFilledLen,
	                 mBufColorConvertDstQ[i],
	                 mOutputBufferHdrs[mBufColorConvertDstQ[i]]->nFlags,
	                 mFlushInProcess);


    }
    mBufColorConvertDstQ.clear();
    for (size_t i = 0 ; i < mBufColorConvertSrcQ.size() ; i++)
    {
        MTK_OMX_LOGD("%s@%d. return CCSQ(%d)", __FUNCTION__, __LINE__, mBufColorConvertSrcQ.size());
#if (ANDROID_VER >= ANDROID_M)
        WaitFence(mOutputBufferHdrs[mBufColorConvertSrcQ[i]], OMX_FALSE);
#endif
        mNumPendingOutput--;
        mOutputBufferHdrs[mBufColorConvertSrcQ[i]]->nFilledLen = 0;
        mOutputBufferHdrs[mBufColorConvertSrcQ[i]]->nTimeStamp = -1;

        mCallback.FillBufferDone((OMX_HANDLETYPE)&mCompHandle,
                                 mAppData,
                                 mOutputBufferHdrs[mBufColorConvertSrcQ[i]]);

	    MTK_OMX_LOGD("FBD of SrcQ (0x%08X) (0x%08X) %lld (%u) GET_DISP i(%d), flags(0x%08x) mFlushInProcess %d",
	                 mOutputBufferHdrs[mBufColorConvertSrcQ[i]],
	                 mOutputBufferHdrs[mBufColorConvertSrcQ[i]]->pBuffer,
	                 mOutputBufferHdrs[mBufColorConvertSrcQ[i]]->nTimeStamp,
	                 mOutputBufferHdrs[mBufColorConvertSrcQ[i]]->nFilledLen,
	                 mBufColorConvertSrcQ[i],
	                 mOutputBufferHdrs[mBufColorConvertSrcQ[i]]->nFlags,
	                 mFlushInProcess);

    }
    mBufColorConvertSrcQ.clear();

    for (size_t i = 0 ; i < mFillThisBufQ.size() ; i++)
    {
#if (ANDROID_VER >= ANDROID_M)
        WaitFence(mOutputBufferHdrs[mFillThisBufQ[i]], OMX_FALSE);
#endif
        mNumPendingOutput--;
        mOutputBufferHdrs[mFillThisBufQ[i]]->nFilledLen = 0;
        mOutputBufferHdrs[mFillThisBufQ[i]]->nTimeStamp = -1;

        MTK_OMX_LOGD("FBD of FTBQ (0x%08X) (0x%08X) %lld (%u) GET_DISP i(%d), flags(0x%08x) mFlushInProcess %d",
	                 mOutputBufferHdrs[mFillThisBufQ[i]],
	                 mOutputBufferHdrs[mFillThisBufQ[i]]->pBuffer,
	                 mOutputBufferHdrs[mFillThisBufQ[i]]->nTimeStamp,
	                 mOutputBufferHdrs[mFillThisBufQ[i]]->nFilledLen,
	                 mFillThisBufQ[i],
	                 mOutputBufferHdrs[mFillThisBufQ[i]]->nFlags,
	                 mFlushInProcess);

        mCallback.FillBufferDone((OMX_HANDLETYPE)&mCompHandle,
                                 mAppData,
                                 mOutputBufferHdrs[mFillThisBufQ[i]]);

    }
    mFillThisBufQ.clear();
#endif

    UNLOCK(mFillThisBufQLock);
}


void MtkOmxVdec::DumpETBQ()
{
    MTK_OMX_LOGD("--- ETBQ: mNumPendingInput %d; mEmptyThisBufQ.size() %d", (int)mNumPendingInput, mEmptyThisBufQ.size());
#if CPP_STL_SUPPORT
    vector<int>::const_iterator iter;
    for (iter = mEmptyThisBufQ.begin(); iter != mEmptyThisBufQ.end(); iter++)
    {
        MTK_OMX_LOGD("[%d] - pBuffHead(0x%08X)", *iter, (unsigned int)mInputBufferHdrs[mEmptyThisBufQ[i]]);
    }
#endif


#if ANDROID
    for (size_t i = 0 ; i < mEmptyThisBufQ.size() ; i++)
    {
        MTK_OMX_LOGD("[%d] - pBuffHead(0x%08X)", mEmptyThisBufQ[i], (unsigned int)mInputBufferHdrs[mEmptyThisBufQ[i]]);
    }
#endif
}




void MtkOmxVdec::DumpFTBQ()
{
    MTK_OMX_LOGD("--- FTBQ: mNumPendingOutput %d; mFillThisBufQ.size() %d,  CCDst: %d, CCSrc: %d",
                 (int)mNumPendingOutput, mFillThisBufQ.size(), mBufColorConvertDstQ.size(),
                 mBufColorConvertSrcQ.size());

    LOCK(mFillThisBufQLock);
#if CPP_STL_SUPPORT
    vector<int>::const_iterator iter;
    for (iter = mFillThisBufQ.begin(); iter != mFillThisBufQ.end(); iter++)
    {
        MTK_OMX_LOGD("[%d] - pBuffHead(0x%08X)", *iter, (unsigned int)mOutputBufferHdrs[mFillThisBufQ[i]]);
    }
#endif

#if ANDROID
    for (size_t i = 0 ; i < mFillThisBufQ.size() ; i++)
    {
        MTK_OMX_LOGD("[%d] - pBuffHead(0x%08X)", mFillThisBufQ[i], (unsigned int)mOutputBufferHdrs[mFillThisBufQ[i]]);
    }

    for (size_t i = 0 ; i < mBufColorConvertDstQ.size() ; i++)
    {
        MTK_OMX_LOGD("[%d] - pCCDstBuffHead(0x%08X)", mBufColorConvertDstQ[i], (unsigned int)mOutputBufferHdrs[mBufColorConvertDstQ[i]]);
    }
    for (size_t i = 0 ; i < mBufColorConvertSrcQ.size() ; i++)
    {
        MTK_OMX_LOGD("[%d] - pCCSrcBuffHead(0x%08X)", mBufColorConvertSrcQ[i], (unsigned int)mOutputBufferHdrs[mBufColorConvertSrcQ[i]]);
    }

#endif
    UNLOCK(mFillThisBufQLock);
}



int MtkOmxVdec::findBufferHeaderIndex(OMX_U32 nPortIndex, OMX_BUFFERHEADERTYPE *pBuffHdr)
{
    OMX_BUFFERHEADERTYPE **pBufHdrPool = NULL;
    int bufCount;

    if (nPortIndex == MTK_OMX_INPUT_PORT)
    {
        pBufHdrPool = mInputBufferHdrs;
        bufCount = mInputPortDef.nBufferCountActual;
    }
    else if (nPortIndex == MTK_OMX_OUTPUT_PORT)
    {
        pBufHdrPool = mOutputBufferHdrs;
        bufCount = mOutputPortDef.nBufferCountActual;
    }
    else
    {
        MTK_OMX_LOGE("[ERROR] findBufferHeaderIndex invalid index(0x%X)", (unsigned int)nPortIndex);
        return -1;
    }

    for (int i = 0 ; i < bufCount ; i++)
    {
        if (pBuffHdr == pBufHdrPool[i])
        {
            // index found
            return i;
        }
    }

    MTK_OMX_LOGE("[ERROR] findBufferHeaderIndex not found. Port = %u, BufferHeader = 0x%x", nPortIndex, pBuffHdr);
    return -1; // nothing found
}

OMX_ERRORTYPE MtkOmxVdec::QureyVideoProfileLevel(VAL_UINT32_T                       u4VideoFormat,
                                                 OMX_VIDEO_PARAM_PROFILELEVELTYPE   *pProfileLevel,
                                                 MTK_VDEC_PROFILE_MAP_ENTRY         *pProfileMapTable,
                                                 VAL_UINT32_T                       nProfileMapTableSize,
                                                 MTK_VDEC_LEVEL_MAP_ENTRY           *pLevelMapTable,
                                                 VAL_UINT32_T                       nLevelMapTableSize)
{
    VAL_UINT32_T nProfileLevelMapTableSize = nProfileMapTableSize * nLevelMapTableSize;

    if (pProfileLevel->nProfileIndex >= nProfileLevelMapTableSize)
    {
        return OMX_ErrorNoMore;
    }
    else
    {
        VDEC_DRV_QUERY_VIDEO_FORMAT_T qInfo;
        VDEC_DRV_MRESULT_T nDrvRet;
        VAL_UINT32_T nProfileMapIndex;
        VAL_UINT32_T nLevelMapIndex;

        // Loop until the supported profile-level found, or reach the end of table
        while (pProfileLevel->nProfileIndex < nProfileLevelMapTableSize)
        {

            nProfileMapIndex = pProfileLevel->nProfileIndex / nLevelMapTableSize;
            nLevelMapIndex = pProfileLevel->nProfileIndex % nLevelMapTableSize;
            memset(&qInfo, 0, sizeof(VDEC_DRV_QUERY_VIDEO_FORMAT_T));

            // Query driver to see if supported
            qInfo.u4VideoFormat = u4VideoFormat;
            qInfo.u4Profile = pProfileMapTable[nProfileMapIndex].u4Profile;
            qInfo.u4Level = pLevelMapTable[nLevelMapIndex].u4Level;
            //nDrvRet = eVDecDrvQueryCapability(VDEC_DRV_QUERY_TYPE_VIDEO_FORMAT, &qInfo, 0);

            // v4l2 todo: correct this part. It's a workaround now.
            // query driver property
            mMtkV4L2Device.checkVideoFormat(&qInfo, NULL);
            nDrvRet = VDEC_DRV_MRESULT_OK;
            // v4l2 todo: correct this part. It's a workaround now.


            if (VDEC_DRV_MRESULT_OK == nDrvRet)
            {
                // If supported, return immediately
                pProfileLevel->eProfile = pProfileMapTable[nProfileMapIndex].profile;
                pProfileLevel->eLevel = pLevelMapTable[nLevelMapIndex].level;
                MTK_OMX_LOGD("Supported nProfileIndex %d, eProfile 0x%x, eLevel 0x%x",
                             pProfileLevel->nProfileIndex,
                             pProfileLevel->eProfile,
                             pProfileLevel->eLevel);
                return OMX_ErrorNone;
            }
            else if (pProfileLevel->nProfileIndex + 1 >= nProfileLevelMapTableSize)
            {
                // Reach the end of table ?
                return OMX_ErrorNoMore;
            }

            // If not supported, continue checking the rest of table ...
            pProfileLevel->nProfileIndex++;
        }
    }

    return OMX_ErrorNoMore;
}


OMX_BOOL MtkOmxVdec::IsInETBQ(OMX_BUFFERHEADERTYPE *ipInputBuf)
{

    LOCK(mEmptyThisBufQLock);
#if CPP_STL_SUPPORT
    vector<int>::const_iterator iter;
    for (iter = mEmptyThisBufQ.begin(); iter != mEmptyThisBufQ.end(); iter++)
    {
        MTK_OMX_LOGD("[%d] - pBuffHead(0x%08X)", *iter, (unsigned int)mInputBufferHdrs[mEmptyThisBufQ[i]]);
    }
#endif


#if ANDROID
    for (size_t i = 0 ; i < mEmptyThisBufQ.size() ; i++)
    {
        if (ipInputBuf == mInputBufferHdrs[mEmptyThisBufQ[i]])
        {
            //MTK_OMX_LOGD ("[%d] - pBuffHead(0x%08X)", mEmptyThisBufQ[i], (unsigned int)mInputBufferHdrs[mEmptyThisBufQ[i]]);
            UNLOCK(mEmptyThisBufQLock);
            return OMX_TRUE;
        }
    }
#endif
    UNLOCK(mEmptyThisBufQLock);
    return OMX_FALSE;
}

OMX_BOOL MtkOmxVdec::IsFreeBuffer(OMX_BUFFERHEADERTYPE *ipOutputBuffer)
{
    OMX_U32 i;

#if (ANDROID_VER >= ANDROID_KK)
    if (OMX_TRUE == mStoreMetaDataInBuffers && (NULL != ipOutputBuffer))
    {
        OMX_BOOL bHeaderExists = OMX_FALSE;
        OMX_BOOL bBufferExists = OMX_FALSE;
        for (i = 0; i < mOutputPortDef.nBufferCountActual; i++)
        {
            if (mFrameBuf[i].ipOutputBuffer == ipOutputBuffer)
            {
                bHeaderExists = OMX_TRUE;
                if (mFrameBuf[i].ionBufHandle > 0)
                {
                    bBufferExists = OMX_TRUE;
                }
            }
            if (mFrameBuf[i].ionBufHandle > 0 && mFrameBuf[i].ipOutputBuffer == ipOutputBuffer )
            {
                MTK_OMX_LOGE("i(%d), mFrameBuf[i].ionBufHandle(0x%08x), mFrameBuf[i].ipOutputBuffer(0x%08x), ipOutputBuffer(0x%08x)", i, mFrameBuf[i].ionBufHandle, mFrameBuf[i].ipOutputBuffer, ipOutputBuffer);
                return OMX_TRUE;
            }
        }
        MTK_OMX_LOGE("[ERROR] IsFreeBuffer Hdr = 0x%x, hdr/buf = %d/%d", ipOutputBuffer, bHeaderExists, bBufferExists);
        return OMX_FALSE;
    }

#endif

    #if 0 // v4l2 todo: need to check if we still need 'bUsed'
    if (NULL != ipOutputBuffer)
    {
        for (i = 0; i < mOutputPortDef.nBufferCountActual; i++)
        {
            if (OMX_TRUE == mFrameBuf[i].bUsed)
            {
                if (ipOutputBuffer == (OMX_BUFFERHEADERTYPE *)mFrameBuf[i].ipOutputBuffer)
                {
                    return OMX_FALSE;
                }
            }
        }
    }
    #endif

    return OMX_TRUE;
}

void MtkOmxVdec::InsertInputBuf(OMX_BUFFERHEADERTYPE *ipInputBuf)
{
    OMX_U32 i = 0;
    for (i = 0; i < mInputPortDef.nBufferCountActual; i++)
    {
        if (mInputBuf[i].ipInputBuffer == NULL)
        {
            mInputBuf[i].ipInputBuffer = ipInputBuf;
            //MTK_OMX_LOGD("InsertInputBuf() (%d 0x%08x)", i, mInputBuf[i].ipInputBuffer);
            break;
        }
    }
}

void MtkOmxVdec::RemoveInputBuf(OMX_BUFFERHEADERTYPE *ipInputBuf)
{
    OMX_U32 i = 0;
    for (i = 0; i < mInputPortDef.nBufferCountActual; i++)
    {
        if (mInputBuf[i].ipInputBuffer == ipInputBuf)
        {
            mInputBuf[i].ipInputBuffer = NULL;
            MTK_OMX_LOGD("RemoveInputBuf frm=0x%x, omx=0x%x, i=%d", &mInputBuf[i].InputBuf, ipInputBuf, i);
            return;
        }
    }
    MTK_OMX_LOGE("Error!! RemoveInputBuf not found");
}

#if (ANDROID_VER >= ANDROID_KK)
OMX_BOOL MtkOmxVdec::GetMetaHandleFromOmxHeader(OMX_BUFFERHEADERTYPE *pBufHdr, OMX_U32 *pBufferHandle)
{
    OMX_U32 bufferType = *((OMX_U32 *)pBufHdr->pBuffer);
    // check buffer type
    if (kMetadataBufferTypeGrallocSource == bufferType)
    {
        //buffer_handle_t _handle = *((buffer_handle_t*)(pBufHdr->pBuffer + 4));
        *pBufferHandle = *((OMX_U32 *)(pBufHdr->pBuffer + 4));
        MTK_OMX_LOGE("GetMetaHandleFromOmxHeader 0x%x", *pBufferHandle);
    }
#if (ANDROID_VER >= ANDROID_M)
    else if (kMetadataBufferTypeANWBuffer == bufferType)
    {
        ANativeWindowBuffer *pNWBuffer = *((ANativeWindowBuffer **)(pBufHdr->pBuffer + 4));
        *pBufferHandle = (OMX_U32)pNWBuffer->handle;
    }
#endif
    else
    {
        MTK_OMX_LOGE("Warning: BufferType is not Gralloc Source !!!! LINE: %d", __LINE__);
        return OMX_FALSE;
    }
    return OMX_TRUE;
}
OMX_BOOL MtkOmxVdec::GetMetaHandleFromBufferPtr(OMX_U8 *pBuffer, OMX_U32 *pBufferHandle)
{
    OMX_U32 bufferType = *((OMX_U32 *)pBuffer);
    // check buffer type
    if (kMetadataBufferTypeGrallocSource == bufferType)
    {
        //buffer_handle_t _handle = *((buffer_handle_t*)(pBufHdr->pBuffer + 4));
        *pBufferHandle = *((OMX_U32 *)(pBuffer + 4));
    }
#if (ANDROID_VER >= ANDROID_M)
    else if (kMetadataBufferTypeANWBuffer == bufferType)
    {
        ANativeWindowBuffer *pNWBuffer = *((ANativeWindowBuffer **)(pBuffer + 4));
        *pBufferHandle = (OMX_U32)pNWBuffer->handle;
        //OMX_U32 pNWBuffer = *((OMX_U32 *)(pBuffer + 4));
        //*pBufferHandle = (ANativeWindowBuffer*)pNWBuffer->handle;
    }
#endif
    else
    {
        MTK_OMX_LOGD("Warning: BufferType is not Gralloc Source !!!! LINE: %d", __LINE__);
        return OMX_FALSE;
    }

    return OMX_TRUE;
}
#endif

OMX_BOOL MtkOmxVdec::GetIonHandleFromGraphicHandle(OMX_U32 *pBufferHandle, int *pIonHandle)
{
    int ionFd = -1;
    ion_user_handle_t ionHandle;
    gralloc_extra_query((buffer_handle_t)*pBufferHandle, GRALLOC_EXTRA_GET_ION_FD, &ionFd);
    if (-1 == mIonDevFd)
    {
        mIonDevFd = mt_ion_open("MtkOmxVdec1");
        if (mIonDevFd < 0)
        {
            MTK_OMX_LOGE("[ERROR] cannot open ION device. LINE: %d", __LINE__);
            return OMX_FALSE;
        }
    }

    if (ionFd > 0)
    {
        if (ion_import(mIonDevFd, ionFd, &ionHandle))
        {
            MTK_OMX_LOGE("[ERROR] ion_import failed, LINE: %d", __LINE__);
            return OMX_FALSE;
        }
    }
    else
    {
        MTK_OMX_LOGE("[ERROR] query ion fd failed(%d), LINE: %d", ionFd, __LINE__);
        return OMX_FALSE;
    }

    *pIonHandle = ionHandle;
    return OMX_TRUE;
}

OMX_BOOL MtkOmxVdec::FreeIonHandle(int ionHandle)
{
    if (-1 == mIonDevFd)
    {
        mIonDevFd = mt_ion_open("MtkOmxVdec1");
        if (mIonDevFd < 0)
        {
            MTK_OMX_LOGE("[ERROR] cannot open ION device. LINE: %d", __LINE__);
            return OMX_FALSE;
        }
    }

    if (ion_free(mIonDevFd, ionHandle))
    {
        MTK_OMX_LOGE("[ERROR] cannot free ion handle(%d). LINE: %d", ionHandle, __LINE__);
        return OMX_FALSE;
    }
    return OMX_TRUE;
}

void MtkOmxVdec::InsertFrmBuf(OMX_BUFFERHEADERTYPE *ipOutputBuffer)
{
    //MTK_OMX_LOGE(" output buffer count %d", mOutputPortDef.nBufferCountActual);
    for (OMX_U32 i = 0; i < mOutputPortDef.nBufferCountActual; i++)
    {
        if (mFrameBuf[i].ipOutputBuffer == NULL)
        {
            mFrameBuf[i].ipOutputBuffer = ipOutputBuffer;
            MTK_OMX_LOGD("InsertFrmBuf , omx=0x%X, i=%d", ipOutputBuffer, i);
            break;
        }
    }
}

void MtkOmxVdec::RemoveFrmBuf(OMX_BUFFERHEADERTYPE *ipOutputBuffer)
{
    for (OMX_U32 i = 0; i < mOutputBufferHdrsCnt; i++)
    {
        if (mFrameBuf[i].ipOutputBuffer == ipOutputBuffer)
        {
            mFrameBuf[i].ipOutputBuffer = NULL;
            MTK_OMX_LOGD("RemoveFrmBuf frm=0x%x, omx=0x%x, i=%d", &mFrameBuf[i].frame_buffer, ipOutputBuffer, i);
            return;
        }
    }
    MTK_OMX_LOGE("Error!! RemoveFrmBuf not found");
}

VDEC_DRV_VIDEO_FORMAT_T MtkOmxVdec::GetVdecFormat(MTK_VDEC_CODEC_ID codecId)
{
    switch (codecId)
    {

        case MTK_VDEC_CODEC_ID_HEVC:
            return VDEC_DRV_VIDEO_FORMAT_H265;
        case MTK_VDEC_CODEC_ID_H263:
            return VDEC_DRV_VIDEO_FORMAT_H263;
        case MTK_VDEC_CODEC_ID_MPEG4:
            return VDEC_DRV_VIDEO_FORMAT_MPEG4;

        case MTK_VDEC_CODEC_ID_DIVX:
            return VDEC_DRV_VIDEO_FORMAT_DIVX4;

        case MTK_VDEC_CODEC_ID_DIVX3:
            return VDEC_DRV_VIDEO_FORMAT_DIVX311;

        case MTK_VDEC_CODEC_ID_XVID:
            return VDEC_DRV_VIDEO_FORMAT_XVID;

        case MTK_VDEC_CODEC_ID_S263:
            return VDEC_DRV_VIDEO_FORMAT_S263;

        case MTK_VDEC_CODEC_ID_AVC:
            if (OMX_TRUE == mIsSecureInst)
            {
                return VDEC_DRV_VIDEO_FORMAT_H264SEC;
            }
            else
            {
                return VDEC_DRV_VIDEO_FORMAT_H264;
            }
        case MTK_VDEC_CODEC_ID_RV:
            return VDEC_DRV_VIDEO_FORMAT_REALVIDEO9;

        case MTK_VDEC_CODEC_ID_VC1:
            return VDEC_DRV_VIDEO_FORMAT_VC1;

        case MTK_VDEC_CODEC_ID_VPX:
            return VDEC_DRV_VIDEO_FORMAT_VP8;
        case MTK_VDEC_CODEC_ID_VP9:
            return VDEC_DRV_VIDEO_FORMAT_VP9;
        case MTK_VDEC_CODEC_ID_MPEG2:
            return VDEC_DRV_VIDEO_FORMAT_MPEG2;
        case MTK_VDEC_CODEC_ID_MJPEG:
            return VDEC_DRV_VIDEO_FORMAT_MJPEG;
        default:
            return VDEC_DRV_VIDEO_FORMAT_UNKNOWN_VIDEO_FORMAT;
    }
}

OMX_BOOL MtkOmxVdec::ConvertFrameToYV12(FrmBufStruct *pFrameBuf, FrmBufStruct *pFrameBufOut, OMX_BOOL bGetResolution)
{
    DpBlitStream blitStream;
    OMX_COLOR_FORMATTYPE mTempSrcColorFormat = mOutputPortFormat.eColorFormat;
    OMX_VIDEO_CODINGTYPE mTempSrcCompressionFormat = mInputPortFormat.eCompressionFormat;
    DP_PROFILE_ENUM srcColourPrimaries = DP_PROFILE_BT601;
    unsigned int srcWStride;
    unsigned int srcHStride;
    unsigned int CSrcSize = 0;
    unsigned int CDstSize = 0;

    VDEC_DRV_QUERY_VIDEO_FORMAT_T qinfoOut;

    MTK_OMX_MEMSET(&qinfoOut, 0, sizeof(VDEC_DRV_QUERY_VIDEO_FORMAT_T));
    QueryDriverFormat(&qinfoOut);

    if (0 != mMtkV4L2Device.getCapFmt(&srcWStride, &srcHStride))
    {
        MTK_OMX_LOGE("[ERROR] Cannot get capture format");
        return OMX_FALSE;
    }

    unsigned int dstWStride = mCropWidth;
    unsigned int dstHStride = mCropHeight;

    MTK_OMX_LOGD("ConvertFrameToYV12 : mTempSrcColorFormat %x, mTempSrcCompressionFormat %x,, srcWStride %d, srcHStride %d, dstWStride %d, dstHStride %d",
       mTempSrcColorFormat, mTempSrcCompressionFormat, srcWStride, srcHStride, dstWStride, dstHStride);
    if (OMX_TRUE == bGetResolution || mFixedMaxBuffer == OMX_TRUE)
    {
        srcWStride = VDEC_ROUND_16(pFrameBuf->ipOutputBuffer->nWidth);
        srcHStride = VDEC_ROUND_32(pFrameBuf->ipOutputBuffer->nHeight);
        dstWStride = srcWStride;
        dstHStride = (pFrameBuf->ipOutputBuffer->nHeight);

        MTK_OMX_LOGD("***mFixedMaxBuffer***(%d), srcWStride(%d), srcHStride(%d), dstWStride(%d), dstHStride(%d)", __LINE__, srcWStride, srcHStride, dstWStride, dstHStride);
    }

    /* output of non-16 alignment resolution in new interace
        UV != (Y/2) size will cause MDP abnormal in previously.
        1. h264_hybrid_dec_init_ex()
        //Assume YV12 format
        //for new interface integration
        pH264Handle->codecOpenSetting.stride.u4YStride  = VDEC_ROUND_16(pH264Handle->rVideoDecYUVBufferParameter.u4Width);
        pH264Handle->codecOpenSetting.stride.u4UVStride = VDEC_ROUND_16(pH264Handle->codecOpenSetting.stride.u4YStride / 2) ;


        2. h264_hybrid_dec_decode()
        Height = VDEC_ROUND_16(pH264Handle->rVideoDecYUVBufferParameter.u4Height);
        YSize = pH264Handle->codecOpenSetting.stride.u4YStride * Height;
        CSize = pH264Handle->codecOpenSetting.stride.u4UVStride * (Height >> 1);

        example:
        144x136
        Y    size is 144x144
        UV  size is (160/2)x(144/2)
       */
    if ((OMX_MTK_COLOR_FormatYV12 == mTempSrcColorFormat))
    {
        CSrcSize = VDEC_ROUND_16(srcWStride / 2) * (srcHStride / 2);
        MTK_OMX_LOGD("SrcColorFormat = OMX_MTK_COLOR_FormatYV12, CSrcSize %d", CSrcSize);
    }
    else
    {
        CSrcSize = 0;
    }

    // Source MTKYUV
    DpRect srcRoi;
    srcRoi.x = mCropLeft;
    srcRoi.y = mCropTop;
    srcRoi.w = mCropWidth;
    srcRoi.h = mCropHeight;
    if (OMX_TRUE == bGetResolution || mFixedMaxBuffer == OMX_TRUE)
    {
        srcRoi.w = pFrameBuf->ipOutputBuffer->nWidth;
        srcRoi.h = pFrameBuf->ipOutputBuffer->nHeight;
        MTK_OMX_LOGD("***mFixedMaxBuffer***(%d), srcRoi.w (%d), ", __LINE__, srcRoi.w, srcRoi.h);
    }

    char *srcPlanar[3];
    char *srcMVAPlanar[3];
    unsigned int srcLength[3];
    srcPlanar[0] = (char *)pFrameBuf->frame_buffer.rBaseAddr.u4VA;
    srcLength[0] = srcWStride * srcHStride;

    switch (pFrameBuf->frame_buffer.rColorPriInfo.eColourPrimaries)
    {
        case COLOR_PRIMARIES_BT601:
            if (pFrameBuf->frame_buffer.rColorPriInfo.u4VideoRange) {
                srcColourPrimaries = DP_PROFILE_FULL_BT601;
            } else {
                srcColourPrimaries = DP_PROFILE_BT601;
            }
            break;
        case COLOR_PRIMARIES_BT709:
            if (pFrameBuf->frame_buffer.rColorPriInfo.u4VideoRange) {
                srcColourPrimaries = DP_PROFILE_FULL_BT709;
            } else {
                srcColourPrimaries = DP_PROFILE_BT709;
            }
            break;
        case COLOR_PRIMARIES_BT2020:
            if (pFrameBuf->frame_buffer.rColorPriInfo.u4VideoRange) {
                srcColourPrimaries = DP_PROFILE_FULL_BT2020;
            } else {
                srcColourPrimaries = DP_PROFILE_BT2020;
            }
            break;
        default:
            srcColourPrimaries = DP_PROFILE_BT601;
            break;
    }

    MTK_OMX_LOGD("srcColourPrimaries =  %d", srcColourPrimaries);

    if ((OMX_COLOR_FormatYUV420Planar == mTempSrcColorFormat) || (OMX_MTK_COLOR_FormatYV12 == mTempSrcColorFormat))
    {
        if (OMX_TRUE == mOutputUseION)
        {
            srcPlanar[1] = srcPlanar[0] + srcLength[0];
            srcLength[1] = (CSrcSize == 0) ? srcWStride * srcHStride / 4 : CSrcSize;
            srcPlanar[2] = srcPlanar[1] + srcLength[1];
            srcLength[2] = (CSrcSize == 0) ? srcWStride * srcHStride / 4 : CSrcSize;

            srcMVAPlanar[0] = (char *)pFrameBuf->frame_buffer.rBaseAddr.u4PA;
            srcMVAPlanar[1] = srcMVAPlanar[0] + srcLength[0];
            srcMVAPlanar[2] = srcMVAPlanar[1] + srcLength[1];

            blitStream.setSrcBuffer((void **)srcPlanar, (void **)srcMVAPlanar, (unsigned int *)srcLength, 3);
            MTK_OMX_LOGD("ConvertFrameToYV12: mOutputUseION, src=%x, srcPlanar %x, srcMVAPlanar %x,srcWStride %d, srcHStride %d, srcLength %d",
                          mTempSrcColorFormat, srcPlanar, srcMVAPlanar, srcWStride, srcHStride, srcLength);
        }
        else
        {
            srcPlanar[1] = srcPlanar[0] + srcLength[0];
            srcLength[1] = (CSrcSize == 0) ? srcWStride * srcHStride / 4 : CSrcSize;

            srcPlanar[2] = srcPlanar[1] + srcLength[1];
            srcLength[2] = (CSrcSize == 0) ? srcWStride * srcHStride / 4 : CSrcSize;

            blitStream.setSrcBuffer((void **)srcPlanar, (unsigned int *)srcLength, 3);
            MTK_OMX_LOGD("ConvertFrameToYV12: output not ION, src=%x, srcPlanar %x, srcWStride %d, srcHStride %d, srcLength %d",
                          mTempSrcColorFormat, srcPlanar, srcWStride, srcHStride, srcLength);
        }

        if (OMX_COLOR_FormatYUV420Planar == mTempSrcColorFormat)
        {
            blitStream.setSrcConfig(srcWStride, srcHStride, srcWStride, (CSrcSize == 0) ? (srcWStride / 2) : (VDEC_ROUND_16(srcWStride / 2)), eI420, srcColourPrimaries, eInterlace_None, &srcRoi);
        }
        else
        {
            blitStream.setSrcConfig(srcWStride, srcHStride, srcWStride, (CSrcSize == 0) ? (srcWStride / 2) : (VDEC_ROUND_16(srcWStride / 2)), eYV12, srcColourPrimaries, eInterlace_None, &srcRoi);
        }
    }
    else if (OMX_COLOR_FormatYUV420SemiPlanar == mTempSrcColorFormat)  // for NV12
    {
        if (OMX_TRUE == mOutputUseION)
        {
            srcPlanar[1] = srcPlanar[0] + srcLength[0];
            srcLength[1] = srcWStride * (srcHStride / 2);

            srcMVAPlanar[0] = (char *)pFrameBuf->frame_buffer.rBaseAddr.u4PA;
            srcMVAPlanar[1] = srcMVAPlanar[0] + srcLength[0];

            blitStream.setSrcBuffer((void **)srcPlanar, (void **)srcMVAPlanar, (unsigned int *)srcLength, 2);

            MTK_OMX_LOGD("ConvertFrameToYV12: mOutputUseION, src=OMX_COLOR_FormatYUV420SemiPlanar, srcPlanar %x, srcMVAPlanar %x,srcWStride %d, srcHStride %d, srcLength %d",
                           srcPlanar, srcMVAPlanar, srcWStride, srcHStride, srcLength);
        }
        else
        {
            srcPlanar[1] = srcPlanar[0] + srcLength[0];
            srcLength[1] = srcWStride * (srcHStride / 2);

            blitStream.setSrcBuffer((void **)srcPlanar, (unsigned int *)srcLength, 2);
            MTK_OMX_LOGD("ConvertFrameToYV12: output not ION, src=OMX_COLOR_FormatYUV420SemiPlanar, srcPlanar %x, srcWStride %d, srcHStride %d, srcLength %d",
               srcPlanar, srcWStride, srcHStride, srcLength);
        }

        blitStream.setSrcConfig(srcWStride, srcHStride, srcWStride, srcWStride, eNV12, srcColourPrimaries, eInterlace_None, &srcRoi);
    }
    else if (OMX_COLOR_FormatVendorMTKYUV_FCM == mTempSrcColorFormat)
    {
        if (OMX_TRUE == mOutputUseION)
        {
            srcPlanar[1] = srcPlanar[0] + srcLength[0];
            srcLength[1] = srcWStride * srcHStride / 2;

            srcMVAPlanar[0] = (char *)pFrameBuf->frame_buffer.rBaseAddr.u4PA;
            srcMVAPlanar[1] = srcMVAPlanar[0] + srcLength[0];

            blitStream.setSrcBuffer((void **)srcPlanar, (void **)srcMVAPlanar, (unsigned int *)srcLength, 2);

            MTK_OMX_LOGD("ConvertFrameToYV12: mOutputUseION, src=OMX_COLOR_FormatVendorMTKYUV_FCM, srcPlanar %x, srcMVAPlanar %x,srcWStride %d, srcHStride %d, srcLength %d",
                           srcPlanar, srcMVAPlanar, srcWStride, srcHStride, srcLength);
        }
        else
        {
            srcPlanar[1] = srcPlanar[0] + srcLength[0];
            srcLength[1] = srcWStride * srcHStride / 2;
            blitStream.setSrcBuffer((void **)srcPlanar, (unsigned int *)srcLength, 2);
            MTK_OMX_LOGD("ConvertFrameToYV12: output not ION, src=OMX_COLOR_FormatVendorMTKYUV_FCM, srcPlanar %x, srcWStride %d, srcHStride %d, srcLength %d",
               srcPlanar, srcWStride, srcHStride, srcLength);
        }

        blitStream.setSrcConfig(srcWStride, srcHStride, srcWStride * 32, srcWStride * 16, eNV12_BLK_FCM, srcColourPrimaries, eInterlace_None, &srcRoi);
    }
    else
    {

        if (mbIs10Bit)
        {
            srcLength[0] *= 1.25;
        }

        if (OMX_TRUE == mOutputUseION)
        {
            srcPlanar[1] = srcPlanar[0] + VDEC_ROUND_N(srcLength[0], 512);
            srcLength[1] = srcWStride * srcHStride / 2;

            srcMVAPlanar[0] = (char *)pFrameBuf->frame_buffer.rBaseAddr.u4PA;
            srcMVAPlanar[1] = srcMVAPlanar[0] + VDEC_ROUND_N(srcLength[0], 512);

            blitStream.setSrcBuffer((void **)srcPlanar, (void **)srcMVAPlanar, (unsigned int *)srcLength, 2);

            MTK_OMX_LOGD("ConvertFrameToYV12: mOutputUseION, src=%x, srcPlanar %x, srcMVAPlanar %x,srcWStride %d, srcHStride %d, srcLength %d",
                          mTempSrcColorFormat, srcPlanar, srcMVAPlanar, srcWStride, srcHStride, srcLength);
        }
        else
        {
            srcPlanar[1] = srcPlanar[0] + VDEC_ROUND_N(srcLength[0], 512);
            srcLength[1] = srcWStride * srcHStride / 2;
            blitStream.setSrcBuffer((void **)srcPlanar, (unsigned int *)srcLength, 2);
            MTK_OMX_LOGD("ConvertFrameToYV12: output not ION, src=%x, srcPlanar %x, srcWStride %d, srcHStride %d, srcLength %d",
                          mTempSrcColorFormat, srcPlanar, srcWStride, srcHStride, srcLength);
        }

        if (mbIs10Bit)
        {
            srcLength[1] *= 1.25;
        }

        if (mbIs10Bit)
        {
            if (mIsHorizontalScaninLSB)
            {
                blitStream.setSrcConfig(srcWStride, srcHStride, srcWStride * 40, srcWStride * 20, DP_COLOR_420_BLKP_10_H, srcColourPrimaries, eInterlace_None, &srcRoi);
            }
            else
            {
                blitStream.setSrcConfig(srcWStride, srcHStride, srcWStride * 40, srcWStride * 20, DP_COLOR_420_BLKP_10_V, srcColourPrimaries, eInterlace_None, &srcRoi);
            }
        }
        else
        {
            blitStream.setSrcConfig(srcWStride, srcHStride, srcWStride * 32, srcWStride * 16, eNV12_BLK, srcColourPrimaries, eInterlace_None, &srcRoi);
        }
    }

    // Target Android YV12
    DpRect dstRoi;
    dstRoi.x = 0;
    dstRoi.y = 0;
    dstRoi.w = srcRoi.w;
    dstRoi.h = srcRoi.h;

    char *dstPlanar[3];
    char *dstMVAPlanar[3];
    unsigned int dstLength[3];

    /*reference from hardware\gpu_mali\mali_midgard\r5p0-eac\gralloc\src\Gralloc_module.cpp
            In gralloc_mtk_lock_ycbcr(), I420 case,
            case HAL_PIXEL_FORMAT_I420:
            int ysize = ycbcr->ystride * hnd->height;
            ycbcr->chroma_step = 1;
            ycbcr->cstride = GRALLOC_ALIGN(hnd->stride / 2, 16) * ycbcr->chroma_step;
            {
                int csize = ycbcr->cstride * hnd->height / 2;
                ycbcr->cb = (void *)((char *)ycbcr->y + ysize);
                ycbcr->cr = (void *)((char *)ycbcr->y + ysize + csize);
            }
        */
#if 0
    if ((OMX_TRUE == mStoreMetaDataInBuffers) && (OMX_TRUE == mbYUV420FlexibleMode))
    {
        CDstSize = VDEC_ROUND_16(dstWStride / 2) * (dstHStride / 2);
        MTK_OMX_LOGD("CDstSize %d", CDstSize);
    }
    else
    {
        CDstSize = 0;
    }
#endif

    if (OMX_TRUE == needColorConvertWithMetaMode())
    {
        VBufInfo BufInfo;
        if (mOutputMVAMgr->getOmxInfoFromHndl((void *)pFrameBuf->bGraphicBufHandle, &BufInfo) < 0)
        {
            MTK_OMX_LOGE("[ERROR][Convert] Can't find Frm in mOutputMVAMgr,LINE:%d", __LINE__);
            return OMX_FALSE;
        }
        else
        {
            dstPlanar[0] = (char *)pFrameBufOut->frame_buffer.rBaseAddr.u4VA;

            if (OMX_TRUE == mOutputUseION)
            {
                dstMVAPlanar[0] = (char *)pFrameBufOut->frame_buffer.rBaseAddr.u4PA;
            }
        }
    }
    else
    {
        dstPlanar[0] = (char *)pFrameBufOut->frame_buffer.rBaseAddr.u4VA;
        if (OMX_TRUE == mOutputUseION)
        {
            dstMVAPlanar[0] = (char *)pFrameBufOut->frame_buffer.rBaseAddr.u4PA;
        }
    }

    dstLength[0] = dstWStride * dstHStride;

#if 0
    if (OMX_TRUE == mOutputUseION)
    {
        dstPlanar[1] = dstPlanar[0] + dstLength[0];
        dstLength[1] = (CDstSize == 0) ? dstWStride * dstHStride / 4 : CDstSize;
        dstPlanar[2] = dstPlanar[1] + dstLength[1];
        dstLength[2] = (CDstSize == 0) ? dstWStride * dstHStride / 4 : CDstSize;

        dstMVAPlanar[1] = dstMVAPlanar[0] + dstLength[0];
        dstMVAPlanar[2] = dstMVAPlanar[1] + dstLength[1];
        blitStream.setDstBuffer((void **)dstPlanar, (void **)dstMVAPlanar, (unsigned int *)dstLength, 3);
    }
    else
    {
        dstPlanar[1] = dstPlanar[0] + dstLength[0];
        dstLength[1] = (CDstSize == 0) ? dstWStride * dstHStride / 4 : CDstSize;
        dstPlanar[2] = dstPlanar[1] + dstLength[1];
        dstLength[2] = (CDstSize == 0) ? dstWStride * dstHStride / 4 : CDstSize;

        blitStream.setDstBuffer((void **)dstPlanar, (unsigned int *)dstLength, 3);
    }
    //blitStream.setDstConfig(dstWStride, dstHStride, dstWStride, dstWStride / 2, eI420, DP_PROFILE_BT601, eInterlace_None, &dstRoi);
    blitStream.setDstConfig(dstWStride, dstHStride, dstWStride, (CDstSize == 0) ? (dstWStride / 2) : (VDEC_ROUND_16(dstWStride / 2)), eI420, DP_PROFILE_BT601, eInterlace_None, &dstRoi);
#else
    if (OMX_TRUE == mOutputUseION)
    {
        dstPlanar[1] = dstPlanar[0] + dstLength[0];
        dstLength[1] = dstWStride * dstHStride / 4;
        dstPlanar[2] = dstPlanar[1] + dstLength[1];
        dstLength[2] = dstWStride * dstHStride / 4;

        dstMVAPlanar[1] = dstMVAPlanar[0] + dstLength[0];
        dstMVAPlanar[2] = dstMVAPlanar[1] + dstLength[1];
        blitStream.setDstBuffer((void **)dstPlanar, (void **)dstMVAPlanar, (unsigned int *)dstLength, 3);
    }
    else
    {
        dstPlanar[1] = dstPlanar[0] + dstLength[0];
        dstLength[1] = dstWStride * dstHStride / 4;
        dstPlanar[2] = dstPlanar[1] + dstLength[1];
        dstLength[2] = dstWStride * dstHStride / 4;

        blitStream.setDstBuffer((void **)dstPlanar, (unsigned int *)dstLength, 3);
    }
    //blitStream.setDstConfig(dstWStride, dstHStride, dstWStride, dstWStride / 2, eI420, DP_PROFILE_BT601, eInterlace_None, &dstRoi);
    blitStream.setDstConfig(dstWStride, dstHStride, dstWStride, dstWStride / 2, eI420, srcColourPrimaries, eInterlace_None, &dstRoi);
#endif

#if PROFILING
    static int64_t _in_time_1 = 0;
    static int64_t _in_time_2 = 0;
    static int64_t _out_time = 0;
    _in_time_1 = getTickCountMs();
#endif

    // Blit
    MTK_OMX_LOGD("Internal blitStream+ Src Va=0x%x, Size=%d, Dst Va=0x%x, Size=%d, Px %x",
                 srcPlanar[0], srcLength[0] * 3 / 2,
                 dstPlanar[0], dstLength[0] * 3 / 2, (char *)pFrameBufOut->frame_buffer.rBaseAddr.u4PA);

    int iRet = blitStream.invalidate();

    MTK_OMX_LOGD("@Invalidate Cache After MDP va %p 0x%x", pFrameBufOut->frame_buffer.rBaseAddr.u4VA, pFrameBufOut->ionBufHandle);
    mOutputMVAMgr->syncBufferCacheFrm((void *)pFrameBufOut->frame_buffer.rBaseAddr.u4VA, (unsigned int)ION_CACHE_INVALID_BY_RANGE);

#if PROFILING
    _out_time = getTickCountMs();
    _in_time_2 = _out_time - _in_time_1;
#endif
    //MTK_OMX_LOGD("Internal blitStream- iRet=%d, %lld ms", iRet, _in_time_2);

    if (0 != iRet)
    {
#if PROFILING
        MTK_OMX_LOGE("MDP iRet=%d, %lld ms", iRet, _in_time_2);
#endif
        return OMX_FALSE;
    }
    return OMX_TRUE;
}

void MtkOmxVdec::FdDebugDump()
{
    int x, inlen = 0, outlen = 0;
    char *IonInFdBuf = (char *)malloc(VDEC_ROUND_16(VIDEO_ION_MAX_BUFFER * 10 + 16));
    char *IonOutFdBuf = (char *)malloc(VDEC_ROUND_16(VIDEO_ION_MAX_BUFFER * 10 + 16));
    for (x = 0; x < VIDEO_ION_MAX_BUFFER; ++x)
    {
        if (IonInFdBuf == NULL) { break; }
        if (IonOutFdBuf == NULL) { break; }
        if (x == 0)
        {
            inlen += sprintf(IonInFdBuf + inlen, "IonInFd %d:", (int)mIonInputBufferCount);
            outlen += sprintf(IonOutFdBuf + outlen, "IonOutFd %d:", (int)mIonOutputBufferCount);
        }
    }
    MTK_OMX_LOGE("%s", IonInFdBuf);
    MTK_OMX_LOGE("%s", IonOutFdBuf);
    if (IonInFdBuf != NULL) { free(IonInFdBuf); }
    if (IonOutFdBuf != NULL) { free(IonOutFdBuf); }
}

OMX_U32 MtkOmxVdec::AllocateIonBuffer(int IonFd, OMX_U32 Size, VdecIonBufInfo *IonBufInfo)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;

    if (-1 == IonFd)
    {
        mIonDevFd = mt_ion_open("MtkOmxVdec-X");
        if (mIonDevFd < 0)
        {
            MTK_OMX_LOGE("[ERROR] cannot open ION device. LINE:%d", __LINE__);
            err = OMX_ErrorUndefined;
            return err;
        }
    }
    int ret = ion_alloc_mm(mIonDevFd, Size, MEM_ALIGN_512, ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC, &IonBufInfo->pIonBufhandle);
    if (0 != ret)
    {
        MTK_OMX_LOGE("[ERROR] ion_alloc_mm failed (%d), LINE:%d", ret, __LINE__);
        err = OMX_ErrorUndefined;
        return err;
    }
    int share_fd;
    if (ion_share(mIonDevFd, IonBufInfo->pIonBufhandle, &share_fd))
    {
        MTK_OMX_LOGE("[ERROR] ion_share failed, LINE:%d", __LINE__);
        err = OMX_ErrorUndefined;
        return err;
    }
    // map virtual address
    OMX_U8 *buffer = (OMX_U8 *) ion_mmap(mIonDevFd, NULL, Size, PROT_READ | PROT_WRITE, MAP_SHARED, share_fd, 0);
    if ((buffer == NULL) || (buffer == (void *) - 1))
    {
        MTK_OMX_LOGE("[ERROR] ion_mmap failed, LINE:%d", __LINE__);
        err = OMX_ErrorUndefined;
        return err;
    }

    // configure buffer
    ConfigIonBuffer(mIonDevFd, IonBufInfo->pIonBufhandle);
    IonBufInfo->u4OriVA = (VAL_UINT32_T)buffer;
    IonBufInfo->fd = share_fd;
    IonBufInfo->u4VA = (VAL_UINT32_T)buffer;
    IonBufInfo->u4PA = GetIonPhysicalAddress(mIonDevFd, IonBufInfo->pIonBufhandle);
    IonBufInfo->u4BuffSize = Size;

    MTK_OMX_LOGD("ION allocate Size (%d), u4VA(0x%08X), share_fd(%d), VA(0x%08X), PA(0x%08X)",
                 Size, buffer, share_fd, IonBufInfo->u4VA, IonBufInfo->u4PA);

    return err;
}

OMX_BOOL MtkOmxVdec::DescribeFlexibleColorFormat(DescribeColorFormatParams *params)
{
    MediaImage &imageInfo = params->sMediaImage;
    memset(&imageInfo, 0, sizeof(imageInfo));

    imageInfo.mType = MediaImage::MEDIA_IMAGE_TYPE_UNKNOWN;
    imageInfo.mNumPlanes = 0;

    const OMX_COLOR_FORMATTYPE fmt = params->eColorFormat;
    imageInfo.mWidth = params->nFrameWidth;
    imageInfo.mHeight = params->nFrameHeight;

    MTK_OMX_LOGD("DescribeFlexibleColorFormat %d fmt %x, W/H(%d, %d), WS/HS(%d, %d), (%d, %d)", sizeof(size_t), fmt, imageInfo.mWidth, imageInfo.mHeight,
                 params->nStride, params->nSliceHeight, mOutputPortDef.format.video.nStride, mOutputPortDef.format.video.nSliceHeight);

    // only supporting YUV420
    if (fmt != OMX_COLOR_FormatYUV420Planar &&
        fmt != OMX_COLOR_FormatYUV420PackedPlanar &&
        fmt != OMX_COLOR_FormatYUV420SemiPlanar &&
        fmt != HAL_PIXEL_FORMAT_I420 &&
        fmt != OMX_COLOR_FormatYUV420PackedSemiPlanar)
    {
        ALOGW("do not know color format 0x%x = %d", fmt, fmt);
        return OMX_FALSE;
    }

    // set-up YUV format
    imageInfo.mType = MediaImage::MEDIA_IMAGE_TYPE_YUV;
    imageInfo.mNumPlanes = 3;
    imageInfo.mBitDepth = 8;
    imageInfo.mPlane[imageInfo.Y].mOffset = 0;
    imageInfo.mPlane[imageInfo.Y].mColInc = 1;
    imageInfo.mPlane[imageInfo.Y].mRowInc = params->nFrameWidth;
    imageInfo.mPlane[imageInfo.Y].mHorizSubsampling = 1;
    imageInfo.mPlane[imageInfo.Y].mVertSubsampling = 1;

    switch (fmt)
    {
        case OMX_COLOR_FormatYUV420Planar: // used for YV12
        case OMX_COLOR_FormatYUV420PackedPlanar:
        case HAL_PIXEL_FORMAT_I420:
            imageInfo.mPlane[imageInfo.U].mOffset = params->nFrameWidth * params->nFrameHeight;
            imageInfo.mPlane[imageInfo.U].mColInc = 1;
            imageInfo.mPlane[imageInfo.U].mRowInc = params->nFrameWidth / 2;
            imageInfo.mPlane[imageInfo.U].mHorizSubsampling = 2;
            imageInfo.mPlane[imageInfo.U].mVertSubsampling = 2;

            imageInfo.mPlane[imageInfo.V].mOffset = imageInfo.mPlane[imageInfo.U].mOffset
                                                    + (params->nFrameWidth * params->nFrameHeight / 4);
            imageInfo.mPlane[imageInfo.V].mColInc = 1;
            imageInfo.mPlane[imageInfo.V].mRowInc = params->nFrameWidth / 2;
            imageInfo.mPlane[imageInfo.V].mHorizSubsampling = 2;
            imageInfo.mPlane[imageInfo.V].mVertSubsampling = 2;
            break;

        case OMX_COLOR_FormatYUV420SemiPlanar:
            // FIXME: NV21 for sw-encoder, NV12 for decoder and hw-encoder
        case OMX_COLOR_FormatYUV420PackedSemiPlanar:
            // NV12
            imageInfo.mPlane[imageInfo.U].mOffset = params->nStride * params->nSliceHeight;
            imageInfo.mPlane[imageInfo.U].mColInc = 2;
            imageInfo.mPlane[imageInfo.U].mRowInc = params->nStride;
            imageInfo.mPlane[imageInfo.U].mHorizSubsampling = 2;
            imageInfo.mPlane[imageInfo.U].mVertSubsampling = 2;

            imageInfo.mPlane[imageInfo.V].mOffset = imageInfo.mPlane[imageInfo.U].mOffset + 1;
            imageInfo.mPlane[imageInfo.V].mColInc = 2;
            imageInfo.mPlane[imageInfo.V].mRowInc = params->nStride;
            imageInfo.mPlane[imageInfo.V].mHorizSubsampling = 2;
            imageInfo.mPlane[imageInfo.V].mVertSubsampling = 2;
            break;

        default:
            MTK_OMX_LOGE("default %x", fmt);
    }
    return OMX_TRUE;
}

OMX_BOOL MtkOmxVdec::IsColorConvertBuffer(OMX_BUFFERHEADERTYPE *pBuffHdr)
{
    //MTK_OMX_LOGD("IsColorConvertBuffer +");

    VAL_UINT32_T u4I;
    OMX_BOOL bRet = OMX_FALSE;

    for (u4I = 0; u4I < MAX_COLORCONVERT_OUTPUTBUFFER_COUNT; u4I++)
    {
        //MTK_OMX_LOGD("[IsColorConvertBuffer][Color Convert BUFFER][%d] Hdr = 0x%08X",
        //                                     u4I,
        //                                     mColorConvertDstBufferHdr[u4I]);

        if (mColorConvertDstBufferHdr[u4I] == (VAL_UINT32_T)pBuffHdr)
        {
            bRet = OMX_TRUE;
            break;
        }
    }

    //MTK_OMX_LOGD("IsColorConvertBuffer -, %d", bRet);

    return bRet;
}

int MtkOmxVdec::PrepareAvaliableColorConvertBuffer(int output_idx, OMX_BOOL direct_dequeue)
{

    // dequeue an output buffer for color convert
    int output_color_convert_idx = -1;
    if (OMX_TRUE == mOutputAllocateBuffer || OMX_TRUE == needColorConvertWithNativeWindow())
    {
        int mCCQSize = mBufColorConvertDstQ.size();

        if ((mMaxColorConvertOutputBufferCnt > mCCQSize) && (OMX_TRUE == direct_dequeue))
        {
            MTK_OMX_LOGE("[Warning][Color Convert BUFFER] %s@%d should not be here", __FUNCTION__, __LINE__);
            /*output_color_convert_idx = DequeueOutputBuffer();
            ALOGD("PrepareAvaliableColorConvertBuffer output_color_convert_idx %d", output_color_convert_idx);
            if (output_color_convert_idx >= 0)
            {
                QueueOutputColorConvertDstBuffer(output_color_convert_idx);
                ALOGD("%d, output_idx = %d, size %d",
                      mNumFreeAvailOutput, output_color_convert_idx, CheckColorConvertBufferSize());
            }*/
        }
        else if (((mCCQSize < mBufColorConvertSrcQ.size()) || (mMaxColorConvertOutputBufferCnt > mCCQSize)) &&
                 (OMX_FALSE == direct_dequeue))
        {
            output_color_convert_idx = output_idx;
            //ALOGD("pVdec->mFlushInProcess %d", mFlushInProcess);
            OMX_BOOL mIsFree = IsFreeBuffer(mOutputBufferHdrs[output_color_convert_idx]);
            if (OMX_FALSE == mIsFree)
            {
                MTK_OMX_LOGE("[Warning][Color Convert BUFFER] %s@%d should not be here", __FUNCTION__, __LINE__);
            }
            else
            {
                QueueOutputColorConvertDstBuffer(output_color_convert_idx);
                ALOGD("%d, mIsFree %d, output_color_convert_idx = %d, size %d",
                      mNumFreeAvailOutput, mIsFree, output_color_convert_idx, CheckColorConvertBufferSize());
            }
        }
        else
        {
            MTK_OMX_LOGE("[Warning][Color Convert BUFFER] %s@%d should not be here %s: %d", __FUNCTION__, __LINE__);
            //ALOGD("CheckColorConvertBufferSize %d", CheckColorConvertBufferSize());
        }
    }
    return output_color_convert_idx;//OMX_TRUE;
}

void MtkOmxVdec::QueueOutputColorConvertSrcBuffer(int index)
{
    LOCK(mFillThisBufQLock);
    VAL_UINT32_T u4y;
    OMX_BUFFERHEADERTYPE *ipOutputBuffer = mOutputBufferHdrs[index];

    //MTK_OMX_LOGD ("@@ QueueOutputColorConvertSrcBuffer index %d, %x, %x",
    //    index,
    //    ipOutputBuffer->pBuffer,
    //    ipOutputBuffer->pOutputPortPrivate);
    if ((OMX_TRUE == mStoreMetaDataInBuffers) && (OMX_TRUE == mbYUV420FlexibleMode))
    {
        OMX_U32 graphicBufHandle = 0;
        int mIndex = 0;
        if (OMX_FALSE == GetMetaHandleFromOmxHeader(ipOutputBuffer, &graphicBufHandle))
        {
            MTK_OMX_LOGE("SetupMetaIonHandle failed, LINE:%d", __LINE__);
        }

        if (0 == graphicBufHandle)
        {
            MTK_OMX_LOGE("GetMetaHandleFromOmxHeader failed, LINE:%d", __LINE__);
        }
        VBufInfo BufInfo;
        if (mOutputMVAMgr->getOmxInfoFromHndl((void *)graphicBufHandle, &BufInfo) < 0)
        {
            MTK_OMX_LOGD("QueueOutputColorConvertSrcBuffer() cannot find buffer info, LINE: %d", __LINE__);
        }
        else
        {
            mFrameBuf[index].bGraphicBufHandle = graphicBufHandle;
            mFrameBuf[index].ionBufHandle = BufInfo.ionBufHndl;
            mFrameBuf[index].frame_buffer.rBaseAddr.u4VA = BufInfo.u4VA;
            mFrameBuf[index].frame_buffer.rBaseAddr.u4PA = BufInfo.u4PA;
        }
    }
    else if (OMX_TRUE == mOutputUseION)
    {
        VBufInfo info;
        int ret = 0;
        if (OMX_TRUE == mStoreMetaDataInBuffers)
        {
            OMX_U32 graphicBufHandle = 0;
            GetMetaHandleFromOmxHeader(ipOutputBuffer, &graphicBufHandle);
            ret = mOutputMVAMgr->getOmxInfoFromHndl((void *)graphicBufHandle, &info);
            MTK_OMX_LOGE("[Warning] it's not Meta mode, Should not be here. line %d", __LINE__);
        }
        else
        {
            ret = mOutputMVAMgr->getOmxInfoFromVA((void *)ipOutputBuffer->pBuffer, &info);
        }

        if (ret < 0)
        {
            MTK_OMX_LOGD("QueueOutputColorConvertSrcBuffer() cannot find buffer info, LINE: %d", __LINE__);
        }

        mFrameBuf[index].frame_buffer.rBaseAddr.u4VA = info.u4VA;
        mFrameBuf[index].frame_buffer.rBaseAddr.u4PA = info.u4PA;
        if (OMX_TRUE == mIsSecureInst)
        {
            mFrameBuf[index].frame_buffer.rSecMemHandle = info.secure_handle;
            mFrameBuf[index].frame_buffer.rFrameBufVaShareHandle = 0;
            MTK_OMX_LOGE("@@ aFrame->rSecMemHandle(0x%08X), aFrame->rFrameBufVaShareHandle(0x%08X)", mFrameBuf[index].frame_buffer.rSecMemHandle, mFrameBuf[index].frame_buffer.rFrameBufVaShareHandle);
        }

        //MTK_OMX_LOGD("[ION] id %x, frame->rBaseAddr.u4VA = 0x%x, frame->rBaseAddr.u4PA = 0x%x", u4y, mFrameBuf[u4y].frame_buffer.rBaseAddr.u4VA, mFrameBuf[u4y].frame_buffer.rBaseAddr.u4PA);
    }

#if ANDROID
    mBufColorConvertSrcQ.push(index);
    //MTK_OMX_LOGD("QueueOutputColorConvertSrcBuffer size %d", mBufColorConvertSrcQ.size());

    //send cmd only for necassary to reduce cmd queue
    if (0 < mBufColorConvertDstQ.size())
    {
        //MTK_OMX_LOGD("MtkOmxVdec::SendCommand cmd=%s", CommandToString(OMX_CommandVendorStartUnused));
        OMX_U32 CmdCat = MTK_OMX_BUFFER_COMMAND;
        OMX_U32 buffer_type = MTK_OMX_FILL_CONVERTED_BUFFER_DONE_TYPE;
        OMX_BUFFERHEADERTYPE *tmpHead;
        OMX_U32 nParam1 = MTK_OMX_OUTPUT_PORT;
        ssize_t ret = 0;

        LOCK(mConvertCmdQLock);
        WRITE_PIPE(CmdCat, mConvertCmdPipe);
        WRITE_PIPE(buffer_type, mConvertCmdPipe);
        WRITE_PIPE(tmpHead, mConvertCmdPipe);
        WRITE_PIPE(nParam1, mConvertCmdPipe);
        //mCountPIPEWRITE++;
        //mCountPIPEWRITEFBD++;
EXIT:
        UNLOCK(mConvertCmdQLock);
    }
#endif
    UNLOCK(mFillThisBufQLock);

    return;

}
void MtkOmxVdec::QueueOutputColorConvertDstBuffer(int index)
{
    LOCK(mFillThisConvertBufQLock);
    VAL_UINT32_T u4y;
    OMX_BUFFERHEADERTYPE *ipOutputBuffer = mOutputBufferHdrs[index];

    //MTK_OMX_LOGD ("@@ QueueOutputColorConvertDstBuffer index %d, %x, %x",
    //    index,
    //    ipOutputBuffer->pBuffer,
    //    ipOutputBuffer->pOutputPortPrivate);

    if(OMX_TRUE == needColorConvertWithMetaMode())
    {
        OMX_U32 graphicBufHandle = 0;
        int mIndex = 0;
        if (OMX_FALSE == GetMetaHandleFromOmxHeader(ipOutputBuffer, &graphicBufHandle))
        {
            MTK_OMX_LOGE("GetMetaHandleFromOmxHeader failed, LINE:%d", __LINE__);
        }

        if (0 == graphicBufHandle)
        {
            MTK_OMX_LOGE("GetMetaHandleFromOmxHeader failed, LINE:%d", __LINE__);
        }

        MTK_OMX_LOGD("DstBuffer graphicBufHandle %p", graphicBufHandle);
        VBufInfo BufInfo;
        if (mOutputMVAMgr->getOmxInfoFromHndl((void *)graphicBufHandle, &BufInfo) < 0)
        {
            MTK_OMX_LOGD("QueueOutputColorConvertDstBuffer() cannot find buffer info, LINE: %d", __LINE__);
        }
        else
        {
            mFrameBuf[index].bGraphicBufHandle = graphicBufHandle;
            mFrameBuf[index].ionBufHandle = BufInfo.ionBufHndl;
            mFrameBuf[index].frame_buffer.rBaseAddr.u4VA = BufInfo.u4VA;
            mFrameBuf[index].frame_buffer.rBaseAddr.u4PA = BufInfo.u4PA;
        }
    }
    else if (OMX_TRUE == mOutputUseION)
    {
        VBufInfo info;
        int ret = 0;
        if (OMX_TRUE == mStoreMetaDataInBuffers)
        {
            OMX_U32 graphicBufHandle = 0;
            GetMetaHandleFromOmxHeader(ipOutputBuffer, &graphicBufHandle);
            ret = mOutputMVAMgr->getOmxInfoFromHndl((void *)graphicBufHandle, &info);
            MTK_OMX_LOGE("[Warning] it's not Meta mode, Should not be here. line %d", __LINE__);
        }
        else
        {
            ret = mOutputMVAMgr->getOmxInfoFromVA((void *)ipOutputBuffer->pBuffer, &info);
        }

        if (ret < 0)
        {
            MTK_OMX_LOGD("QueueOutputColorConvertSrcBuffer() cannot find buffer info, LINE: %d", __LINE__);
        }

        mFrameBuf[index].frame_buffer.rBaseAddr.u4VA = info.u4VA;
        mFrameBuf[index].frame_buffer.rBaseAddr.u4PA = info.u4PA;

        if (OMX_TRUE == mIsSecureInst)
        {
            mFrameBuf[index].frame_buffer.rSecMemHandle = info.secure_handle;
            mFrameBuf[index].frame_buffer.rFrameBufVaShareHandle = 0;
            MTK_OMX_LOGE("@@ aFrame->rSecMemHandle(0x%08X), aFrame->rFrameBufVaShareHandle(0x%08X)", mFrameBuf[index].frame_buffer.rSecMemHandle, mFrameBuf[index].frame_buffer.rFrameBufVaShareHandle);
        }

        //MTK_OMX_LOGD("[ION] id %x, frame->rBaseAddr.u4VA = 0x%x, frame->rBaseAddr.u4PA = 0x%x", u4y, mFrameBuf[u4y].frame_buffer.rBaseAddr.u4VA, mFrameBuf[u4y].frame_buffer.rBaseAddr.u4PA);
    }

    //MTK_OMX_LOGD("QueueOutputColorConvertDstBuffer %d", index);
    mBufColorConvertDstQ.push(index);

    //send cmd only for necassary to reduce cmd queue
    if (0 < mBufColorConvertSrcQ.size())
    {
        //MTK_OMX_LOGD("QueueOutputColorConvertDstBuffer::SendCommand cmd=%s", CommandToString(OMX_CommandVendorStartUnused));

        OMX_U32 CmdCat = MTK_OMX_BUFFER_COMMAND;
        OMX_U32 buffer_type = MTK_OMX_FILL_CONVERTED_BUFFER_DONE_TYPE;
        OMX_BUFFERHEADERTYPE *tmpHead;
        OMX_U32 nParam1 = MTK_OMX_OUTPUT_PORT;
        ssize_t ret = 0;

        LOCK(mConvertCmdQLock);
        WRITE_PIPE(CmdCat, mConvertCmdPipe);
        WRITE_PIPE(buffer_type, mConvertCmdPipe);
        WRITE_PIPE(tmpHead, mConvertCmdPipe);
        WRITE_PIPE(nParam1, mConvertCmdPipe);
        //mCountPIPEWRITEFBD++;
        //mCountPIPEWRITE++;
EXIT:
        UNLOCK(mConvertCmdQLock);
    }
    UNLOCK(mFillThisConvertBufQLock);

    return;
}

int MtkOmxVdec::DeQueueOutputColorConvertSrcBuffer()
{
    int output_idx = -1, i;
    LOCK(mFillThisBufQLock);
    //MTK_OMX_LOGD("DeQueueOutputColorConvertSrcBuffer() size (%d, %d, %d)",
    //mBufColorConvertSrcQ.size(), mBufColorConvertDstQ.size(), mFillThisBufQ.size());
#if ANDROID
    if (0 == mBufColorConvertSrcQ.size())
    {
        MTK_OMX_LOGE("DeQueueOutputColorConvertSrcBuffer(), mFillThisBufQ.size() is 0, return original idx %d", output_idx);
        output_idx = -1;
        UNLOCK(mFillThisBufQLock);
#ifdef HAVE_AEE_FEATURE
        aee_system_warning("CRDISPATCH_KEY:OMX video decode issue", NULL, DB_OPT_DEFAULT, "\nOmxVdec should no be here will push dummy output buffer!");
#endif //HAVE_AEE_FEATURE
        UNLOCK(mFillThisBufQLock);
        return output_idx;
    }

    output_idx = mBufColorConvertSrcQ[0];
    mBufColorConvertSrcQ.removeAt(0);
#endif
    UNLOCK(mFillThisBufQLock);
    return output_idx;
}


int MtkOmxVdec::DeQueueOutputColorConvertDstBuffer()
{
    int output_idx = -1, i;
    LOCK(mFillThisConvertBufQLock);
    MTK_OMX_LOGD("DeQueueOutputColorConvertDstBuffer() size (max-%d, %d, %d)",
                  mMaxColorConvertOutputBufferCnt, mBufColorConvertDstQ.size(), mFillThisBufQ.size());
#if ANDROID
    for (i = 0; i < mBufColorConvertDstQ.size(); i++)
    {
        output_idx = mBufColorConvertDstQ[i];
        if (OMX_FALSE == IsFreeBuffer(mOutputBufferHdrs[output_idx]))
        {
            MTK_OMX_LOGE("DeQueueOutputColorConvertDstBuffer(), mOutputBufferHdrs[%d] is not free (0x%08X)", output_idx, mOutputBufferHdrs[output_idx]);
        }
        else
        {
            //MTK_OMX_LOGE("DeQueueOutputColorConvertDstBuffer(), mOutputBufferHdrs[%d] is free", output_idx, mOutputBufferHdrs[output_idx]);
            break;
        }
    }

    if (0 == mBufColorConvertDstQ.size())
    {
        MTK_OMX_LOGE("DeQueueOutputColorConvertDstBuffer(), mFillThisBufQ.size() is 0, return original idx %d", output_idx);
        output_idx = -1;
        UNLOCK(mFillThisConvertBufQLock);
#ifdef HAVE_AEE_FEATURE
        aee_system_warning("CRDISPATCH_KEY:OMX video decode issue", NULL, DB_OPT_DEFAULT, "\nOmxVdec should no be here will push dummy output buffer!");
#endif //HAVE_AEE_FEATURE
        UNLOCK(mFillThisConvertBufQLock);
        return output_idx;
    }

    if (i == mBufColorConvertDstQ.size())
    {
        output_idx = mBufColorConvertDstQ[0];
        mBufColorConvertDstQ.removeAt(0);
    }
    else
    {
        output_idx = mBufColorConvertDstQ[i];
        mBufColorConvertDstQ.removeAt(i);
    }
#endif
    UNLOCK(mFillThisConvertBufQLock);
    return output_idx;
}

OMX_U32 MtkOmxVdec::CheckColorConvertBufferSize()
{
    return mBufColorConvertDstQ.size();
}

OMX_ERRORTYPE MtkOmxVdec::HandleColorConvertForFillBufferDone(OMX_U32 nPortIndex, OMX_BOOL fromDecodet)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    //MTK_OMX_LOGD("MtkOmxVdec::HandleColorConvertForFillBufferDone nPortIndex(0x%X)", (unsigned int)nPortIndex);

    int size_of_FBDQ = mBufColorConvertSrcQ.size();
    int size_of_CCQ = mBufColorConvertDstQ.size();
    int in_index = 0;
    ALOGD("size_of_FBDQ %d, CCQ %d", size_of_FBDQ, size_of_CCQ);
    if ((0 != size_of_FBDQ) && (0 != size_of_CCQ))
    {
        const int minSize = ((size_of_FBDQ >= size_of_CCQ) ? size_of_CCQ : size_of_FBDQ);
        for (int i = 0 ; i < minSize; i++)
        {
            //ALOGD("DeQueue i: %d", i);
            in_index = DeQueueOutputColorConvertSrcBuffer();

            if (in_index > 0)
            {
                if (mFrameBuf[in_index].ipOutputBuffer->nTimeStamp == -1)
                {
                    LOCK(mFillThisBufQLock);
                    QueueOutputBuffer(in_index);
                    UNLOCK(mFillThisBufQLock);
                    continue;
                }
            }

            if (0 > in_index)
            {
                MTK_OMX_LOGE("[%s]: DeQueueOutputColorConvertSrcBuffer fail", __func__);
                return OMX_ErrorBadParameter;
            }
            HandleColorConvertForFillBufferDone_1(in_index, OMX_FALSE);
            //ALOGD("FBDQ.size %d, CCQ.size %d", mBufColorConvertSrcQ.size(), mBufColorConvertDstQ.size());
        }
    }
    ALOGD("HandleColorConvertForFillBufferDone exit");
    return err;
}

OMX_ERRORTYPE MtkOmxVdec::HandleColorConvertForFillBufferDone_1(OMX_U32 input_index, OMX_BOOL fromDecodet)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    if (0 > input_index)
    {
        MTK_OMX_LOGE("@@ input_index = %d", input_index);
        return OMX_ErrorUndefined;
    }

    MTK_OMX_LOGD("@@ GetFrmStructure frm=0x%x, i=%d, color= %x, type= %x", &mFrameBuf[input_index].frame_buffer, input_index,
                 mOutputPortFormat.eColorFormat, mInputPortFormat.eCompressionFormat);
    MTK_OMX_LOGD("@@ nFlags = %x",
                 mFrameBuf[input_index].ipOutputBuffer->nFlags);

    // dequeue an output buffer
    int output_idx = DeQueueOutputColorConvertDstBuffer();
    int mNomoreColorConvertQ = 0;
    //ALOGD("output_idx = %d", output_idx);
    if (-1 == output_idx)
    {
        ALOGE("OOPS! fix me");
        output_idx = input_index;
        mNomoreColorConvertQ = 1;
    }
    // check if this buffer is really "FREED"
    if (OMX_FALSE == IsFreeBuffer(mOutputBufferHdrs[output_idx]))
    {
        ALOGD("Output [0x%08X] is not free, mFlushInProcess %d, mNumFreeAvailOutput %d", mOutputBufferHdrs[output_idx],
              mFlushInProcess, mNumFreeAvailOutput);
    }
    else
    {
        ALOGD("now NumFreeAvailOutput = %d (%d %d), s:%x, d:%x, %d, %d, %lld",
              mNumFreeAvailOutput, input_index, output_idx, mFrameBuf[input_index].ipOutputBuffer, mFrameBuf[output_idx].ipOutputBuffer,
              mFrameBuf[input_index].ipOutputBuffer->nFlags,
              mFrameBuf[input_index].ipOutputBuffer->nFilledLen,
              mFrameBuf[input_index].ipOutputBuffer->nTimeStamp);
        //clone source buffer status
        mFrameBuf[output_idx].ipOutputBuffer->nFlags = mFrameBuf[input_index].ipOutputBuffer->nFlags;
        mFrameBuf[output_idx].ipOutputBuffer->nFilledLen = mFrameBuf[input_index].ipOutputBuffer->nFilledLen;
        mFrameBuf[output_idx].ipOutputBuffer->nTimeStamp = mFrameBuf[input_index].ipOutputBuffer->nTimeStamp;
    }

    if ((meDecodeType == VDEC_DRV_DECODER_MTK_SOFTWARE && mCodecId == MTK_VDEC_CODEC_ID_HEVC) ||
        (meDecodeType == VDEC_DRV_DECODER_MTK_SOFTWARE && mCodecId == MTK_VDEC_CODEC_ID_VPX))
    {
        MTK_OMX_LOGD("@Flush Cache Before MDP");
        eVideoFlushCache(NULL, 0, 0);
        sched_yield();
        usleep(1000);       //For CTS checksum fail to make sure flush cache to dram
    }

    // MtkOmxVdec::DescribeFlexibleColorFormat() assumes all frame would be converted
    // Any modification skip this convertion here please revise the function above as well
    OMX_BOOL converted = ConvertFrameToYV12((FrmBufStruct *)&mFrameBuf[input_index], (FrmBufStruct *)&mFrameBuf[output_idx], (mFixedMaxBuffer == OMX_TRUE) ? OMX_TRUE : OMX_FALSE);

    if (OMX_FALSE == converted)
    {
        MTK_OMX_LOGE("Internal color conversion not complete");
    }

    if (mFrameBuf[input_index].ipOutputBuffer->nFilledLen != 0)
    {
        //update buffer filled length after converted if it is non zero
        if( OMX_TRUE == needColorConvertWithNativeWindow() )
        {
            OMX_U32 bufferType = *((OMX_U32 *)mFrameBuf[output_idx].ipOutputBuffer->pBuffer);
            //MTK_OMX_LOGD("bufferType %d, %d, %d", bufferType, sizeof(VideoGrallocMetadata),
            //    sizeof(VideoNativeMetadata));
            // check buffer type
            if (kMetadataBufferTypeGrallocSource == bufferType)
            {
                mFrameBuf[output_idx].ipOutputBuffer->nFilledLen = sizeof(VideoGrallocMetadata);//8
            }
            else if (kMetadataBufferTypeANWBuffer == bufferType)
            {
                mFrameBuf[output_idx].ipOutputBuffer->nFilledLen = sizeof(VideoNativeMetadata);//12 in 32 bit
            }
        }
        else
        {
            mFrameBuf[output_idx].ipOutputBuffer->nFilledLen = (mOutputPortDef.format.video.nFrameWidth * mOutputPortDef.format.video.nFrameHeight) * 3 >> 1;
        }
    }

    if (mDumpOutputFrame == OMX_TRUE)
    {
        // dump converted frames
        char filename[256];
        sprintf(filename, "/sdcard/VdecOutFrm_w%d_h%d_t%d.yuv",
                mOutputPortDef.format.video.nFrameWidth,
                mOutputPortDef.format.video.nFrameHeight,
                gettid());
        //(char *)mFrameBuf[i].frame_buffer.rBaseAddr.u4VA
        //mFrameBuf[output_idx].ipOutputBuffer->pBuffer
        writeBufferToFile(filename, (char *)mFrameBuf[output_idx].frame_buffer.rBaseAddr.u4VA,
                          (mOutputPortDef.format.video.nFrameWidth * mOutputPortDef.format.video.nFrameHeight) * 3 >> 1);
        //dump converted frames in data/vdec
        FILE *fp;
        fp = fopen(filename, "ab");
        if (NULL == fp)
        {
            sprintf(filename, "/data/vdec/VdecOutConvertFrm_w%d_h%d_t%d.yuv",
                    mOutputPortDef.format.video.nFrameWidth,
                    mOutputPortDef.format.video.nFrameHeight,
                    gettid());
            writeBufferToFile(filename, (char *)mFrameBuf[output_idx].frame_buffer.rBaseAddr.u4VA,
                              (mOutputPortDef.format.video.nFrameWidth * mOutputPortDef.format.video.nFrameHeight) * 3 >> 1);
        }
        else
        {
            fclose(fp);
        }
        //
    }

    LOCK(mFillThisBufQLock);
    if (0 == mNomoreColorConvertQ)
    {
        QueueOutputBuffer(input_index);

        mNumFreeAvailOutput++;
    }

    if (mFrameBuf[output_idx].ipOutputBuffer->nFlags & OMX_BUFFERFLAG_EOS)
    {
        mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                               mAppData,
                               OMX_EventBufferFlag,
                               MTK_OMX_OUTPUT_PORT,
                               mFrameBuf[output_idx].ipOutputBuffer->nFlags,
                               NULL);
    }

    //#ifdef MTK_CROSSMOUNT_SUPPORT
    if (OMX_TRUE == mCrossMountSupportOn)
    {
        //VBufInfo info; //mBufInfo
        int ret = 0;
        buffer_handle_t _handle;
        if (OMX_TRUE == mStoreMetaDataInBuffers)
        {
            OMX_U32 graphicBufHandle = 0;
            GetMetaHandleFromOmxHeader(mFrameBuf[output_idx].ipOutputBuffer, &graphicBufHandle);
            ret = mOutputMVAMgr->getOmxInfoFromHndl((void *)graphicBufHandle, &mBufInfo);
        }
        else
        {
            ret = mOutputMVAMgr->getOmxInfoFromVA((void *)mFrameBuf[output_idx].ipOutputBuffer->pBuffer, &mBufInfo);
        }

        if (ret < 0)
        {
            MTK_OMX_LOGD("HandleGrallocExtra cannot find buffer info, LINE: %d", __LINE__);
        }
        else
        {
            MTK_OMX_LOGD("mBufInfo u4VA %x, u4PA %x, iIonFd %d", mBufInfo.u4VA,
                         mBufInfo.u4PA, mBufInfo.iIonFd);
            mFrameBuf[output_idx].ipOutputBuffer->pPlatformPrivate = (OMX_U8 *)&mBufInfo;
        }
        mFrameBuf[output_idx].ipOutputBuffer->nFlags |= OMX_BUFFERFLAG_VDEC_OUTPRIVATE;
    }
    //#endif //MTK_CROSSMOUNT_SUPPORT

    mNumPendingOutput--;

    if (0) //if (mFrameBuf[input_index].bUsed == OMX_TRUE) // v4l2 todo: check this if() later
    {
        mFrameBuf[input_index].bFillThis = OMX_TRUE;
    }
    else
    {
        MTK_OMX_LOGD("0x%08x SIGNAL mDecodeSem from HandleColorConvertForFillBufferDone_1()", this);
        SIGNAL(mOutputBufferSem);
    }
    UNLOCK(mFillThisBufQLock);

#if (ANDROID_VER >= ANDROID_M)
    WaitFence(mFrameBuf[output_idx].ipOutputBuffer, OMX_FALSE);
#endif

    //MTK_OMX_LOGD ("FBD mNumPendingOutput(%d), line: %d", mNumPendingOutput, __LINE__);
    mCallback.FillBufferDone((OMX_HANDLETYPE)&mCompHandle,
                             mAppData,
                             mFrameBuf[output_idx].ipOutputBuffer);

    MTK_OMX_LOGE(" FBD, mNumPendingOutput(%d), input_index = %d, output_index = %d (0x%08x)(%d, %d)", mNumPendingOutput, input_index, output_idx, mFrameBuf[output_idx].ipOutputBuffer, mBufColorConvertDstQ.size(), mFillThisBufQ.size());

    return err;
}

void MtkOmxVdec::DISetGrallocExtra(OMX_BUFFERHEADERTYPE *pBuffHdr)
{
    VAL_UINT32_T u4I;

    //MTK_OMX_LOGD("[DI] DISetGrallocExtra +");

    if (OMX_TRUE == mOutputUseION)
    {
        VBufInfo  bufInfo;
        int ret;
        if (OMX_TRUE == mStoreMetaDataInBuffers && (NULL != pBuffHdr)) //should not be here
        {
            OMX_U32 graphicBufHandle = 0;
            GetMetaHandleFromOmxHeader(pBuffHdr, &graphicBufHandle);
            ret = mOutputMVAMgr->getOmxInfoFromHndl((void *)graphicBufHandle, &bufInfo);
        }
        else
        {
            ret = mOutputMVAMgr->getOmxInfoFromVA((void *)pBuffHdr->pBuffer, &bufInfo);
        }

        if (ret < 0)
        {
            MTK_OMX_LOGE("[DI][ERROR] DISetGrallocExtra() cannot find Buffer Handle from BufHdr ");
        }
        else
        {
            buffer_handle_t _handle = (buffer_handle_t)bufInfo.pNativeHandle;
            if (NULL != _handle)
            {
                if (mOutputPortFormat.eColorFormat == OMX_MTK_COLOR_FormatYV12)
                {
                    __setBufParameter(_handle, GRALLOC_EXTRA_MASK_CM, GRALLOC_EXTRA_BIT_CM_YV12);
                    MTK_OMX_LOGD("[DI] DISetGrallocExtra GRALLOC_EXTRA_BIT_CM_YV12");
                }
                else if (mOutputPortFormat.eColorFormat == OMX_COLOR_FormatVendorMTKYUV)
                {
                    if (mbIs10Bit)
                    {
                        if (mIsHorizontalScaninLSB)
                        {
                            __setBufParameter(_handle, GRALLOC_EXTRA_MASK_CM, GRALLOC_EXTRA_BIT_CM_NV12_BLK_10BIT_H);
                            MTK_OMX_LOGD("[DI] DISetGrallocExtra GRALLOC_EXTRA_BIT_CM_NV12_BLK_10BIT_H");
                        }
                        else
                        {
                            __setBufParameter(_handle, GRALLOC_EXTRA_MASK_CM, GRALLOC_EXTRA_BIT_CM_NV12_BLK_10BIT_V);
                            MTK_OMX_LOGD("[DI] DISetGrallocExtra GRALLOC_EXTRA_BIT_CM_NV12_BLK_10BIT_V");
                        }
                    }
                    else
                    {
                        __setBufParameter(_handle, GRALLOC_EXTRA_MASK_CM, GRALLOC_EXTRA_BIT_CM_NV12_BLK);
                        MTK_OMX_LOGD("[DI] DISetGrallocExtra GRALLOC_EXTRA_BIT_CM_NV12_BLK");
                    }
                }
                else if (mOutputPortFormat.eColorFormat == OMX_COLOR_FormatVendorMTKYUV_FCM)
                {
                    __setBufParameter(_handle, GRALLOC_EXTRA_MASK_CM, GRALLOC_EXTRA_BIT_CM_NV12_BLK_FCM);
                    MTK_OMX_LOGD("[DI] DISetGrallocExtra GRALLOC_EXTRA_BIT_CM_NV12_BLK_FCM");
                }
                else
                {
                    //MTK_OMX_LOGE ("[DI] DISetGrallocExtra eColorFormat = %d", mOutputPortDef.format.video.eColorFormat);
                    //__setBufParameter(_handle, GRALLOC_EXTRA_MASK_CM, GRALLOC_EXTRA_BIT_CM_NV12_BLK);
                    //MTK_OMX_LOGD ("[DI] DISetGrallocExtra GRALLOC_EXTRA_BIT_CM_NV12_BLK");
                }
            }
            else
            {
                //it should be handle the NULL case in non-meta mode
                //MTK_OMX_LOGD ("GrallocExtraSetBufParameter handle is null, skip once");
            }

        }
    }

    if (OMX_TRUE == mStoreMetaDataInBuffers && (NULL != pBuffHdr))
    {
        OMX_U32 graphicBufHandle = 0;
        GetMetaHandleFromOmxHeader(pBuffHdr, &graphicBufHandle);

        if (mOutputPortFormat.eColorFormat == OMX_MTK_COLOR_FormatYV12)
        {
            __setBufParameter((buffer_handle_t)graphicBufHandle, GRALLOC_EXTRA_MASK_CM, GRALLOC_EXTRA_BIT_CM_YV12);
            MTK_OMX_LOGD("[DI] DISetGrallocExtra GRALLOC_EXTRA_BIT_CM_YV12");
        }
        else if (mOutputPortFormat.eColorFormat == OMX_COLOR_FormatVendorMTKYUV)
        {
            if (mbIs10Bit)
            {
                if (mIsHorizontalScaninLSB)
                {
                    __setBufParameter((buffer_handle_t)graphicBufHandle, GRALLOC_EXTRA_MASK_CM, GRALLOC_EXTRA_BIT_CM_NV12_BLK_10BIT_H);
                    MTK_OMX_LOGD("[DI] DISetGrallocExtra GRALLOC_EXTRA_BIT_CM_NV12_BLK_10BIT_H");
                }
                else
                {
                    __setBufParameter((buffer_handle_t)graphicBufHandle, GRALLOC_EXTRA_MASK_CM, GRALLOC_EXTRA_BIT_CM_NV12_BLK_10BIT_V);
                    MTK_OMX_LOGD("[DI] DISetGrallocExtra GRALLOC_EXTRA_BIT_CM_NV12_BLK_10BIT_V");
                }
            }
            else
            {
                __setBufParameter((buffer_handle_t)graphicBufHandle, GRALLOC_EXTRA_MASK_CM, GRALLOC_EXTRA_BIT_CM_NV12_BLK);
                MTK_OMX_LOGD("[DI] DISetGrallocExtra GRALLOC_EXTRA_BIT_CM_NV12_BLK");
            }

        }
        else if (mOutputPortFormat.eColorFormat == OMX_COLOR_FormatVendorMTKYUV_FCM)
        {
            __setBufParameter((buffer_handle_t)graphicBufHandle, GRALLOC_EXTRA_MASK_CM, GRALLOC_EXTRA_BIT_CM_NV12_BLK_FCM);
            MTK_OMX_LOGD("[DI] DISetGrallocExtra GRALLOC_EXTRA_BIT_CM_NV12_BLK_FCM");
        }
        else
        {
            //MTK_OMX_LOGE ("[DI] DISetGrallocExtra eColorFormat = %d", mOutputPortDef.format.video.eColorFormat);
            //__setBufParameter((buffer_handle_t)graphicBufHandle, GRALLOC_EXTRA_MASK_CM, GRALLOC_EXTRA_BIT_CM_NV12_BLK);
            //MTK_OMX_LOGD ("[DI] DISetGrallocExtra GRALLOC_EXTRA_BIT_CM_NV12_BLK");
        }
    }

    //MTK_OMX_LOGD("[DI] DISetGrallocExtra -");
}

OMX_BOOL MtkOmxVdec::WaitFence(OMX_BUFFERHEADERTYPE *mBufHdrType, OMX_BOOL mWaitFence)
{
    if (OMX_TRUE == mStoreMetaDataInBuffers)
    {
        VideoNativeMetadata &nativeMeta = *(VideoNativeMetadata *)(mBufHdrType->pBuffer);

        if (kMetadataBufferTypeANWBuffer == nativeMeta.eType)
        {
            if (0 <= nativeMeta.nFenceFd)
            {
                MTK_OMX_LOGD(" %s for fence %d", (OMX_TRUE == mWaitFence ? "wait" : "noWait"), nativeMeta.nFenceFd);

                //OMX_FLASE for flush and other FBD without getFrmBuffer case
                //should close FD directly
                if (OMX_TRUE == mWaitFence)
                {
                    sp<Fence> fence = new Fence(nativeMeta.nFenceFd);
                    int64_t startTime = getTickCountUs();
                    status_t ret = fence->wait(MTK_OMX_FENCE_TIMEOUT_MS);
                    int64_t duration = getTickCountUs() - startTime;
                    //Log waning on long duration. 10ms is an empirical value.
                    if (duration >= 10000)
                    {
                        MTK_OMX_LOGE("ret %x, wait fence %d took %lld us", ret, nativeMeta.nFenceFd, (long long)duration);
                    }
                }
                else
                {
                    //Fence::~Fence() would close fd automatically so decoder should not close
                    close(nativeMeta.nFenceFd);
                }
                //client need close and set -1 after waiting fence
                nativeMeta.nFenceFd = -1;
            }
        }
    }
    return OMX_TRUE;
}

OMX_BOOL MtkOmxVdec::needLegacyMode(void)
{
    if (mCodecId == MTK_VDEC_CODEC_ID_AVC)
    {
        VDEC_DRV_MRESULT_T rResult = VDEC_DRV_MRESULT_OK;
        if(mIsSecureInst == OMX_TRUE)
        {
            //support svp legacy: D3/55/57/59/63/71/75/97/99
            OMX_U64 bIsSupportSVPPlatform = VAL_FALSE;
            //rResult = eVDecDrvGetParam(mDrvHandle, VDEC_DRV_GET_TYPE_SUPPORT_SVP_LEGACY, (VAL_VOID_T *)&mChipName, (VAL_VOID_T *)&bIsSupportSVPPlatform);
            if(bIsSupportSVPPlatform == VAL_TRUE)
            {
                return OMX_TRUE;
            }
        }
        else
        {
            //legacy: 70/80/D2
            OMX_U64 bIsNeedLegacyMode = VAL_FALSE;
            //rResult = eVDecDrvGetParam(mDrvHandle, VDEC_DRV_GET_TYPE_NEED_LEGACY_MODE, (VAL_VOID_T *)&mChipName, (VAL_VOID_T *)&bIsNeedLegacyMode);
            if(bIsNeedLegacyMode == VAL_TRUE)
            {
                return OMX_TRUE;
            }
        }
    }
    return OMX_FALSE;
}

OMX_BOOL MtkOmxVdec::needColorConvertWithNativeWindow(void)
{
    if (OMX_TRUE == needColorConvertWithMetaMode() || OMX_TRUE == needColorConvertWithoutMetaMode() )
    {
        return OMX_TRUE;
    }
    return OMX_FALSE;
}

OMX_BOOL MtkOmxVdec::needColorConvertWithMetaMode(void)
{
    if ((OMX_TRUE == mStoreMetaDataInBuffers && OMX_TRUE == mbYUV420FlexibleMode))
    {
        return OMX_TRUE;
    }
    return OMX_FALSE;
}

OMX_BOOL MtkOmxVdec::needColorConvertWithoutMetaMode(void)
{
    if ((OMX_TRUE == needLegacyMode() && OMX_TRUE == mbYUV420FlexibleMode))
    {
        return OMX_TRUE;
    }
    return OMX_FALSE;
}

bool MtkOmxVdec::supportAutoEnlarge(void)
{
    if ((mCodecId == MTK_VDEC_CODEC_ID_AVC) && (OMX_FALSE == mIsSecureInst))
    {
        return true;
    }
    return false;
}

OMX_BOOL MtkOmxVdec::CheckLogEnable()
{
    OMX_BOOL nResult = OMX_TRUE;
    char BuildType[PROPERTY_VALUE_MAX];
    char OmxVdecLogValue[PROPERTY_VALUE_MAX];
    char OmxVdecPerfLogValue[PROPERTY_VALUE_MAX];

    property_get("ro.build.type", BuildType, "eng");
    if (!strcmp(BuildType,"eng")){
        property_get("mtk.omx.vdec.log", OmxVdecLogValue, "0");
        mOmxVdecLogEnable = (OMX_BOOL) atoi(OmxVdecLogValue);
        property_get("mtk.omx.vdec.perf.log", OmxVdecPerfLogValue, "0");
        mOmxVdecPerfLogEnable = (OMX_BOOL) atoi(OmxVdecPerfLogValue);
    } else if (!strcmp(BuildType,"userdebug") || !strcmp(BuildType,"user")) {
        property_get("mtk.omx.vdec.log", OmxVdecLogValue, "0");
        mOmxVdecLogEnable = (OMX_BOOL) atoi(OmxVdecLogValue);
        property_get("mtk.omx.vdec.perf.log", OmxVdecPerfLogValue, "0");
        mOmxVdecPerfLogEnable = (OMX_BOOL) atoi(OmxVdecPerfLogValue);
    }
    return nResult;
}


/////////////////////////// -------------------   globalc functions -----------------------------------------///////////
OMX_ERRORTYPE MtkVdec_ComponentInit(OMX_IN OMX_HANDLETYPE hComponent,
                                    OMX_IN OMX_STRING componentName)
{
    OMX_ERRORTYPE err = OMX_ErrorUndefined;
    //MTK_OMX_LOGD ("MtkVdec_ComponentInit");
    OMX_COMPONENTTYPE *pHandle = NULL;
    pHandle = (OMX_COMPONENTTYPE *)hComponent;
    if (NULL != pHandle->pComponentPrivate)
    {
        err = ((MtkOmxBase *)pHandle->pComponentPrivate)->ComponentInit(hComponent, componentName);
    }
    return err;
}


OMX_ERRORTYPE MtkVdec_SetCallbacks(OMX_IN OMX_HANDLETYPE hComponent,
                                   OMX_IN OMX_CALLBACKTYPE *pCallBacks,
                                   OMX_IN OMX_PTR pAppData)
{
    OMX_ERRORTYPE err = OMX_ErrorUndefined;
    //MTK_OMX_LOGD ("MtkVdec_SetCallbacks");
    OMX_COMPONENTTYPE *pHandle = NULL;
    pHandle = (OMX_COMPONENTTYPE *)hComponent;
    if (NULL != pHandle->pComponentPrivate)
    {
        err = ((MtkOmxBase *)pHandle->pComponentPrivate)->SetCallbacks(hComponent, pCallBacks, pAppData);
    }

    return err;
}


OMX_ERRORTYPE MtkVdec_ComponentDeInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE err = OMX_ErrorUndefined;
    //MTK_OMX_LOGD ("MtkVdec_ComponentDeInit");
    OMX_COMPONENTTYPE *pHandle = NULL;
    pHandle = (OMX_COMPONENTTYPE *)hComponent;
    if (NULL != pHandle->pComponentPrivate)
    {
        err = ((MtkOmxBase *)pHandle->pComponentPrivate)->ComponentDeInit(hComponent);
        delete(MtkOmxBase *)pHandle->pComponentPrivate;
    }
    return err;
}


OMX_ERRORTYPE MtkVdec_GetComponentVersion(OMX_IN OMX_HANDLETYPE hComponent,
                                          OMX_IN OMX_STRING componentName,
                                          OMX_OUT OMX_VERSIONTYPE *componentVersion,
                                          OMX_OUT OMX_VERSIONTYPE *specVersion,
                                          OMX_OUT OMX_UUIDTYPE *componentUUID)
{
    OMX_ERRORTYPE err = OMX_ErrorUndefined;
    //MTK_OMX_LOGD ("MtkVdec_GetComponentVersion");
    OMX_COMPONENTTYPE *pHandle = NULL;
    pHandle = (OMX_COMPONENTTYPE *)hComponent;
    if (NULL != pHandle->pComponentPrivate)
    {
        err = ((MtkOmxBase *)pHandle->pComponentPrivate)->GetComponentVersion(hComponent, componentName, componentVersion, specVersion, componentUUID);
    }
    return err;
}

OMX_ERRORTYPE MtkVdec_SendCommand(OMX_IN OMX_HANDLETYPE hComponent,
                                  OMX_IN OMX_COMMANDTYPE Cmd,
                                  OMX_IN OMX_U32 nParam1,
                                  OMX_IN OMX_PTR pCmdData)
{
    OMX_ERRORTYPE err = OMX_ErrorUndefined;
    //MTK_OMX_LOGD ("MtkVdec_SendCommand");
    OMX_COMPONENTTYPE *pHandle = NULL;
    pHandle = (OMX_COMPONENTTYPE *)hComponent;
    if (NULL != pHandle->pComponentPrivate)
    {
        err = ((MtkOmxBase *)pHandle->pComponentPrivate)->SendCommand(hComponent, Cmd, nParam1, pCmdData);
    }
    return err;
}


OMX_ERRORTYPE MtkVdec_SetParameter(OMX_IN OMX_HANDLETYPE hComponent,
                                   OMX_IN OMX_INDEXTYPE nParamIndex,
                                   OMX_IN OMX_PTR pCompParam)
{
    OMX_ERRORTYPE err = OMX_ErrorUndefined;
    //MTK_OMX_LOGD ("MtkVdec_SetParameter");
    OMX_COMPONENTTYPE *pHandle = NULL;
    pHandle = (OMX_COMPONENTTYPE *)hComponent;
    if (NULL != pHandle->pComponentPrivate)
    {
        err = ((MtkOmxBase *)pHandle->pComponentPrivate)->SetParameter(hComponent, nParamIndex, pCompParam);
    }
    return err;
}

OMX_ERRORTYPE MtkVdec_GetParameter(OMX_IN OMX_HANDLETYPE hComponent,
                                   OMX_IN  OMX_INDEXTYPE nParamIndex,
                                   OMX_INOUT OMX_PTR ComponentParameterStructure)
{
    OMX_ERRORTYPE err = OMX_ErrorUndefined;
    //MTK_OMX_LOGD ("MtkVdec_GetParameter");
    OMX_COMPONENTTYPE *pHandle = NULL;
    pHandle = (OMX_COMPONENTTYPE *)hComponent;
    if (NULL != pHandle->pComponentPrivate)
    {
        err = ((MtkOmxBase *)pHandle->pComponentPrivate)->GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
    }
    return err;
}


OMX_ERRORTYPE MtkVdec_GetExtensionIndex(OMX_IN OMX_HANDLETYPE hComponent,
                                        OMX_IN OMX_STRING parameterName,
                                        OMX_OUT OMX_INDEXTYPE *pIndexType)
{
    OMX_ERRORTYPE err = OMX_ErrorUndefined;
    //MTK_OMX_LOGD ("MtkVdec_GetExtensionIndex");
    OMX_COMPONENTTYPE *pHandle = NULL;
    pHandle = (OMX_COMPONENTTYPE *)hComponent;
    if (NULL != pHandle->pComponentPrivate)
    {
        err = ((MtkOmxBase *)pHandle->pComponentPrivate)->GetExtensionIndex(hComponent, parameterName, pIndexType);
    }
    return err;
}

OMX_ERRORTYPE MtkVdec_GetState(OMX_IN OMX_HANDLETYPE hComponent,
                               OMX_INOUT OMX_STATETYPE *pState)
{
    OMX_ERRORTYPE err = OMX_ErrorUndefined;
    //MTK_OMX_LOGD ("MtkVdec_GetState");
    OMX_COMPONENTTYPE *pHandle = NULL;
    pHandle = (OMX_COMPONENTTYPE *)hComponent;
    if (NULL != pHandle->pComponentPrivate)
    {
        err = ((MtkOmxBase *)pHandle->pComponentPrivate)->GetState(hComponent, pState);
    }
    return err;
}


OMX_ERRORTYPE MtkVdec_SetConfig(OMX_IN OMX_HANDLETYPE hComponent,
                                OMX_IN OMX_INDEXTYPE nConfigIndex,
                                OMX_IN OMX_PTR ComponentConfigStructure)
{
    OMX_ERRORTYPE err = OMX_ErrorUndefined;
    //MTK_OMX_LOGD ("MtkVdec_SetConfig");
    OMX_COMPONENTTYPE *pHandle = NULL;
    pHandle = (OMX_COMPONENTTYPE *)hComponent;
    if (NULL != pHandle->pComponentPrivate)
    {
        err = ((MtkOmxBase *)pHandle->pComponentPrivate)->SetConfig(hComponent, nConfigIndex, ComponentConfigStructure);
    }
    return err;
}


OMX_ERRORTYPE MtkVdec_GetConfig(OMX_IN OMX_HANDLETYPE hComponent,
                                OMX_IN OMX_INDEXTYPE nConfigIndex,
                                OMX_INOUT OMX_PTR ComponentConfigStructure)
{
    OMX_ERRORTYPE err = OMX_ErrorUndefined;
    //MTK_OMX_LOGD ("MtkVdec_GetConfig");
    OMX_COMPONENTTYPE *pHandle = NULL;
    pHandle = (OMX_COMPONENTTYPE *)hComponent;
    if (NULL != pHandle->pComponentPrivate)
    {
        err = ((MtkOmxBase *)pHandle->pComponentPrivate)->GetConfig(hComponent, nConfigIndex, ComponentConfigStructure);
    }
    return err;
}


OMX_ERRORTYPE MtkVdec_AllocateBuffer(OMX_IN OMX_HANDLETYPE hComponent,
                                     OMX_INOUT OMX_BUFFERHEADERTYPE **pBuffHead,
                                     OMX_IN OMX_U32 nPortIndex,
                                     OMX_IN OMX_PTR pAppPrivate,
                                     OMX_IN OMX_U32 nSizeBytes)
{
    OMX_ERRORTYPE err = OMX_ErrorUndefined;
    //MTK_OMX_LOGD ("MtkVdec_AllocateBuffer");
    OMX_COMPONENTTYPE *pHandle = NULL;
    pHandle = (OMX_COMPONENTTYPE *)hComponent;
    if (NULL != pHandle->pComponentPrivate)
    {
        err = ((MtkOmxBase *)pHandle->pComponentPrivate)->AllocateBuffer(hComponent, pBuffHead, nPortIndex, pAppPrivate, nSizeBytes);
    }
    return err;
}


OMX_ERRORTYPE MtkVdec_UseBuffer(OMX_IN OMX_HANDLETYPE hComponent,
                                OMX_INOUT OMX_BUFFERHEADERTYPE **ppBufferHdr,
                                OMX_IN OMX_U32 nPortIndex,
                                OMX_IN OMX_PTR pAppPrivate,
                                OMX_IN OMX_U32 nSizeBytes,
                                OMX_IN OMX_U8 *pBuffer)
{
    OMX_ERRORTYPE err = OMX_ErrorUndefined;
    // MTK_OMX_LOGD ("MtkVdec_UseBuffer");
    OMX_COMPONENTTYPE *pHandle = NULL;
    pHandle = (OMX_COMPONENTTYPE *)hComponent;
    if (NULL != pHandle->pComponentPrivate)
    {
        err = ((MtkOmxBase *)pHandle->pComponentPrivate)->UseBuffer(hComponent, ppBufferHdr, nPortIndex, pAppPrivate, nSizeBytes, pBuffer);
    }
    return err;
}


OMX_ERRORTYPE MtkVdec_FreeBuffer(OMX_IN OMX_HANDLETYPE hComponent,
                                 OMX_IN OMX_U32 nPortIndex,
                                 OMX_IN OMX_BUFFERHEADERTYPE *pBuffHead)
{
    OMX_ERRORTYPE err = OMX_ErrorUndefined;
    //MTK_OMX_LOGD ("MtkVdec_FreeBuffer");
    OMX_COMPONENTTYPE *pHandle = NULL;
    pHandle = (OMX_COMPONENTTYPE *)hComponent;
    if (NULL != pHandle->pComponentPrivate)
    {
        err = ((MtkOmxBase *)pHandle->pComponentPrivate)->FreeBuffer(hComponent, nPortIndex, pBuffHead);
    }
    return err;
}


OMX_ERRORTYPE MtkVdec_EmptyThisBuffer(OMX_IN OMX_HANDLETYPE hComponent,
                                      OMX_IN OMX_BUFFERHEADERTYPE *pBuffHead)
{
    OMX_ERRORTYPE err = OMX_ErrorUndefined;
    //MTK_OMX_LOGD ("MtkVdec_EmptyThisBuffer");
    OMX_COMPONENTTYPE *pHandle = NULL;
    pHandle = (OMX_COMPONENTTYPE *)hComponent;
    if (NULL != pHandle->pComponentPrivate)
    {
        err = ((MtkOmxBase *)pHandle->pComponentPrivate)->EmptyThisBuffer(hComponent, pBuffHead);
    }
    return err;
}


OMX_ERRORTYPE MtkVdec_FillThisBuffer(OMX_IN OMX_HANDLETYPE hComponent,
                                     OMX_IN OMX_BUFFERHEADERTYPE *pBuffHead)
{
    OMX_ERRORTYPE err = OMX_ErrorUndefined;
    //MTK_OMX_LOGD ("MtkVdec_FillThisBuffer");
    OMX_COMPONENTTYPE *pHandle = NULL;
    pHandle = (OMX_COMPONENTTYPE *)hComponent;
    if (NULL != pHandle->pComponentPrivate)
    {
        err = ((MtkOmxBase *)pHandle->pComponentPrivate)->FillThisBuffer(hComponent, pBuffHead);
    }
    return err;
}


OMX_ERRORTYPE MtkVdec_ComponentRoleEnum(OMX_IN OMX_HANDLETYPE hComponent,
                                        OMX_OUT OMX_U8 *cRole,
                                        OMX_IN OMX_U32 nIndex)
{
    OMX_ERRORTYPE err = OMX_ErrorUndefined;
    //MTK_OMX_LOGD ("MtkVdec_ComponentRoleEnum");
    OMX_COMPONENTTYPE *pHandle = NULL;
    pHandle = (OMX_COMPONENTTYPE *)hComponent;
    if (NULL != pHandle->pComponentPrivate)
    {
        err = ((MtkOmxBase *)pHandle->pComponentPrivate)->ComponentRoleEnum(hComponent, cRole, nIndex);
    }
    return err;
}


// Note: each MTK OMX component must export 'MtkOmxComponentCreate" to MtkOmxCore
extern "C" OMX_COMPONENTTYPE *MtkOmxComponentCreate(OMX_STRING componentName)
{

    MtkOmxBase *pVdec  = new MtkOmxVdec;

    if (NULL == pVdec)
    {
        ALOGE("[0x%08x] MtkOmxComponentCreate out of memory!!!", pVdec);
        return NULL;
    }

    OMX_COMPONENTTYPE *pHandle = pVdec->GetComponentHandle();
    ALOGD("[0x%08x] MtkOmxComponentCreate mCompHandle(0x%08X)", pVdec, (unsigned int)pHandle);

    pHandle->SetCallbacks                  = MtkVdec_SetCallbacks;
    pHandle->ComponentDeInit               = MtkVdec_ComponentDeInit;
    pHandle->SendCommand                   = MtkVdec_SendCommand;
    pHandle->SetParameter                  = MtkVdec_SetParameter;
    pHandle->GetParameter                  = MtkVdec_GetParameter;
    pHandle->GetExtensionIndex        = MtkVdec_GetExtensionIndex;
    pHandle->GetState                      = MtkVdec_GetState;
    pHandle->SetConfig                     = MtkVdec_SetConfig;
    pHandle->GetConfig                     = MtkVdec_GetConfig;
    pHandle->AllocateBuffer                = MtkVdec_AllocateBuffer;
    pHandle->UseBuffer                     = MtkVdec_UseBuffer;
    pHandle->FreeBuffer                    = MtkVdec_FreeBuffer;
    pHandle->GetComponentVersion           = MtkVdec_GetComponentVersion;
    pHandle->EmptyThisBuffer            = MtkVdec_EmptyThisBuffer;
    pHandle->FillThisBuffer                 = MtkVdec_FillThisBuffer;

    OMX_ERRORTYPE err = MtkVdec_ComponentInit((OMX_HANDLETYPE)pHandle, componentName);
    if (err != OMX_ErrorNone)
    {
        ALOGE("[0x%08x] MtkOmxComponentCreate init failed, error = 0x%x", pVdec, err);
        MtkVdec_ComponentDeInit((OMX_HANDLETYPE)pHandle);
        pHandle = NULL;
    }

    return pHandle;
}

extern "C" void MtkOmxSetCoreGlobal(OMX_COMPONENTTYPE *pHandle, void *data)
{
    ((mtk_omx_core_global *)data)->video_instance_count++;
    ((MtkOmxBase *)(pHandle->pComponentPrivate))->SetCoreGlobal(data);
}

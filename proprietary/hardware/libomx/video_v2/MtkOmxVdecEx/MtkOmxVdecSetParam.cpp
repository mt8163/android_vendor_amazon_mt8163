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
 *   MtkOmxVdecDriver.cpp
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

#include "MtkOmxVdecEx.h"
#include <ui/GraphicBufferMapper.h>
#include <ui/gralloc_extra.h>
#include <ion.h>
#include <ui/Rect.h>

#if (ANDROID_VER >= ANDROID_ICS)
#include <android/native_window.h>
#include <HardwareAPI.h>
//#include <gralloc_priv.h>
#include <ui/GraphicBufferMapper.h>
#include <ui/gralloc_extra.h>
#include "graphics_mtk_defs.h"
#include "gralloc_mtk_defs.h"  //for GRALLOC_USAGE_SECURE
#endif
#include <system/window.h>

#define CHECK_EQ(x, y) (x==y)? true:false
#define CHECK_NEQ(x, y) (x!=y)? true:false

OMX_ERRORTYPE MtkOmxVdec::CheckSetParamState()
{
#if (ANDROID_VER >= ANDROID_ICS)
    if ((mState != OMX_StateLoaded) && (mPortReconfigInProgress != OMX_TRUE))
    {
#else
    if (mState != OMX_StateLoaded)
    {
#endif
        return OMX_ErrorIncorrectStateOperation;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MtkOmxVdec::SetInputPortDefinition(OMX_PARAM_PORTDEFINITIONTYPE *pCompParam)
{
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *)pCompParam;

    memcpy(&mInputPortDef, pCompParam, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
    if (mInputPortDef.format.video.nFrameWidth * mInputPortDef.format.video.nFrameHeight >= FHD_AREA)
    {
        if (mInputPortDef.nBufferSize < SIZE_512K)
        {
            mInputPortDef.nBufferSize = SIZE_512K;
        }
    }
    if (mInputPortDef.format.video.nFrameWidth * mInputPortDef.format.video.nFrameHeight > FHD_AREA)
    {
        if (mCodecId == MTK_VDEC_CODEC_ID_AVC)
        {
            if (mInputPortDef.nBufferSize < MTK_VDEC_AVC_DEFAULT_INPUT_BUFFER_SIZE_4K)
            {
                mInputPortDef.nBufferSize = MTK_VDEC_AVC_DEFAULT_INPUT_BUFFER_SIZE_4K;
            }
        }
        if (mCodecId == MTK_VDEC_CODEC_ID_HEVC)
        {
            if (mInputPortDef.nBufferSize < MTK_VDEC_HEVC_DEFAULT_INPUT_BUFFER_SIZE_4K)
            {
                mInputPortDef.nBufferSize = MTK_VDEC_HEVC_DEFAULT_INPUT_BUFFER_SIZE_4K;
            }
        }
        if (mCodecId == MTK_VDEC_CODEC_ID_VP9)
        {
            if (mInputPortDef.nBufferSize < MTK_VDEC_VP9_DEFAULT_INPUT_BUFFER_SIZE_4K)
            {
                mInputPortDef.nBufferSize = MTK_VDEC_VP9_DEFAULT_INPUT_BUFFER_SIZE_4K;
            }
        }
    }
    MTK_OMX_LOGD("[INPUT] (%d)(%d)(%d)", mInputPortDef.format.video.nFrameWidth, mInputPortDef.format.video.nFrameHeight, mInputPortDef.nBufferSize);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MtkOmxVdec::SetOutputPortDefinition(OMX_PARAM_PORTDEFINITIONTYPE *pCompParam)
{
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *) pCompParam;


    MTK_OMX_LOGD("--- SetOutputPortDefinition --- (%d %d %d %d %d %d) -> (%d %d %d %d %d %d)",
                     mOutputPortDef.format.video.nFrameWidth, mOutputPortDef.format.video.nFrameHeight,
                     mOutputPortDef.format.video.nStride, mOutputPortDef.format.video.nSliceHeight,
                     mOutputPortDef.nBufferCountActual, mOutputPortDef.nBufferSize,
                     pCompParam->format.video.nFrameWidth, pCompParam->format.video.nFrameHeight,
                     pCompParam->format.video.nStride, pCompParam->format.video.nSliceHeight,
                     pCompParam->nBufferCountActual, pCompParam->nBufferSize);


    if (pPortDef->format.video.nFrameWidth == 0 || pPortDef->format.video.nFrameHeight == 0)
    {
        MTK_OMX_LOGE("SetParameter bad parameter width/height is 0");
        return OMX_ErrorBadParameter;
    }

    OMX_VIDEO_PORTDEFINITIONTYPE *pstOMXVideoPortDef = &(mOutputPortDef.format.video);

    if (CHECK_EQ(pstOMXVideoPortDef->nFrameWidth, pPortDef->format.video.nFrameWidth) &&
        CHECK_EQ(pstOMXVideoPortDef->nFrameHeight, pPortDef->format.video.nFrameHeight))
    {
        if (pPortDef->nBufferCountActual - mOutputPortDef.nBufferCountActual > mStarvationSize)
        {
            return OMX_ErrorBadParameter;
        }
        else
        {
            memcpy(&mOutputPortDef, pCompParam, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
            if (OMX_TRUE == mDecoderInitCompleteFlag)
            {
                return OMX_ErrorNone; // return ok
            }
            else
            {
                MTK_OMX_LOGD("keep going before decoder init");
            }
        }
    }
    else if (CHECK_NEQ(pstOMXVideoPortDef->nFrameWidth, pPortDef->format.video.nFrameWidth) ||
             CHECK_NEQ(pstOMXVideoPortDef->nFrameHeight, pPortDef->format.video.nFrameHeight))
    {
        memcpy(&mOutputPortDef, pCompParam, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
        pstOMXVideoPortDef->nFrameWidth = VDEC_ROUND_N(pstOMXVideoPortDef->nFrameWidth, 2);
        pstOMXVideoPortDef->nFrameHeight = VDEC_ROUND_N(pstOMXVideoPortDef->nFrameHeight, 2);
    }
    else   // for error case
    {
        MTK_OMX_LOGE("OMX_IndexParamPortDefinition for output buffer error");
        return OMX_ErrorBadParameter;
    }

    MTK_OMX_MEMSET(&mQInfoOut, 0, sizeof(VDEC_DRV_QUERY_VIDEO_FORMAT_T));

    if (OMX_FALSE == QueryDriverFormat(&mQInfoOut))
    {
        pstOMXVideoPortDef->nStride             = mQInfoOut.u4Width;
        pstOMXVideoPortDef->nSliceHeight    = mQInfoOut.u4Height;
        MTK_OMX_LOGD("Unsupported Video Resolution (%d, %d), MAX(%d, %d)", pstOMXVideoPortDef->nFrameWidth, pstOMXVideoPortDef->nFrameHeight, mQInfoOut.u4Width, mQInfoOut.u4Height);

        return OMX_ErrorBadParameter;
    }
    else
    {
        //for MTK VIDEO 4KH264 SUPPORT  // //query output color format again avoid wrong format
        SetColorFormatbyDrvType(mQInfoOut.ePixelFormat);

        if (mFixedMaxBuffer == OMX_TRUE)
        {
            pstOMXVideoPortDef->nFrameWidth     = mQInfoOut.u4Width;
            pstOMXVideoPortDef->nFrameHeight    = mQInfoOut.u4Height;
        }
        pstOMXVideoPortDef->nStride             = VDEC_ROUND_N(pstOMXVideoPortDef->nFrameWidth, mQInfoOut.u4StrideAlign);
        pstOMXVideoPortDef->nSliceHeight    	= VDEC_ROUND_N(pstOMXVideoPortDef->nFrameHeight, mQInfoOut.u4SliceHeightAlign);
        MTK_OMX_LOGD(" Video Resolution width/height(%d, %d), nStride/nSliceHeight(%d, %d)",
                     pstOMXVideoPortDef->nFrameWidth, pstOMXVideoPortDef->nFrameHeight,
                     pstOMXVideoPortDef->nStride, pstOMXVideoPortDef->nSliceHeight);
    }


#if (ANDROID_VER >= ANDROID_KK)
    mCropLeft = 0;
    mCropTop = 0;
    mCropWidth = mOutputPortDef.format.video.nFrameWidth;
    mCropHeight = mOutputPortDef.format.video.nFrameHeight;
    MTK_OMX_LOGD("mCropWidth : %d , mCropHeight : %d", mCropWidth, mCropHeight);
#endif

    mOutputPortDef.nBufferSize = (pstOMXVideoPortDef->nStride * (pstOMXVideoPortDef->nSliceHeight + 1) * 3 >> 1);// + 16

	if (mInputPortFormat.eCompressionFormat == OMX_VIDEO_CodingMJPEG)
	{
    	SetupBufferInfoForMJPEG();
	}

    MTK_OMX_LOGD("nStride = %d,  nSliceHeight = %d, PicSize = %d, ori_nBufferCountActual = %d", mOutputPortDef.format.video.nStride, mOutputPortDef.format.video.nSliceHeight,
                 mOutputPortDef.nBufferSize, mOutputPortDef.nBufferCountActual);

    if(OMX_FALSE == mSeqInfoCompleteFlag)
    {
    	switch(mInputPortFormat.eCompressionFormat)
    	{
    		case OMX_VIDEO_CodingAVC:
    		{
    	    	SetupBufferInfoForH264();
    		}
    		break;

    	    case OMX_VIDEO_CodingHEVC:
    		{
    	    	SetupBufferInfoForHEVC();
    		}
    		break;

    		default:
    			break;
    	}
    }
    SetupBufferInfoForCrossMount();

    return OMX_ErrorNone;
}

void MtkOmxVdec::SetColorFormatbyDrvType(int ePixelFormat)
{
    {
        switch (ePixelFormat)
        {
            case VDEC_DRV_PIXEL_FORMAT_YUV_420_PLANER:
                mOutputPortFormat.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Planar;
                mOutputPortDef.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Planar;
                MTK_OMX_LOGE("SetColorFormatbyDrvType: VDEC_DRV_PIXEL_FORMAT_YUV_420_PLANER");
                break;

            case VDEC_DRV_PIXEL_FORMAT_YUV_420_PLANER_MTK:
                if (mIsInterlacing)
                {
                    mOutputPortFormat.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatVendorMTKYUV_FCM;
                    mOutputPortDef.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatVendorMTKYUV_FCM;
                }
                else
                {
                    mOutputPortFormat.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatVendorMTKYUV;
                    mOutputPortDef.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatVendorMTKYUV;
                }
                MTK_OMX_LOGE("SetColorFormatbyDrvType: VDEC_DRV_PIXEL_FORMAT_YUV_420_PLANER_MTK");
                break;

            case VDEC_DRV_PIXEL_FORMAT_YUV_YV12:
                mOutputPortFormat.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_MTK_COLOR_FormatYV12;
                mOutputPortDef.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_MTK_COLOR_FormatYV12;
                MTK_OMX_LOGE("SetColorFormatbyDrvType: VDEC_DRV_PIXEL_FORMAT_YUV_YV12");
                break;

            case VDEC_DRV_PIXEL_FORMAT_YUV_420_PLANER_UFO:
                mOutputPortFormat.eColorFormat = OMX_COLOR_FormatVendorMTKYUV_UFO;
                mOutputPortDef.format.video.eColorFormat = OMX_COLOR_FormatVendorMTKYUV_UFO;
                MTK_OMX_LOGD("SetColorFormatbyDrvType: VDEC_DRV_PIXEL_FORMAT_YUV_420_PLANER_UFO");
                break;
            case VDEC_DRV_PIXEL_FORMAT_YUV_NV12:
                mOutputPortFormat.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420SemiPlanar;
                mOutputPortDef.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420SemiPlanar;
                MTK_OMX_LOGUD("SetColorFormatbyDrvType: VDEC_DRV_PIXEL_FORMAT_YUV_NV12");
                break;

            default:
                break;
        }
    }
}

void MtkOmxVdec::SetupBufferInfoForMJPEG()
{
    //for MTK SUPPORT MJPEG [
    //if (mInputPortFormat.eCompressionFormat == OMX_VIDEO_CodingMJPEG)
    {
        mOutputPortDef.nBufferSize = (mOutputPortDef.format.video.nStride * mOutputPortDef.format.video.nSliceHeight) << 2;
        if (mThumbnailMode == OMX_TRUE)
        {
            mOutputPortDef.nBufferCountActual = 2;
            mOutputPortDef.nBufferCountMin = 2;
        }
    }
    //] MJPEG
}

void MtkOmxVdec::SetupBufferInfoForH264()
{
    int32_t MaxDPBSize, PicSize, MaxDPBNumber, ActualOutBuffNums, ActualOutBuffSize;
    OMX_VIDEO_PORTDEFINITIONTYPE *pstOMXVideoPortDef = &(mOutputPortDef.format.video);

    if (mSeqInfoCompleteFlag == OMX_FALSE)
    {
        //MaxDPBSize = 6912000; // level 3.1
        MaxDPBSize = 70778880; // level 5.1
        PicSize = ((mOutputPortDef.format.video.nFrameWidth * mOutputPortDef.format.video.nFrameHeight) * 3) >> 1;
        MaxDPBNumber = MaxDPBSize / PicSize;
    }
    else
    {
        MaxDPBNumber = mDPBSize;
    }

    MaxDPBNumber = (((MaxDPBNumber) < (16)) ? MaxDPBNumber : 16);

    if (mFixedMaxBuffer == OMX_TRUE)
    {
        MaxDPBNumber = 16;
		MTK_OMX_LOGD("***mFixedMaxBuffer***(%d), MaxDPBNumber(%d)", __LINE__, MaxDPBNumber);
    }
    ActualOutBuffNums = MaxDPBNumber + mMinUndequeuedBufs + FRAMEWORK_OVERHEAD; // for some H.264 baseline with other nal headers

    if ((mChipName == VAL_CHIP_NAME_MT6580) ||
        ((mChipName == VAL_CHIP_NAME_MT6592) && ((pstOMXVideoPortDef->nFrameWidth * pstOMXVideoPortDef->nFrameHeight) > (1920 * 1088) ||
                                                 (pstOMXVideoPortDef->nFrameWidth > 1920) || (pstOMXVideoPortDef->nFrameHeight > 1920))))
    {
        mIs4kSwAvc = OMX_TRUE;
    }

    mOutputPortDef.nBufferCountActual = ActualOutBuffNums;
    mOutputPortDef.nBufferCountMin = ActualOutBuffNums - mMinUndequeuedBufs;

	MTK_OMX_LOGD("(%s)@(%d), mOutputPortDef.nBufferCountActual(%d), mOutputPortDef.nBufferCountMin(%d)", __FUNCTION__, __LINE__, mOutputPortDef.nBufferCountActual, mOutputPortDef.nBufferCountMin);

    MTK_OMX_LOGD("mSeqInfoCompleteFlag %d, mFixedMaxBuffer = %d, mIsUsingNativeBuffers %d",
                 mSeqInfoCompleteFlag, mFixedMaxBuffer, mIsUsingNativeBuffers);

    if ((mSeqInfoCompleteFlag == OMX_FALSE) && (mFixedMaxBuffer == OMX_FALSE))
    {
        // v4l2 todo: no port-reconfig. need to refine the initial buffer count
        mOutputPortDef.nBufferCountActual = 3 + mMinUndequeuedBufs;//16 + 3 + mMinUndequeuedBufs + MAX_COLORCONVERT_OUTPUTBUFFER_COUNT;  //For initHW get DPB size and free
        mOutputPortDef.nBufferCountMin = 3;//16 + 3 + MAX_COLORCONVERT_OUTPUTBUFFER_COUNT;
        MTK_OMX_LOGD("(%s)@(%d), mOutputPortDef.nBufferCountActual(%d), mOutputPortDef.nBufferCountMin(%d)", __FUNCTION__, __LINE__, mOutputPortDef.nBufferCountActual, mOutputPortDef.nBufferCountMin);


        /* Workaround to make a little change on the crop size, so that we won't fail
                               in the formatHasNotablyChanged when format changed was sent.
                          */
        //mCropWidth --; // for output format change
        //mCropWidth -= mOutputPortDef.format.video.nFrameWidth > 32 ? 32 : 1;
        //MTK_OMX_LOGD("Force port re-config for actual output buffer size!!");
    }

    if (mThumbnailMode == OMX_TRUE)
    {
        mOutputPortDef.nBufferCountActual = 3;
        mOutputPortDef.nBufferCountMin = 3;
    }
    MTK_OMX_LOGD("MaxDPBNumber = %d,  OutputBuffers = %d (%d) ,PicSize=%d", MaxDPBNumber, ActualOutBuffNums, mOutputPortDef.nBufferCountActual, mOutputPortDef.nBufferSize);

}

void MtkOmxVdec::SetupBufferInfoForHEVC()
{
    int32_t PicSize, MaxDPBNumber, ActualOutBuffNums, ActualOutBuffSize;
    OMX_VIDEO_PORTDEFINITIONTYPE *pstOMXVideoPortDef = &(mOutputPortDef.format.video);

    PicSize = ((pstOMXVideoPortDef->nFrameWidth * pstOMXVideoPortDef->nFrameHeight) * 3) >> 1;
    MaxDPBNumber = mDPBSize;
    //MaxDPBNumber = (((MaxDPBNumber) < (16)) ? MaxDPBNumber : 16);
    ActualOutBuffNums = MaxDPBNumber + mMinUndequeuedBufs + FRAMEWORK_OVERHEAD; // for some HEVC baseline with other nal headers

    //#endif
    mOutputPortDef.nBufferCountActual = ActualOutBuffNums;
    mOutputPortDef.nBufferCountMin = ActualOutBuffNums - mMinUndequeuedBufs;
	MTK_OMX_LOGD("(%s)@(%d), mOutputPortDef.nBufferCountActual(%d), mOutputPortDef.nBufferCountMin(%d)", __FUNCTION__, __LINE__, mOutputPortDef.nBufferCountActual, mOutputPortDef.nBufferCountMin);

    if (mSeqInfoCompleteFlag == OMX_FALSE)
    {
        {
            mOutputPortDef.nBufferCountActual = 2 + mMinUndequeuedBufs;  //For initHW get DPB size and free
            mOutputPortDef.nBufferCountMin = 2;
        }
        if (mThumbnailMode == OMX_FALSE)
        {
            MTK_OMX_LOGD("Force port re-config for actual output buffer size!!");
        }
    }

    if (mThumbnailMode == OMX_TRUE)
    {
        mOutputPortDef.nBufferCountActual = 1;
        mOutputPortDef.nBufferCountMin = 1;
    }

    MTK_OMX_LOGD("MaxDPBNumber = %d,  OutputBuffers = %d (%d) ,PicSize=%d", MaxDPBNumber, ActualOutBuffNums, mOutputPortDef.nBufferCountActual, mOutputPortDef.nBufferSize);
}

void MtkOmxVdec::SetupBufferInfoForCrossMount()
{
    if (/*(OMX_FALSE == mIsUsingNativeBuffers) &&*/ (OMX_TRUE == mCrossMountSupportOn))
    {
        //if( CROSSMOUNT_MAX_COLORCONVERT_OUTPUTBUFFER_COUNT == mMaxColorConvertOutputBufferCnt )
        {
            //for port reconfig
            mMaxColorConvertOutputBufferCnt = (mOutputPortDef.nBufferCountActual / 2);
        }
        mOutputPortDef.nBufferCountActual += mMaxColorConvertOutputBufferCnt;
        MTK_OMX_LOGD("original nBufferCountActual after adjust = %d(+%d) ",
                     mOutputPortDef.nBufferCountActual, mMaxColorConvertOutputBufferCnt);
    }
}

OMX_ERRORTYPE MtkOmxVdec::HandleSetPortDefinition(OMX_PARAM_PORTDEFINITIONTYPE *pCompParam)
{
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *)pCompParam;
    OMX_ERRORTYPE err;

    if (mInputPortDef.nPortIndex == pPortDef->nPortIndex)
    {
        err = SetInputPortDefinition(pCompParam);
        return err;
    }
    else if (mOutputPortDef.nPortIndex == pPortDef->nPortIndex)
    {
        err = SetOutputPortDefinition(pCompParam);
        return err;
    }
    else
    {
        return OMX_ErrorBadParameter;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MtkOmxVdec::HandleSetVideoPortFormat(OMX_PARAM_PORTDEFINITIONTYPE *pCompParam)
{
    OMX_VIDEO_PARAM_PORTFORMATTYPE *pPortFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pCompParam;
    if (pPortFormat->nPortIndex == mInputPortFormat.nPortIndex)
    {
        // TODO: should we allow setting the input port param?
    }
    else if (pPortFormat->nPortIndex == mOutputPortFormat.nPortIndex)
    {
        MTK_OMX_LOGD("Set Output eColorFormat before %x, after %x", mOutputPortFormat.eColorFormat, pPortFormat->eColorFormat);
        //keep original format for CTS Vp8EncoderTest and GTS Vp8CocecTest, sw decode YV12 -> I420 -> set to OMX component
        //L-MR1 change the flow to preference the flexible format if hasNativeWindow=true but component can not support flexible format
        //framework will do SW render(clear NW), OMX component will do color convert to I420 for this SW rendering
        if (mOutputPortFormat.eColorFormat != pPortFormat->eColorFormat)
        {
            mOutputPortFormat.eColorFormat = pPortFormat->eColorFormat;
        }
        else
        {
            //CTS DecodeEditEncodeTest, Surface for output config in mediacodec will cause flexibleYUV flow enable.
            //acodec log: does not support color format 7f000789, err 80000000
            if (OMX_TRUE == mbYUV420FlexibleMode)
            {
                MTK_OMX_LOGD("Disable FlexibleYUV Flow for surface format, MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface in CTS case");
                mbYUV420FlexibleMode = OMX_FALSE;
            }
        }
    }
    else
    {
        return OMX_ErrorBadParameter;
    }

    return OMX_ErrorNone;
}

void MtkOmxVdec::HandleSetMtkOmxVdecThumbnailMode()
{
    MTK_OMX_LOGD("Set thumbnail mode enable %06x", this);
    mThumbnailMode = OMX_TRUE;
    if ((mCodecId == MTK_VDEC_CODEC_ID_VPX) ||
        (mCodecId == MTK_VDEC_CODEC_ID_VP9) ||
        (mCodecId == MTK_VDEC_CODEC_ID_VC1) ||
        (mCodecId == MTK_VDEC_CODEC_ID_MPEG2) ||
        (mCodecId == MTK_VDEC_CODEC_ID_MPEG4))
    {
        mOutputPortDef.nBufferCountActual = MTK_VDEC_THUMBNAIL_OUTPUT_BUFFER_COUNT;
        MTK_OMX_LOGD("SetParameter OMX_IndexVendorMtkOmxVdecThumbnailMode mOutputPortDef.nBufferCountActual %d", mOutputPortDef.nBufferCountActual);
    }
}

void MtkOmxVdec::HandleSetMtkOmxVdecUseClearMotion()
{
#if 0
    if (mMJCEnable == OMX_TRUE)
    {
        if (bOmxMJCLogEnable)
        {
            MTK_OMX_LOGD("[MJC] ======== Use ClearMotion ========");
        }

        if (OMX_TRUE == mIsSecureInst)
        {
            MTK_OMX_LOGD("BYPASS MJC for SVP");
            mUseClearMotion = OMX_FALSE;
            mMJCReconfigFlag = OMX_FALSE;
            MJC_MODE mMode;
            mMode = MJC_MODE_BYPASS;
            m_fnMJCSetParam(mpMJC, MJC_PARAM_MODE, &mMode);
            return;
        }


        if (mUseClearMotion == OMX_FALSE)
        {
            if ((mOutputPortDef.nBufferCountActual + FRAMEWORK_OVERHEAD + mMinUndequeuedBufs + TOTAL_MJC_BUFFER_CNT) <= MAX_TOTAL_BUFFER_CNT) // Jimmy temp
            {
                MTK_OMX_LOGD("[MJC] (1)\n");

                MJC_MODE mMode;
                if (mInputPortFormat.eCompressionFormat == OMX_VIDEO_CodingAVC || mInputPortFormat.eCompressionFormat == OMX_VIDEO_CodingHEVC)
                {
                    mMJCReconfigFlag = OMX_TRUE;    //MJCReconfigFlag for port reconfig can reopen MJC
                    mUseClearMotion = OMX_TRUE;
                    mMode = MJC_MODE_NORMAL;
                }
                else if (mOutputPortDef.format.video.nFrameWidth * mOutputPortDef.format.video.nFrameHeight > 1920 * 1088)
                {
                    mOutputPortDef.nBufferCountActual += (FRAMEWORK_OVERHEAD + mMinUndequeuedBufs);
                    mOutputPortDef.nBufferCountMin += (FRAMEWORK_OVERHEAD + mMinUndequeuedBufs);
                    mMJCReconfigFlag = OMX_FALSE;
                    mUseClearMotion = OMX_FALSE;
                    mMode = MJC_MODE_BYPASS;
                }
                else
                {
                    mOutputPortDef.nBufferCountActual += (FRAMEWORK_OVERHEAD + TOTAL_MJC_BUFFER_CNT + mMinUndequeuedBufs); // Jimmy temp
                    mOutputPortDef.nBufferCountMin += (FRAMEWORK_OVERHEAD + TOTAL_MJC_BUFFER_CNT + mMinUndequeuedBufs); // Jimmy temp
                    mMJCReconfigFlag = OMX_FALSE;
                    mUseClearMotion = OMX_TRUE;
                    mMode = MJC_MODE_NORMAL;
                }

                m_fnMJCSetParam(mpMJC, MJC_PARAM_MODE, &mMode);
            }
            else
            {
                MTK_OMX_LOGD("[MJC] (0)\n");
                mUseClearMotion = OMX_FALSE;
                mMJCReconfigFlag = OMX_FALSE;
                MJC_MODE mMode;
                mMode = MJC_MODE_BYPASS;
                m_fnMJCSetParam(mpMJC, MJC_PARAM_MODE, &mMode);
            }
        }
        else
        {
            MTK_OMX_LOGD("[MJC] (2)\n");
        }

        MTK_OMX_LOGD("[MJC] buf %d\n", mOutputPortDef.nBufferCountActual);
    }
#endif
}

void MtkOmxVdec::HandleMinUndequeuedBufs(VAL_UINT32_T* pCompParam)
{
    mMinUndequeuedBufsDiff = *((VAL_UINT32_T *)pCompParam) - mMinUndequeuedBufs;
    mMinUndequeuedBufs = *((VAL_UINT32_T *)pCompParam);

    if (mMinUndequeuedBufs > MAX_MIN_UNDEQUEUED_BUFS)
    {
        MTK_OMX_LOGD("[MJC][ERROR] mMinUndequeuedBufs : %d\n", mMinUndequeuedBufs);
    }

    if ((mMinUndequeuedBufsFlag == OMX_FALSE) && (mOutputPortDef.nBufferCountActual + mMinUndequeuedBufsDiff <= MAX_TOTAL_BUFFER_CNT))
    {
        //mOutputPortDef.nBufferCountActual += mMinUndequeuedBufsDiff;
        mMinUndequeuedBufsFlag = OMX_TRUE;
    }

    MTK_OMX_LOGD("[MJC] mMinUndequeuedBufs : %d (%i, %d)\n", mMinUndequeuedBufs, mMinUndequeuedBufsDiff, mOutputPortDef.nBufferCountActual);
}

OMX_ERRORTYPE MtkOmxVdec::HandleEnableAndroidNativeBuffers(OMX_PTR pCompParam)
{
    EnableAndroidNativeBuffersParams *pEnableNativeBuffers = (EnableAndroidNativeBuffersParams *) pCompParam;
    if (pEnableNativeBuffers->nPortIndex != mOutputPortFormat.nPortIndex)
    {
        MTK_OMX_LOGE("@@ OMX_GoogleAndroidIndexEnableAndroidNativeBuffers: invalid port index");
        return OMX_ErrorBadParameter;
    }

    if (NULL != pEnableNativeBuffers)
    {
        MTK_OMX_LOGE("OMX_GoogleAndroidIndexEnableAndroidNativeBuffers enable(%d)", pEnableNativeBuffers->enable);
        mIsUsingNativeBuffers = pEnableNativeBuffers->enable;
    }
    else
    {
        MTK_OMX_LOGE("@@ OMX_GoogleAndroidIndexEnableAndroidNativeBuffers: pEnableNativeBuffers is NULL");
        return OMX_ErrorBadParameter;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MtkOmxVdec::UseAndroidNativeBuffer_CheckSecureBuffer(OMX_HANDLETYPE hComp, OMX_PTR pCompParam)
{
    OMX_ERRORTYPE err;

    UseAndroidNativeBufferParams *pUseNativeBufferParams = (UseAndroidNativeBufferParams *)pCompParam;

    sp<android_native_buffer_t> nBuf = pUseNativeBufferParams->nativeBuffer;
    buffer_handle_t _handle = nBuf->handle;
    int secure_buffer_handle;

    size_t bufferSize = (nBuf->stride * nBuf->height * 3 >> 1);
    if (meDecodeType == VDEC_DRV_DECODER_MTK_SOFTWARE)
    {
        bufferSize = (nBuf->stride * (nBuf->height + 1) * 3 >> 1);
    }
    //GraphicBufferMapper::getInstance().getSecureBuffer(_handle, &buffer_type, &secure_buffer_handle);
    mSecFrmBufInfo[mSecFrmBufCount].pNativeHandle = (void *)_handle;

    gralloc_extra_query(_handle, GRALLOC_EXTRA_GET_SECURE_HANDLE, &secure_buffer_handle);

    MTK_OMX_LOGD("@@ _handle(0x%08X), secure_buffer_handle(0x%08X)", _handle, secure_buffer_handle);

    mOutputUseION = OMX_FALSE;
    mSecFrmBufInfo[mSecFrmBufCount].u4BuffId        = secure_buffer_handle;
    mSecFrmBufInfo[mSecFrmBufCount].u4SecHandle = secure_buffer_handle;
    mSecFrmBufInfo[mSecFrmBufCount].u4BuffSize  = bufferSize;

    err = UseBuffer(hComp, pUseNativeBufferParams->bufferHeader, pUseNativeBufferParams->nPortIndex, pUseNativeBufferParams->pAppPrivate, bufferSize, (OMX_U8 *)secure_buffer_handle);

    if (err != OMX_ErrorNone)
    {
        MTK_OMX_LOGE("[ERROR] UseBuffer failed, LINE:%d", __LINE__);
        return err;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MtkOmxVdec::HandleUseIonBuffer(OMX_HANDLETYPE hComp, UseIonBufferParams *pUseIonBufferParams)
{
    OMX_ERRORTYPE err;

    //MTK_OMX_LOGD ("@@ OMX_IndexVendorMtkOmxVencUseIonBuffer");
    //UseIonBufferParams *pUseIonBufferParams = (UseIonBufferParams *)pCompParam;

    if (pUseIonBufferParams->nPortIndex == mInputPortFormat.nPortIndex)
    {
        //MTK_OMX_LOGD ("@@ OMX_IndexVendorMtkOmxVideoUseIonBuffer for port[%d]", pUseIonBufferParams->nPortIndex);
        mInputUseION = OMX_TRUE;
    }
    if (pUseIonBufferParams->nPortIndex == mOutputPortFormat.nPortIndex)
    {
        //MTK_OMX_LOGD ("@@ OMX_IndexVendorMtkOmxVideoUseIonBuffer for port[%d]", pUseIonBufferParams->nPortIndex);
        mOutputUseION = OMX_TRUE;
    }

    MTK_OMX_LOGD("@@ mIsClientLocally(%d), mIsFromGralloc(%d), VA(0x%08X), FD(%d), size(%d)", mIsClientLocally, mIsFromGralloc, pUseIonBufferParams->virAddr, pUseIonBufferParams->Ionfd, pUseIonBufferParams->size);
#if USE_MVA_MANAGER
    int ret;
    OMX_U8 *buffer;
    size_t bufferSize = pUseIonBufferParams->size;
    if (pUseIonBufferParams->nPortIndex == mInputPortFormat.nPortIndex)
    {
        ret = mInputMVAMgr->newOmxMVAandVA(MEM_ALIGN_512, pUseIonBufferParams->size, NULL, (void **)&buffer);
    }
    else
    {
        ret = mOutputMVAMgr->newOmxMVAandVA(MEM_ALIGN_512, pUseIonBufferParams->size, NULL, (void **)&buffer);
    }

    if (ret < 0)
    {
        MTK_OMX_LOGE("[ERROR][ION]Allocate Node failed,line:%d\n", __LINE__);
        return OMX_ErrorUndefined;
    }

#else   //USE_MVA_MANAGER
    if (-1 == mIonDevFd)
    {
        mIonDevFd = mt_ion_open("MtkOmxVdec3");
        if (mIonDevFd < 0)
        {
            MTK_OMX_LOGE("[ERROR] cannot open ION device.");
            err = OMX_ErrorUndefined;
            return err;
        }
    }
    if (ion_import(mIonDevFd, pUseIonBufferParams->Ionfd, (pUseIonBufferParams->nPortIndex == mOutputPortFormat.nPortIndex ? &mIonOutputBufferInfo[mIonOutputBufferCount].pIonBufhandle : &mIonInputBufferInfo[mIonInputBufferCount].pIonBufhandle)))
    {
        MTK_OMX_LOGE("[ERROR] ion_import failed");
    }
    int share_fd;
    if (ion_share(mIonDevFd, (pUseIonBufferParams->nPortIndex == mOutputPortFormat.nPortIndex ? mIonOutputBufferInfo[mIonOutputBufferCount].pIonBufhandle : mIonInputBufferInfo[mIonInputBufferCount].pIonBufhandle), &share_fd))
    {
        MTK_OMX_LOGE("[ERROR] ion_share failed");
    }
    // map virtual address
    size_t bufferSize = pUseIonBufferParams->size;
    OMX_U8 *buffer = (OMX_U8 *) ion_mmap(mIonDevFd, NULL, bufferSize, PROT_READ | PROT_WRITE, MAP_SHARED, share_fd, 0);
    if ((buffer == NULL) || (buffer == (void *) - 1))
    {
        MTK_OMX_LOGE("[ERROR] ion_mmap failed");
        err = OMX_ErrorUndefined;
        return err;
    }

    //MTK_OMX_LOGD ("pBuffer(0x%08x), ion_buf_handle (0x%08X)", buffer, mIonBufferInfo[mIonBufferCount].pIonBufhandle);
    MTK_OMX_LOGD("u4OriVA (0x%08X), u4VA(0x%08X)", pUseIonBufferParams->virAddr, buffer);

    // configure buffer
    ConfigIonBuffer(mIonDevFd, (pUseIonBufferParams->nPortIndex == mOutputPortFormat.nPortIndex ? mIonOutputBufferInfo[mIonOutputBufferCount].pIonBufhandle : mIonInputBufferInfo[mIonInputBufferCount].pIonBufhandle));
    if (pUseIonBufferParams->nPortIndex == mInputPortFormat.nPortIndex)
    {
        mIonInputBufferInfo[mIonInputBufferCount].u4OriVA = (VAL_UINT32_T)pUseIonBufferParams->virAddr;
        mIonInputBufferInfo[mIonInputBufferCount].ori_fd = pUseIonBufferParams->Ionfd;
        mIonInputBufferInfo[mIonInputBufferCount].fd = share_fd;
        mIonInputBufferInfo[mIonInputBufferCount].u4VA = (VAL_UINT32_T)buffer;
        mIonInputBufferInfo[mIonInputBufferCount].u4PA = GetIonPhysicalAddress(mIonDevFd, mIonInputBufferInfo[mIonInputBufferCount].pIonBufhandle);
        mIonInputBufferInfo[mIonInputBufferCount].u4BuffSize = bufferSize;
    }
    else
    {
        mIonOutputBufferInfo[mIonOutputBufferCount].u4OriVA = (VAL_UINT32_T)pUseIonBufferParams->virAddr;
        mIonOutputBufferInfo[mIonOutputBufferCount].ori_fd = pUseIonBufferParams->Ionfd;
        mIonOutputBufferInfo[mIonOutputBufferCount].fd = share_fd;
        mIonOutputBufferInfo[mIonOutputBufferCount].u4VA = (VAL_UINT32_T)buffer;
        mIonOutputBufferInfo[mIonOutputBufferCount].u4PA = GetIonPhysicalAddress(mIonDevFd, mIonOutputBufferInfo[mIonOutputBufferCount].pIonBufhandle);
        mIonOutputBufferInfo[mIonOutputBufferCount].u4BuffSize = bufferSize;
    }
#endif  //USE_MVA_MANAGER
    err = UseBuffer(hComp, pUseIonBufferParams->bufferHeader, pUseIonBufferParams->nPortIndex, pUseIonBufferParams->pAppPrivate, bufferSize, buffer);

    if (err != OMX_ErrorNone)
    {
        MTK_OMX_LOGE("[ERROR] UseBuffer failed");
        err = OMX_ErrorUndefined;
        return err;
    }

    return OMX_ErrorNone;
}



OMX_ERRORTYPE MtkOmxVdec::UseAndroidNativeBuffer_CheckNormalBuffer(OMX_HANDLETYPE hComp, OMX_PTR pCompParam)
{
    UseAndroidNativeBufferParams *pUseNativeBufferParams = (UseAndroidNativeBufferParams *)pCompParam;

    OMX_ERRORTYPE err;

    sp<android_native_buffer_t> nBuf = pUseNativeBufferParams->nativeBuffer;
    buffer_handle_t _handle = nBuf->handle;
    native_handle_t *pGrallocHandle = (native_handle_t *) _handle;
    int ion_buf_fd = -1;

    gralloc_extra_query(_handle, GRALLOC_EXTRA_GET_ION_FD, &ion_buf_fd);

    if ((-1 < ion_buf_fd) && (mCodecId != MTK_VDEC_CODEC_ID_MJPEG))
    {
        // fd from gralloc and no need to free it in FreeBuffer function
        mIsFromGralloc = OMX_TRUE;
        // use ION
        mOutputUseION = OMX_TRUE;
    }
    else
    {
        // use M4U
        mOutputUseION = OMX_FALSE;
#if USE_MVA_MANAGER
        if (mOutputMVAMgr != NULL)
        {
            delete mOutputMVAMgr;
            mOutputMVAMgr = new OmxMVAManager("m4u", "MtkOmxVdec2");
        }
#endif
    }

    if (OMX_TRUE == mOutputUseION)
    {
        MTK_OMX_LOGD("buffer_handle_t(0x%08x), ionFd(0x%08X)", _handle, ion_buf_fd);

        if (OMX_TRUE != mIsUsingNativeBuffers)
        {
            MTK_OMX_LOGE("[ERROR] OMX_GoogleAndroidIndexUseAndroidNativeBuffer: we are not using native buffers");
            err = OMX_ErrorBadParameter;
            return err;
        }
#if !USE_MVA_MANAGER
        if (-1 == mIonDevFd)
        {
            mIonDevFd = mt_ion_open("MtkOmxVdec2");

            if (mIonDevFd < 0)
            {
                MTK_OMX_LOGE("[ERROR] cannot open ION device.");
                err = OMX_ErrorUndefined;
                return err;
            }
        }

        if (ion_import(mIonDevFd, ion_buf_fd, &mIonOutputBufferInfo[mIonOutputBufferCount].pIonBufhandle))
        {
            MTK_OMX_LOGE("[ERROR] ion_import failed");
        }
        int share_fd;
        if (ion_share(mIonDevFd, mIonOutputBufferInfo[mIonOutputBufferCount].pIonBufhandle, &share_fd))
        {
            MTK_OMX_LOGE("[ERROR] ion_share failed");
        }

        mIonOutputBufferInfo[mIonOutputBufferCount].pNativeHandle = (void *)_handle;
#endif  //!USE_MVA_MANAGER
        // map virtual address
        size_t bufferSize = (nBuf->stride * nBuf->height * 3 >> 1);
        //fore MTK VIDEO 4KH264 SUPPORT [
        if (MTK_VDEC_CODEC_ID_AVC == mCodecId)
        {
            // query output color format and stride and sliceheigt
            VDEC_DRV_QUERY_VIDEO_FORMAT_T qinfoOut;
            MTK_OMX_MEMSET(&qinfoOut, 0, sizeof(VDEC_DRV_QUERY_VIDEO_FORMAT_T));
            if (OMX_TRUE == QueryDriverFormat(&qinfoOut))
            {
                meDecodeType = qinfoOut.eDecodeType;
                MTK_OMX_LOGD(" AVC meDecodeType = %d", meDecodeType);
            }
            else
            {
                MTK_OMX_LOGE("[ERROR] AVC QueryDriverFormat failed");
            }
        }
        //] 4KH264
        if (meDecodeType == VDEC_DRV_DECODER_MTK_SOFTWARE)
        {
            bufferSize = (nBuf->stride * (nBuf->height + 1) * 3 >> 1);
        }

#if USE_MVA_MANAGER
        int size, ret;
        OMX_U8 *buffer;
        gralloc_extra_query(_handle, GRALLOC_EXTRA_GET_ALLOC_SIZE, &size);

        if (bufferSize > size)
        {
            MTK_OMX_LOGE("[ERROR] ion_mmap size(%d) < buffer Size(%d)", size, bufferSize);
        }
        ret = mOutputMVAMgr->newOmxMVAwithHndl((void *)_handle, NULL);

        if (ret < 0)
        {
            MTK_OMX_LOGE("[ERROR]newOmxMVAwithHndl() failed");
        }

        VBufInfo BufInfo;
        if (mOutputMVAMgr->getOmxInfoFromHndl((void *)_handle, &BufInfo) < 0)
        {
            MTK_OMX_LOGE("[ERROR][ION][Output]Can't find Frm in mOutputMVAMgr,LINE:%d", __LINE__);
            err = OMX_ErrorUndefined;
            return err;
        }

        buffer = (OMX_U8 *)BufInfo.u4VA;

#else   //USE_MVA_MANAGER
        OMX_U8 *buffer = (OMX_U8 *) ion_mmap(mIonDevFd, NULL, bufferSize, PROT_READ | PROT_WRITE, MAP_SHARED, share_fd, 0);
        if ((buffer == NULL) || (buffer == (void *) - 1))
        {
            MTK_OMX_LOGE("[ERROR] ion_mmap failed");
            err = OMX_ErrorUndefined;
            return err;
        }

        MTK_OMX_LOGD("share_fd(0x%08X), pBuffer(0x%08x), ion_buf_handle (0x%08X), mIsClientLocally(%d), bufferSize(%d)",
                     share_fd, buffer, mIonOutputBufferInfo[mIonOutputBufferCount].pIonBufhandle, mIsClientLocally, bufferSize);

        // configure buffer
        ConfigIonBuffer(mIonDevFd, mIonOutputBufferInfo[mIonOutputBufferCount].pIonBufhandle);
        mIonOutputBufferInfo[mIonOutputBufferCount].ori_fd = ion_buf_fd;
        mIonOutputBufferInfo[mIonOutputBufferCount].fd = share_fd;
        mIonOutputBufferInfo[mIonOutputBufferCount].u4OriVA = (VAL_UINT32_T)buffer;
        mIonOutputBufferInfo[mIonOutputBufferCount].u4VA = (VAL_UINT32_T)buffer;
        mIonOutputBufferInfo[mIonOutputBufferCount].u4PA = GetIonPhysicalAddress(mIonDevFd, mIonOutputBufferInfo[mIonOutputBufferCount].pIonBufhandle);
        mIonOutputBufferInfo[mIonOutputBufferCount].u4BuffSize = bufferSize;
#endif  //USE_MVA_MANAGER

        if (OMX_TRUE == mIsSecureInst)
        {
            unsigned long secHandle = 0;
            unsigned long va_share_handle = 0;
            // get secure buffer handle from gralloc (DEBUG ONLY)
            if (INHOUSE_TEE == mTeeType)
            {
                //#if defined(MTK_SEC_VIDEO_PATH_SUPPORT) && defined(MTK_IN_HOUSE_TEE_SUPPORT)
                MtkVideoAllocateSecureFrameBuffer_Ptr *pfnMtkVideoAllocateSecureFrameBuffer_Ptr = (MtkVideoAllocateSecureFrameBuffer_Ptr *) dlsym(mH264SecVdecInHouseLib, MTK_H264_SEC_VDEC_IN_HOUSE_ALLOCATE_SECURE_FRAME_BUFFER);
                if (NULL == pfnMtkVideoAllocateSecureFrameBuffer_Ptr)
                {
                    MTK_OMX_LOGE("[ERROR] cannot find MtkVideoAllocateSecureFrameBuffer, LINE: %d", __LINE__);
                    err = OMX_ErrorUndefined;
                    return err;

                }
                MtkVideoRegisterSharedMemory_Ptr *pfnMtkVideoRegisterSharedMemory_Ptr = (MtkVideoRegisterSharedMemory_Ptr *) dlsym(mH264SecVdecInHouseLib, MTK_H264_SEC_VDEC_IN_HOUSE_REGISTER_SHARED_MEMORY);
                if (NULL == pfnMtkVideoRegisterSharedMemory_Ptr)
                {
                    MTK_OMX_LOGE("[ERROR] cannot find MtkVideoRegisterSharedMemory, LINE: %d", __LINE__);
                    err = OMX_ErrorUndefined;
                    return err;

                }
                secHandle = pfnMtkVideoAllocateSecureFrameBuffer_Ptr(bufferSize, 512);
                va_share_handle = pfnMtkVideoRegisterSharedMemory_Ptr(buffer, bufferSize);
#if USE_MVA_MANAGER
                //int ret = mOutputMVAMgr->setSecHandleFromVa((void*)buffer, secHandle );
                //if(ret < 0)
                //{
                //MTK_OMX_LOGE("[ERROR] setSecHandleFromVa failed, LINE: %d", __LINE__);
                //}
#else
                mIonOutputBufferInfo[mIonOutputBufferCount].secure_handle = secHandle;
                mIonOutputBufferInfo[mIonOutputBufferCount].va_share_handle = va_share_handle;
                //#endif
#endif  //USE_MVA_MANAGER
            }
            else //TRUSTONIC_TEE
            {
                // get secure buffer handle from gralloc (DEBUG ONLY)
                if (NULL != mH264SecVdecTlcLib)
                {
                    MtkH264SecVdec_secMemAllocate_Ptr *pfnMtkH264SecVdec_secMemAllocate = (MtkH264SecVdec_secMemAllocate_Ptr *) dlsym(mH264SecVdecTlcLib, MTK_H264_SEC_VDEC_SEC_MEM_ALLOCATE_NAME);

                    if (NULL != pfnMtkH264SecVdec_secMemAllocate)
                    {
                        secHandle = pfnMtkH264SecVdec_secMemAllocate(1024, bufferSize);
#if USE_MVA_MANAGER
                        //int ret = mOutputMVAMgr->setSecHandleFromVa((void*)buffer, secHandle);
                        //if(ret < 0)
                        //{
                        //    MTK_OMX_LOGE("[ERROR] setSecHandleFromVa failed, LINE: %d", __LINE__);
                        //}
#else
                        mIonOutputBufferInfo[mIonOutputBufferCount].secure_handle = secHandle;
#endif  //USE_MVA_MANAGER
                    }
                    else
                    {
                        MTK_OMX_LOGE("[ERROR] cannot find MtkH264SecVdec_secMemAllocate, LINE: %d", __LINE__);
                        err = OMX_ErrorUndefined;
                        return err;
                    }
                }
                else
                {
                    MTK_OMX_LOGE("[ERROR] mH264SecVdecTlcLib is NULL, LINE: %d", __LINE__);
                    err = OMX_ErrorUndefined;
                    return err;
                }
                MTK_OMX_LOGD("mIonOutputBufferInfo[%d].secure_handle(0x%08X) %d", mIonOutputBufferCount, secHandle, mIonOutputBufferCount);
            }
            MTK_OMX_LOGD("mIonOutputBufferInfo[%d].secure_handle(0x%08X), mIonOutputBufferInfo[%d].va_share_handle(0x%08X)", mIonOutputBufferCount, secHandle, mIonOutputBufferCount, va_share_handle);
        }

        err = UseBuffer(hComp, pUseNativeBufferParams->bufferHeader, pUseNativeBufferParams->nPortIndex, pUseNativeBufferParams->pAppPrivate, bufferSize, (OMX_U8 *)buffer);

        if (err != OMX_ErrorNone)
        {
            MTK_OMX_LOGE("[ERROR] UseBuffer failed");
            err = OMX_ErrorUndefined;
            return err;
        }
    }
    else     // M4U
    {
        sp<android_native_buffer_t> nBuf = pUseNativeBufferParams->nativeBuffer;
        buffer_handle_t _handle = nBuf->handle;
        //MTK_OMX_LOGD ("@@ buffer_handle_t(0x%08x)", _handle);

        //IMG_native_handle_t *pGrallocHandle = (IMG_native_handle_t*) _handle;
        native_handle_t *pGrallocHandle = (native_handle_t *) _handle;
        //MTK_OMX_LOGD ("0x%08x iFormat(0x%08X)(%d)(%d)", _handle, pGrallocHandle->iFormat, pGrallocHandle->iWidth, pGrallocHandle->iHeight);
        //MTK_OMX_LOGD ("0x%08x width(%d), height(%d), stride(%d)", _handle, nBuf->width, nBuf->height, nBuf->stride);
        if (OMX_TRUE != mIsUsingNativeBuffers)
        {
            MTK_OMX_LOGE("@@ OMX_GoogleAndroidIndexUseAndroidNativeBuffer: we are not using native buffers");
            err = OMX_ErrorBadParameter;
            return err;
        }

        //LOGD ("@@ OMX_GoogleAndroidIndexUseAndroidNativeBuffer");
        OMX_U8 *buffer;
        GraphicBufferMapper &gbm = GraphicBufferMapper::getInstance();
        int iRet;
        iRet = gbm.lock(_handle, GRALLOC_USAGE_SW_READ_OFTEN, Rect(nBuf->width, nBuf->height), (void **)&buffer);
        if (iRet != 0)
        {
            MTK_OMX_LOGE("gbm->lock(...) failed: %d", iRet);
            return OMX_ErrorNone;
        }

        //LOGD ("@@ buffer(0x%08x)", buffer);
        err = UseBuffer(hComp, pUseNativeBufferParams->bufferHeader, pUseNativeBufferParams->nPortIndex, pUseNativeBufferParams->pAppPrivate, nBuf->stride * nBuf->height * 3 >> 1, buffer);

        iRet = gbm.unlock(_handle);
        if (iRet != 0)
        {
            MTK_OMX_LOGE("gbm->unlock() failed: %d", iRet);
            return OMX_ErrorNone;
        }
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MtkOmxVdec::HandleUseAndroidNativeBuffer(OMX_HANDLETYPE hComp, OMX_PTR pCompParam)
{
    OMX_ERRORTYPE err;

    // Bruce Hsu 20120329 cancel workaround
    // Morris Yang 20111128 temp workaround for ANativeWindowBuffer
    UseAndroidNativeBufferParams *pUseNativeBufferParams = (UseAndroidNativeBufferParams *)pCompParam;
    //UseAndroidNativeBufferParams3* pUseNativeBufferParams = (UseAndroidNativeBufferParams3*)pCompParam;

    if (pUseNativeBufferParams->nPortIndex != mOutputPortFormat.nPortIndex)
    {
        MTK_OMX_LOGE("@@ OMX_GoogleAndroidIndexUseAndroidNativeBuffer: invalid port index");
        return OMX_ErrorBadParameter;
    }

    if (OMX_TRUE != mIsUsingNativeBuffers)
    {
        MTK_OMX_LOGE("@@ OMX_GoogleAndroidIndexUseAndroidNativeBuffer: we are not using native buffers");
        return OMX_ErrorBadParameter;
    }

    sp<android_native_buffer_t> nBuf = pUseNativeBufferParams->nativeBuffer;
    buffer_handle_t _handle = nBuf->handle;
    native_handle_t *pGrallocHandle = (native_handle_t *) _handle;
    int ion_buf_fd = -1;

    // TODO: check secure case
    int secure_buffer_handle;

    if (OMX_TRUE == mIsSecureInst)
    {
        err = UseAndroidNativeBuffer_CheckSecureBuffer(hComp, pCompParam);
        return err;
    }
    else
    {
        err = UseAndroidNativeBuffer_CheckNormalBuffer(hComp, pCompParam);
        return err;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MtkOmxVdec::CheckICSLaterSetParameters(OMX_HANDLETYPE hComp, OMX_INDEXTYPE nParamIndex, OMX_PTR pCompParam)
{
    OMX_ERRORTYPE err;

    //MTK_OMX_LOGD("CheckICSLaterSetParameters");

#if (ANDROID_VER >= ANDROID_ICS)
    switch (nParamIndex)
    {
        case OMX_GoogleAndroidIndexEnableAndroidNativeBuffers:
        {
            MTK_OMX_LOGD("HandleEnableAndroidNativeBuffers");
            err = HandleEnableAndroidNativeBuffers(pCompParam);
            return err;
        }
        break;

        case OMX_GoogleAndroidIndexUseAndroidNativeBuffer:
        {
            MTK_OMX_LOGD("HandleUseAndroidNativeBuffer");
            err = HandleUseAndroidNativeBuffer(hComp, pCompParam);
            return err;
        }
        break;

        default:
        {
            //MTK_OMX_LOGD("not ICS later SetParam");
            return OMX_ErrorNotImplemented;
        }
    }
#endif

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MtkOmxVdec::CheckKKLaterSetParameters(OMX_INDEXTYPE nParamIndex, OMX_PTR pCompParam)
{
#if (ANDROID_VER >= ANDROID_KK)
    switch (nParamIndex)
    {
        case OMX_GoogleAndroidIndexStoreMetaDataInBuffers:
        {
            StoreMetaDataInBuffersParams *pStoreMetaDataInBuffersParams = (StoreMetaDataInBuffersParams *)pCompParam;
            if (pStoreMetaDataInBuffersParams->nPortIndex == mOutputPortFormat.nPortIndex)
            {
                // return unsupport intentionally
                if (mAdaptivePlayback)
                {

                }
                else
                {
                    return  OMX_ErrorUnsupportedIndex;
                }

                mStoreMetaDataInBuffers = pStoreMetaDataInBuffersParams->bStoreMetaData;
                MTK_OMX_LOGD("@@ mStoreMetaDataInBuffers(%d) for port(%d)", mStoreMetaDataInBuffers, pStoreMetaDataInBuffersParams->nPortIndex);
            }
            else
            {
                return OMX_ErrorBadPortIndex;
            }
        }
        break;

        /* // Temporary fix - disable the support of adaptive playback to workaround cts
        case OMX_GoogleAndroidIndexPrepareForAdaptivePlayback:
        {
            PrepareForAdaptivePlaybackParams* pAdaptivePlaybackParams = (PrepareForAdaptivePlaybackParams*)pCompParam;
            if (pAdaptivePlaybackParams->nPortIndex == mOutputPortFormat.nPortIndex) {
                mEnableAdaptivePlayback = pAdaptivePlaybackParams->bEnable;
                mMaxWidth = pAdaptivePlaybackParams->nMaxFrameWidth;
                mMaxHeight = pAdaptivePlaybackParams->nMaxFrameHeight;
                MTK_OMX_LOGD ("@@ mEnableAdaptivePlayback(%d), mMaxWidth(%d), mMaxHeight(%d)", mEnableAdaptivePlayback, mMaxWidth, mMaxHeight);
            }
            else {
                err = OMX_ErrorBadPortIndex;
                goto EXIT;
            }
            break;
        }
        */
        default:
            return OMX_ErrorNotImplemented;
    }
#endif

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MtkOmxVdec::CheckMLaterSetParameters(OMX_INDEXTYPE nParamIndex, OMX_PTR pCompParam)
{
#if (ANDROID_VER >= ANDROID_M)
    switch (nParamIndex)
    {
        case OMX_GoogleAndroidIndexstoreANWBufferInMetadata:
        {
            StoreMetaDataInBuffersParams *pStoreMetaDataInBuffersParams = (StoreMetaDataInBuffersParams *)pCompParam;
            if (pStoreMetaDataInBuffersParams->nPortIndex == mOutputPortFormat.nPortIndex)
            {
                char value2[PROPERTY_VALUE_MAX];

                property_get("mtk.omxvdec.USANWInMetadata", value2, "0");
                OMX_BOOL  mDisableANWInMetadata = (OMX_BOOL) atoi(value2);
                if (1 == mDisableANWInMetadata)
                {
                    MTK_OMX_LOGD("@@ OMX_GoogleAndroidIndexstoreANWBufferInMetadata return un support by setting property");
                    return OMX_ErrorUnsupportedIndex;
                }
                // return unsupport intentionally
                if (mAdaptivePlayback)
                {

                }
                else
                {
                    return OMX_ErrorUnsupportedIndex;
                }

                mStoreMetaDataInBuffers = pStoreMetaDataInBuffersParams->bStoreMetaData;
                MTK_OMX_LOGD("@@ OMX_GoogleAndroidIndexstoreANWBufferInMetadata");
                MTK_OMX_LOGD("@@ mStoreMetaDataInBuffers(%d) for port(%d)", mStoreMetaDataInBuffers, pStoreMetaDataInBuffersParams->nPortIndex);
            }
            else
            {
                return OMX_ErrorBadPortIndex;
            }
        }
        break;

        default:
            return OMX_ErrorNotImplemented;
    }
#endif

    return OMX_ErrorNone;
}


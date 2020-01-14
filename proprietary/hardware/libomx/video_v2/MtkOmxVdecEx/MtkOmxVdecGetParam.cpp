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
#include "graphics_mtk_defs.h"
#include "gralloc_mtk_defs.h"  //for GRALLOC_USAGE_SECURE

#define CHECK_EQ(x, y) (x==y)? true:false
#define CHECK_NEQ(x, y) (x!=y)? true:false

#if 1
MTK_VDEC_PROFILE_MAP_ENTRY HevcProfileMapTable[] =
{
    {OMX_VIDEO_HEVCProfileMain,      VDEC_DRV_H265_VIDEO_PROFILE_H265_MAIN},
    {OMX_VIDEO_HEVCProfileMain10,  VDEC_DRV_H265_VIDEO_PROFILE_H265_MAIN_10}
};

MTK_VDEC_LEVEL_MAP_ENTRY HevcLevelMapTable[] =
{
    {OMX_VIDEO_HEVCMainTierLevel1,   VDEC_DRV_VIDEO_LEVEL_1},
    {OMX_VIDEO_HEVCHighTierLevel1,  VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_1},
    {OMX_VIDEO_HEVCMainTierLevel2,  VDEC_DRV_VIDEO_LEVEL_2},
    {OMX_VIDEO_HEVCHighTierLevel2,  VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_2},
    {OMX_VIDEO_HEVCMainTierLevel21,  VDEC_DRV_VIDEO_LEVEL_2_1},
    {OMX_VIDEO_HEVCHighTierLevel21,   VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_2_1},
    {OMX_VIDEO_HEVCMainTierLevel3,  VDEC_DRV_VIDEO_LEVEL_3},
    {OMX_VIDEO_HEVCHighTierLevel3,  VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_3},
    {OMX_VIDEO_HEVCMainTierLevel31,   VDEC_DRV_VIDEO_LEVEL_3_1},
    {OMX_VIDEO_HEVCHighTierLevel31,  VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_3_1},
    {OMX_VIDEO_HEVCMainTierLevel4,   VDEC_DRV_VIDEO_LEVEL_4},
    {OMX_VIDEO_HEVCHighTierLevel4,  VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_4},
    {OMX_VIDEO_HEVCMainTierLevel41,  VDEC_DRV_VIDEO_LEVEL_4_1},
    {OMX_VIDEO_HEVCHighTierLevel41,   VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_4_1},
    {OMX_VIDEO_HEVCMainTierLevel5,  VDEC_DRV_VIDEO_LEVEL_5},
    {OMX_VIDEO_HEVCHighTierLevel5,  VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_5},
    {OMX_VIDEO_HEVCMainTierLevel51,   VDEC_DRV_VIDEO_LEVEL_5_1},
    {OMX_VIDEO_HEVCHighTierLevel51,  VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_5_1},
    {OMX_VIDEO_HEVCMainTierLevel52,   VDEC_DRV_VIDEO_LEVEL_5_2},
    {OMX_VIDEO_HEVCHighTierLevel52,  VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_5_2},
    {OMX_VIDEO_HEVCMainTierLevel6,   VDEC_DRV_VIDEO_LEVEL_6},
    {OMX_VIDEO_HEVCHighTierLevel6,  VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_6},
    {OMX_VIDEO_HEVCMainTierLevel61,   VDEC_DRV_VIDEO_LEVEL_6_1},
    {OMX_VIDEO_HEVCHighTierLevel61,  VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_6_1},
    {OMX_VIDEO_HEVCMainTierLevel62,   VDEC_DRV_VIDEO_LEVEL_6_2},
    {OMX_VIDEO_HEVCHighTierLevel62,  VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_6_2}
};

#define MAX_HEVC_PROFILE_MAP_TABLE_SIZE    	(sizeof(HevcProfileMapTable)/sizeof(MTK_VDEC_PROFILE_MAP_ENTRY))
#define MAX_HEVC_LEVEL_MAP_TABLE_SIZE    	(sizeof(HevcLevelMapTable)/sizeof(MTK_VDEC_LEVEL_MAP_ENTRY))
#define MAX_AVC_PROFILE_MAP_TABLE_SIZE    	(sizeof(AvcProfileMapTable)/sizeof(MTK_VDEC_PROFILE_MAP_ENTRY))
#define MAX_AVC_LEVEL_MAP_TABLE_SIZE    	(sizeof(AvcLevelMapTable)/sizeof(MTK_VDEC_LEVEL_MAP_ENTRY))
#define MAX_MPEG4_PROFILE_MAP_TABLE_SIZE    (sizeof(MPEG4ProfileMapTable)/sizeof(MTK_VDEC_PROFILE_MAP_ENTRY))
#define MAX_MPEG4_LEVEL_MAP_TABLE_SIZE    	(sizeof(MPEG4LevelMapTable)/sizeof(MTK_VDEC_LEVEL_MAP_ENTRY))
#define MAX_H263_PROFILE_MAP_TABLE_SIZE    	(sizeof(H263ProfileMapTable)/sizeof(MTK_VDEC_PROFILE_MAP_ENTRY))
#define MAX_H263_LEVEL_MAP_TABLE_SIZE    	(sizeof(H263LevelMapTable)/sizeof(MTK_VDEC_LEVEL_MAP_ENTRY))
#define MAX_VP8_PROFILE_MAP_TABLE_SIZE    	(sizeof(VP8ProfileMapTable)/sizeof(MTK_VDEC_PROFILE_MAP_ENTRY))
#define MAX_VP8_LEVEL_MAP_TABLE_SIZE    	(sizeof(VP8LevelMapTable)/sizeof(MTK_VDEC_LEVEL_MAP_ENTRY))


MTK_VDEC_PROFILE_MAP_ENTRY AvcProfileMapTable[] =
{
    /*
        // from OMX_VIDEO_AVCPROFILETYPE
        public static final int AVCProfileBaseline = 0x01;
        public static final int AVCProfileMain     = 0x02;
        public static final int AVCProfileExtended = 0x04;
        public static final int AVCProfileHigh     = 0x08;
        public static final int AVCProfileHigh10   = 0x10;
        public static final int AVCProfileHigh422  = 0x20;
        public static final int AVCProfileHigh444  = 0x40;
    */
    {OMX_VIDEO_AVCProfileBaseline,  VDEC_DRV_H264_VIDEO_PROFILE_H264_BASELINE},
    {OMX_VIDEO_AVCProfileMain,      VDEC_DRV_H264_VIDEO_PROFILE_H264_MAIN},
    {OMX_VIDEO_AVCProfileExtended,  VDEC_DRV_H264_VIDEO_PROFILE_H264_EXTENDED},
    {OMX_VIDEO_AVCProfileHigh,      VDEC_DRV_H264_VIDEO_PROFILE_H264_HIGH},
    {OMX_VIDEO_AVCProfileHigh10,    VDEC_DRV_H264_VIDEO_PROFILE_H264_HIGH_10},
    {OMX_VIDEO_AVCProfileHigh422,   VDEC_DRV_H264_VIDEO_PROFILE_H264_HIGH422},
    {OMX_VIDEO_AVCProfileHigh444,   VDEC_DRV_H264_VIDEO_PROFILE_H264_HIGH444},
};

MTK_VDEC_LEVEL_MAP_ENTRY AvcLevelMapTable[] =
{
    /*
        // from OMX_VIDEO_AVCLEVELTYPE
        public static final int AVCLevel1       = 0x01;
        public static final int AVCLevel1b      = 0x02;
        public static final int AVCLevel11      = 0x04;
        public static final int AVCLevel12      = 0x08;
        public static final int AVCLevel13      = 0x10;
        public static final int AVCLevel2       = 0x20;
        public static final int AVCLevel21      = 0x40;
        public static final int AVCLevel22      = 0x80;
        public static final int AVCLevel3       = 0x100;
        public static final int AVCLevel31      = 0x200;
        public static final int AVCLevel32      = 0x400;
        public static final int AVCLevel4       = 0x800;
        public static final int AVCLevel41      = 0x1000;
        public static final int AVCLevel42      = 0x2000;
        public static final int AVCLevel5       = 0x4000;
        public static final int AVCLevel51      = 0x8000;
    */
    {OMX_VIDEO_AVCLevel1,   VDEC_DRV_VIDEO_LEVEL_1},
    {OMX_VIDEO_AVCLevel1b,  VDEC_DRV_VIDEO_LEVEL_1b},
    {OMX_VIDEO_AVCLevel11,  VDEC_DRV_VIDEO_LEVEL_1_1},
    {OMX_VIDEO_AVCLevel12,  VDEC_DRV_VIDEO_LEVEL_1_2},
    {OMX_VIDEO_AVCLevel13,  VDEC_DRV_VIDEO_LEVEL_1_3},
    {OMX_VIDEO_AVCLevel2,   VDEC_DRV_VIDEO_LEVEL_2},
    {OMX_VIDEO_AVCLevel21,  VDEC_DRV_VIDEO_LEVEL_2_1},
    {OMX_VIDEO_AVCLevel22,  VDEC_DRV_VIDEO_LEVEL_2_2},
    {OMX_VIDEO_AVCLevel3,   VDEC_DRV_VIDEO_LEVEL_3},
    {OMX_VIDEO_AVCLevel31,  VDEC_DRV_VIDEO_LEVEL_3_1},
    {OMX_VIDEO_AVCLevel32,  VDEC_DRV_VIDEO_LEVEL_3_2},
    {OMX_VIDEO_AVCLevel4,   VDEC_DRV_VIDEO_LEVEL_4},
    {OMX_VIDEO_AVCLevel41,  VDEC_DRV_VIDEO_LEVEL_4_1},
    {OMX_VIDEO_AVCLevel42,  VDEC_DRV_VIDEO_LEVEL_4_2},
    {OMX_VIDEO_AVCLevel5,   VDEC_DRV_VIDEO_LEVEL_5},
    {OMX_VIDEO_AVCLevel51,  VDEC_DRV_VIDEO_LEVEL_5_1},
};

MTK_VDEC_PROFILE_MAP_ENTRY H263ProfileMapTable[] =
{
    /*
        // from OMX_VIDEO_H263PROFILETYPE
        public static final int H263ProfileBaseline             = 0x01;
        public static final int H263ProfileH320Coding           = 0x02;
        public static final int H263ProfileBackwardCompatible   = 0x04;
        public static final int H263ProfileISWV2                = 0x08;
        public static final int H263ProfileISWV3                = 0x10;
        public static final int H263ProfileHighCompression      = 0x20;
        public static final int H263ProfileInternet             = 0x40;
        public static final int H263ProfileInterlace            = 0x80;
        public static final int H263ProfileHighLatency          = 0x100;
    */
    {OMX_VIDEO_H263ProfileBaseline,             VDEC_DRV_MPEG_VIDEO_PROFILE_H263_0},
    {OMX_VIDEO_H263ProfileH320Coding,           VDEC_DRV_MPEG_VIDEO_PROFILE_H263_1},
    {OMX_VIDEO_H263ProfileBackwardCompatible,   VDEC_DRV_MPEG_VIDEO_PROFILE_H263_2},
    {OMX_VIDEO_H263ProfileISWV2,                VDEC_DRV_MPEG_VIDEO_PROFILE_H263_3},
    {OMX_VIDEO_H263ProfileISWV3,                VDEC_DRV_MPEG_VIDEO_PROFILE_H263_4},
    {OMX_VIDEO_H263ProfileHighCompression,      VDEC_DRV_MPEG_VIDEO_PROFILE_H263_5},
    {OMX_VIDEO_H263ProfileInternet,             VDEC_DRV_MPEG_VIDEO_PROFILE_H263_6},
    {OMX_VIDEO_H263ProfileInterlace,            VDEC_DRV_MPEG_VIDEO_PROFILE_H263_7},
    {OMX_VIDEO_H263ProfileHighLatency,          VDEC_DRV_MPEG_VIDEO_PROFILE_H263_8},
};

MTK_VDEC_LEVEL_MAP_ENTRY H263LevelMapTable[] =
{
    /*
        // from OMX_VIDEO_H263LEVELTYPE
        public static final int H263Level10      = 0x01;
        public static final int H263Level20      = 0x02;
        public static final int H263Level30      = 0x04;
        public static final int H263Level40      = 0x08;
        public static final int H263Level45      = 0x10;
        public static final int H263Level50      = 0x20;
        public static final int H263Level60      = 0x40;
        public static final int H263Level70      = 0x80;
    */
    {OMX_VIDEO_H263Level10,    VDEC_DRV_VIDEO_LEVEL_1},
    {OMX_VIDEO_H263Level20,    VDEC_DRV_VIDEO_LEVEL_2},
    {OMX_VIDEO_H263Level30,    VDEC_DRV_VIDEO_LEVEL_2},
    {OMX_VIDEO_H263Level40,    VDEC_DRV_VIDEO_LEVEL_2},
    {OMX_VIDEO_H263Level45,    VDEC_DRV_VIDEO_LEVEL_2},
    {OMX_VIDEO_H263Level50,    VDEC_DRV_VIDEO_LEVEL_2},
    {OMX_VIDEO_H263Level60,    VDEC_DRV_VIDEO_LEVEL_3},
    {OMX_VIDEO_H263Level70,    VDEC_DRV_VIDEO_LEVEL_4},
};

MTK_VDEC_PROFILE_MAP_ENTRY MPEG4ProfileMapTable[] =
{
    /*
        // from OMX_VIDEO_MPEG4PROFILETYPE
        public static final int MPEG4ProfileSimple              = 0x01;
        public static final int MPEG4ProfileSimpleScalable      = 0x02;
        public static final int MPEG4ProfileCore                = 0x04;
        public static final int MPEG4ProfileMain                = 0x08;
        public static final int MPEG4ProfileNbit                = 0x10;
        public static final int MPEG4ProfileScalableTexture     = 0x20;
        public static final int MPEG4ProfileSimpleFace          = 0x40;
        public static final int MPEG4ProfileSimpleFBA           = 0x80;
        public static final int MPEG4ProfileBasicAnimated       = 0x100;
        public static final int MPEG4ProfileHybrid              = 0x200;
        public static final int MPEG4ProfileAdvancedRealTime    = 0x400;
        public static final int MPEG4ProfileCoreScalable        = 0x800;
        public static final int MPEG4ProfileAdvancedCoding      = 0x1000;
        public static final int MPEG4ProfileAdvancedCore        = 0x2000;
        public static final int MPEG4ProfileAdvancedScalable    = 0x4000;
        public static final int MPEG4ProfileAdvancedSimple      = 0x8000;
    */
    {OMX_VIDEO_MPEG4ProfileSimple,              VDEC_DRV_MPEG_VIDEO_PROFILE_MPEG4_SIMPLE},
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
    {OMX_VIDEO_MPEG4ProfileAdvancedSimple,      VDEC_DRV_MPEG_VIDEO_PROFILE_MPEG4_ADVANCED_SIMPLE},
};

MTK_VDEC_LEVEL_MAP_ENTRY MPEG4LevelMapTable[] =
{
    /*
        // from OMX_VIDEO_MPEG4LEVELTYPE
        public static final int MPEG4Level0      = 0x01;
        public static final int MPEG4Level0b     = 0x02;
        public static final int MPEG4Level1      = 0x04;
        public static final int MPEG4Level2      = 0x08;
        public static final int MPEG4Level3      = 0x10;
        public static final int MPEG4Level4      = 0x20;
        public static final int MPEG4Level4a     = 0x40;
        public static final int MPEG4Level5      = 0x80;
    */
    {OMX_VIDEO_MPEG4Level0,     VDEC_DRV_VIDEO_LEVEL_0},
    {OMX_VIDEO_MPEG4Level0b,    VDEC_DRV_VIDEO_LEVEL_1},
    {OMX_VIDEO_MPEG4Level1,     VDEC_DRV_VIDEO_LEVEL_1},
    {OMX_VIDEO_MPEG4Level2,     VDEC_DRV_VIDEO_LEVEL_2},
    {OMX_VIDEO_MPEG4Level3,     VDEC_DRV_VIDEO_LEVEL_3},
    {OMX_VIDEO_MPEG4Level4,     VDEC_DRV_VIDEO_LEVEL_4},
    {OMX_VIDEO_MPEG4Level4a,    VDEC_DRV_VIDEO_LEVEL_5},
    {OMX_VIDEO_MPEG4Level5,     VDEC_DRV_VIDEO_LEVEL_5},
};

MTK_VDEC_PROFILE_MAP_ENTRY VP8ProfileMapTable[] =
{
    /*
        // from OMX_VIDEO_VP8PROFILETYPE
        public static final int VP8ProfileMain = 0x01;
    */
    //{OMX_VIDEO_VP8ProfileMain,    VDEC_DRV_VIDEO_UNSUPPORTED},
};

MTK_VDEC_LEVEL_MAP_ENTRY VP8LevelMapTable[] =
{
    /*
        // from OMX_VIDEO_VP8LEVELTYPE
        public static final int VP8Level_Version0 = 0x01;
        public static final int VP8Level_Version1 = 0x02;
        public static final int VP8Level_Version2 = 0x04;
        public static final int VP8Level_Version3 = 0x08;
    */
    //{OMX_VIDEO_VP8Level_Version0,    VDEC_DRV_VIDEO_UNSUPPORTED},
    //{OMX_VIDEO_VP8Level_Version1,    VDEC_DRV_VIDEO_UNSUPPORTED},
    //{OMX_VIDEO_VP8Level_Version2,    VDEC_DRV_VIDEO_UNSUPPORTED},
    //{OMX_VIDEO_VP8Level_Version3,    VDEC_DRV_VIDEO_UNSUPPORTED},
};
#endif
OMX_ERRORTYPE MtkOmxVdec::CheckGetParamState()
{
    if (mState == OMX_StateInvalid)
    {
        return OMX_ErrorIncorrectStateOperation;
    }

    return OMX_ErrorNone;
}

void MtkOmxVdec::CheckAndEnableUFO(CheckAndEnableUFO_Scenario eScenario)
{
    VAL_BOOL_T bMJCOff = VAL_FALSE;
    char value[PROPERTY_VALUE_MAX], value2[PROPERTY_VALUE_MAX];
    property_get("sys.display.clearMotion.dimmed", value, "0");
    property_get("persist.sys.display.clearMotion", value2, "0");

    if (atoi(value) == 1 || atoi(value2) == 0)
    {
        MTK_OMX_LOGD("Enable UFO (MJC is off)(OMX_IndexParamPortDefinition)");
        bMJCOff = VAL_TRUE;
    }

    VAL_BOOL_T bUFOsupport = VAL_FALSE;
    isUFOSupported(&bUFOsupport);

    if ((mCodecId == MTK_VDEC_CODEC_ID_HEVC || mCodecId == MTK_VDEC_CODEC_ID_AVC || mCodecId == MTK_VDEC_CODEC_ID_VP9)
        && mIsUsingNativeBuffers == OMX_TRUE
        && bUFOsupport == OMX_TRUE
        && ((mOutputPortDef.format.video.nFrameWidth * mOutputPortDef.format.video.nFrameHeight > 1920 * 1088) ||
            ((mOutputPortDef.format.video.nFrameWidth * mOutputPortDef.format.video.nFrameHeight >= 1920 * 1080) && bMJCOff == VAL_TRUE && OMX_FALSE == mIsSecureInst)))
    {
        VAL_CHAR_T UFOSetting[PROPERTY_VALUE_MAX];
        OMX_BOOL UFOEnable = OMX_TRUE;
        property_get("mtk.omxvdec.ufo", UFOSetting, "1");
        UFOEnable = (OMX_BOOL) atoi(UFOSetting);
        if (UFOEnable == OMX_TRUE)
        {
            if (OMX_TRUE == mbIs10Bit)
            {
                if (OMX_TRUE == mIsHorizontalScaninLSB)
                {
                    mOutputPortFormat.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatVendorMTKYUV_UFO_10BIT_H;
                    mOutputPortDef.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatVendorMTKYUV_UFO_10BIT_H;
                }
                else
                {
                    mOutputPortFormat.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatVendorMTKYUV_UFO_10BIT_V;
                    mOutputPortDef.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatVendorMTKYUV_UFO_10BIT_V;
                }
            }
            else
            {
                mOutputPortFormat.eColorFormat = OMX_COLOR_FormatVendorMTKYUV_UFO;
                mOutputPortDef.format.video.eColorFormat = OMX_COLOR_FormatVendorMTKYUV_UFO;
            }
            // V4L2 todo: we delegate the on/off of UFO to VPUD.
            // eVDecDrvSetParam(mDrvHandle, VDEC_DRV_SET_TYPE_SET_UFO_DECODE, NULL, NULL);
            //MTK_OMX_LOGD("Enable UFO (Scenario:%d)", eScenario);
        }
    }
}

void MtkOmxVdec::SetColorFormat_YUV420Planar(OMX_PARAM_PORTDEFINITIONTYPE* pPortDef)
{
    // FIXME: reflect real codec output format
    if (mIsUsingNativeBuffers)
    {
        if (OMX_TRUE == mbYUV420FlexibleMode)
        {
            pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_I420;
            MTK_OMX_LOGE("[SetColorFormat_YUV420Planar] -> HAL_PIXEL_FORMAT_I420");
        }
        else
        {
            if (VDEC_DRV_DECODER_MTK_SOFTWARE == meDecodeType) //VPX VP9 sw
            {
                pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_YCbCr_420_888;
                MTK_OMX_LOGE("[STANDARD][HAL_PIXEL_FORMAT_I420] -> HAL_PIXEL_FORMAT_YCbCr_420_888");

                if (OMX_TRUE == mDeInterlaceEnable)
                {
                    pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_YUV_PRIVATE;
                    MTK_OMX_LOGD("[MJC][HAL_PIXEL_FORMAT_I420] -> HAL_PIXEL_FORMAT_YUV_PRIVATE");
                }
            }
            else
            {
                // Should not be here! MTK HW codec only output MTK YUV
                pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_NV12_BLK;

                if (OMX_TRUE == mDeInterlaceEnable/*||( OMX_TRUE == mUsePPFW )*/)
                {
                    pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_YUV_PRIVATE;
                    MTK_OMX_LOGD("[MJC][HAL_PIXEL_FORMAT_NV12_BLK] -> HAL_PIXEL_FORMAT_YUV_PRIVATE (%d)", __LINE__);
                }
                else if (mQInfoOut.u4StrideAlign > 16 || mQInfoOut.u4SliceHeightAlign > 16)
                {
                    pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_YCbCr_420_888;
                    MTK_OMX_LOGE("[UNIQUE][HAL_PIXEL_FORMAT_I420] -> HAL_PIXEL_FORMAT_YCbCr_420_888");
                }
            }
        }
    }
    else
    {
        pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Planar;
        MTK_OMX_LOGE("[SetColorFormat_YUV420Planar] -> OMX_COLOR_FormatYUV420Planar");
    }
    MTK_OMX_LOGD("SetColorFormat_YUV420Planar  %d", pPortDef->format.video.eColorFormat);
}

void MtkOmxVdec::SetColorFormat_MTKYUV(OMX_PARAM_PORTDEFINITIONTYPE *pPortDef)
{
    // FIXME: reflect real codec output format
    if (mIsUsingNativeBuffers)
    {
        if (OMX_TRUE == mbYUV420FlexibleMode)
        {
            pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_I420;
        }
        else
        {

            if (OMX_TRUE == mbIs10Bit && OMX_TRUE == mIsHorizontalScaninLSB)
            {
                pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_NV12_BLK_10BIT_H;
            }
            else if (OMX_TRUE == mbIs10Bit && OMX_FALSE == mIsHorizontalScaninLSB)
            {
                pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_NV12_BLK_10BIT_V;
            }
            else
            {
                pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_NV12_BLK;
            }

            if (OMX_TRUE == mDeInterlaceEnable)
            {
                MtkVideoAllocateSecureFrameBuffer_Ptr *pfnMtkVideoAllocateSecureFrameBuffer_Ptr = (MtkVideoAllocateSecureFrameBuffer_Ptr *) dlsym(mH264SecVdecInHouseLib, MTK_H264_SEC_VDEC_IN_HOUSE_ALLOCATE_SECURE_FRAME_BUFFER);
                if (NULL != pfnMtkVideoAllocateSecureFrameBuffer_Ptr)
                {
                }
                else
                {
                    if (OMX_TRUE == mbIs10Bit)
                    {
                        pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_YUV_PRIVATE_10BIT;
                        MTK_OMX_LOGD("[MJC][HAL_PIXEL_FORMAT_NV12_BLK] -> HAL_PIXEL_FORMAT_YUV_PRIVATE_10BIT");
                    }
                    else
                    {
                        pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_YUV_PRIVATE;
                        MTK_OMX_LOGD("[MJC][HAL_PIXEL_FORMAT_NV12_BLK] -> HAL_PIXEL_FORMAT_YUV_PRIVATE");
                    }
                }
            }
        }
    }
    else if (OMX_TRUE == mThumbnailMode)
    {
        pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Planar;
    }
    else
    {
        pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Planar;
        pPortDef->format.video.nStride = pPortDef->format.video.nFrameWidth = mCropWidth;
        pPortDef->format.video.nSliceHeight = pPortDef->format.video.nFrameHeight = mCropHeight;
        MTK_OMX_LOGD("UsingConverted, FrameWidth(%d), FrameHeight(%d)", pPortDef->format.video.nFrameWidth, pPortDef->format.video.nFrameHeight);
    }
}

void MtkOmxVdec::SetColorFormat_MTKYUV_FCM(OMX_PARAM_PORTDEFINITIONTYPE *pPortDef)
{
    // FIXME: reflect real codec output format
    if (mIsUsingNativeBuffers)
    {
        pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_NV12_BLK_FCM;
    }
    else
    {
        pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Planar;
    }
}

void MtkOmxVdec::SetColorFormat_MTKYUV_UFO(OMX_PARAM_PORTDEFINITIONTYPE *pPortDef)
{
    if (mIsUsingNativeBuffers)
    {
        pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_UFO;
    }
    else
    {
        pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatVendorMTKYUV_UFO;
    }
}

void MtkOmxVdec::SetColorFormat_MTKYUV_UFO_10BIT_H(OMX_PARAM_PORTDEFINITIONTYPE *pPortDef)
{
    if (mIsUsingNativeBuffers)
    {
        pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_UFO_10BIT_H;
    }
    else
    {
        pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatVendorMTKYUV_UFO_10BIT_H;
    }
}

void MtkOmxVdec::SetColorFormat_MTKYUV_UFO_10BIT_V(OMX_PARAM_PORTDEFINITIONTYPE *pPortDef)
{
    if (mIsUsingNativeBuffers)
    {
        pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_UFO_10BIT_V;
    }
    else
    {
        pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatVendorMTKYUV_UFO_10BIT_V;
    }
}

void MtkOmxVdec::SetColorFormat_YV12(OMX_PARAM_PORTDEFINITIONTYPE *pPortDef)
{
    // FIXME: reflect real codec output format
    if (mIsUsingNativeBuffers)
    {
        if (OMX_TRUE == mbYUV420FlexibleMode)
        {
            pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_I420;
            MTK_OMX_LOGD("[HAL_PIXEL_FORMAT_YV12] -> HAL_PIXEL_FORMAT_I420");
        }
        else //TBD
        {
            // Video codec requires 16 align for vertical stride, not standard YV12
            pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_YCbCr_420_888;
            MTK_OMX_LOGD("[STANDARD][HAL_PIXEL_FORMAT_YV12] -> HAL_PIXEL_FORMAT_YCbCr_420_888");
            if ((OMX_TRUE == mDeInterlaceEnable)/*||( OMX_TRUE == mUsePPFW )*/)
            {
                pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_YUV_PRIVATE;
                MTK_OMX_LOGD("[MJC][HAL_PIXEL_FORMAT_YV12] -> HAL_PIXEL_FORMAT_YUV_PRIVATE");
            }
        }
    }
    else
    {
        pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Planar;
        pPortDef->format.video.nStride = pPortDef->format.video.nFrameWidth;
        pPortDef->format.video.nSliceHeight = pPortDef->format.video.nFrameHeight;
        MTK_OMX_LOGD("UsingConverted, FrameWidth(%d), FrameHeight(%d)", pPortDef->format.video.nFrameWidth, pPortDef->format.video.nFrameHeight);
    }
    MTK_OMX_LOGD("SetColorFormat_YV12  %d", pPortDef->format.video.eColorFormat);
}

void MtkOmxVdec::SetColorFormat(OMX_PARAM_PORTDEFINITIONTYPE* pPortDef)
{
    //getParameter should get from component, should not check pPortDef and re-assign it.
    //so remove the pPortDef->format.video.eColorFormat check

    switch (mOutputPortFormat.eColorFormat)
    {
        case OMX_COLOR_FormatYUV420Planar:
        {
            SetColorFormat_YUV420Planar(pPortDef);
        }
        break;

        case OMX_COLOR_FormatVendorMTKYUV:
        {
            SetColorFormat_MTKYUV(pPortDef);
        }
        break;

        case OMX_COLOR_FormatVendorMTKYUV_FCM:
        {
            SetColorFormat_MTKYUV_FCM(pPortDef);
        }
        break;

        case OMX_COLOR_FormatVendorMTKYUV_UFO:
        {
            SetColorFormat_MTKYUV_UFO(pPortDef);
        }
        break;

        case OMX_COLOR_FormatVendorMTKYUV_UFO_10BIT_H:
        {
            SetColorFormat_MTKYUV_UFO_10BIT_H(pPortDef);
        }
        break;

        case OMX_COLOR_FormatVendorMTKYUV_UFO_10BIT_V:
        {
            SetColorFormat_MTKYUV_UFO_10BIT_V(pPortDef);
        }
        break;

        case OMX_COLOR_FormatYUV420SemiPlanar:// for NV12
        case OMX_MTK_COLOR_FormatYV12:
        {
            SetColorFormat_YV12(pPortDef);
        }
        break;

        case OMX_COLOR_Format32bitARGB8888:
        {
            pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_RGBA_8888;
        }
        break;

        default:
        {
            pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_I420;
        }
        break;
    }
}

void MtkOmxVdec::AlignBufferSize(OMX_PTR pCompParam)
{
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *) pCompParam;

    // Use alignment of 32x32
    unsigned int srcWStride;
    unsigned int srcHStride;
    VDEC_DRV_QUERY_VIDEO_FORMAT_T qinfoOut;
    MTK_OMX_MEMSET(&qinfoOut, 0, sizeof(VDEC_DRV_QUERY_VIDEO_FORMAT_T));

    QueryDriverFormat(&qinfoOut);
    srcWStride = VDEC_ROUND_N(mOutputPortDef.format.video.nFrameWidth, qinfoOut.u4StrideAlign);
    srcHStride = VDEC_ROUND_N(mOutputPortDef.format.video.nFrameHeight, qinfoOut.u4SliceHeightAlign);

    if (HAL_PIXEL_FORMAT_RGBA_8888 == pPortDef->format.video.eColorFormat)
    {
        //for MJPEG, output is RGBA
        ((OMX_PARAM_PORTDEFINITIONTYPE *)pCompParam)->nBufferSize = (VDEC_ROUND_32(srcWStride) * VDEC_ROUND_32(srcHStride)) << 2;
    }
    else
    {
        if (meDecodeType == VDEC_DRV_DECODER_MTK_SOFTWARE)
        {
            ((OMX_PARAM_PORTDEFINITIONTYPE *)pCompParam)->nBufferSize = (VDEC_ROUND_32(srcWStride) * (VDEC_ROUND_32(srcHStride) + 1) * 3 >> 1);// + 16;
        }
        else
        {
            if (OMX_TRUE == mbIs10Bit)
            {
                ((OMX_PARAM_PORTDEFINITIONTYPE *)pCompParam)->nBufferSize = ((VDEC_ROUND_32(srcWStride) * VDEC_ROUND_32(srcHStride) * 3 >> 1) + 16) * 1.25;
            }
            else
            {
                ((OMX_PARAM_PORTDEFINITIONTYPE *)pCompParam)->nBufferSize = (VDEC_ROUND_32(srcWStride) * VDEC_ROUND_32(srcHStride) * 3 >> 1) + 16;
            }
        }
    }

    { //for SW decoder
        //OMX_U32 totalOutputBufCnt = mOutputPortDef.nBufferCountMin;
        //eVDecDrvSetParam(mDrvHandle, VDEC_DRV_SET_TYPE_SET_TOTAL_OUTPUT_BUF_SIZE, &totalOutputBufCnt, NULL);
    }

    MTK_OMX_LOGD("32x32 Aligned! mOutputPortDef.nBufferSize(%d), nStride(%d), nSliceHeight(%d) nBufferCountActual(%d)",
                 ((OMX_PARAM_PORTDEFINITIONTYPE *)pCompParam)->nBufferSize, VDEC_ROUND_32(srcWStride), VDEC_ROUND_32(srcHStride), mOutputPortDef.nBufferCountActual);

}

void MtkOmxVdec::GetOutputPortDefinition(OMX_PTR pCompParam)
{
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *) pCompParam;

    if (mMinUndequeuedBufsFlag == OMX_TRUE)
    {
        if (mOutputBufferPopulatedCnt == 0)
        {
            mOutputPortDef.nBufferCountActual += mMinUndequeuedBufsDiff;
        }
        else
        {
            mMinUndequeuedBufsDiff = 0;
        }
        MTK_OMX_LOGD("nBufferCountActual %d (%i)", mOutputPortDef.nBufferCountActual, mMinUndequeuedBufsDiff);
        mMinUndequeuedBufsFlag = OMX_FALSE;
    }

    CheckAndEnableUFO(CheckAndEnableUFO_PortDefinition);

    if (mForceOutputBufferCount != 0)
    {
        mOutputPortDef.nBufferCountActual = mForceOutputBufferCount;
		MTK_OMX_LOGD("(%s)@(%d), mOutputPortDef.nBufferCountActual(%d), mOutputPortDef.nBufferCountMin(%d)", __FUNCTION__, __LINE__, mOutputPortDef.nBufferCountActual, mOutputPortDef.nBufferCountMin);
    }

    memcpy(pPortDef, &mOutputPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));

    MTK_OMX_LOGE("mOutputPortDef eColorFormat(%x), eColorFormat(%x), meDecodeType(%x), mForceOutputBufferCount(%d), mIsUsingNativeBuffers(%d)", mOutputPortDef.format.video.eColorFormat, mOutputPortFormat.eColorFormat, meDecodeType, mForceOutputBufferCount, mIsUsingNativeBuffers);


    MTK_OMX_LOGD("--- GetOutputPortDefinition --- (%d %d %d %d %d %d)",
                     mOutputPortDef.format.video.nFrameWidth, mOutputPortDef.format.video.nFrameHeight,
                     mOutputPortDef.format.video.nStride, mOutputPortDef.format.video.nSliceHeight,
                     mOutputPortDef.nBufferCountActual, mOutputPortDef.nBufferSize);



    SetColorFormat(pPortDef);

    AlignBufferSize(pPortDef);
}

OMX_ERRORTYPE MtkOmxVdec::GetOutputPortFormat(OMX_PTR pCompParam)
{
    OMX_VIDEO_PARAM_PORTFORMATTYPE *pPortFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pCompParam;

    if (pPortFormat->nIndex == 0)
    {
        CheckAndEnableUFO(CheckAndEnableUFO_PortFormat);

        pPortFormat->eColorFormat = mOutputPortFormat.eColorFormat;
        pPortFormat->eCompressionFormat =  mOutputPortFormat.eCompressionFormat; // must be OMX_VIDEO_CodingUnused;
    }
    //for CTS case "com.android.cts.videoperf.VideoEncoderDecoderTest "
    //push one more format(YUV420) if there is under the decoding and with MTKBLK or MTKYV12 format
    else if (pPortFormat->nIndex == 1)
    {
        pPortFormat->eColorFormat = OMX_COLOR_FormatYUV420Planar;//mOutputPortFormat.eColorFormat;
        pPortFormat->eCompressionFormat =  mOutputPortFormat.eCompressionFormat; // must be OMX_VIDEO_CodingUnused;
    }
    else
    {
        return OMX_ErrorNoMore;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MtkOmxVdec::HandleGetPortDefinition(OMX_PTR pCompParam)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;

    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *)pCompParam;

    if (pPortDef->nPortIndex == mInputPortDef.nPortIndex)
    {
        MTK_OMX_LOGD("[INPUT] (%d)(%d)(%d)", mInputPortDef.format.video.nFrameWidth, mInputPortDef.format.video.nFrameHeight, mInputPortDef.nBufferSize);
        memcpy(pCompParam, &mInputPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
    }
    else if (pPortDef->nPortIndex == mOutputPortDef.nPortIndex)
    {
        GetOutputPortDefinition(pCompParam);
    }
    else
    {
        err = OMX_ErrorBadPortIndex;
    }

    return err;
}

OMX_ERRORTYPE MtkOmxVdec::HandleGetPortFormat(OMX_PTR pCompParam)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;

    OMX_VIDEO_PARAM_PORTFORMATTYPE *pPortFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pCompParam;

    if (pPortFormat->nPortIndex == mInputPortFormat.nPortIndex)
    {
        if (pPortFormat->nIndex == 0)   // note: each component only plays one role and support only one format on each input port
        {
            pPortFormat->eColorFormat =  OMX_COLOR_FormatUnused;
            pPortFormat->eCompressionFormat = mInputPortFormat.eCompressionFormat;
        }
        else
        {
            err = OMX_ErrorNoMore;
        }
    }
    else if (pPortFormat->nPortIndex == mOutputPortFormat.nPortIndex)
    {
        err = GetOutputPortFormat(pCompParam);
    }
    else
    {
        err = OMX_ErrorBadPortIndex;
    }

    return err;
}

OMX_ERRORTYPE MtkOmxVdec::HandleGetVideoProfileLevelQuerySupported(OMX_PTR pCompParam)
{
    OMX_VIDEO_PARAM_PROFILELEVELTYPE *pProfileLevel = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pCompParam;
    OMX_ERRORTYPE err = OMX_ErrorNone;
#if 1
    if (pProfileLevel->nPortIndex == mInputPortFormat.nPortIndex)
    {
        switch (mInputPortFormat.eCompressionFormat)
        {
            case OMX_VIDEO_CodingHEVC:
            {
                err = QureyVideoProfileLevel(VDEC_DRV_VIDEO_FORMAT_H265, pProfileLevel,
                HevcProfileMapTable, MAX_HEVC_PROFILE_MAP_TABLE_SIZE,
                HevcLevelMapTable, MAX_HEVC_LEVEL_MAP_TABLE_SIZE);
            }
            break;

            case OMX_VIDEO_CodingAVC:
            {
                err = QureyVideoProfileLevel(VDEC_DRV_VIDEO_FORMAT_H264, pProfileLevel,
                                             AvcProfileMapTable, MAX_AVC_PROFILE_MAP_TABLE_SIZE,
                                             AvcLevelMapTable, MAX_AVC_LEVEL_MAP_TABLE_SIZE);
            }
            break;

            case OMX_VIDEO_CodingDIVX:
            case OMX_VIDEO_CodingDIVX3:
            case OMX_VIDEO_CodingXVID:
            case OMX_VIDEO_CodingS263:
            case OMX_VIDEO_CodingMPEG4:
            {
                err = QureyVideoProfileLevel(VDEC_DRV_VIDEO_FORMAT_MPEG4, pProfileLevel,
                                             MPEG4ProfileMapTable, MAX_MPEG4_PROFILE_MAP_TABLE_SIZE,
                                             MPEG4LevelMapTable, MAX_MPEG4_LEVEL_MAP_TABLE_SIZE);
            }
            break;

            case OMX_VIDEO_CodingH263:
            {
                err = QureyVideoProfileLevel(VDEC_DRV_VIDEO_FORMAT_H263, pProfileLevel,
                                             H263ProfileMapTable, MAX_H263_PROFILE_MAP_TABLE_SIZE,
                                             H263LevelMapTable, MAX_H263_LEVEL_MAP_TABLE_SIZE);
            }
            break;

            case OMX_VIDEO_CodingVP8:
            {
                err = QureyVideoProfileLevel(VDEC_DRV_VIDEO_FORMAT_VP8, pProfileLevel,
                                             VP8ProfileMapTable, MAX_VP8_PROFILE_MAP_TABLE_SIZE,
                                             VP8LevelMapTable, MAX_VP8_LEVEL_MAP_TABLE_SIZE);
            }
            break;

            default:
            {
                err = OMX_ErrorBadParameter;
            }
            break;
        }
    }
    else
    {
        err = OMX_ErrorBadPortIndex;
    }
#endif
    return err;
}

OMX_ERRORTYPE MtkOmxVdec::HandleGetVdecVideoSpecQuerySupported(OMX_PTR pCompParam)
{
    OMX_VIDEO_PARAM_SPEC_QUERY *pSpecQuery = (OMX_VIDEO_PARAM_SPEC_QUERY *)pCompParam;

    pSpecQuery->bSupported = OMX_FALSE;
    switch (mInputPortFormat.eCompressionFormat)
    {
        case OMX_VIDEO_CodingAVC:
        {
            if (pSpecQuery->nFrameWidth > 1280 || pSpecQuery->nFrameHeight > 720)
            {
                pSpecQuery->bSupported = OMX_FALSE;
                break;
            }
            if (pSpecQuery->profile == OMX_VIDEO_AVCProfileBaseline)
            {
                if ((pSpecQuery->level <= OMX_VIDEO_AVCLevel31) && (pSpecQuery->nFps <= 30))
                {
                    pSpecQuery->bSupported = OMX_TRUE;
                }
            }
            else if (pSpecQuery->profile == OMX_VIDEO_AVCProfileMain)
            {
                if ((pSpecQuery->level <= OMX_VIDEO_AVCLevel31) && (pSpecQuery->nFps <= 25))
                {
                    pSpecQuery->bSupported = OMX_TRUE;
                }
            }
            else if (pSpecQuery->profile == OMX_VIDEO_AVCProfileHigh)
            {
                if ((pSpecQuery->level <= OMX_VIDEO_AVCLevel31) && (pSpecQuery->nFps <= 25))
                {
                    pSpecQuery->bSupported = OMX_TRUE;
                }
            }
            else
            {
                pSpecQuery->bSupported = OMX_FALSE;
            }
        }
        break;

        case OMX_VIDEO_CodingDIVX:
        case OMX_VIDEO_CodingDIVX3:
        case OMX_VIDEO_CodingXVID:
        case OMX_VIDEO_CodingS263:
        case OMX_VIDEO_CodingMPEG4:
        {
            if (pSpecQuery->nFrameWidth > 1280 || pSpecQuery->nFrameHeight > 720)
            {
                pSpecQuery->bSupported = OMX_FALSE;
                break;
            }
            if (pSpecQuery->profile == OMX_VIDEO_MPEG4ProfileSimple)
            {
                if ((pSpecQuery->level <= OMX_VIDEO_MPEG4Level5) && (pSpecQuery->nFps <= 30))
                {
                    pSpecQuery->bSupported = OMX_TRUE;
                }
            }
            else if (pSpecQuery->profile == OMX_VIDEO_MPEG4ProfileAdvancedSimple)
            {
                if ((pSpecQuery->level <= OMX_VIDEO_MPEG4Level5) && (pSpecQuery->nFps <= 30))
                {
                    pSpecQuery->bSupported = OMX_TRUE;
                }
            }
            else
            {
                pSpecQuery->bSupported = OMX_FALSE;
            }
        }
        break;

        case OMX_VIDEO_CodingH263:
        {
            if (pSpecQuery->nFrameWidth > 1280 || pSpecQuery->nFrameHeight > 720)
            {
                pSpecQuery->bSupported = OMX_FALSE;
                break;
            }
            if (pSpecQuery->profile == OMX_VIDEO_H263ProfileBaseline)
            {
                if ((pSpecQuery->level <= OMX_VIDEO_H263Level70) && (pSpecQuery->nFps <= 30))
                {
                    pSpecQuery->bSupported = OMX_TRUE;
                }
            }
            else
            {
                pSpecQuery->bSupported = OMX_FALSE;
            }
        }
        break;

        case OMX_VIDEO_CodingVP8:
        {
            if (pSpecQuery->nFrameWidth > 1280 || pSpecQuery->nFrameHeight > 720)
            {
                pSpecQuery->bSupported = OMX_FALSE;
            }
            if (pSpecQuery->nFps > 30)
            {
                pSpecQuery->bSupported = OMX_FALSE;
            }
        }
        break;

        case OMX_VIDEO_CodingHEVC:
        case OMX_VIDEO_CodingRV:
        case OMX_VIDEO_CodingWMV:
        case OMX_VIDEO_CodingMPEG2:
        default:
            break;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MtkOmxVdec::CheckICSLaterGetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pCompParam)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;

    //MTK_OMX_LOGD("+CheckICSLaterGetParameter");

#if (ANDROID_VER >= ANDROID_ICS)
    switch(nParamIndex)
    {
        case OMX_GoogleAndroidIndexGetAndroidNativeBufferUsage:
        {
            GetAndroidNativeBufferUsageParams *pNativeBuffersUsage = (GetAndroidNativeBufferUsageParams *)pCompParam;
            if (pNativeBuffersUsage->nPortIndex == mOutputPortFormat.nPortIndex)
            {
#if 0//def MTK S3D SUPPORT
                switch (m3DStereoMode)
                {
                    case OMX_VIDEO_H264FPA_SIDEBYSIDE:
                        pNativeBuffersUsage->nUsage = GRALLOC_USAGE_S3D_SIDE_BY_SIDE;
                        break;
                    case OMX_VIDEO_H264FPA_TOPANDBOTTOM:
                        pNativeBuffersUsage->nUsage = GRALLOC_USAGE_S3D_TOP_AND_BOTTOM;
                        break;
                    default:
                        pNativeBuffersUsage->nUsage = 0;
                        break;
                }
#else
                pNativeBuffersUsage->nUsage = (GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN);
                if(OMX_TRUE == mIsSecureInst)
                {
                    pNativeBuffersUsage->nUsage |= GRALLOC_USAGE_SECURE;
                    MTK_OMX_LOGD("OMX_GoogleAndroidIndexGetAndroidNativeBufferUsage %d, %x", mIsSecureInst, pNativeBuffersUsage->nUsage);
                }
#endif
            }
            else
            {
                err = OMX_ErrorBadParameter;
            }
        }
        break;

        default:
            return OMX_ErrorNotImplemented;
    }
#endif

    return err;
}

OMX_ERRORTYPE MtkOmxVdec::HandleGetDescribeColorFormat(OMX_PTR pCompParam)
{
    DescribeColorFormatParams *describeParams = (DescribeColorFormatParams *)pCompParam;
    OMX_ERRORTYPE err = OMX_ErrorNone;

    //OMX_COLOR_FORMATTYPE *colorFormat = (OMX_COLOR_FORMATTYPE *)pCompParam;
    //*colorFormat = describeParams.eColorFormat;
    MTK_OMX_LOGD("colorFormat %lx, bUsingNativeBuffers %d, mbYUV420FlexibleMode %d", describeParams->eColorFormat,
    describeParams->bUsingNativeBuffers, mbYUV420FlexibleMode);

    if ((OMX_TRUE == describeParams->bUsingNativeBuffers) &&
        ((OMX_COLOR_FormatYUV420Planar == describeParams->eColorFormat) || (HAL_PIXEL_FORMAT_I420 == describeParams->eColorFormat) ||
        (HAL_PIXEL_FORMAT_YCbCr_420_888 == describeParams->eColorFormat) || (OMX_COLOR_FormatYUV420Flexible == describeParams->eColorFormat)))
    {
        bool err_return = 0;
        mbYUV420FlexibleMode = OMX_TRUE;
        err_return = DescribeFlexibleColorFormat((DescribeColorFormatParams *)describeParams);

        MTK_OMX_LOGD("client query OMX_COLOR_FormatYUV420Flexible mbYUV420FlexibleMode %d, ret: %d", mbYUV420FlexibleMode, err_return);
    }
    else
    {
        //treat the framework to push YUVFlexible format in codeccodec::queryCodecs()
        err = OMX_ErrorUnsupportedIndex;
    }

    return err;
}

void MtkOmxVdec::HandleAllInit(OMX_INDEXTYPE nParamIndex, OMX_PTR pCompParam)
{
    OMX_PORT_PARAM_TYPE *pPortParam = (OMX_PORT_PARAM_TYPE *)pCompParam;
    pPortParam->nSize = sizeof(OMX_PORT_PARAM_TYPE);

    switch (nParamIndex)
    {
        case OMX_IndexParamVideoInit:
        {
            pPortParam->nStartPortNumber = MTK_OMX_INPUT_PORT;
            pPortParam->nPorts = 2;
        }
        break;

        case OMX_IndexParamAudioInit:
        case OMX_IndexParamImageInit:
        case OMX_IndexParamOtherInit:
        {
            pPortParam->nStartPortNumber = 0;
            pPortParam->nPorts = 0;
        }
        break;

        default:
        break;
    }
}


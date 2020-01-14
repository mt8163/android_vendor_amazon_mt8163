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

#if 1 // for VAL_CHIP_NAME_MT6755 || VAL_CHIP_NAME_DENALI_3
#include <linux/svp_region.h>
#endif


#include <vector>
#include <algorithm>


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
extern int64_t getTickCountMs();

#if (ANDROID_VER >= ANDROID_ICS)
#include <android/native_window.h>
#include <HardwareAPI.h>
//#include <gralloc_priv.h>
#include <ui/GraphicBufferMapper.h>
#include <ui/gralloc_extra.h>
#include <ion.h>
#include "graphics_mtk_defs.h"
#include <poll.h>
#endif

#include <errno.h>

const VAL_UINT32_T crc32_table[256] =
{
    0x0,        0x4C11DB7,  0x9823B6E,  0xD4326D9,  0x130476DC, 0x17C56B6B, 0x1A864DB2, 0x1E475005,
    0x2608EDB8, 0x22C9F00F, 0x2F8AD6D6, 0x2B4BCB61, 0x350C9B64, 0x31CD86D3, 0x3C8EA00A, 0x384FBDBD,
    0x4C11DB70, 0x48D0C6C7, 0x4593E01E, 0x4152FDA9, 0x5F15ADAC, 0x5BD4B01B, 0x569796C2, 0x52568B75,
    0x6A1936C8, 0x6ED82B7F, 0x639B0DA6, 0x675A1011, 0x791D4014, 0x7DDC5DA3, 0x709F7B7A, 0x745E66CD,
    0x9823B6E0, 0x9CE2AB57, 0x91A18D8E, 0x95609039, 0x8B27C03C, 0x8FE6DD8B, 0x82A5FB52, 0x8664E6E5,
    0xBE2B5B58, 0xBAEA46EF, 0xB7A96036, 0xB3687D81, 0xAD2F2D84, 0xA9EE3033, 0xA4AD16EA, 0xA06C0B5D,
    0xD4326D90, 0xD0F37027, 0xDDB056FE, 0xD9714B49, 0xC7361B4C, 0xC3F706FB, 0xCEB42022, 0xCA753D95,
    0xF23A8028, 0xF6FB9D9F, 0xFBB8BB46, 0xFF79A6F1, 0xE13EF6F4, 0xE5FFEB43, 0xE8BCCD9A, 0xEC7DD02D,
    0x34867077, 0x30476DC0, 0x3D044B19, 0x39C556AE, 0x278206AB, 0x23431B1C, 0x2E003DC5, 0x2AC12072,
    0x128E9DCF, 0x164F8078, 0x1B0CA6A1, 0x1FCDBB16, 0x18AEB13,  0x54BF6A4,  0x808D07D,  0xCC9CDCA,
    0x7897AB07, 0x7C56B6B0, 0x71159069, 0x75D48DDE, 0x6B93DDDB, 0x6F52C06C, 0x6211E6B5, 0x66D0FB02,
    0x5E9F46BF, 0x5A5E5B08, 0x571D7DD1, 0x53DC6066, 0x4D9B3063, 0x495A2DD4, 0x44190B0D, 0x40D816BA,
    0xACA5C697, 0xA864DB20, 0xA527FDF9, 0xA1E6E04E, 0xBFA1B04B, 0xBB60ADFC, 0xB6238B25, 0xB2E29692,
    0x8AAD2B2F, 0x8E6C3698, 0x832F1041, 0x87EE0DF6, 0x99A95DF3, 0x9D684044, 0x902B669D, 0x94EA7B2A,
    0xE0B41DE7, 0xE4750050, 0xE9362689, 0xEDF73B3E, 0xF3B06B3B, 0xF771768C, 0xFA325055, 0xFEF34DE2,
    0xC6BCF05F, 0xC27DEDE8, 0xCF3ECB31, 0xCBFFD686, 0xD5B88683, 0xD1799B34, 0xDC3ABDED, 0xD8FBA05A,
    0x690CE0EE, 0x6DCDFD59, 0x608EDB80, 0x644FC637, 0x7A089632, 0x7EC98B85, 0x738AAD5C, 0x774BB0EB,
    0x4F040D56, 0x4BC510E1, 0x46863638, 0x42472B8F, 0x5C007B8A, 0x58C1663D, 0x558240E4, 0x51435D53,
    0x251D3B9E, 0x21DC2629, 0x2C9F00F0, 0x285E1D47, 0x36194D42, 0x32D850F5, 0x3F9B762C, 0x3B5A6B9B,
    0x315D626,  0x7D4CB91,  0xA97ED48,  0xE56F0FF,  0x1011A0FA, 0x14D0BD4D, 0x19939B94, 0x1D528623,
    0xF12F560E, 0xF5EE4BB9, 0xF8AD6D60, 0xFC6C70D7, 0xE22B20D2, 0xE6EA3D65, 0xEBA91BBC, 0xEF68060B,
    0xD727BBB6, 0xD3E6A601, 0xDEA580D8, 0xDA649D6F, 0xC423CD6A, 0xC0E2D0DD, 0xCDA1F604, 0xC960EBB3,
    0xBD3E8D7E, 0xB9FF90C9, 0xB4BCB610, 0xB07DABA7, 0xAE3AFBA2, 0xAAFBE615, 0xA7B8C0CC, 0xA379DD7B,
    0x9B3660C6, 0x9FF77D71, 0x92B45BA8, 0x9675461F, 0x8832161A, 0x8CF30BAD, 0x81B02D74, 0x857130C3,
    0x5D8A9099, 0x594B8D2E, 0x5408ABF7, 0x50C9B640, 0x4E8EE645, 0x4A4FFBF2, 0x470CDD2B, 0x43CDC09C,
    0x7B827D21, 0x7F436096, 0x7200464F, 0x76C15BF8, 0x68860BFD, 0x6C47164A, 0x61043093, 0x65C52D24,
    0x119B4BE9, 0x155A565E, 0x18197087, 0x1CD86D30, 0x29F3D35,  0x65E2082,  0xB1D065B,  0xFDC1BEC,
    0x3793A651, 0x3352BBE6, 0x3E119D3F, 0x3AD08088, 0x2497D08D, 0x2056CD3A, 0x2D15EBE3, 0x29D4F654,
    0xC5A92679, 0xC1683BCE, 0xCC2B1D17, 0xC8EA00A0, 0xD6AD50A5, 0xD26C4D12, 0xDF2F6BCB, 0xDBEE767C,
    0xE3A1CBC1, 0xE760D676, 0xEA23F0AF, 0xEEE2ED18, 0xF0A5BD1D, 0xF464A0AA, 0xF9278673, 0xFDE69BC4,
    0x89B8FD09, 0x8D79E0BE, 0x803AC667, 0x84FBDBD0, 0x9ABC8BD5, 0x9E7D9662, 0x933EB0BB, 0x97FFAD0C,
    0xAFB010B1, 0xAB710D06, 0xA6322BDF, 0xA2F33668, 0xBCB4666D, 0xB8757BDA, 0xB5365D03, 0xB1F740B4
};  // Table of 8-bit remainders

unsigned int _calculate_crc32(unsigned char *input_buffer, unsigned int size)
{
    unsigned int crc_accum = 0;

    while (size-- > 0)
    {
        crc_accum = (crc_accum << 8) ^
                    crc32_table[(unsigned char)(crc_accum >> 24) ^ (*input_buffer++)];
    }

    return crc_accum;
}

// Get DMA FD
OMX_BOOL MtkOmxVdec::getIonFdByHeaderIndex(int inputIndex, int *inputIonFd, int outputIndex, int *outputIonFd)
{
    VBufInfo  info;
    //MTK_OMX_LOGE("getIonFdByHeaderIndex,line %d\n", __LINE__);

    // Get bitstream fd
    if (-1 != inputIndex && NULL != inputIonFd)
    {
        int ret = mInputMVAMgr->getOmxInfoFromVA(mInputBufferHdrs[inputIndex]->pBuffer , &info);
        if (ret < 0)
        {
            MTK_OMX_LOGE("[ERROR]][Inputput][VideoDecode]Can't find Frm in MVAMgr,line %d\n", __LINE__);
            goto GetIonFd_FAIL;
        }
        else
        {
            *inputIonFd = info.iIonFd;
        }
    }

    // Get Frame buffer fd
    if (-1 != outputIndex && NULL != outputIonFd)
    {
        //int ret = mOutputMVAMgr->getOmxInfoFromVA(mOutputBufferHdrs[outputIndex]->pBuffer, &info);

        int ret = -1;
        if (OMX_TRUE == mStoreMetaDataInBuffers)
        {
            OMX_U32 graphicBufHandle = 0;
            GetMetaHandleFromOmxHeader(mOutputBufferHdrs[outputIndex], &graphicBufHandle);
            ret = mOutputMVAMgr->getOmxInfoFromHndl((void *)graphicBufHandle, &info);
            MTK_OMX_LOGD("mOutputBufferHdrs[outputIndex]->pBuffer:0x%08x, info.iIonFd:%d,line %d\n", mOutputBufferHdrs[outputIndex]->pBuffer, info.iIonFd, __LINE__);
        }
        else
        {
            ret = mOutputMVAMgr->getOmxInfoFromVA(mOutputBufferHdrs[outputIndex]->pBuffer, &info);
            MTK_OMX_LOGD("mOutputBufferHdrs[outputIndex]->pBuffer:0x%08x, info.iIonFd:%d", mOutputBufferHdrs[outputIndex]->pBuffer, info.iIonFd);
        }

        if (ret < 0)
        {
            MTK_OMX_LOGE("[ERROR]][outputput][VideoDecode]Can't find Frm in MVAMgr,line %d\n", __LINE__);
            goto GetIonFd_FAIL;
        }
        else
        {
            *outputIonFd = info.iIonFd;
        }
    }

    return OMX_TRUE;

GetIonFd_FAIL:

    return OMX_FALSE;
}


VAL_UINT32_T MtkOmxVdec::OMXGetOutputBufferFd(P_VDEC_DRV_FRAMEBUF_T *pFrame, int *frameBufferIndex, int *ionFd, VAL_UINT32_T u4Flag, VAL_BOOL_T bNewInterface, VAL_VOID_T *pExtra)
{
#if 1
    if (mInputFlushALL == OMX_TRUE)
    {
        //MTK_OMX_LOGD("mInputFlushALL is TRUE.");
        *pFrame = NULL;
        *frameBufferIndex = -1;
        return -1;
    }
#endif

    *frameBufferIndex = DequeueOutputBuffer();

    //MTK_OMX_LOGD("(%s)(%d)-> YUV was derived from DequeueOutputBuffer. frameBufferIndex(%d)", __FUNCTION__, __LINE__, *frameBufferIndex);

    if (*frameBufferIndex >= 0)
    {
        MTK_OMX_LOGD("(%s)(%d), *frameBufferIndex(%d), mpCurrInput->nTimeStamp = %lld", __FUNCTION__, __LINE__, *frameBufferIndex, mpCurrInput->nTimeStamp);
        mOutputBufferHdrs[*frameBufferIndex]->nTimeStamp = mpCurrInput->nTimeStamp; // v4l2 todo: need to adjust timestamp
        mOutputBufferHdrs[*frameBufferIndex]->nFilledLen = mOutputPortDef.format.video.nStride * mOutputPortDef.format.video.nSliceHeight * 3 >> 1;

#if 0
        if (mDeInterlaceEnable)
        {
            DISetGrallocExtra(mOutputBufferHdrs[*frameBufferIndex]);
        }
#endif
        P_VDEC_DRV_FRAMEBUF_T pTempBuf = NULL;

        //
        // GetFrmBuf :
        //   1. set buffer properties, ex: bUsed, bDisplay and iTimeStamp
        //   2. return VDEC_DRV_FRAMEBUF_T
        //
        pTempBuf = GetFrmBuf(0, mOutputBufferHdrs[*frameBufferIndex]);
        //ALOGD("pFrame : 0x%08x (VA: 0x%08x, PA: 0x%08x, 0x%08x)", *pFrame, (*pFrame)->rBaseAddr.u4VA, (*pFrame)->rBaseAddr.u4PA, pVdec->mOutputBufferHdrs[output_idx]->pBuffer);

        //
        // GetM4UFrame:
        //   1. SetupMetaIonHandleAndGetFrame
        //   2. get buffer VA from ION mapping
        //
        GetM4UFrame(pTempBuf, mOutputBufferHdrs[*frameBufferIndex]->pBuffer);

        // Get DMA FD
        if (OMX_FALSE == getIonFdByHeaderIndex(-1, NULL, *frameBufferIndex, ionFd))
        {
            ALOGE("getIonFdByHeaderIndex failed. BSIndex:%d, FrameBufferIndex:%d", -1, *frameBufferIndex);

            return -1;
        }

        return 0;
    }

    *pFrame = NULL;
    return -1;
}

void MtkOmxVdec::InitCommonParam(char *pszCompRole,
                                 OMX_VIDEO_CODINGTYPE eInputPortFormat,
                                 char *pszMIMEType,
                                 OMX_VIDEO_CODINGTYPE eInputPortDefFormat,
                                 int INPUT_DEFAULT_INPUT_BUFFER_COUNT,
                                 int INPUT_DEFAULT_INPUT_BUFFER_SIZE,
                                 int OUTPUT_DEFAULT_OUTPUT_BUFFER_COUNT,
                                 int OUTPUT_DEFAULT_OUTPUT_BUFFER_SIZE)
{
    // init input port format
    strcpy((char *)mCompRole, pszCompRole);

    mInputPortFormat.nPortIndex         = MTK_OMX_INPUT_PORT;
    mInputPortFormat.nIndex             = 0;
    mInputPortFormat.eCompressionFormat                 = eInputPortFormat;

    // init output port format
    mOutputPortFormat.nPortIndex            = MTK_OMX_OUTPUT_PORT;
    mOutputPortFormat.nIndex                = 0;
    mOutputPortFormat.eCompressionFormat    = OMX_VIDEO_CodingUnused;
    mOutputPortFormat.eColorFormat          = OMX_COLOR_FormatYUV420Planar;

    // init input port definition
    mInputPortDef.nPortIndex                            = MTK_OMX_INPUT_PORT;
    mInputPortDef.eDir                                  = OMX_DirInput;
    mInputPortDef.eDomain                               = OMX_PortDomainVideo;
    mInputPortDef.format.video.pNativeRender            = NULL;
    mInputPortDef.format.video.nFrameWidth              = 176;
    mInputPortDef.format.video.nFrameHeight             = 144;
    mInputPortDef.format.video.nStride                  = 176;
    mInputPortDef.format.video.nSliceHeight             = 144;
    mInputPortDef.format.video.nBitrate                 = 64000;
    mInputPortDef.format.video.xFramerate               = (15 << 16);
    mInputPortDef.format.video.cMIMEType                = (OMX_STRING)pszMIMEType;
    mInputPortDef.format.video.bFlagErrorConcealment    = OMX_FALSE;
    mInputPortDef.format.video.eCompressionFormat       = eInputPortDefFormat;
    mInputPortDef.format.video.eColorFormat             = OMX_COLOR_FormatUnused;

    mInputPortDef.nBufferCountActual                    = INPUT_DEFAULT_INPUT_BUFFER_COUNT;

    if (mAdaptivePlayback)
    {
        mInputPortDef.nBufferCountMin                   = INPUT_DEFAULT_INPUT_BUFFER_COUNT;
    }
    else
    {
        mInputPortDef.nBufferCountMin       = 1;
    }

    mInputPortDef.nBufferSize                           = INPUT_DEFAULT_INPUT_BUFFER_SIZE;
    mInputPortDef.bEnabled              = OMX_TRUE;
    mInputPortDef.bPopulated            = OMX_FALSE;

    // init output port definition
    mOutputPortDef.nPortIndex                           = MTK_OMX_OUTPUT_PORT;
    mOutputPortDef.eDomain                              = OMX_PortDomainVideo;
    mOutputPortDef.format.video.cMIMEType               = (OMX_STRING)"raw";
    mOutputPortDef.format.video.pNativeRender           = 0;
    mOutputPortDef.format.video.bFlagErrorConcealment   = OMX_FALSE;
    mOutputPortDef.format.video.eCompressionFormat      = OMX_VIDEO_CodingUnused;
    mOutputPortDef.format.video.eColorFormat            = OMX_COLOR_FormatYUV420Planar;
    mOutputPortDef.format.video.nFrameWidth             = 176; //320; //176;
    mOutputPortDef.format.video.nStride                 = 176;
    mOutputPortDef.format.video.nFrameHeight            = 144; //240; //144;
    mOutputPortDef.format.video.nSliceHeight            = 144;
    mOutputPortDef.format.video.nBitrate                = 64000;
    mOutputPortDef.format.video.xFramerate              = (15 << 16);
    mOutputPortDef.eDir                                 = OMX_DirOutput;

    mOutputPortDef.nBufferCountActual                   = (mForceOutputBufferCount == 0 ? OUTPUT_DEFAULT_OUTPUT_BUFFER_COUNT : mForceOutputBufferCount);
    mOutputPortDef.nBufferCountMin                      = mOutputPortDef.nBufferCountActual - mMinUndequeuedBufs;
    mOutputPortDef.nBufferSize                          = OUTPUT_DEFAULT_OUTPUT_BUFFER_SIZE;// + 16;
    mOutputPortDef.bEnabled = OMX_TRUE;
    mOutputPortDef.bPopulated = OMX_FALSE;

    mInitRetryNum                                       = 0;
}

OMX_BOOL MtkOmxVdec::InitHEVCSecParams() //HEVC.SEC.M0
{
    // check secure environment
    int svp = 0;
    char value[PROPERTY_VALUE_MAX];
    int svp_mode = 0;

    property_get("ro.mtk_sec_video_path_support", value, "0");
    svp = atoi(value);

    if (svp)
    {
        mIsSecureInst = OMX_TRUE;
        property_get("ro.mtk_trustonic_tee_support", value, "0");
#if 1
        if (atoi(value))
        {
            svp_mode = 1;
            mTeeType = TRUSTONIC_TEE;
        }
        else //in house tee
        {
            svp_mode = 2;
            mTeeType = INHOUSE_TEE;
        }
#endif
        property_set("mtk_sec_enable", "svp_mode");

    }
    if (svp_mode == 0)
    {
        MTK_OMX_LOGE("[ERROR] InitHEVCSecParams, check Tee Type Failed !!");
        return OMX_FALSE;
    }
    MTK_OMX_LOGE("InitAvcSecParams, Tee Type = %d", mTeeType);

    InitCommonParam("video_decoder_hevc",
                    OMX_VIDEO_CodingHEVC,
                    "video/hevc",
                    OMX_VIDEO_CodingHEVC,
                    MTK_VDEC_HEVCSEC_DEFAULT_INPUT_BUFFER_COUNT,
                    MTK_VDEC_HEVCSEC_DEFAULT_INPUT_BUFFER_SIZE,
                    MTK_VDEC_HEVC_DEFAULT_OUTPUT_BUFFER_COUNT,
                    MTK_VDEC_HEVC_DEFAULT_OUTPUT_BUFFER_SIZE + 16);

    mInputPortDef.nBufferCountMin   = 1;
    mOutputPortDef.nBufferCountMin  = 1;


    if (INHOUSE_TEE == mTeeType)
    {
        mHEVCSecVdecInHouseLib = dlopen(MTK_HEVC_SEC_VDEC_IN_HOUSE_LIB_NAME, RTLD_NOW);
        if (NULL == mHEVCSecVdecInHouseLib)
        {
            MTK_OMX_LOGE("lib_uree_mtk_video_secure_al open failed: [%s]", dlerror());
            return OMX_FALSE;
        }
        MtkVideoSecureMemAllocatorInit_Ptr *pfnMtkVideoSecureMemAllocatorInit_Ptr = (MtkVideoSecureMemAllocatorInit_Ptr *) dlsym(mH264SecVdecInHouseLib, MTK_H264_SEC_VDEC_IN_HOUSE_INIT_NAME);
        if (NULL == pfnMtkVideoSecureMemAllocatorInit_Ptr)
        {
            MTK_OMX_LOGE("cannot find pfnMtkVideoSecureMemAllocatorInit_Ptr, LINE: %d", __LINE__);
            return OMX_FALSE;
        }
        if (MTK_SECURE_AL_SUCCESS != pfnMtkVideoSecureMemAllocatorInit_Ptr())
        {
            MTK_OMX_LOGE("[ERROR] pfnMtkVideoSecureMemAllocatorInit_Ptr error\n");
            return OMX_FALSE;
        }
    }
    else // TRUSTONIC_TEE
    {
        /* THE path is for smart phone */
    }
    /* Same with AVC */
    mInputUseION  = OMX_FALSE;
    mOutputUseION = OMX_FALSE;
    mStoreMetaDataInBuffers = OMX_FALSE;
    return OMX_TRUE;
}

OMX_BOOL MtkOmxVdec::InitHEVCParams()
{
    InitCommonParam("video_decoder_hevc",
                    OMX_VIDEO_CodingHEVC,
                    "video/hevc",
                    OMX_VIDEO_CodingHEVC,
                    MTK_VDEC_HEVC_DEFAULT_INPUT_BUFFER_COUNT,
                    MTK_VDEC_HEVC_DEFAULT_INPUT_BUFFER_SIZE,
                    MTK_VDEC_HEVC_DEFAULT_OUTPUT_BUFFER_COUNT,
                    MTK_VDEC_HEVC_DEFAULT_OUTPUT_BUFFER_SIZE + 16);

    mInputPortDef.nBufferCountMin = 1;
    mOutputPortDef.nBufferCountMin  = 1;

    return OMX_TRUE;
}

OMX_BOOL MtkOmxVdec::InitH263Params()
{
    InitCommonParam("video_decoder.h263",
                    OMX_VIDEO_CodingH263,
                    "video/h263",
                    OMX_VIDEO_CodingH263,
                    MTK_VDEC_H263_DEFAULT_INPUT_BUFFER_COUNT,
                    MTK_VDEC_H263_DEFAULT_INPUT_BUFFER_SIZE,
                    MTK_VDEC_H263_DEFAULT_OUTPUT_BUFFER_COUNT + mMinUndequeuedBufs,
                    MTK_VDEC_H263_DEFAULT_OUTPUT_BUFFER_SIZE + 16);


    return OMX_TRUE;
}

OMX_BOOL MtkOmxVdec::InitMpeg4Params()
{

    InitCommonParam("video_decoder.mpeg4",
                    OMX_VIDEO_CodingMPEG4,
                    "video/mpeg4",
                    OMX_VIDEO_CodingMPEG4,
                    MTK_VDEC_MPEG4_DEFAULT_INPUT_BUFFER_COUNT,
                    MTK_VDEC_MPEG4_DEFAULT_INPUT_BUFFER_SIZE,
                    MTK_VDEC_MPEG4_DEFAULT_OUTPUT_BUFFER_COUNT + mMinUndequeuedBufs,
                    MTK_VDEC_MPEG4_DEFAULT_OUTPUT_BUFFER_SIZE + 16);

    return OMX_TRUE;
}

OMX_BOOL MtkOmxVdec::InitDivxParams()
{

    InitCommonParam("video_decoder.divx",
                    OMX_VIDEO_CodingDIVX,
                    "video/divx",
                    OMX_VIDEO_CodingMPEG4,
                    MTK_VDEC_MPEG4_DEFAULT_INPUT_BUFFER_COUNT,
                    MTK_VDEC_MPEG4_DEFAULT_INPUT_BUFFER_SIZE,
                    MTK_VDEC_MPEG4_DEFAULT_OUTPUT_BUFFER_COUNT + mMinUndequeuedBufs,
                    MTK_VDEC_MPEG4_DEFAULT_OUTPUT_BUFFER_SIZE + 16);

    return OMX_TRUE;
}

OMX_BOOL MtkOmxVdec::InitDivx3Params()
{
    InitCommonParam("video_decoder.divx3",
                    OMX_VIDEO_CodingDIVX3,
                    "video/divx3",
                    OMX_VIDEO_CodingMPEG4,
                    MTK_VDEC_MPEG4_DEFAULT_INPUT_BUFFER_COUNT,
                    MTK_VDEC_MPEG4_DEFAULT_INPUT_BUFFER_SIZE,
                    MTK_VDEC_MPEG4_DEFAULT_OUTPUT_BUFFER_COUNT + mMinUndequeuedBufs,
                    MTK_VDEC_MPEG4_DEFAULT_OUTPUT_BUFFER_SIZE + 16);

    return OMX_TRUE;
}

OMX_BOOL MtkOmxVdec::InitXvidParams()
{
    InitCommonParam("video_decoder.xvid",
                    OMX_VIDEO_CodingXVID,
                    "video/xvid",
                    OMX_VIDEO_CodingMPEG4,
                    MTK_VDEC_MPEG4_DEFAULT_INPUT_BUFFER_COUNT,
                    MTK_VDEC_MPEG4_DEFAULT_INPUT_BUFFER_SIZE,
                    MTK_VDEC_MPEG4_DEFAULT_OUTPUT_BUFFER_COUNT + mMinUndequeuedBufs,
                    MTK_VDEC_MPEG4_DEFAULT_OUTPUT_BUFFER_SIZE + 16);

    return OMX_TRUE;
}

OMX_BOOL MtkOmxVdec::InitS263Params()
{
    InitCommonParam("video_decoder.s263",
                    OMX_VIDEO_CodingS263,
                    "video/flv1",
                    OMX_VIDEO_CodingMPEG4,
                    MTK_VDEC_MPEG4_DEFAULT_INPUT_BUFFER_COUNT,
                    MTK_VDEC_MPEG4_DEFAULT_INPUT_BUFFER_SIZE,
                    MTK_VDEC_MPEG4_DEFAULT_OUTPUT_BUFFER_COUNT + mMinUndequeuedBufs,
                    MTK_VDEC_MPEG4_DEFAULT_OUTPUT_BUFFER_SIZE + 16);

    return OMX_TRUE;
}

OMX_BOOL MtkOmxVdec::InitAvcParams()
{
    InitCommonParam("video_decoder.avc",
                    OMX_VIDEO_CodingAVC,
                    "video/avc",
                    OMX_VIDEO_CodingAVC,
                    MTK_VDEC_AVC_DEFAULT_INPUT_BUFFER_COUNT,
                    MTK_VDEC_AVC_DEFAULT_INPUT_BUFFER_SIZE,
                    MTK_VDEC_AVC_DEFAULT_OUTPUT_BUFFER_COUNT,
                    MTK_VDEC_AVC_DEFAULT_OUTPUT_BUFFER_SIZE + 16);

    return OMX_TRUE;
}

OMX_BOOL MtkOmxVdec::InitAvcSecParams()
{
    // check secure environment
    int svp = 0;
    char value[PROPERTY_VALUE_MAX];

    int svp_mode = 0;

    property_get("ro.mtk_sec_video_path_support", value, "0");
    svp = atoi(value);

    if (svp)
    {
        mIsSecureInst = OMX_TRUE;
        property_get("ro.mtk_trustonic_tee_support", value, "0");
#if 1
        if (atoi(value))
        {
            svp_mode = 1;
            mTeeType = TRUSTONIC_TEE;
        }
        else //in house tee
        {
            svp_mode = 2;
            mTeeType = INHOUSE_TEE;
        }
#endif
        property_set("mtk_sec_enable", "svp_mode");

    }
    if (svp_mode == 0)
    {
        MTK_OMX_LOGE("[ERROR] InitAvcSecParams, check Tee Type Failed !!");
        return OMX_FALSE;
    }

    MTK_OMX_LOGE("InitAvcSecParams, Tee Type = %d", mTeeType);

    InitCommonParam("video_decoder.avc",
                    OMX_VIDEO_CodingAVC,
                    "video/avc",
                    OMX_VIDEO_CodingAVC,
                    MTK_VDEC_AVC_DEFAULT_INPUT_BUFFER_COUNT,
                    MTK_VDEC_AVC_DEFAULT_INPUT_BUFFER_SIZE,
                    MTK_VDEC_AVC_DEFAULT_OUTPUT_BUFFER_COUNT,
                    MTK_VDEC_AVC_DEFAULT_OUTPUT_BUFFER_SIZE + 16);

    if (INHOUSE_TEE == mTeeType)
    {
        //#if defined(MTK_SEC_VIDEO_PATH_SUPPORT) && defined(MTK_IN_HOUSE_TEE_SUPPORT)
        mH264SecVdecInHouseLib = dlopen(MTK_H264_SEC_VDEC_IN_HOUSE_LIB_NAME, RTLD_NOW);
        if (NULL == mH264SecVdecInHouseLib)
        {
            MTK_OMX_LOGE("lib_uree_mtk_video_secure_al open failed: [%s]", dlerror());
            return OMX_FALSE;
        }
        MtkVideoSecureMemAllocatorInit_Ptr *pfnMtkVideoSecureMemAllocatorInit_Ptr = (MtkVideoSecureMemAllocatorInit_Ptr *) dlsym(mH264SecVdecInHouseLib, MTK_H264_SEC_VDEC_IN_HOUSE_INIT_NAME);
        if (NULL == pfnMtkVideoSecureMemAllocatorInit_Ptr)
        {
            MTK_OMX_LOGE("cannot find pfnMtkVideoSecureMemAllocatorInit_Ptr, LINE: %d", __LINE__);
            return OMX_FALSE;
        }
        if (MTK_SECURE_AL_SUCCESS != pfnMtkVideoSecureMemAllocatorInit_Ptr())
        {
            MTK_OMX_LOGE("[ERROR] pfnMtkVideoSecureMemAllocatorInit_Ptr error\n");
            return OMX_FALSE;
        }
        //#endif
    }
    else // TRUSTONIC_TEE
    {
        mInputPortDef.nBufferSize = MTK_VDEC_AVCSEC_DEFAULT_INPUT_BUFFER_SIZE;   // for Trustonic t-base (D9)
        // init TLC lib
        mH264SecVdecTlcLib = dlopen(MTK_H264_SEC_VDEC_TLC_LIB_NAME, RTLD_NOW);
        if (NULL == mH264SecVdecTlcLib)
        {
            MTK_OMX_LOGE("mH264SecVdecTlcLib open failed: [%s]", dlerror());
            return OMX_FALSE;
        }

        MtkH264SecVdec_secMemInit_Ptr *pfnMtkH264SecVdec_secMemInit = (MtkH264SecVdec_secMemInit_Ptr *) dlsym(mH264SecVdecTlcLib, MTK_H264_SEC_VDEC_SEC_MEM_INIT_NAME);
        if (NULL == pfnMtkH264SecVdec_secMemInit)
        {
            MTK_OMX_LOGE("cannot find MtkH264SecVdec_secMemInit, LINE: %d", __LINE__);
            return OMX_FALSE;
        }

        MtkH264SecVdec_tlcHandleCreate_Ptr *pfnMtkH264SecVdec_tlcHandleCreate = (MtkH264SecVdec_tlcHandleCreate_Ptr *) dlsym(mH264SecVdecTlcLib, MTK_H264_SEC_VDEC_TLC_HANDLE_CREATE_NAME);
        if (NULL == pfnMtkH264SecVdec_tlcHandleCreate)
        {
            MTK_OMX_LOGE("cannot find MtkH264SecVdec_tlcHandleCreate, LINE: %d", __LINE__);
            return OMX_FALSE;
        }

        MtkH264SecVdec_tlcOpen_Ptr *pfnMtkH264SecVdec_tlcOpen = (MtkH264SecVdec_tlcOpen_Ptr *) dlsym(mH264SecVdecTlcLib, MTK_H264_SEC_VDEC_TLC_OPEN_NAME);
        if (NULL == pfnMtkH264SecVdec_tlcOpen)
        {
            MTK_OMX_LOGE("cannot find MtkH264SecVdec_tlcOpen, LINE: %d", __LINE__);
            return OMX_FALSE;
        }

        // init secure memory sevice
        if (pfnMtkH264SecVdec_secMemInit() < 0)
        {
            MTK_OMX_LOGE("MtkH264SecVdec_secMemInit error, LINE: %d", __LINE__);
            return OMX_FALSE;
        }

        mTlcHandle = pfnMtkH264SecVdec_tlcHandleCreate();

#if 0
        if (pfnMtkH264SecVdec_tlcOpen(mTlcHandle) < 0)
        {
            MTK_OMX_LOGE("MtkH264SecVdec_tlcOpen failed, LINE: %d", __LINE__);
            return OMX_FALSE;
        }
#endif
    }
    //mIsSecureInst = OMX_TRUE;
    mInputUseION = OMX_FALSE;
    mOutputUseION = OMX_FALSE;
    mAdaptivePlayback = VAL_FALSE;
    mStoreMetaDataInBuffers = OMX_FALSE;
    return OMX_TRUE;
}


OMX_BOOL MtkOmxVdec::InitRvParams()
{
    InitCommonParam("video_decoder.rv",
                    OMX_VIDEO_CodingRV,
                    "video/vnd.rn-realvideo",
                    OMX_VIDEO_CodingRV,
                    MTK_VDEC_RV_DEFAULT_INPUT_BUFFER_COUNT,
                    MTK_VDEC_RV_DEFAULT_INPUT_BUFFER_SIZE,
                    MTK_VDEC_RV_DEFAULT_OUTPUT_BUFFER_COUNT,
                    MTK_VDEC_RV_DEFAULT_OUTPUT_BUFFER_SIZE + 16);


    // init rv type
    MTK_OMX_MEMSET(&mRvType, 0x00, sizeof(OMX_VIDEO_PARAM_RVTYPE));
    mRvType.bEnableLatencyMode = OMX_FALSE;
    mRvType.bEnablePostFilter = OMX_FALSE;
    mRvType.bEnableTemporalInterpolation = OMX_FALSE;
    mRvType.eFormat = OMX_VIDEO_RVFormat9;
    mRvType.nBitsPerPixel = 16;
    mRvType.nBitstreamFlags = 0;
    mRvType.nBitstreamVersion = 0;
    mRvType.nFrameRate = 30;
    mRvType.nMaxEncodeFrameSize = 0;
    mRvType.nPaddedHeight = 0;
    mRvType.nPaddedWidth = 0;
    mRvType.nPortIndex = MTK_OMX_INPUT_PORT;

    return OMX_TRUE;
}


OMX_BOOL MtkOmxVdec::InitVc1Params()
{
    InitCommonParam("video_decoder.vc1",
                    OMX_VIDEO_CodingWMV,
                    "video/x-ms-wmv",
                    OMX_VIDEO_CodingWMV,
                    MTK_VDEC_VC1_DEFAULT_INPUT_BUFFER_COUNT,
                    MTK_VDEC_VC1_DEFAULT_INPUT_BUFFER_SIZE,
                    MTK_VDEC_VC1_DEFAULT_OUTPUT_BUFFER_COUNT,
                    MTK_VDEC_VC1_DEFAULT_OUTPUT_BUFFER_SIZE + 16);

    return OMX_TRUE;
}

OMX_BOOL MtkOmxVdec::InitVpxParams()
{
    InitCommonParam("video_decoder.vp8",
                    OMX_VIDEO_CodingVP8,
                    "video/x-vnd.on2.vp8",
                    OMX_VIDEO_CodingVP8,
                    MTK_VDEC_VPX_DEFAULT_INPUT_BUFFER_COUNT,
                    MTK_VDEC_VPX_DEFAULT_INPUT_BUFFER_SIZE,
                    0,
                    MTK_VDEC_VPX_DEFAULT_OUTPUT_BUFFER_SIZE + 16);

#if (ANDROID_VER >= ANDROID_ICS)
    mOutputPortDef.nBufferCountActual                   = (mForceOutputBufferCount == 0 ? MTK_VDEC_VPX_DEFAULT_OUTPUT_BUFFER_COUNT + mMinUndequeuedBufs : mForceOutputBufferCount);
#else
    mOutputPortDef.nBufferCountActual                   = (mForceOutputBufferCount == 0 ? MTK_VDEC_VPX_DEFAULT_OUTPUT_BUFFER_COUNT : mForceOutputBufferCount);
#endif
    mOutputPortDef.nBufferCountMin                      = mOutputPortDef.nBufferCountActual - mMinUndequeuedBufs;


    return OMX_TRUE;
}

OMX_BOOL MtkOmxVdec::InitVp9Params()
{
    InitCommonParam("video_decoder.vp9",
                    OMX_VIDEO_CodingVP9,
                    "video/x-vnd.on2.vp9",
                    OMX_VIDEO_CodingVP9,
                    MTK_VDEC_VPX_DEFAULT_INPUT_BUFFER_COUNT,
                    MTK_VDEC_VP9_DEFAULT_INPUT_BUFFER_SIZE,
                    0,
                    MTK_VDEC_VPX_DEFAULT_OUTPUT_BUFFER_SIZE + 16);


#if (ANDROID_VER >= ANDROID_ICS)
    mOutputPortDef.nBufferCountActual                   = (mForceOutputBufferCount == 0 ? MTK_VDEC_VPX_DEFAULT_OUTPUT_BUFFER_COUNT + mMinUndequeuedBufs : mForceOutputBufferCount);
#else
    mOutputPortDef.nBufferCountActual                   = (mForceOutputBufferCount == 0 ? MTK_VDEC_VPX_DEFAULT_OUTPUT_BUFFER_COUNT : mForceOutputBufferCount);
#endif
    mOutputPortDef.nBufferCountMin                      = mOutputPortDef.nBufferCountActual - mMinUndequeuedBufs;


    return OMX_TRUE;
}

OMX_BOOL MtkOmxVdec::InitMpeg2Params()
{
    InitCommonParam("video_decoder.mpeg2",
                    OMX_VIDEO_CodingMPEG2,
                    "video/mpeg2",
                    OMX_VIDEO_CodingMPEG2,
                    MTK_VDEC_MPEG2_DEFAULT_INPUT_BUFFER_COUNT,
                    MTK_VDEC_MPEG2_DEFAULT_INPUT_BUFFER_SIZE,
                    MTK_VDEC_MPEG2_DEFAULT_OUTPUT_BUFFER_COUNT,
                    MTK_VDEC_MPEG2_DEFAULT_OUTPUT_BUFFER_SIZE + 16);

    return OMX_TRUE;
}

OMX_BOOL MtkOmxVdec::InitMJpegParams()
{
    InitCommonParam("video_decoder.mjpeg",
                    OMX_VIDEO_CodingMJPEG,
                    "video/x-motion-jpeg",
                    OMX_VIDEO_CodingMJPEG,
                    MTK_VDEC_MJPEG_DEFAULT_INPUT_BUFFER_COUNT,
                    MTK_VDEC_MJPEG_DEFAULT_INPUT_BUFFER_SIZE,
                    MTK_VDEC_MJPEG_DEFAULT_OUTPUT_BUFFER_COUNT,
                    MTK_VDEC_MJPEG_DEFAULT_OUTPUT_BUFFER_SIZE);

    mInputPortFormat.eColorFormat = OMX_COLOR_FormatUnused;
    mOutputPortFormat.eColorFormat = OMX_COLOR_Format32bitARGB8888;

    return OMX_TRUE;
}

OMX_BOOL MtkOmxVdec::DecodeVideo(OMX_BUFFERHEADERTYPE *pInputBuf, OMX_BUFFERHEADERTYPE *pOutputBuf)
{
    return OMX_TRUE;
}

OMX_BOOL MtkOmxVdec::initCodec_PortReconfig(OMX_BUFFERHEADERTYPE *pInputBuf,
                                            OMX_S32 iDisplayWidth,
                                            OMX_S32 iDisplayHeight,
                                            OMX_S32 iAspectRatioWidth,
                                            OMX_S32 iAspectRatioHeight,
                                            OMX_BOOL *needEarlyExit)
{
    mReconfigOutputPortSettings.u4Width = iDisplayWidth;
    mReconfigOutputPortSettings.u4Height = iDisplayHeight;
    mReconfigOutputPortSettings.u4RealWidth = VDEC_ROUND_N(mSeqInfo.u4Width, mQInfoOut.u4StrideAlign);
    mReconfigOutputPortSettings.u4RealHeight = VDEC_ROUND_N(mSeqInfo.u4Height, mQInfoOut.u4SliceHeightAlign);
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

    if (OMX_FALSE == mIsSecureInst)
    {
        if (1 != mMtkV4L2Device.getDPBSize(&mDPBSize))
        {
            MTK_OMX_LOGE("[ERROR] Cannot get param: VDEC_DRV_GET_TYPE_QUERY_VIDEO_DPB_SIZE");
            HandleEmptyBufferDone(pInputBuf);
            //return OMX_FALSE;
            goto PORT_RECONFIG_FAIL;
        }
    }
    else
    {
        MTK_OMX_LOGE("[Info][1] Secure Video, mDPBSize = 16");
        // never touch pVdec_H264_InstanceInst->mSPS[] here!!!
        mDPBSize = 16;
    }

    if (mThumbnailMode == OMX_FALSE)
    {
        int32_t MaxDPBNumber, ActualOutBuffNums, ActualOutBuffSize;

        MaxDPBNumber = mDPBSize;

        if (mFixedMaxBuffer == OMX_TRUE)
        {
            MaxDPBNumber = 16;
            MTK_OMX_LOGD("***mFixedMaxBuffer***, MaxDPBNumber = 16");
        }
        ActualOutBuffNums = MaxDPBNumber + mMinUndequeuedBufs + FRAMEWORK_OVERHEAD; // for some HEVC baseline with other nal headers
        MTK_OMX_LOGD("%s@%d, ActualOutBuffNums(%d), MaxDPBNumber(%d), mMinUndequeuedBufs(%d), FRAMEWORK_OVERHEAD(%d)", __FUNCTION__, __LINE__, ActualOutBuffNums, MaxDPBNumber, mMinUndequeuedBufs, FRAMEWORK_OVERHEAD);

        if (mMinUndequeuedBufsFlag == OMX_TRUE)
        {
            mMinUndequeuedBufsFlag = OMX_FALSE;
            MTK_OMX_LOGD("[Debug] avoid to re-count mMinUndequeuedBufsDiff\n");
        }

        mReconfigOutputPortBufferCount = ActualOutBuffNums;
        mOutputPortDef.nBufferCountMin = ActualOutBuffNums - mMinUndequeuedBufs;

    }

    mOutputPortDef.nBufferCountActual = mReconfigOutputPortBufferCount; //Buffer slim: Increased buffer count can actually update portDef, prevent bIsHEVC_ETBQcontainsPPS port reconfig twice.
    MTK_OMX_LOGD("--- nBufferCountActual: %d;  mDPBSize: %d---", mReconfigOutputPortBufferCount , mDPBSize);

    //mPortReconfigInProgress = OMX_TRUE;
    //enable this value , just for ignore GUI alloc
    //mIgnoreGUI = OMX_TRUE;
    MTK_OMX_LOGD("--- OMX_EventPortSettingsChanged ---");
    if ((iAspectRatioWidth != 1 || iAspectRatioHeight != 1) && iAspectRatioWidth != 0 && iAspectRatioHeight != 0)
    {
        MTK_OMX_LOGD("--- OMX_EventPortSettingsChanged (with Aspect ratio)--- %i %i", iAspectRatioWidth, iAspectRatioHeight);
        mAspectRatioWidth = iAspectRatioWidth;
        mAspectRatioHeight = iAspectRatioHeight;
        mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                               mAppData,
                               OMX_EventPortSettingsChanged,
                               MTK_OMX_OUTPUT_PORT,
                               OMX_IndexVendorMtkOmxVdecGetAspectRatio,
                               NULL);
    }
    else
    {
        // v4l2 todo: we're meant to disable the 1st port setting change. Since we don't want flush codec here.
        //mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
        //                       mAppData,
        //                       OMX_EventPortSettingsChanged,
        //                       MTK_OMX_OUTPUT_PORT,
        //                       NULL,
        //                       NULL);
    }


    *needEarlyExit = OMX_FALSE;
    return OMX_TRUE;

PORT_RECONFIG_FAIL:
    *needEarlyExit = OMX_TRUE;
    return OMX_FALSE;
}

OMX_BOOL MtkOmxVdec::initCodec(OMX_BUFFERHEADERTYPE *pInputBuf, OMX_BOOL *needEarlyExit)
{
    OMX_BOOL bRet = OMX_TRUE;
    OMX_U8 *aInputBuf   = pInputBuf->pBuffer + pInputBuf->nOffset;
    OMX_U32 aInputSize  = pInputBuf->nFilledLen;

    mReconfigOutputPortSettings.u4Width = mOutputPortDef.format.video.nFrameWidth;
    mReconfigOutputPortSettings.u4Height = mOutputPortDef.format.video.nFrameHeight;
    mReconfigOutputPortSettings.u4RealWidth = mOutputPortDef.format.video.nStride;
    mReconfigOutputPortSettings.u4RealHeight = mOutputPortDef.format.video.nSliceHeight;
    mReconfigOutputPortBufferCount = mOutputPortDef.nBufferCountActual;
    mReconfigOutputPortBufferSize = mOutputPortDef.nBufferSize;

    if ((pInputBuf->nFlags & OMX_BUFFERFLAG_EOS) && (pInputBuf->nFilledLen == 0))
    {
        MTK_OMX_LOGD(" %s EOS before init %lld", GetVDECSrting(mCodecId, NULL), pInputBuf->nTimeStamp);

        // return the EOS output buffer
        //pOutputBuf->nFlags |= OMX_BUFFERFLAG_EOS;
        //pOutputBuf->nFilledLen = 0;
        //pOutputBuf->nTimeStamp = 0;
        //HandleFillBufferDone(pOutputBuf, mRealCallBackFillBufferDone);
        if (mInputFlushALL != OMX_TRUE)
        {
            MTK_OMX_LOGD("wait one output buffer for EOS %d", get_sem_value(&mOutputBufferSem));
            WAIT(mOutputBufferSem);
        }

        MTK_OMX_LOGD("mNumPendingInput(%d), mNumPendingOutput(%d)", (int)mNumPendingInput, (int)mNumPendingOutput);
        DumpFTBQ();
        //return OMX_TRUE;
        goto INIT_CODEC_SUCCESS;
    }

    OMX_S32 iDisplayWidth;
    OMX_S32 iDisplayHeight;
    OMX_S32 iAspectRatioWidth;
    OMX_S32 iAspectRatioHeight;

    if (OMX_FALSE == InitVideoDecodeHW(&iDisplayWidth, &iDisplayHeight, &iAspectRatioWidth, &iAspectRatioHeight, aInputBuf, &aInputSize, pInputBuf))
    {
        mFailInitCounter++;

        if (mFailInitCounter >= 30)
        {
            mFATALError = OMX_TRUE;
            MTK_OMX_LOGE("InitVideoDecodeHW failed over %d times", mFailInitCounter);
        }
        else
        {
            MTK_OMX_LOGE("InitVideoDecodeHW failed and retry");
        }
        HandleEmptyBufferDone(pInputBuf);
        //pOutputBuf->nFilledLen = 0;
        //pOutputBuf->nTimeStamp = 0;
        //HandleFillBufferDone(pOutputBuf, mRealCallBackFillBufferDone);
        if (mFATALError == OMX_TRUE)
        {
            // report error bad parameter
            mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                   mAppData,
                                   OMX_EventError,
                                   OMX_ErrorBadParameter,
                                   NULL,
                                   NULL);
            MTK_OMX_LOGE("InitVideoDecodeHW failed (not compatible");
            //return OMX_FALSE;
            goto INIT_CODEC_FAIL;
        }
        else
        {
            //return OMX_TRUE;
            goto INIT_CODEC_SUCCESS;
        }
    }
    MTK_OMX_LOGD("InitHW OK (%u)(%u)(%u)(%u)(%u)(%u), %u, %u, %u, %u, (%u, %u)",
                 (unsigned int)iDisplayWidth, (unsigned int)iDisplayHeight, (unsigned int)mOutputPortDef.format.video.nFrameWidth, (unsigned int)mOutputPortDef.format.video.nFrameHeight,
                 (unsigned int)mOutputPortDef.format.video.nStride, (unsigned int) mOutputPortDef.format.video.nSliceHeight, mSeqInfo.u4Width, mSeqInfo.u4Height, mQInfoOut.u4StrideAlign,
                 mQInfoOut.u4SliceHeightAlign, mSeqInfo.u4BitDepthLuma, mSeqInfo.u4BitDepthChroma);
    mDecoderInitCompleteFlag = OMX_TRUE;

    if (mSeqInfo.u4BitDepthLuma == 10 || mSeqInfo.u4BitDepthChroma == 10)
    {
        mbIs10Bit = OMX_TRUE;
        mIsHorizontalScaninLSB = mSeqInfo.bIsHorizontalScaninLSB;
    }

    // check if output port setting changes
    if (0) // v4l2 todo: do we need port reconfig??
#if 0
        if ((mThumbnailMode == OMX_FALSE)
            || (OMX_TRUE == mbIs10Bit)
            //|| ((OMX_U32)iDisplayWidth != mOutputPortDef.format.video.nFrameWidth)
            //|| ((OMX_U32)iDisplayHeight != mOutputPortDef.format.video.nFrameHeight)
            || mOutputPortDef.format.video.nStride != VDEC_ROUND_N(mSeqInfo.u4Width, mQInfoOut.u4StrideAlign)
            || mOutputPortDef.format.video.nSliceHeight != VDEC_ROUND_N(mSeqInfo.u4Height, mQInfoOut.u4SliceHeightAlign)
            || (mMJCEnable == OMX_TRUE && mMJCScalerByPassFlag == OMX_FALSE)
           )
#endif
        {
            handleResolutionChange();
            //OMX_BOOL needEarlyExit = OMX_FALSE;
            //OMX_BOOL bRet = OMX_TRUE;
            //bRet = initCodec_PortReconfig(pInputBuf, iDisplayWidth, iDisplayHeight, iAspectRatioWidth, iAspectRatioHeight, &needEarlyExit);
            //if (OMX_TRUE == needEarlyExit)
            //{
            //    return bRet;
            //}
        }
    //pOutputBuf->nFilledLen = 0;
    //pOutputBuf->nTimeStamp = 0;
    //TODO : MPEG2, H.263 in MPEG4 needs to queue first input buffer ?
    //QueueInputBuffer(findBufferHeaderIndex(MTK_OMX_INPUT_PORT, pInputBuf)); // v4l2
    //QueueOutputBuffer(findBufferHeaderIndex(MTK_OMX_OUTPUT_PORT, pOutputBuf));
    //SIGNAL(mDecodeSem); // v4l2
    //HandleFillBufferDone(pOutputBuf, mRealCallBackFillBufferDone);

    VAL_UINT32_T  FrameInterval;
    if (1 == mMtkV4L2Device.getFrameInterval(&FrameInterval))
    {
        mFrameInterval = FrameInterval;
        MTK_OMX_LOGD("Frame Interval = 0x%llX", mFrameInterval);
    }
    else
    {
        mFrameInterval = 0;
        MTK_OMX_LOGE("ERROR: query VDEC_DRV_GET_TYPE_GET_FRAME_INTERVAL failed");
    }

    if (1)
    {
        // For HEVC buffer align and crop info +
        v4l2_crop temp_crop_info;

        if (1 == mMtkV4L2Device.getCrop(&temp_crop_info))
        {
            MTK_OMX_LOGE("Get crop info succeed....");

            mCropLeft = temp_crop_info.c.left;
            mCropTop = temp_crop_info.c.top;
            mCropWidth = temp_crop_info.c.width;
            mCropHeight = temp_crop_info.c.height;
            //set crop info
            MTK_OMX_LOGD("--- OMX_EventPortSettingsChanged  (with Crop info)---");
            mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                   mAppData,
                                   OMX_EventPortSettingsChanged,
                                   MTK_OMX_OUTPUT_PORT,
                                   OMX_IndexVendorMtkOmxVdecGetCropInfo,
                                   NULL);
        }
        else
        {
            MTK_OMX_LOGE("Get crop info fail....");
            mCropLeft = 0;
            mCropTop = 0;
            mCropWidth = mReconfigOutputPortSettings.u4Width;
            mCropHeight = mReconfigOutputPortSettings.u4Height;
        }

        MTK_OMX_LOGD("initCodec : mCropLeft %d, mCropTop %d, mCropWidth %d, mCropHeight %d\n", mCropLeft, mCropTop, mCropWidth, mCropHeight);

    }

    bRet = OMX_TRUE;
    *needEarlyExit = OMX_FALSE;
    return bRet;

INIT_CODEC_SUCCESS:
    bRet = OMX_TRUE;
    *needEarlyExit = OMX_TRUE;
    return bRet;

INIT_CODEC_FAIL:
    bRet = OMX_FALSE;
    *needEarlyExit = OMX_TRUE;
    return bRet;

}

OMX_BOOL MtkOmxVdec::decodeRoutine_EOS(OMX_BUFFERHEADERTYPE *pInputBuf, OMX_BOOL *needEarlyExit)
{
    OMX_U8 *aInputBuf   = pInputBuf->pBuffer + pInputBuf->nOffset;
    OMX_U32 aInputSize  = pInputBuf->nFilledLen;

    *needEarlyExit = OMX_FALSE;
    mEOSQueued = OMX_TRUE;

    MTK_OMX_LOGD("(%s) %lld, EOS Len (%d) (0x%X)", GetVDECSrting(mCodecId, NULL), pInputBuf->nTimeStamp, (unsigned int)pInputBuf->nFilledLen, (unsigned int)pInputBuf);

    //mEOSTS = mDescendingPTS[0]; //max timestamp
    if (pInputBuf->nFilledLen == 0)
    {
        OMX_U32 i;

        //MTK_OMX_LOGD("EOS timestamp!!!%lld %lld", mEOSTS, pInputBuf->nTimeStamp);

        MTK_OMX_LOGD("mNumPendingInput(%d), mNumPendingOutput(%d)", (int)mNumPendingInput, (int)mNumPendingOutput);
        DumpFTBQ();
        //return OMX_TRUE;
        goto EOS_SUCCESS;
    }
    else        // Morris for CTS test (early EOS)
    {
        mEarlyEOS = OMX_TRUE;
        MTK_OMX_LOGD("(%d) early EOS !!!%lld %lld", mEOSTS, pInputBuf->nTimeStamp);
        // normal decode flow
    }

    return OMX_TRUE;

EOS_SUCCESS:
    *needEarlyExit = OMX_TRUE;
    return OMX_TRUE;
}

OMX_BOOL MtkOmxVdec::decodeRoutine(OMX_BUFFERHEADERTYPE *pInputBuf, OMX_BOOL *needEarlyExit)
{
    OMX_BOOL bRet 					= OMX_TRUE;
    OMX_U8 *aInputBuf   			= pInputBuf->pBuffer + pInputBuf->nOffset;
    OMX_U32 aInputSize  			= pInputBuf->nFilledLen;
    VDEC_DRV_RINGBUF_T *pRingBuf 	= NULL;
    OMX_BOOL bEverOutput 			= OMX_TRUE;
    OMX_BUFFERHEADERTYPE *ipOutBuf 	= NULL;
    VDEC_DRV_FRAMEBUF_T *FrameBuf  	= NULL;
    VAL_ULONG_T u4VA            	= 0;
    VDEC_DRV_MRESULT_T rResult 		= VDEC_DRV_MRESULT_OK;

    if (pInputBuf->nFlags & OMX_BUFFERFLAG_EOS)    // last frame
    {
        OMX_BOOL needEarlyExit = OMX_FALSE;
        bRet = decodeRoutine_EOS(pInputBuf, &needEarlyExit);
        if (OMX_TRUE == needEarlyExit)
        {
            goto DECODE_SUCCESS;
        }
    }

    if (mDescendingPTS.size() > 0)
    {
        mEOSTS = mDescendingPTS[0]; //max timestamp
        //MTK_OMX_LOGD("EOS TS = %lld", mEOSTS);
    }
    else
    {
        mEOSTS = 0;
    }

    if ((meDecodeType == VDEC_DRV_DECODER_MTK_SOFTWARE) &&
        (mPropFlags & MTK_OMX_VDEC_ENABLE_PRIORITY_ADJUSTMENT))
    {
        mCodecTidInitialized = OMX_TRUE;
    }

    if (aInputSize == 0)
    {
        MTK_OMX_LOGD("Input size is 0");
        HandleEmptyBufferDone(pInputBuf);
        if (mThumbnailMode) //for CR ALPS01590372
        {
            MTK_OMX_LOGD("thumbnail Mode(ETB 0), FBD for avoiding decoding thread blocked");
        }

        //return OMX_TRUE;
        goto DECODE_SUCCESS;
    }

    // Check if the bitstream size is larger than input bufer size
    if ((pInputBuf->nFilledLen + DEC_BS_PROTECT_MEM_SZ) > mInputPortDef.nBufferSize)
    {
        MTK_OMX_LOGD("pInputBuf->nFilledLen %d", pInputBuf->nFilledLen);
        MTK_OMX_LOGD("mInputPortDef.nBufferSize  %d", mInputPortDef.nBufferSize);
    }

    //MTK_OMX_LOGD("mPrepareSeek %d, mSeekMode %d, mSeekTargetTime %lld", mPrepareSeek, mSeekMode, mSeekTargetTime);
    if (OMX_TRUE == mPrepareSeek)
    {
        VDEC_DRV_SET_DECODE_MODE_T rtSetDecodeMode;
        rtSetDecodeMode.u4DisplayFrameNum = 0;
        rtSetDecodeMode.u4DropFrameNum = 0;
        rtSetDecodeMode.eDecodeMode = VDEC_DRV_DECODE_MODE_B_SKIP; // seek mode
        mMtkV4L2Device.setDecodeMode(&rtSetDecodeMode);
        mSeekMode = OMX_TRUE;
        mPrepareSeek = OMX_FALSE;
            mOmxVdecLogEnable = OMX_FALSE;
            mOmxVdecPerfLogEnable = OMX_FALSE;
    }

    mCBTime = 0;

    //MTK_OMX_LOGD("Seek mode %d, target time %lld, input time %lld", mSeekMode, mSeekTargetTime, pInputBuf->nTimeStamp);
    if ((mSeekMode == OMX_TRUE) && (mSeekTargetTime > 0))
    {
        if (pInputBuf->nTimeStamp >= mSeekTargetTime)
        {
            MTK_OMX_LOGD("SDone, %lld", pInputBuf->nTimeStamp);
            mSeekMode = OMX_FALSE;
            VDEC_DRV_SET_DECODE_MODE_T rtSetDecodeMode;
            rtSetDecodeMode.u4DisplayFrameNum = 0;
            rtSetDecodeMode.u4DropFrameNum = 0;
            rtSetDecodeMode.eDecodeMode = VDEC_DRV_DECODE_MODE_NORMAL; // normal mode
            mMtkV4L2Device.setDecodeMode(&rtSetDecodeMode);
        }
    }


    if (mDeInterlaceEnable)
    {
        if ((mInterlaceChkComplete == OMX_FALSE) && (mThumbnailMode != OMX_TRUE))
        {
            // Need to discussion interlace detail process flow
            VAL_UINT32_T u32VideoInteraceing  = 0;
            if (0 == mMtkV4L2Device.getInterlacing(&u32VideoInteraceing))
            {
                MTK_OMX_LOGE("VDEC_DRV_GET_TYPE_QUERY_VIDEO_INTERLACING not support");
                u32VideoInteraceing = 0;
            }
            mIsInterlacing = (VAL_BOOL_T)u32VideoInteraceing;
            MTK_OMX_LOGD("mIsInterlacing %d", mIsInterlacing);
            if (mIsInterlacing == OMX_TRUE)
            {
                mOutputPortFormat.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatVendorMTKYUV_FCM;
                mOutputPortDef.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatVendorMTKYUV_FCM;
                mInterlaceChkComplete = OMX_TRUE;
                rResult = VDEC_DRV_MRESULT_RESOLUTION_CHANGED; // to go port reconfig flow.
            }
        }
    }


    if (OMX_FALSE == mPortReconfigInProgress && mFullSpeedOn)
    {
        int on = 0;
        mFullSpeedOn = false;
    }

#if 0 // Morris CTS test (early EOS)
    if (OMX_TRUE == mEarlyEOS)
    {
        // flush decoder
        mMtkV4L2Device.StreamOffBitstream();
        mMtkV4L2Device.StreamOffFrameBuffer();
        FlushDecoder(OMX_TRUE);

        MTK_OMX_LOGD("%s EOS flush", GetVDECSrting(mCodecId, NULL));
        DumpFTBQ();

        mEarlyEOS = OMX_FALSE;   // for eosFlushSeek of AdaptivePlaybacKTest
        //return OMX_TRUE;
        goto DECODE_SUCCESS;
    }
#endif
    bRet = OMX_TRUE;


DECODE_SUCCESS:
    *needEarlyExit = OMX_TRUE;
    return OMX_TRUE;

DECODE_FAIL:

    *needEarlyExit = OMX_TRUE;
    return OMX_FALSE;

}
OMX_BOOL MtkOmxVdec::DecodeVideoEx(OMX_BUFFERHEADERTYPE *pInputBuf)
{
    ATRACE_CALL();
    OMX_BOOL bRet = OMX_TRUE;

    static int64_t _in_time_1 = 0;
    static int64_t _in_time_2 = 0;
    static int64_t _out_time = 0;
    _in_time_1 = getTickCountMs();

    if (mDecoderInitCompleteFlag == OMX_FALSE)     // decode config buffer (opaque data)
    {
        // We didn't init codec yet.
        OMX_BOOL needEarlyExit = OMX_FALSE;
        bRet = initCodec(pInputBuf, &needEarlyExit);

        if (OMX_TRUE == needEarlyExit)
        {
            return bRet;
        }
    }
    else if (mFATALError == OMX_TRUE)
    {
        // Something went wrong during decoding. EBD right away.
        HandleEmptyBufferDone(pInputBuf);
    }
    else
    {
        // Normal decode routine
        OMX_BOOL needEarlyExit = OMX_FALSE;
        bRet = decodeRoutine(pInputBuf, &needEarlyExit);
        if (OMX_TRUE == needEarlyExit)
        {
            return bRet;
        }

    }

    //MTK_OMX_LOGD("DecodeVideoEx() --");

    return OMX_TRUE;
}

#if 0
OMX_BOOL MtkOmxVdec::FlushDecoder(OMX_BOOL mBufFlag_EOS)
{
    MTK_OMX_LOGD("+MtkOmxVdec::FlushDecoder");
    int ret;

    if (mDecoderInitCompleteFlag == OMX_FALSE)
    {
        MTK_OMX_LOGD("-MtkOmxVdec::FlushDecoder - decoder not init !!!");

        if (mMJCEnable == OMX_TRUE)
        {
            MJCFlushAllBufQ();
        }

        return OMX_TRUE;
    }

    //for MTK SUPPORT MJPEG [
    if (mCodecId == MTK_VDEC_CODEC_ID_MJPEG)
    {
        //Bruce 20120829 clean hang due to race condition of WAIT(mDecodeSem)
        if (get_sem_value(&mDecodeSem) > 0)
        {
            LOCK(mWaitDecSemLock);
            while (get_sem_value(&mDecodeSem) > 0)
            {
                WAIT(mDecodeSem);
            }
            UNLOCK(mWaitDecSemLock);
        }
        MTK_OMX_LOGD("No Hang");
        MTK_OMX_LOGD("-MtkOmxVdec::FlushDecoder");

        if (OMX_TRUE == mMJCEnable && mBufFlag_EOS == OMX_FALSE)
        {
            MJCFlushAllBufQ();
        }

        return OMX_TRUE;
    }
    //] MJPEG

    int bNeedWaitBSFlushDone = 0;
    int bNeedWaitFBFlushDone = 0;

    //mMtkV4L2Device.StreamOffBitstream();
    //mMtkV4L2Device.StreamOffFrameBuffer();


	if (OMX_FALSE == mPortReconfigInProgress)
	{
	    mMtkV4L2Device.flushBitstreamQ(&bNeedWaitBSFlushDone);
	    if (1 == bNeedWaitBSFlushDone)
	    {
	        MTK_OMX_LOGD("+ Wait BS flush done");
	        WAIT(mFlushBitstreamBufferDoneSem);
	        MTK_OMX_LOGD("- Wait BS flush done");
	    }
	}

	mMtkV4L2Device.flushFrameBufferQ(&bNeedWaitFBFlushDone);
	//mMtkV4L2Device.StreamOffFrameBuffer();
    if (1 == bNeedWaitFBFlushDone)
    {
        MTK_OMX_LOGD("+ Wait FB flush done");
        WAIT(mFlushFrameBufferDoneSem);
        MTK_OMX_LOGD("- Wait FB flush done");
    }

    mMtkV4L2Device.dumpDebugInfo();

    iTSIn = 0;
    DisplayTSArray[0] = 0;

    //#ifdef MTK_CLEARMOTION_SUPPORT
    if (OMX_TRUE == mMJCEnable && mBufFlag_EOS == OMX_FALSE)
    {
        MJCFlushAllBufQ();
    }
    //#endif

    MTK_OMX_LOGD("-MtkOmxVdec::FlushDecoder");
    return OMX_TRUE;
}
#endif

bool myDescendingSort(OMX_TICKS i, OMX_TICKS j) {return i > j;}

OMX_BOOL MtkOmxVdec::ValidateAndRemovePTS(OMX_TICKS timeTS)
{
    auto position = std::find(mDescendingPTS.begin(), mDescendingPTS.end(), timeTS);
    if (position != mDescendingPTS.end()) // == myVector.end() means the element was not found
    {
        mDescendingPTS.erase(position);
        return OMX_TRUE;
    }

	MTK_OMX_LOGE("Dequeued PTS: %lld  NOT exist", timeTS);
    return OMX_FALSE;
}

OMX_BOOL MtkOmxVdec::InsertionSortForInputPTS(OMX_TICKS timeTS)
{
    //MTK_OMX_LOGD("InsertionSortForInputPTS ++, TSIn -> %d, PTS -> %lld", (int) iTSIn, timeTS);

    mDescendingPTS.push_back(timeTS);
    std::sort(mDescendingPTS.begin(), mDescendingPTS.end(), myDescendingSort);

#if 0
    MTK_OMX_LOGD("mDescendingPTS.size(%d)", mDescendingPTS.size());
    for (int i = 0; i < mDescendingPTS.size(); ++i)
    {
        MTK_OMX_LOGD("mDescendingPTS[%d] = %lld", i, mDescendingPTS[i]);
    }
#endif



#if 0
    OMX_S32 i, j;
    if (iTSIn >= MTK_VDEC_AVC_DEC_TIMESTAMP_ARRAY_SIZE)
    {
        MTK_OMX_LOGE("Input picture is over MAX.");
        return OMX_FALSE;
    }

    for (i = 0; i < iTSIn; i++)
    {
        if (DisplayTSArray[i] == timeTS)
        {
            break;    // still insert;
        }
        else if (DisplayTSArray[i] > timeTS)
        {
            break;
        }
    }

    for (j = iTSIn; j > i; j--)
    {
        DisplayTSArray[j] = DisplayTSArray[j - 1];
    }
    DisplayTSArray[i] = timeTS;

#if 0
    for (j = 0; j <= iTSIn; j++)
    {
        MTK_OMX_LOGD("return input PTS[%2d] -> %10lld", (int)j, DisplayTSArray[j]);
    }
#endif

#endif
    iTSIn++;

    //MTK_OMX_LOGD("InsertionSortForInputPTS --");
    return OMX_TRUE;
}

OMX_TICKS MtkOmxVdec::RemoveForInputPTS()
{
    //MTK_OMX_LOGD("RemoveForInputPTS ++, index -> %2d, DisplayTSArray[0] -> %lld", (int)iTSIn, DisplayTSArray[0]);

    OMX_TICKS PTS = mDescendingPTS[mDescendingPTS.size() - 1];
    mDescendingPTS.pop();
    iTSIn--;

    MTK_OMX_LOGD("pop. mDescendingPTS.size(%d), PTS(%lld)", mDescendingPTS.size(), PTS);

#if 0
    OMX_S32 i;
    OMX_TICKS PTS = DisplayTSArray[0];

    if (iTSIn > 0)
    {
        for (i = 1; i < iTSIn; i++)
        {
            DisplayTSArray[i - 1] = DisplayTSArray[i];
        }
        iTSIn--;
    }

#if 0
    for (i = 0; i < iTSIn; i++)
    {
        MTK_OMX_LOGD("return input PTS[%2d] -> %10lld", (int)i, DisplayTSArray[i]);
    }
#endif

    //MTK_OMX_LOGD("RemoveForInputPTS --");
#endif

    return PTS;
}

OMX_BOOL MtkOmxVdec::RemoveForInputAtPTS(OMX_TICKS timeTS)
{
    //MTK_OMX_LOGD("RemoveForInputAtPTS ++, index -> %2d, timeTS -> %lld", (int)iTSIn, timeTS);

    OMX_TICKS PTS = mDescendingPTS[0];

    if (std::find(mDescendingPTS.begin(), mDescendingPTS.end(), timeTS) != mDescendingPTS.end())
    {
        mDescendingPTS.erase(std::find(mDescendingPTS.begin(), mDescendingPTS.end(), timeTS));

        iTSIn--;

        MTK_OMX_LOGD("erase. mDescendingPTS.size(%d)", mDescendingPTS.size());
        for (int i = 0; i < mDescendingPTS.size(); ++i)
        {
            MTK_OMX_LOGD("erase. mDescendingPTS[%d] = %lld", i, mDescendingPTS[i]);
        }
    }
    else
    {
        if (timeTS != 0)
        {
            MTK_OMX_LOGE("RFIAtPTS not found %lld", timeTS);
        }
    }



#if 0

    OMX_S32 i, j;
    OMX_TICKS PTS = DisplayTSArray[0];

    for (i = 0; i < iTSIn; i++)
    {
        if (DisplayTSArray[i] == timeTS)
        {
            break;    // found;
        }
    }

    if (i < iTSIn)
    {
        for (j = i; j < iTSIn - 1; j++)
        {
            DisplayTSArray[j] = DisplayTSArray[j + 1];
        }
        iTSIn--;
    }
    else
    {
        if (timeTS != 0)
        {
            MTK_OMX_LOGE("RFIAtPTS not found %lld", timeTS);
        }
    }

#if 0
    for (i = 0; i < iTSIn; i++)
    {
        MTK_OMX_LOGD("return input PTS[%2d] -> %10lld", (int)i, DisplayTSArray[i]);
    }
#endif

#endif

    //MTK_OMX_LOGD("RemoveForInputAtPTS --");
    return OMX_TRUE;
}

OMX_BOOL MtkOmxVdec::isUFOSupported(VAL_BOOL_T *UFOSupport)
{
    // V4L2 todo: query UFO from codec
    *UFOSupport = OMX_FALSE;
    int isUFOSupport = 0;
    if (0 == mMtkV4L2Device.getUFOCapability(&isUFOSupport))
    {
        MTK_OMX_LOGE("[MtkOmxVdec] Get decoder property, VDEC_DRV_QUERY_TYPE_UFO_SUPPORT: %d, %d", UFOSupport, __LINE__);
    }
    else
    {
        *UFOSupport = (isUFOSupport == 1)? OMX_TRUE : OMX_FALSE;
    }

    return OMX_TRUE;
}

OMX_BOOL MtkOmxVdec::QueryDriverFormat(VDEC_DRV_QUERY_VIDEO_FORMAT_T *pQinfoOut)
{
    VAL_UINT32_T is_support;
    VDEC_DRV_QUERY_VIDEO_FORMAT_T  qinfo;
    VDEC_DRV_QUERY_VIDEO_FORMAT_T   *pQinfoIn = &qinfo;
    MTK_OMX_LOGE("[MtkOmxVdec] QueryDriverFormat\n");

    pQinfoIn->u4VideoFormat = GetVdecFormat(mCodecId);
    pQinfoIn->u4Resolution = VDEC_DRV_RESOLUTION_SUPPORT_720P;
    pQinfoIn->u4Width = mOutputPortDef.format.video.nFrameWidth;
    pQinfoIn->u4Height = mOutputPortDef.format.video.nFrameHeight;

    switch (mCodecId)
    {
        case MTK_VDEC_CODEC_ID_HEVC:
            pQinfoIn->u4VideoFormat = VDEC_DRV_VIDEO_FORMAT_H265;
            pQinfoIn->u4Profile = VDEC_DRV_H265_VIDEO_PROFILE_H265_MAIN;
            pQinfoIn->u4Level = VDEC_DRV_VIDEO_LEVEL_UNKNOWN;
            break;
        case MTK_VDEC_CODEC_ID_MPEG4:
            pQinfoIn->u4VideoFormat = VDEC_DRV_VIDEO_FORMAT_MPEG4;
            pQinfoIn->u4Profile = VDEC_DRV_MPEG_VIDEO_PROFILE_MPEG4_ADVANCED_SIMPLE;
            pQinfoIn->u4Level = VDEC_DRV_VIDEO_LEVEL_5;
            break;
        case MTK_VDEC_CODEC_ID_H263:
            pQinfoIn->u4VideoFormat = VDEC_DRV_VIDEO_FORMAT_H263;
            pQinfoIn->u4Profile = VDEC_DRV_MPEG_VIDEO_PROFILE_H263_0;
            pQinfoIn->u4Level = VDEC_DRV_VIDEO_LEVEL_4;
            break;
        case MTK_VDEC_CODEC_ID_RV:
            pQinfoIn->u4VideoFormat = VDEC_DRV_VIDEO_FORMAT_REALVIDEO9;
            pQinfoIn->u4Profile = VDEC_DRV_MPEG_VIDEO_PROFILE_MPEG4_SIMPLE;
            pQinfoIn->u4Level = VDEC_DRV_VIDEO_LEVEL_3;
            break;
        case MTK_VDEC_CODEC_ID_AVC:
            pQinfoIn->u4VideoFormat = VDEC_DRV_VIDEO_FORMAT_H264;
            pQinfoIn->u4Profile = VDEC_DRV_H264_VIDEO_PROFILE_H264_HIGH;
            pQinfoIn->u4Level = VDEC_DRV_VIDEO_LEVEL_3_1;
            break;
        case MTK_VDEC_CODEC_ID_VC1:
            pQinfoIn->u4VideoFormat = VDEC_DRV_VIDEO_FORMAT_VC1;
            pQinfoIn->u4Profile = VDEC_DRV_MS_VIDEO_PROFILE_VC1_ADVANCED;
            pQinfoIn->u4Level = VDEC_DRV_VIDEO_LEVEL_3;
            break;
        case MTK_VDEC_CODEC_ID_VPX:
            pQinfoIn->u4VideoFormat = VDEC_DRV_VIDEO_FORMAT_VP8;
            pQinfoIn->u4Profile = VDEC_DRV_MPEG_VIDEO_PROFILE_MPEG4_SIMPLE;
            pQinfoIn->u4Level = VDEC_DRV_VIDEO_LEVEL_MEDIUM;
            break;
        case MTK_VDEC_CODEC_ID_VP9:
            pQinfoIn->u4VideoFormat = VDEC_DRV_VIDEO_FORMAT_VP9;
            pQinfoIn->u4Profile = VDEC_DRV_VP9_VIDEO_PROFILE_0;
            pQinfoIn->u4Level = VDEC_DRV_VIDEO_LEVEL_3;
            break;
        case MTK_VDEC_CODEC_ID_MPEG2:
            pQinfoIn->u4VideoFormat = VDEC_DRV_VIDEO_FORMAT_MPEG2;
            pQinfoIn->u4Profile = VDEC_DRV_MPEG_VIDEO_PROFILE_MPEG2_MAIN;
            pQinfoIn->u4Level = VDEC_DRV_VIDEO_LEVEL_MEDIUM;
            break;
        case MTK_VDEC_CODEC_ID_DIVX:
            pQinfoIn->u4VideoFormat = VDEC_DRV_VIDEO_FORMAT_DIVX4;
            pQinfoIn->u4Profile = VDEC_DRV_MPEG_VIDEO_PROFILE_MPEG4_ADVANCED_SIMPLE;
            pQinfoIn->u4Level = VDEC_DRV_VIDEO_LEVEL_5;
            break;
        case MTK_VDEC_CODEC_ID_DIVX3:
            pQinfoIn->u4VideoFormat = VDEC_DRV_VIDEO_FORMAT_DIVX311;
            pQinfoIn->u4Profile = VDEC_DRV_MPEG_VIDEO_PROFILE_MPEG4_ADVANCED_SIMPLE;
            pQinfoIn->u4Level = VDEC_DRV_VIDEO_LEVEL_5;
            break;
        case MTK_VDEC_CODEC_ID_XVID:
            pQinfoIn->u4VideoFormat = VDEC_DRV_VIDEO_FORMAT_XVID;
            pQinfoIn->u4Profile = VDEC_DRV_MPEG_VIDEO_PROFILE_MPEG4_ADVANCED_SIMPLE;
            pQinfoIn->u4Level = VDEC_DRV_VIDEO_LEVEL_5;
            break;
        case MTK_VDEC_CODEC_ID_S263:
            pQinfoIn->u4VideoFormat = VDEC_DRV_VIDEO_FORMAT_S263;
            pQinfoIn->u4Profile = VDEC_DRV_MPEG_VIDEO_PROFILE_MPEG4_ADVANCED_SIMPLE;
            pQinfoIn->u4Level = VDEC_DRV_VIDEO_LEVEL_5;
            break;
        case MTK_VDEC_CODEC_ID_MJPEG:
#define MJ_MAX_W    1280
#define MJ_MAX_H    720
            pQinfoOut->u4VideoFormat = VDEC_DRV_VIDEO_FORMAT_MJPEG;
            if (pQinfoIn->u4Width * pQinfoIn->u4Height > MJ_MAX_W * MJ_MAX_H)
            {
                pQinfoOut->u4Width = MJ_MAX_W;
                pQinfoOut->u4Height = MJ_MAX_H;
                return OMX_FALSE;
            }
            pQinfoOut->u4Width = pQinfoIn->u4Width;
            pQinfoOut->u4Height = pQinfoIn->u4Height;
            pQinfoOut->u4StrideAlign = 32;
            pQinfoOut->u4SliceHeightAlign = 16;
            return OMX_TRUE;
            break;
        default:
            break;
    }

    // query driver property
    if (0 == mMtkV4L2Device.checkVideoFormat(pQinfoIn, pQinfoOut))
    {
        return OMX_FALSE;
    }

    return OMX_TRUE;
}

uint32_t MtkOmxVdec::getV4L2Format(MTK_VDEC_CODEC_ID codecId)
{
    uint32_t pixel_format = 0xFFFFFFFF;

    switch (codecId)
    {
        case MTK_VDEC_CODEC_ID_AVC:
        {
            pixel_format = V4L2_PIX_FMT_H264;
            break;
        }

        case MTK_VDEC_CODEC_ID_H263:
        case MTK_VDEC_CODEC_ID_MPEG4:
        case MTK_VDEC_CODEC_ID_RV:
        case MTK_VDEC_CODEC_ID_VC1:
        case MTK_VDEC_CODEC_ID_VPX:
        case MTK_VDEC_CODEC_ID_VP9:
        case MTK_VDEC_CODEC_ID_MPEG2:
        case MTK_VDEC_CODEC_ID_DIVX:
        case MTK_VDEC_CODEC_ID_DIVX3:
        case MTK_VDEC_CODEC_ID_XVID:
        case MTK_VDEC_CODEC_ID_MJPEG:
        case MTK_VDEC_CODEC_ID_S263:
        case MTK_VDEC_CODEC_ID_HEVC:
        default:
            MTK_OMX_LOGE("Unsupported v4l2 video codec format(%d)", codecId);
            break;
    }

    return pixel_format;
}

OMX_BOOL MtkOmxVdec::InitVideoDecodeHW(OMX_S32 *aWidth, OMX_S32 *aHeight,
                                       OMX_S32 *aAspectRatioWidth, OMX_S32 *aAspectRatioHeight,
                                       OMX_U8 *aBuffer, OMX_U32 *aSize,
                                       OMX_BUFFERHEADERTYPE *pInputBuf)
{
    VDEC_DRV_MRESULT_T eResult;

    VAL_BOOL_T bUFOsupport = VAL_FALSE;

    VAL_BOOL_T bMJCOff = VAL_FALSE;
    char value[PROPERTY_VALUE_MAX], value2[PROPERTY_VALUE_MAX];

    MTK_OMX_MEMSET((void *)&mSeqInfo, 0x00, sizeof(VDEC_DRV_SEQINFO_T));
    if (*aSize == 0 && mCodecId != MTK_VDEC_CODEC_ID_MPEG4 && mCodecId != MTK_VDEC_CODEC_ID_DIVX
        && mCodecId != MTK_VDEC_CODEC_ID_DIVX3 && mCodecId != MTK_VDEC_CODEC_ID_XVID
        && mCodecId != MTK_VDEC_CODEC_ID_S263)
    {
        MTK_OMX_LOGE("Error!! Input buffer size %d", *aSize);
        return OMX_FALSE;
    }

    if (*aSize == 0 && (mCodecId == MTK_VDEC_CODEC_ID_MPEG4 || mCodecId == MTK_VDEC_CODEC_ID_DIVX ||
                        mCodecId == MTK_VDEC_CODEC_ID_DIVX3 || mCodecId == MTK_VDEC_CODEC_ID_XVID ||
                        mCodecId == MTK_VDEC_CODEC_ID_S263))
    {
        VAL_UINT32_T nFrameSize = (mOutputPortDef.format.video.nFrameWidth & 0x0000FFFF) | ((mOutputPortDef.format.video.nFrameHeight & 0x0000FFFF) << 16);
        MTK_OMX_LOGE("InitVideoDecodeHW() %d", nFrameSize);

        mMtkV4L2Device.setMPEG4FrameSize(&nFrameSize);
    }

    if (mThumbnailMode == OMX_TRUE)   // for SW and hybrid decoder
    {
        VDEC_DRV_SET_DECODE_MODE_T rtSetDecodeMode;
        rtSetDecodeMode.eDecodeMode = VDEC_DRV_DECODE_MODE_THUMBNAIL; // thumbnail mode
        rtSetDecodeMode.u4DisplayFrameNum = 0;
        rtSetDecodeMode.u4DropFrameNum = 0;
        mSeqInfo.bIsThumbnail = OMX_TRUE; //for 4k sw H264
        mMtkV4L2Device.setDecodeMode(&rtSetDecodeMode);
    }
    else
    {
        mSeqInfo.bIsThumbnail = OMX_FALSE;
    }

#ifdef MTK_SW_RESOLUTION_CHANGE_SUPPORT
    mIsResChg = OMX_FALSE;
#endif


    // init driver using sequance layer

    VBufInfo  info;

    if (OMX_TRUE == mIsSecureInst)
    {
        mRingbuf.rBase.u4VA  = 0;
        mRingbuf.rBase.u4PA  = 0;
        mRingbuf.rSecMemHandle = (OMX_U32)aBuffer;
        mRingbuf.rBase.u4Size = *aSize;
        mRingbuf.u4Read  = 0;
        mRingbuf.u4Write = (VAL_UINT32_T) * aSize;
    }
    else
    {
        if (OMX_TRUE == mInputUseION)
        {
            //VBufInfo  info;
            int ret = mInputMVAMgr->getOmxInfoFromVA(pInputBuf->pBuffer, &info);
            if (ret < 0)
            {
                MTK_OMX_LOGE("[ERROR][ION][Input][InitVideoDecodeHW], LINE: %d\n", __LINE__);
                return OMX_FALSE;
            }
            else
            {
                mRingbuf.rBase.u4VA = info.u4VA;
                mRingbuf.rBase.u4PA = info.u4PA;
                mRingbuf.rBase.u4Size = *aSize;
                mRingbuf.u4Read = info.u4VA;
                mRingbuf.u4Write = info.u4VA + (VAL_UINT32_T) * aSize;
            }
        }
        else
        {
            mRingbuf.rBase.u4VA  = (VAL_UINT32_T)(aBuffer);
            mRingbuf.rBase.u4PA  = (VAL_UINT32_T)(aBuffer);
            mRingbuf.rBase.u4Size = *aSize;
            mRingbuf.u4Read  = (VAL_UINT32_T)(aBuffer);
            mRingbuf.u4Write = mRingbuf.u4Read + (VAL_UINT32_T) * aSize;
        }
    }
    OMX_BOOL bNoReorderMode = OMX_FALSE;

    eResult = VDEC_DRV_MRESULT_OK;


    MTK_OMX_LOGD(" + v4l2 InitVideoDecodeHW...\n");


    // V4L2 Set format and QueueBuffer
    {
        unsigned int formatWidth, formatHeight, imageSize;
        int bitstramIndex;

        //mMtkV4L2Device.setFormatBistream(getV4L2Format(mCodecId), mInputPortDef.nBufferSize);
        //mMtkV4L2Device.setFormatFrameBuffer(0xFFFFFFFF); // v4l2 todo: 0xFFFFFFFF should be modified as the real format

        //mMtkV4L2Device.ReqBufsOutput(mInputPortDef.nBufferCountActual);

        //for (OMX_U32 i = 0 ; i < mInputPortDef.nBufferCountActual ; i++)
        //{
        //    if (pInputBuf == mInputBufferHdrs[i])
        //    {
        //        bitstramIndex = i;
        //    }
        //}

        //mMtkV4L2Device.queueBitstream(bitstramIndex, info.iIonFd, mRingbuf.rBase.u4Size, mInputPortDef.nBufferSize);
        //mMtkV4L2Device.StreamOnBitstream();


        //
        // Get sequence info
        //

        while (mMtkV4L2Device.getCapFmt(&formatWidth, &formatHeight) == 1)
        {
            MTK_OMX_LOGD("[Info] GetCapFmt not ready. Try again...\n");
            //Todo: should we add a time-out
        }

        imageSize = formatWidth * formatHeight;

        mSeqInfo.u4Width    = formatWidth;
        mSeqInfo.u4Height   = formatHeight;


        //
        // Todo: the following items should be modified as correct values
        //

        v4l2_crop temp_crop_info;
        mMtkV4L2Device.getCrop(&temp_crop_info);
        mSeqInfo.u4PicWidth     = temp_crop_info.c.width;
        mSeqInfo.u4PicHeight    = temp_crop_info.c.height;

        v4l2_aspect_ratio aspectRatio;
        mMtkV4L2Device.getAspectRatio(&aspectRatio);
        mSeqInfo.i4AspectRatioWidth     = aspectRatio.aspectRatioWidth;
        mSeqInfo.i4AspectRatioHeight    = aspectRatio.aspectRatioHeight;

        v4l2_bitdepth_info bitDepthInfo;
        mMtkV4L2Device.getBitDepthInfo(&bitDepthInfo);
        mSeqInfo.u4BitDepthLuma     = bitDepthInfo.bitDepthLuma;
        mSeqInfo.u4BitDepthChroma   = bitDepthInfo.bitDepthChroma;


        // V4L2 todo: need to add re-try mechanism
#if 0
        VDEC_DRV_QUERY_VIDEO_FORMAT_T qinfoOut;
        if (OMX_FALSE == QueryDriverFormat(&qinfoOut) || eResult == VDEC_DRV_MRESULT_FATAL)
        {
            mFATALError = OMX_TRUE;
        }
#endif

    }
	mEOSTS = -1;
	mEOSFound = mEOSQueued = OMX_FALSE;

    MTK_OMX_LOGD(" - v4l2 InitVideoDecodeHW...\n");
#if 0
    if ((eResult = eVDecDrvInit(mDrvHandle, &mRingbuf, &mSeqInfo)) != VDEC_DRV_MRESULT_OK)
    {
        MTK_OMX_LOGE("Error!! Cannot init driver");
        VDEC_DRV_QUERY_VIDEO_FORMAT_T qinfoOut;
        MTK_OMX_MEMSET(&qinfoOut, 0, sizeof(VDEC_DRV_QUERY_VIDEO_FORMAT_T));
        if (OMX_FALSE == QueryDriverFormat(&qinfoOut) || eResult == VDEC_DRV_MRESULT_FATAL)
        {
            mFATALError = OMX_TRUE;
        }
        goto ERROR;
    }
#endif

    mSeqInfoCompleteFlag = OMX_TRUE;

    // Because ViLTE only supports H.264 baseline profile which dones't have B frames,
    // we can enable no reorder mode directly.
    bNoReorderMode = (mViLTESupportOn == OMX_TRUE) ? OMX_TRUE : mNoReorderMode;

    // Set special decode mode to sw codecs
    {
        VDEC_DRV_SET_DECODE_MODE_T rtSetDecodeMode;
        rtSetDecodeMode.u4DisplayFrameNum = 0;
        rtSetDecodeMode.u4DropFrameNum = 0;

        if (mThumbnailMode == OMX_TRUE)   // for HW decoder
        {
            rtSetDecodeMode.eDecodeMode = VDEC_DRV_DECODE_MODE_THUMBNAIL; // thumbnail mode

        }
        else if (bNoReorderMode == OMX_TRUE)
        {
            rtSetDecodeMode.eDecodeMode = VDEC_DRV_DECODE_MODE_NO_REORDER; // no reorder mode
        }

        if (mSkipReferenceCheckMode == OMX_TRUE)
        {
            rtSetDecodeMode.eDecodeMode = VDEC_DRV_DECODE_MODE_SKIP_REFERENCE_CHECK; // no reorder mode
        }

        if (mLowLatencyDecodeMode == OMX_TRUE)
        {
            rtSetDecodeMode.eDecodeMode = VDEC_DRV_DECODE_MODE_LOW_LATENCY_DECODE; // no reorder mode
        }

        mMtkV4L2Device.setDecodeMode(&rtSetDecodeMode);
    }

    *aWidth  = (OMX_S32)mSeqInfo.u4PicWidth;
    *aHeight = (OMX_S32)mSeqInfo.u4PicHeight;

    //MTK_OMX_LOGD("InitVideoDecodeHW *aWidth = %u, *aHeight = %u, mCodecId(0x%X)", (unsigned int)*aWidth, (unsigned int)*aHeight, mCodecId);

    if ((aAspectRatioWidth != NULL) && (aAspectRatioHeight != NULL))
    {
        *aAspectRatioWidth = (OMX_S32)mSeqInfo.i4AspectRatioWidth;
        *aAspectRatioHeight = (OMX_S32)mSeqInfo.i4AspectRatioHeight;
        //MTK_OMX_LOGD ("InitVideoDecodeHW *aAspectRatioWidth = %d, *aAspectRatioHeight = %d", (int)*aAspectRatioWidth, (int)*aAspectRatioHeight);
    }

    if (mSeekTargetTime > 0 && mSeekMode == OMX_FALSE)
    {
        mSeekMode = OMX_TRUE;
        VDEC_DRV_SET_DECODE_MODE_T rtSetDecodeMode;
        rtSetDecodeMode.u4DisplayFrameNum = 0;
        rtSetDecodeMode.u4DropFrameNum = 0;
        rtSetDecodeMode.eDecodeMode = VDEC_DRV_DECODE_MODE_B_SKIP; // seek mode

        mMtkV4L2Device.setDecodeMode(&rtSetDecodeMode);
    }


    VAL_UINT32_T is_support;
    MTK_OMX_MEMSET(&mQInfoOut, 0, sizeof(VDEC_DRV_QUERY_VIDEO_FORMAT_T));
    // query driver capability
    if (OMX_FALSE == QueryDriverFormat(&mQInfoOut))
    {
        MTK_OMX_LOGD("Error!! Cannot get decoder property.");
        mFATALError = OMX_TRUE;
        goto ERROR;
    }

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
            //MTK_OMX_LOGE("InitVideoDecodeHW:VDEC_DRV_PIXEL_FORMAT_YUV_YV12");
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
            break;
    }

    if (mFixedMaxBuffer == OMX_TRUE)
    {
        mOutputPortDef.format.video.nFrameWidth = mQInfoOut.u4Width;
        mOutputPortDef.format.video.nFrameHeight = mQInfoOut.u4Height;
        MTK_OMX_LOGD("***mFixedMaxBuffer***(%d), mOutputPortDef.format.video.nFrameWidth(%d), mOutputPortDef.format.video.nFrameHeight(%d)", __LINE__, mOutputPortDef.format.video.nFrameWidth, mOutputPortDef.format.video.nFrameHeight);
    }
    mOutputPortDef.format.video.nStride = VDEC_ROUND_N(mOutputPortDef.format.video.nFrameWidth, mQInfoOut.u4StrideAlign);
    mOutputPortDef.format.video.nSliceHeight = VDEC_ROUND_N(mOutputPortDef.format.video.nFrameHeight, mQInfoOut.u4SliceHeightAlign);
    meDecodeType = mQInfoOut.eDecodeType;
    if (meDecodeType == VDEC_DRV_DECODER_MTK_HARDWARE)
    {
        mPropFlags &= ~MTK_OMX_VDEC_ENABLE_PRIORITY_ADJUSTMENT;
        //MTK_OMX_LOGD("MtkOmxVdec::SetConfig -> disable priority adjustment");
    }
    else
    {
        mPropFlags |= MTK_OMX_VDEC_ENABLE_PRIORITY_ADJUSTMENT;
        //MTK_OMX_LOGD("MtkOmxVdec::SetConfig -> enable priority adjustment");
    }

    MTK_OMX_LOGD("InitVideoDecodeHW(): meDecodeType=%d, mThumbnailMode=%d, color format=%x, nStride=%d, nSliceHeight=%d ",
                 meDecodeType, mThumbnailMode, mOutputPortFormat.eColorFormat, mOutputPortDef.format.video.nStride, mOutputPortDef.format.video.nSliceHeight);

    // query chip name
    VAL_UINT32_T u4ChipName;
    if (0 == mMtkV4L2Device.getChipName(&u4ChipName))
    {
        MTK_OMX_LOGE("Error!! Cannot get decoder property, VDEC_DRV_QUERY_TYPE_CHIP_NAME");
        goto ERROR;
    }

    switch (u4ChipName)
    {
        case VAL_CHIP_NAME_MT6516:
            MTK_OMX_LOGE("VAL_CHIP_NAME_MT6516");
            break;
        case VAL_CHIP_NAME_MT6571:
            MTK_OMX_LOGE("VAL_CHIP_NAME_MT6571");
            break;
        case VAL_CHIP_NAME_MT6572:
            MTK_OMX_LOGE("VAL_CHIP_NAME_MT6572");
            break;
        case VAL_CHIP_NAME_MT6573:
            MTK_OMX_LOGE("VAL_CHIP_NAME_MT6573");
            break;
        case VAL_CHIP_NAME_MT6575:
            MTK_OMX_LOGE("VAL_CHIP_NAME_MT6575");
            break;
        case VAL_CHIP_NAME_MT6577:
            MTK_OMX_LOGE("VAL_CHIP_NAME_MT6577");
            break;
        case VAL_CHIP_NAME_MT6589:
            MTK_OMX_LOGE("VAL_CHIP_NAME_MT6589");
            break;
        case VAL_CHIP_NAME_MT6582:
            MTK_OMX_LOGE("VAL_CHIP_NAME_MT6582");
            break;
        case VAL_CHIP_NAME_MT8135:
            MTK_OMX_LOGE("VAL_CHIP_NAME_MT8135");
            break;
        case VAL_CHIP_NAME_ROME:
            MTK_OMX_LOGE("VAL_CHIP_NAME_ROME");
            break;
        case VAL_CHIP_NAME_MT6592:
            MTK_OMX_LOGE("VAL_CHIP_NAME_MT6592");
            break;
        case VAL_CHIP_NAME_MT8127:
            MTK_OMX_LOGE("VAL_CHIP_NAME_MT8127");
            break;
        case VAL_CHIP_NAME_MT6752:
            MTK_OMX_LOGE("VAL_CHIP_NAME_MT6752");
            break;
        case VAL_CHIP_NAME_MT6795:
            MTK_OMX_LOGE("VAL_CHIP_NAME_MT6795");
            break;
        case VAL_CHIP_NAME_DENALI_1:
            MTK_OMX_LOGE("VAL_CHIP_NAME_DENALI_1");
            break;
        case VAL_CHIP_NAME_DENALI_2:
            MTK_OMX_LOGE("VAL_CHIP_NAME_DENALI_2");
            break;
        case VAL_CHIP_NAME_DENALI_3:
            MTK_OMX_LOGE("VAL_CHIP_NAME_DENALI_3");
            break;
        case VAL_CHIP_NAME_MT6580:
            MTK_OMX_LOGE("VAL_CHIP_NAME_MT6580");
            break;
        case VAL_CHIP_NAME_MT8163:
            MTK_OMX_LOGE("VAL_CHIP_NAME_MT8163");
            break;
        case VAL_CHIP_NAME_MT6755:
            MTK_OMX_LOGE("VAL_CHIP_NAME_MT6755");
            break;
        case VAL_CHIP_NAME_MT6797:
            MTK_OMX_LOGE("VAL_CHIP_NAME_MT6797");
            break;
        case VAL_CHIP_NAME_MT8173:
            MTK_OMX_LOGE("VAL_CHIP_NAME_MT8173");
            break;
        default:
            MTK_OMX_LOGE("[ERROR] VAL_CHIP_NAME_UNKNOWN");
            goto ERROR;
            break;
    }

    mBitstreamBufferSize = 1024 * 1000;

    isUFOSupported(&bUFOsupport);

    // check whether to enable UFO right after driver init
    property_get("sys.display.clearMotion.dimmed", value, "0");
    property_get("persist.sys.display.clearMotion", value2, "0");

    if (atoi(value) == 1 || atoi(value2) == 0)
    {
        MTK_OMX_LOGD("Enable UFO (MJC is off)(OMX_IndexParamPortDefinition)");
        bMJCOff = VAL_TRUE;
    }

    if ((mCodecId == MTK_VDEC_CODEC_ID_HEVC || mCodecId == MTK_VDEC_CODEC_ID_AVC || mCodecId == MTK_VDEC_CODEC_ID_VP9) && mIsUsingNativeBuffers == OMX_TRUE
        && bUFOsupport == OMX_TRUE &&
        ((mOutputPortDef.format.video.nFrameWidth * mOutputPortDef.format.video.nFrameHeight > 1920 * 1088) || ((mOutputPortDef.format.video.nFrameWidth * mOutputPortDef.format.video.nFrameHeight >= 1920 * 1080) && bMJCOff == VAL_TRUE && OMX_FALSE == mIsSecureInst)))
    {
        VAL_CHAR_T UFOSetting[PROPERTY_VALUE_MAX];
        OMX_BOOL UFOEnable = OMX_TRUE;
        property_get("mtk.omxvdec.ufo", UFOSetting, "1");
        UFOEnable = (OMX_BOOL) atoi(UFOSetting);
        if (UFOEnable == OMX_TRUE)
        {
            if (mSeqInfo.u4BitDepthLuma == 10 || mSeqInfo.u4BitDepthChroma == 10)
            {
                if (OMX_TRUE == mSeqInfo.bIsHorizontalScaninLSB)
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

            mMtkV4L2Device.setUFOOn();
            MTK_OMX_LOGD("Enable UFO (OMX_IndexParamPortDefinition)");
        }
    }

#if 0//def MTK S3D SUPPORT
    return AsvdInit();
#else
    return OMX_TRUE;
#endif

ERROR:

    mMtkV4L2Device.deinitialize();
    return OMX_FALSE;
}

OMX_BOOL MtkOmxVdec::DeInitVideoDecodeHW()
{
    MTK_OMX_LOGD("+DeInitVideoDecodeHW");

    if (mDecoderInitCompleteFlag == OMX_TRUE)
    {
        //mMtkV4L2Device.deinitialize();
        mDecoderInitCompleteFlag = OMX_FALSE;
        mSeqInfoCompleteFlag = OMX_FALSE;
    }

	mMtkV4L2Device.reset();

    MTK_OMX_LOGD("-DeInitVideoDecodeHW");
    return OMX_TRUE;
}

char *MtkOmxVdec::GetVDECSrting(MTK_VDEC_CODEC_ID mCodecId, void *pBuffer)
{
    OMX_U32 NalType;
    switch (mCodecId)
    {
        case MTK_VDEC_CODEC_ID_HEVC:
            if (OMX_FALSE == mIsSecureInst && pBuffer != NULL)
            {
                NalType = *((unsigned char *) pBuffer + 4) / 2;
                sprintf(mCodecSTR, "HEVC DEC, NAL %d", NalType);
            }
            else
            {
                sprintf(mCodecSTR, "HEVC DEC");
            }
            break;
        case MTK_VDEC_CODEC_ID_DIVX:
            sprintf(mCodecSTR, "DIVX DEC");
            break;
        case MTK_VDEC_CODEC_ID_DIVX3:
            sprintf(mCodecSTR, "DIVX3 DEC");
            break;
        case MTK_VDEC_CODEC_ID_XVID:
            sprintf(mCodecSTR, "XVID DEC");
            break;
        case MTK_VDEC_CODEC_ID_S263:
            sprintf(mCodecSTR, "S263 DEC");
            break;
        case MTK_VDEC_CODEC_ID_MPEG4:
            sprintf(mCodecSTR, "MPEG4 DEC");
            break;
        case MTK_VDEC_CODEC_ID_H263:
            sprintf(mCodecSTR, "H263 DEC");
            break;
        case MTK_VDEC_CODEC_ID_MPEG2:
            sprintf(mCodecSTR, "MPEG2 DEC");
            break;
        case MTK_VDEC_CODEC_ID_RV:
            sprintf(mCodecSTR, "RV DEC");
            break;
        case MTK_VDEC_CODEC_ID_AVC:
            if (OMX_FALSE == mIsSecureInst && pBuffer != NULL)
            {
                if (*((unsigned char *) pBuffer) == 0x00)
                {
                    OMX_U32 i = 0;
                    while (i < 10)
                    {
                        if (*(((unsigned char *) pBuffer) + i) != 0)
                        {
                            if (*(((unsigned char *) pBuffer) + i) == 0x01)
                            {
                                NalType = *(((unsigned char *) pBuffer) + i + 1) & 0x1F;
                            }
                            break;
                        }
                        i++;
                    }
                }
                else
                {
                    NalType = *((unsigned char *) pBuffer) & 0x1F;
                }
                sprintf(mCodecSTR, "AVC DEC, NAL %d", NalType);
            }
            else
            {
                sprintf(mCodecSTR, "AVC DEC");
            }
            break;
        case MTK_VDEC_CODEC_ID_VC1:
            sprintf(mCodecSTR, "VC1 DEC");
            break;
        case MTK_VDEC_CODEC_ID_VPX:
            sprintf(mCodecSTR, "VPX DEC");
            break;
        case MTK_VDEC_CODEC_ID_VP9:
            sprintf(mCodecSTR, "VP9 DEC");
            break;
        case MTK_VDEC_CODEC_ID_MJPEG:
            sprintf(mCodecSTR, "MJPEG DEC");
            break;
        default:
            sprintf(mCodecSTR, "Unknown Codec");
            break;
    }
    return mCodecSTR;
}



char* MtkOmxVdec::GetParameterSrting(OMX_INDEXTYPE nParamIndex)
{
    switch (nParamIndex)
    {
        case OMX_IndexParamPortDefinition:
    	{
            sprintf(mParameterSTR, "PortDefinition");
    	}
		break;

        case OMX_IndexParamVideoPortFormat:
        {
            sprintf(mParameterSTR, "VideoPortFormat");
        }
		break;

        case OMX_IndexParamStandardComponentRole:
        {
            sprintf(mParameterSTR, "StandardComponentRole");
        }
		break;

        case OMX_IndexParamVideoRv:
        {
            sprintf(mParameterSTR, "VideoRv");
        }
		break;

        case OMX_IndexVendorMtkOmxVdecThumbnailMode:
        {
            sprintf(mParameterSTR, "tkOmxVdecThumbnailMode");
        }
		break;

        case OMX_IndexVendorMtkOmxVdecUseClearMotion:
    	{
			sprintf(mParameterSTR, "UseClearMotion");
    	}
		break;

        case OMX_IndexVendorMtkOmxVdecGetMinUndequeuedBufs:
        {
            sprintf(mParameterSTR, "GetMinUndequeuedBufs");
        }
		break;

        case OMX_IndexVendorMtkOmxVdecStreamingMode:
        {
            sprintf(mParameterSTR, "StreamingMode");
        }
		break;

        case OMX_IndexVendorMtkOmxVdecACodecColorConvertMode:
        {
            sprintf(mParameterSTR, "ACodecColorConvertMode");
        }
		break;

        case OMX_IndexVendorMtkOmxVdecFixedMaxBuffer:
        {
            sprintf(mParameterSTR, "FixedMaxBuffer");
        }
		break;

        case OMX_IndexVendorMtkOmxVideoUseIonBuffer:
        {
            sprintf(mParameterSTR, "UseIonBuffer");
		}
		break;

        case OMX_IndexVendorMtkOmxVideoSetClientLocally:
        {
            sprintf(mParameterSTR, "SetClientLocally");
        }

        case OMX_IndexVendorMtkOmxVdecNoReorderMode:
        {
            sprintf(mParameterSTR, "NoReorderMode");
        }

        case OMX_IndexVendorMtkOmxVdecSkipReferenceCheckMode:
        {
            sprintf(mParameterSTR, "SkipReferenceCheckMode");
        }

        case OMX_IndexVendorMtkOmxVdecLowLatencyDecode:
        {
            sprintf(mParameterSTR, "LowLatencyDecode");
        }

        case OMX_IndexVendorMtkOmxVdecUse16xSlowMotion:
        {
            sprintf(mParameterSTR, "Use16xSlowMotion");

        }
        break;

        case OMX_IndexVendorMtkOmxVdecSetScenario:
        {
            sprintf(mParameterSTR, "SetScenario");
        }
		break;

        case OMX_GoogleAndroidIndexEnableAndroidNativeBuffers:
        {
            sprintf(mParameterSTR, "EnableAndroidNativeBuffers");
        }
        break;

        case OMX_GoogleAndroidIndexUseAndroidNativeBuffer:
        {
            sprintf(mParameterSTR, "UseAndroidNativeBuffer");
        }
        break;

        case OMX_GoogleAndroidIndexStoreMetaDataInBuffers:
        {
            sprintf(mParameterSTR, "StoreMetaDataInBuffers");
        }
        break;

        case OMX_GoogleAndroidIndexstoreANWBufferInMetadata:
        {
            sprintf(mParameterSTR, "storeANWBufferInMetadata");
        }
        break;

        case OMX_IndexParamVideoInit:
        {
            sprintf(mParameterSTR, "VideoInit");
        }
        break;

		case OMX_IndexParamAudioInit:
        {
            sprintf(mParameterSTR, "AudioInit");
        }
        break;

		case OMX_IndexParamImageInit:
        {
            sprintf(mParameterSTR, "ImageInit");
        }
        break;

		case OMX_IndexParamOtherInit:
        {
            sprintf(mParameterSTR, "OtherInit");
        }
        break;

        case OMX_IndexParamVideoProfileLevelQuerySupported:
        {
			sprintf(mParameterSTR, "VideoProfileLevelQuerySupported");
        }
		break;

        case OMX_IndexVendorMtkOmxVdecVideoSpecQuerySupported:
		{
			sprintf(mParameterSTR, "VideoSpecQuerySupported");
    	}
		break;

        case OMX_IndexVendorMtkOmxPartialFrameQuerySupported:
        {
            sprintf(mParameterSTR, "PartialFrameQuerySupported");
        }
        break;

        case OMX_IndexVendorMtkOmxVdecGetColorFormat:
        {
            sprintf(mParameterSTR, "GetColorFormat");
        }
		break;

        case OMX_GoogleAndroidIndexDescribeColorFormat:
        {
            sprintf(mParameterSTR, "DescribeColorFormat");
        }
		break;

        case OMX_IndexVendorMtkOmxHandle:
        {
            sprintf(mParameterSTR, "MtkOmxHandle");
        }
        break;

        case OMX_GoogleAndroidIndexGetAndroidNativeBufferUsage:
        {
            sprintf(mParameterSTR, "GetAndroidNativeBufferUsage");
        }
        break;

        default:
        {
            sprintf(mParameterSTR, "unsupported nParamIndex");
        }
		break;
    }
    return mParameterSTR;
}



OMX_BUFFERHEADERTYPE *MtkOmxVdec::GetDisplayBuffer(OMX_BOOL bGetResolution, VDEC_DRV_FRAMEBUF_T **ppFrameBuf, OMX_TICKS *timestamp, OMX_S32 *isLastFrame)
{
    VAL_ULONG_T tmp_frame_addr = 0;
    VAL_UINT32_T i, cnt;
    //VDEC_DRV_FRAMEBUF_T *pFrameBuf = NULL;

    int frameBufferIndex = -1;
    int GotLastFrame = 0;

    mMtkV4L2Device.dequeueFrameBuffer(&frameBufferIndex, (long long *)timestamp, (int *)isLastFrame);

    if (-1 == frameBufferIndex)
    {
        //MTK_OMX_LOGD("GetDisplayBuffer() -> No YUV was dequeued...");
        return NULL;
    }

    OMX_BUFFERHEADERTYPE *ipOutputBuffer = mOutputBufferHdrs[frameBufferIndex];

    LOCK(mFillThisBufQLock);
    for (i = 0; i < mOutputPortDef.nBufferCountActual; i++)
    {
        if (mFrameBuf[i].ipOutputBuffer == ipOutputBuffer)
        {
            tmp_frame_addr = (VAL_ULONG_T)(&mFrameBuf[i].frame_buffer);
            //MTK_OMX_LOGD("Ready to return 0x%08x from GetDisplayBuffer", mFrameBuf[i].ipOutputBuffer);
            break;
        }
    }
    UNLOCK(mFillThisBufQLock);

    if (0 != tmp_frame_addr)
    {
        *ppFrameBuf = (VDEC_DRV_FRAMEBUF_T *)tmp_frame_addr;
        MTK_OMX_LOGE("[TEST] GetDisplay pFrameBuf = 0x%x, VA = 0x%x, MVA = 0x%x", *ppFrameBuf, (*ppFrameBuf)->rBaseAddr.u4VA, (*ppFrameBuf)->rBaseAddr.u4PA);


        MTK_OMX_LOGE("[TEST] GetDisplay[%d] VA = 0x%x, VA=0x%x", i, mFrameBuf[i].frame_buffer.rBaseAddr.u4VA, (*ppFrameBuf)->rBaseAddr.u4VA);
        //if ((OMX_FALSE == mIsSecureInst && (*ppFrameBuf)->rBaseAddr.u4VA == mFrameBuf[i].frame_buffer.rBaseAddr.u4VA) ||
        //    (OMX_TRUE == mIsSecureInst && (*ppFrameBuf)->rSecMemHandle == mFrameBuf[i].frame_buffer.rSecMemHandle))
        //
        if (1)
        {
            if (OMX_FALSE == mIsSecureInst && (&mFrameBuf[i].frame_buffer != (*ppFrameBuf)))
            {
                mFrameBuf[i].frame_buffer.u4FrameBufferStatus = (*ppFrameBuf)->u4FrameBufferStatus;
            }

            mFrameBuf[i].bDisplay = OMX_TRUE;
            //MTK_OMX_LOGD("GET_DISP, i:%d, frm_buf:%08x, omx_buf:%08x, width=%d, height=%d", i, tmp_frame_addr, mFrameBuf[i].ipOutputBuffer, mFrameBuf[i].frame_buffer.u4DispWidth, mFrameBuf[i].frame_buffer.u4DispHeight);
            mGET_DISP_i = i;
            mGET_DISP_tmp_frame_addr = tmp_frame_addr;
            //MTK_OMX_LOGD("GET_DISP, i:%d, frm_buf:%08x, omx_buf:%08x", i, tmp_frame_addr, mFrameBuf[i].ipOutputBuffer);
            if (OMX_TRUE == bGetResolution || mFixedMaxBuffer == OMX_TRUE)
            {
                MTK_OMX_LOGD("***mFixedMaxBuffer***, bGetResolution(%d)", __LINE__);
                VDEC_DRV_PICINFO_T temp_pic_info;
                if (0 == mMtkV4L2Device.getPicInfo(&temp_pic_info))
                {
                    MTK_OMX_LOGE("[ERROR] Cannot get param: VDEC_DRV_GET_TYPE_GET_PICTURE_INFO");
                    temp_pic_info.u4Width = mOutputPortDef.format.video.nFrameWidth;
                    temp_pic_info.u4Height = mOutputPortDef.format.video.nFrameHeight;
                    temp_pic_info.u4RealWidth = mOutputPortDef.format.video.nStride;
                    temp_pic_info.u4RealHeight = mOutputPortDef.format.video.nSliceHeight;

                    MTK_OMX_LOGD("***mFixedMaxBuffer***, bGetResolution(%d), w(%d), h(%d), rw(%d), rh(%d)", __LINE__, temp_pic_info.u4Width, temp_pic_info.u4Height, temp_pic_info.u4RealWidth, temp_pic_info.u4RealHeight);
                }

                MTK_OMX_LOGD("***mFixedMaxBuffer***, GetDisplayBuffer() u4Width %d, u4Height %d, u4RealWidth %d, u4RealHeight %d\n",
                             temp_pic_info.u4Width, temp_pic_info.u4Height, temp_pic_info.u4RealWidth, temp_pic_info.u4RealHeight);


                MTK_OMX_LOGD("GetDisplayBuffer() u4Width %d, u4Height %d, u4RealWidth %d, u4RealHeight %d\n",
                             temp_pic_info.u4Width, temp_pic_info.u4Height, temp_pic_info.u4RealWidth, temp_pic_info.u4RealHeight);
                mFrameBuf[i].ipOutputBuffer->nWidth = temp_pic_info.u4Width;
                mFrameBuf[i].ipOutputBuffer->nHeight = temp_pic_info.u4Height;
                if (mChipName == VAL_CHIP_NAME_MT6572 || mChipName == VAL_CHIP_NAME_MT6571)
                {
                    mFrameBuf[i].ipOutputBuffer->nStride = VDEC_ROUND_32(temp_pic_info.u4RealWidth);
                    mFrameBuf[i].ipOutputBuffer->nSliceHeight = VDEC_ROUND_16(temp_pic_info.u4RealHeight);
                }
                else
                {
                    mFrameBuf[i].ipOutputBuffer->nStride = VDEC_ROUND_16(temp_pic_info.u4RealWidth);
                    mFrameBuf[i].ipOutputBuffer->nSliceHeight = VDEC_ROUND_32(temp_pic_info.u4RealHeight);
                }
            }

            if (mCorrectTsFromOMX)   // for VC1
            {
                if (OMX_FALSE == mFirstFrameRetrieved)
                {
                    mFirstFrameRetrieved = OMX_TRUE;
                }
                else
                {
                    mCurrentFrameTs += mFrameTsInterval;
                }
                mFrameBuf[i].ipOutputBuffer->nTimeStamp = mCurrentFrameTs;
                //MTK_OMX_LOGE ("@@ Correct TS to %lld", mCurrentFrameTs);
            }

            if ((mFrameBuf[i].frame_buffer.u4FrameBufferStatus & VDEC_DRV_FBSTSTUS_INVALID_TIMESTAMP) != 0)
            {
                mFrameBuf[i].ipOutputBuffer->nFlags |= OMX_BUFFERFLAG_INVALID_TIMESTAMP;
            }

#if defined(DYNAMIC_PRIORITY_ADJUSTMENT)
            mllLastDispTime = mFrameBuf[i].ipOutputBuffer->nTimeStamp;
#endif
            mFrameBuf[i].bNonRealDisplay = OMX_FALSE;
            if ((mFrameBuf[i].frame_buffer.u4FrameBufferStatus & VDEC_DRV_FBSTSTUS_NOT_DISPLAY) != 0 ||
                (mFrameBuf[i].frame_buffer.u4FrameBufferStatus & VDEC_DRV_FBSTSTUS_REPEAT_LAST) != 0 ||
                (mFrameBuf[i].frame_buffer.u4FrameBufferStatus & VDEC_DRV_FBSTSTUS_NOT_USED) != 0)   // not to display
            {
                mFrameBuf[i].ipOutputBuffer->nFilledLen = 0;
                mFrameBuf[i].bNonRealDisplay = OMX_TRUE;
                mNumNotDispAvailOutput++;
                if ((mFrameBuf[i].frame_buffer.u4FrameBufferStatus & VDEC_DRV_FBSTSTUS_REPEAT_LAST) != 0)
                {
                    MTK_OMX_LOGD("RL return");
#ifdef DYNAMIC_PRIORITY_ADJUSTMENT
                    mErrorCount = 0;
#endif
                }
                else if ((mFrameBuf[i].frame_buffer.u4FrameBufferStatus & VDEC_DRV_FBSTSTUS_NOT_USED) != 0)
                {
                    RemoveForInputAtPTS(mFrameBuf[i].ipOutputBuffer->nTimeStamp);
                    mFrameBuf[i].ipOutputBuffer->nTimeStamp = -1;
                    MTK_OMX_LOGD("NU return");
                }
                else
                {
                    MTK_OMX_LOGD("ND return");
#ifdef DYNAMIC_PRIORITY_ADJUSTMENT
                    mErrorCount++;
#endif
                }
            }
            else
            {
                if ((*ppFrameBuf)->u4FrameBufferType == VDEC_DRV_FBTYPE_3D_SBS)
                {
                    m3DStereoMode = OMX_VIDEO_H264FPA_SIDEBYSIDE;
                }
                else if ((*ppFrameBuf)->u4FrameBufferType == VDEC_DRV_FBTYPE_3D_TAB)
                {
                    m3DStereoMode = OMX_VIDEO_H264FPA_TOPANDBOTTOM;
                }
                MTK_OMX_LOGD("CURRENT_DISPLAY return code, length %d, ts %lld, 3D %d", mFrameBuf[i].ipOutputBuffer->nFilledLen, mFrameBuf[i].ipOutputBuffer->nTimeStamp, m3DStereoMode);
                mFrameBuf[i].ipOutputBuffer->nTimeStamp = 0;
                //MTK_OMX_LOGD("CURRENT_DISPLAY return code, length %d, ts %lld", mFrameBuf[i].ipOutputBuffer->nFilledLen, mFrameBuf[i].ipOutputBuffer->nTimeStamp);
#ifdef DYNAMIC_PRIORITY_ADJUSTMENT
                mErrorCount = 0;
#endif

                if (mDumpOutputFrame == OMX_TRUE)
                {
                    FILE *fp_output;
                    OMX_U32 size_fp_output;
                    char ucStringyuv[100];
                    size_t temp_size, temp_size_Y, temp_size_C, w_size, h_size;
                    //char *ptemp_buff = (char *)mFrameBuf[i].ipOutputBuffer->pBuffer;
                    char *ptemp_buff = (char *)mFrameBuf[i].frame_buffer.rBaseAddr.u4VA;

                    w_size = mOutputPortDef.format.video.nStride;
                    h_size = mOutputPortDef.format.video.nSliceHeight;

                    if (mSeqInfo.u4BitDepthLuma == 10 || mSeqInfo.u4BitDepthChroma == 10)
                    {
                        temp_size_Y = w_size * h_size * 1.25;
                        temp_size_C = temp_size_Y / 2;
                        temp_size_Y = (((temp_size_Y + 511) >> 9) << 9);
                        temp_size = (temp_size_Y + temp_size_C);
                    }
                    else
                    {
                        temp_size_Y = w_size * h_size;
                        temp_size_C = temp_size_Y / 2;
                        temp_size = (temp_size_Y + temp_size_C);
                    }

#if 1
                    sprintf(ucStringyuv, "//sdcard/Vdec%d_%4d.yuv", (int)temp_size, (int)gettid());
                    //MTK_OMX_LOGD("[DEBUG]nBufferSize %d, nFilledLen %d, w %d, h %d",mOutputPortDef.nBufferSize,mFrameBuf[i].ipOutputBuffer->nFilledLen,mOutputPortDef.format.video.nFrameWidth, mOutputPortDef.format.video.nFrameHeight);
                    //MTK_OMX_LOGD("[DEBUG] nStride %d, nSliceHeight %d",mOutputPortDef.format.video.nStride,mOutputPortDef.format.video.nSliceHeight);

                    fp_output = fopen(ucStringyuv, "ab");
                    if (fp_output != NULL)
                    {
                        size_fp_output = temp_size;
                        MTK_OMX_LOGD("sdcard/mfv_264.out write size = %d\n", size_fp_output);
                        size_fp_output = fwrite(ptemp_buff, 1, size_fp_output, fp_output);
                        MTK_OMX_LOGD("sdcard/mfv_264.out real write size = %d\n", size_fp_output);
                        fclose(fp_output);
                    }
                    else
                    {
                        MTK_OMX_LOGE("sdcard/mfv_264.out file create error\n");
                        //dump frame buffer in data/vdec/
                        sprintf(ucStringyuv, "/data/vdec/Vdec_w%d_h%d_%4d.yuv",
                                mOutputPortDef.format.video.nFrameWidth,
                                mOutputPortDef.format.video.nFrameHeight, (int)gettid());
                        fp_output = fopen(ucStringyuv, "ab");
                        if (fp_output != NULL)
                        {
                            size_fp_output = mFrameBuf[i].ipOutputBuffer->nFilledLen;
                            MTK_OMX_LOGD("/data/vdec/ write size = %d\n", size_fp_output);
                            size_fp_output = fwrite(ptemp_buff, 1, size_fp_output, fp_output);
                            MTK_OMX_LOGD("/data/vdec/ real write size = %d\n", size_fp_output);
                            fclose(fp_output);
                        }
                        else
                        {
                            MTK_OMX_LOGE("/data/vdec/out file create error\n");
                        }
                        //end
                    }
#else                       // CRC check for output frame
                    OMX_U32  crc_accum;
                    size_fp_output = mFrameBuf[i].ipOutputBuffer->nFilledLen;
                    crc_accum = _calculate_crc32((unsigned char *)ptemp_buff, size_fp_output);
                    LOGE("%d %d crc_accum : %08x", FNum, this, crc_accum);
#if 0                       // to create CRC golden
                    ptemp_buff = (char *) &crc_accum;
                    sprintf(ucStringyuv, "//sdcard/Vdec%04d.crc", FNum);
                    fp_output = fopen(ucStringyuv, "ab");
                    if (fp_output != NULL)
                    {
                        fwrite(ptemp_buff, 1, 4, fp_output);
                        fclose(fp_output);
                    }
                    FNum++;
#else                       // to compare CRC golden
                    OMX_U32 crc_accum_rec;
                    char crcRead[4];
                    sprintf(ucStringyuv, "//sdcard/Vdec%04d.crc", FNum);
                    fp_output = fopen(ucStringyuv, "r+b");
                    if (fp_output != NULL)
                    {
                        fread(crcRead, 1, 4, fp_output);
                        fclose(fp_output);
                        crc_accum_rec = *((OMX_U32 *)crcRead);
                        if (crc_accum_rec != crc_accum)
                        {
                            LOGE("%d %d crc_accum_rec error: %x", FNum, this, crc_accum_rec);
                        }
                        else
                        {
                            LOGE("%d %d crc_accum_rec ok: %x", FNum, this, crc_accum_rec);
                        }
                    }
                    FNum++;
#endif
#endif
                }
            }
            mNumAllDispAvailOutput++;
            MTK_OMX_LOGD("return 0x%08x from GetDisplayBuffer", mFrameBuf[i].ipOutputBuffer);
            return mFrameBuf[i].ipOutputBuffer;
        }


    }

    return NULL;
}

OMX_BUFFERHEADERTYPE *MtkOmxVdec::GetFreeInputBuffer(OMX_BOOL bFlushAll, OMX_BUFFERHEADERTYPE *ipInputBuf)
{
    VAL_ULONG_T tmp_frame_addr = 0;
    VAL_UINT32_T i;


    for (i = 0; i < mInputPortDef.nBufferCountActual; i++)
    {
        if (ipInputBuf == mInputBuf[i].ipInputBuffer)
        {
            tmp_frame_addr = (VAL_ULONG_T)&mInputBuf[i].InputBuf;
        }
    }

    //MTK_OMX_LOGD("tmp_frame_addr 0x%08x", tmp_frame_addr);

    //for (i = 0; i < mInputPortDef.nBufferCountActual; i++)
    //{
    //    MTK_OMX_LOGD("mInputBuf[%d] 0x%08x (%d)", i, (VAL_UINT32_T)&mInputBuf[i].InputBuf, mInputBuf[i].bUsed);
    //}


RECHECK:


    //get un-used free frame.

    if (VAL_NULL != tmp_frame_addr)
    {
        //MTK_OMX_LOGD("tmp_frame_addr 0x%08x", tmp_frame_addr);
        for (i = 0; i < mInputPortDef.nBufferCountActual; i++)
        {
            //MTK_OMX_LOGD("mInputBuf[%d] 0x%08x (%d)", i, (VAL_UINT32_T)&mInputBuf[i].InputBuf, mInputBuf[i].bUsed);
            if (tmp_frame_addr == (VAL_ULONG_T)&mInputBuf[i].InputBuf || bFlushAll == OMX_TRUE)
            {
                if (!IsInETBQ(mInputBuf[i].ipInputBuffer))// v4l2 todo: do we need to check bUsed ???
                {
                    HandleEmptyBufferDone(mInputBuf[i].ipInputBuffer);
                    MTK_OMX_LOGD("got free input. i:%d, mInputBuf[i].ipInputBuffer:0x%08x", i, mInputBuf[i].ipInputBuffer);
                    return mInputBuf[i].ipInputBuffer;
                }
                else
                {
                    MTK_OMX_LOGD("strange!! Input buffer is not free(0x%08x, %d) %d", mInputBuf[i].ipInputBuffer, !IsInETBQ(mInputBuf[i].ipInputBuffer), mInputPortDef.nBufferCountActual);
                }
            }
        }

        if (i == mInputPortDef.nBufferCountActual)
        {
            MTK_OMX_LOGD("unknown input 0x%08x ??", tmp_frame_addr);
            for (i = 0; i < mInputPortDef.nBufferCountActual; i++)
            {
                MTK_OMX_LOGD("GetFreeInputBuffer() not found input buffer to free (0x%08x, 0x%08x, %d) %d", mInputBuf[i].ipInputBuffer, &mInputBuf[i].InputBuf, !IsInETBQ(mInputBuf[i].ipInputBuffer), mInputPortDef.nBufferCountActual);
            }
        }
    }
    if (VAL_NULL != tmp_frame_addr && bFlushAll == OMX_TRUE)
    {
        goto RECHECK;
    }
    return NULL;
}

VDEC_DRV_FRAMEBUF_T *MtkOmxVdec::GetFrmBuf(OMX_TICKS iTimestamp, OMX_BUFFERHEADERTYPE *ipOutputBuffer)
{
#if (ANDROID_VER >= ANDROID_KK)
    if (OMX_TRUE == mStoreMetaDataInBuffers)
    {
        OMX_U32 graphicBufHandle = 0;
        GetMetaHandleFromOmxHeader(ipOutputBuffer, &graphicBufHandle);

#if (ANDROID_VER >= ANDROID_M)
        WaitFence(ipOutputBuffer, OMX_TRUE);
#endif
        // we are in initial stage and cannot find any buffer handle, find from OMX header
        LOCK(mFillThisBufQLock);
        OMX_U32 j = 0;
        for (j = 0; j < mOutputPortDef.nBufferCountActual; j++)
        {
            if (mFrameBuf[j].ipOutputBuffer == ipOutputBuffer)
            {
                if (1)
                {
                    mFrameBuf[j].bDisplay = OMX_FALSE;
                    mFrameBuf[j].bFillThis = OMX_FALSE;
                    mFrameBuf[j].iTimestamp = iTimestamp;
                    mFrameBuf[j].frame_buffer.u4Timestamp = iTimestamp;
                    mFrameBuf[j].frame_buffer.u4FrameBufferStatus = 0;
                    if (mFrameBuf[j].ionBufHandle == 0)
                    {
                        VBufInfo info;
                        int ret = mOutputMVAMgr->getOmxInfoFromHndl((void *)graphicBufHandle, &info);
                        if (ret >= 0)
                        {
                            // found handle
                            MTK_OMX_LOGD("GetFrmBuf found handle, u4BuffHdr(0x%08x)", ipOutputBuffer);
                        }
                        else
                        {
                            // cannot found handle, create a new entry
                            MTK_OMX_LOGE("GetFrmBuf cannot find handle, ERROR", __LINE__);
                        }
                        mFrameBuf[j].bGraphicBufHandle = graphicBufHandle;
                        mFrameBuf[j].ionBufHandle = info.ionBufHndl;
                        //MTK_OMX_LOGD("assign mFrameBuf[%d].ionBufHandle = %d", j, info.ionBufHndl);
                    }
                    // TODO: secure meta mode
                    int refCount = 0;
                    VBufInfo info;
                    int ret = mOutputMVAMgr->getOmxInfoFromHndl((void *)graphicBufHandle, &info);
                    if (ret < 0)
                    {
                        MTK_OMX_LOGE("GetFrmBuf cannot find handle, ERROR", __LINE__);
                    }
                    UNLOCK(mFillThisBufQLock);
                    return (&mFrameBuf[j].frame_buffer);
                }
                else
                {
                    MTK_OMX_LOGE("Error!! GetFrmBuf is not free, LINE:%d", __LINE__);
                }
            }
        }
        UNLOCK(mFillThisBufQLock);

        MTK_OMX_LOGE("Error!! GetFrmBuf not found 0x%08x (%d), line: %d", ipOutputBuffer, mOutputPortDef.nBufferCountActual, __LINE__);
        return NULL;
    }
#endif

    LOCK(mFillThisBufQLock);
    for (OMX_U32 i = 0; i < mOutputPortDef.nBufferCountActual; i++)
    {
        if (mFrameBuf[i].ipOutputBuffer == ipOutputBuffer)
        {
            if (1)
            {
                mFrameBuf[i].bDisplay = OMX_FALSE;
                mFrameBuf[i].bFillThis = OMX_FALSE;
                mFrameBuf[i].iTimestamp = iTimestamp;
                mFrameBuf[i].frame_buffer.u4Timestamp = iTimestamp;
                mFrameBuf[i].frame_buffer.u4FrameBufferStatus = 0;
                //mFrameBuf[i].ipOutputBuffer = ipOutputBuffer;
                //MTK_OMX_LOGE ("@@ GetFrmBuf frm=0x%x, omx=0x%x, i=%d", &mFrameBuf[i].frame_buffer, ipOutputBuffer, i);
                UNLOCK(mFillThisBufQLock);
                return (&mFrameBuf[i].frame_buffer);
            }
            else
            {
                MTK_OMX_LOGE("Error!! GetFrmBuf is not free");
            }
        }
    }
    UNLOCK(mFillThisBufQLock);
    MTK_OMX_LOGE("Error!! GetFrmBuf not found 0x%08x, line: %d", ipOutputBuffer, __LINE__);
    return NULL;
}

OMX_U32 MtkOmxVdec::CheckFreeBuffer(OMX_BOOL bLOG)
{
    VAL_UINT32_T i, cnt = 0;
    //MTK_OMX_LOGD ("CheckFreeBuffer+");
    #if 0
    for (i = 0; i < mOutputPortDef.nBufferCountActual; i++)
    {
        if (mFrameBuf == NULL)
        {
            MTK_OMX_LOGE("[CheckFreeBuffer] mFrameBuf = NULL");
        }
        else
        {
            if (i >= mFrameBufSize)
            {
                MTK_OMX_LOGD("[CheckFreeBuffer] index out of range (%d, %d, %d)", i, mFrameBufSize, mOutputPortDef.nBufferCountActual);
                break;
            }
        }
    }
	#endif

    // handle seek (for VC1)
    if (mCorrectTsFromOMX)
    {
        mFirstFrameRetrieved = OMX_FALSE;
        mResetFirstFrameTs = OMX_TRUE;
        mCurrentFrameTs = 0;
    }

    //MTK_OMX_LOGD ("CheckFreeBuffer-, not free cnt:%u", cnt);
    return cnt;
}

VDEC_DRV_RINGBUF_T *MtkOmxVdec::GetInputBuf(OMX_BUFFERHEADERTYPE *ipInputBuf)
{
    OMX_U32 i = 0;
    for (i = 0; i < mInputPortDef.nBufferCountActual; i++)
    {
        if (mInputBuf[i].ipInputBuffer == ipInputBuf)
        {
            if (1)
            {
                mInputBuf[i].ipInputBuffer = ipInputBuf;
                return (&mInputBuf[i].InputBuf);
            }
            else
            {
                MTK_OMX_LOGE("Error!! GetInputBuf is not free");
            }
        }
    }
    MTK_OMX_LOGE("Error!! GetInputBuf not found 0x%08x", ipInputBuf);
    return NULL;
}


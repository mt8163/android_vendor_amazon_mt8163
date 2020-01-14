
#include "MtkOmxVenc.h"

#include <cutils/log.h>
#include <utils/Trace.h>

#include <cutils/properties.h>
#include <ui/gralloc_extra.h>
#include <HardwareAPI.h>

#undef LOG_TAG
#define LOG_TAG "MtkOmxVenc"

#include "OMX_IndexExt.h"


#ifdef ATRACE_TAG
#undef ATRACE_TAG
#define ATRACE_TAG ATRACE_TAG_VIDEO
#endif//ATRACE_TAG

void MtkOmxVenc::ConvertVideo(OMX_BUFFERHEADERTYPE *pInputBuf, OMX_BUFFERHEADERTYPE *pOutputBuf)
{
    ATRACE_BEGIN("ColorConvert");
    int64_t _start_tick = 0;
    int64_t _end_tick = 0;

    _start_tick = getTickCountMs();
    MTK_OMX_LOGD_ENG("Convert %lld, %d, 0x%08X, input VA=0x%08X :Offset 0x%08lX, output VA=0x%08X :Offset 0x%08lX",
                 pInputBuf->nTimeStamp, (int)pInputBuf->nFilledLen, (unsigned int)pInputBuf->nFlags,
                 (unsigned int)(pInputBuf->pBuffer), pInputBuf->nOffset, (unsigned int)(pOutputBuf->pBuffer),
                 pOutputBuf->nOffset);

    //check MetaMode input format
    mInputMetaDataFormat = CheckOpaqueFormat(pInputBuf);
    if (0xFFFFFFFF == mGrallocWStride)
    {
        mGrallocWStride = CheckGrallocWStride(pInputBuf);
    }


    if (pInputBuf->nOffset > pInputBuf->nFilledLen) //check the incorrect access
    {
        MTK_OMX_LOGE("[ERROR] incorrect buffer access");
        return;
    }

    VENC_DRV_DONE_RESULT_T  rEncResult;
    rEncResult.eMessage = VENC_DRV_MESSAGE_OK;
    VENC_DRV_PARAM_ENC_EXTRA_T rEncoderExtraConfig;

    OMX_U8 *aInputBuf   = pInputBuf->pBuffer + pInputBuf->nOffset;
    OMX_U8 *aOutputBuf = pOutputBuf->pBuffer + pOutputBuf->nOffset;
    OMX_U32 aInputSize  = pInputBuf->nFilledLen;
    OMX_U32 aOutputSize = pOutputBuf->nAllocLen;
    VAL_RECORD_SIZE_T   rDrvRecordSize;

    if (pInputBuf->nFlags & OMX_BUFFERFLAG_EOS)    // EOS frame
    {
        MTK_OMX_LOGD("Convert EOS received, TS=%lld", pInputBuf->nTimeStamp);
        pOutputBuf->nFlags = pInputBuf->nFlags;
        pOutputBuf->nTimeStamp = mLastFrameTimeStamp;
        pOutputBuf->nFilledLen = 0;

        HandleEmptyBufferDone(pInputBuf);
        CnvtHandleFillBufferDone(pOutputBuf);
    }
    else    // conver frame
    {
        // assign timestamp
        pOutputBuf->nTimeStamp = pInputBuf->nTimeStamp;
        // for SEC VIDEO PATH [
        // check there are secure convert buffers, if no, allocate here.
        if (checkSecSwitchInCC(pInputBuf, pOutputBuf))
        {
            return;
        }
        // ] SEC VIDEO PATH

        WaitFence(aInputBuf, OMX_TRUE);

        if (OMX_TRUE == NeedConversion())
        {
            if (OMX_FALSE == mEncoderInitCompleteFlag)
            {
                MTK_OMX_LOGD("Need Convert but Encoder not init");
                // queue input buffer back inputBufQ
                QueueBufferAdvance(mpConvertInputBufQ, pInputBuf);
                // pass an empty frame buffer to encoder for init
                CnvtHandleFillBufferDone(pOutputBuf);

                sched_yield();
                int i = 0;
                while (OMX_FALSE == mEncoderInitCompleteFlag)
                {
                    if (i++ > 200)
                    {
                        MTK_OMX_LOGE("Wait Encoder init complete timeout");
                        break;
                    }

                    MTK_OMX_LOGD("Wait Encoder init complete...");
                    SLEEP_MS(5);
                }
                return;
            }

            MTK_OMX_LOGD("Need Convert %lld, %d, 0x%08X, input VA=0x%08X :Offset 0x%08lX, output VA=0x%08X :Offset 0x%08lX",
                 pInputBuf->nTimeStamp, (int)pInputBuf->nFilledLen, (unsigned int)pInputBuf->nFlags,
                 (unsigned int)(pInputBuf->pBuffer), pInputBuf->nOffset, (unsigned int)(pOutputBuf->pBuffer),
                 pOutputBuf->nOffset);

            if (colorConvert(aInputBuf, aInputSize, aOutputBuf, aOutputSize) <= 0)
            {
                MTK_OMX_LOGE("## CONVERT ERROR !!!");
                // report bitstream corrupt error
                mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                       mAppData,
                                       OMX_EventError,
                                       OMX_ErrorStreamCorrupt,
                                       (OMX_U32)NULL,
                                       NULL);
            }

            // assign flag
            pOutputBuf->nFlags = pInputBuf->nFlags;
            pOutputBuf->nFilledLen = pOutputBuf->nAllocLen;
            pOutputBuf->pMarkData = 0;
            _end_tick = getTickCountMs();
            MTK_OMX_LOGD("RGB_2_YUV=%lld, buf timestamp=%lld, s:%d, out:0x%x, 0x%x",
                         _end_tick - _start_tick, pInputBuf->nTimeStamp, mIsSecureSrc,
                         pOutputBuf->pMarkData, pOutputBuf->nFlags);

            mLastFrameTimeStamp = pOutputBuf->nTimeStamp;
            //mFrameCount++;

            HandleEmptyBufferDone(pInputBuf);
            CnvtHandleFillBufferDone(pOutputBuf);
        }
        else//get YV12
        {
            MTK_OMX_LOGD("No Need Convert %lld, %d, 0x%08X, input VA=0x%08X :Offset 0x%08lX, output VA=0x%08X :Offset 0x%08lX",
                 pInputBuf->nTimeStamp, (int)pInputBuf->nFilledLen, (unsigned int)pInputBuf->nFlags,
                 (unsigned int)(pInputBuf->pBuffer), pInputBuf->nOffset, (unsigned int)(pOutputBuf->pBuffer),
                 pOutputBuf->nOffset);

            pOutputBuf->nFlags = pInputBuf->nFlags;
            pOutputBuf->nFlags |= OMX_BUFFERFLAG_COLORCONVERT_NEEDRETURN;
            pOutputBuf->nFilledLen = 8;
            pOutputBuf->pMarkData = pInputBuf;

            memcpy(aOutputBuf, aInputBuf, 8);
            CnvtHandleFillBufferDone(pOutputBuf);
            //MTK_OMX_LOGD("convert out: 0x%x, 0x%x", pOutputBuf->pMarkData, pOutputBuf->nFlags);
        }
    }
    ATRACE_END();
    return;
}

/* for DirectLink Meta Mode + */
OMX_BOOL MtkOmxVenc::DLMetaModeParseandSendBufInfo(VAL_BufInfo *a_prBufInfo)
{
    VAL_ULONG_T ulFrameWidth = mInputPortDef.format.video.nFrameWidth;
    VAL_ULONG_T ulFrameHeight = mInputPortDef.format.video.nFrameHeight;
    VAL_ULONG_T ulFrameSize = (ulFrameWidth * ulFrameHeight * 3) >> 1;

    if (mSeqHdrEncoded == VAL_FALSE)
    {
        a_prBufInfo->fgIsConfigData = 1;
    }
    else
    {
        a_prBufInfo->fgIsConfigData = 0;
    }

    a_prBufInfo->u4BSVA = mFrameBuf.rFrmBufAddr.u4VA;
    a_prBufInfo->fgBSStatus = *((VAL_UINT8_T *)(mFrameBuf.rFrmBufAddr.u4VA + ulFrameSize - 6));
    a_prBufInfo->fgIsKeyFrame = *((VAL_UINT8_T *)(mFrameBuf.rFrmBufAddr.u4VA + ulFrameSize - 5));
    a_prBufInfo->u4BSSize = *((VAL_UINT32_T *)(mFrameBuf.rFrmBufAddr.u4VA + ulFrameSize - 4));

    MTK_OMX_LOGD("[DLMetaModeParseandSendBufInfo] ConfigData(0x%x), BSVA(0x%x), BSStatus(0x%x), "
                 "IsKeyFrame(0x%x), BSSize(0x%x)"
                 "FrameWidth(%d), FrameHeight(%d), FrameSize(%d)",
                 a_prBufInfo->fgIsConfigData,
                 a_prBufInfo->u4BSVA,
                 a_prBufInfo->fgBSStatus,
                 a_prBufInfo->fgIsKeyFrame,
                 a_prBufInfo->u4BSSize,
                 ulFrameWidth,
                 ulFrameHeight,
                 ulFrameSize
                );

    if (a_prBufInfo->u4BSSize > mBitStreamBuf.rBSAddr.u4Size)
    {
        MTK_OMX_LOGE("[ERROR] [DLMetaModeParseandSendBufInfo] EncodedBSSize(%d) > BSBufSize(%d)",
                     a_prBufInfo->u4BSSize,
                     mBitStreamBuf.rBSAddr.u4Size
                    );

        return OMX_FALSE;
    }

    memcpy((void *)mBitStreamBuf.u4BSStartVA, (void *)a_prBufInfo->u4BSVA, a_prBufInfo->u4BSSize);
    mBitStreamBuf.u4BSSize = a_prBufInfo->u4BSSize;

    return OMX_TRUE;
}

OMX_BOOL MtkOmxVenc::DLMetaModeEnable()
{
    if ((OMX_TRUE == mStoreMetaDataInBuffers) &&
        (((unsigned int)mInputPortDef.format.video.eColorFormat) == OMX_MTK_COLOR_FormatBitStream))
    {
        return OMX_TRUE;
    }
    else
    {
        return OMX_FALSE;
    }
}

void MtkOmxVenc::DLMetaModeEncodeVideo(OMX_BUFFERHEADERTYPE *pInputBuf, OMX_BUFFERHEADERTYPE *pOutputBuf)
{
    int64_t _start_tick = 0;
    int64_t _end_tick = 0;

    _start_tick = getTickCountMs();

    MTK_OMX_LOGD("DLMetaModeEncodeVideo");

    ATRACE_CALL();

    MTK_OMX_LOGD("DL MetaMode %lld, %d, 0x%08X (0x%08X, 0x%08X)",
                 pInputBuf->nTimeStamp, (int)pInputBuf->nFilledLen, (unsigned int)pInputBuf->nFlags,
                 (unsigned int)(pInputBuf->pBuffer), (unsigned int)(pOutputBuf->pBuffer));

    VAL_BufInfo rBufInfo;

    OMX_U8 *aInputBuf = pInputBuf->pBuffer + pInputBuf->nOffset;
    OMX_U8 *aOutputBuf = pOutputBuf->pBuffer + pOutputBuf->nOffset;
    OMX_U32 aInputSize = pInputBuf->nFilledLen;
    OMX_U32 aOutputSize = pOutputBuf->nAllocLen;

    if (OMX_FALSE == mSeqHdrEncoded)
    {
        // assign timestamp
        pOutputBuf->nTimeStamp = pInputBuf->nTimeStamp;

        // get mBitStreamBuf from meta buffer handle
        GetVEncDrvBSBuffer(aOutputBuf, aOutputSize);

        // get mFrameBuf from meta buffer handle
        GetVEncDrvFrmBuffer(aInputBuf, aInputSize);

        DLMetaModeParseandSendBufInfo(&rBufInfo);

        if ((VAL_TRUE != rBufInfo.fgBSStatus) || (VAL_TRUE != rBufInfo.fgIsConfigData))
        {
            MTK_OMX_LOGE("[ERROR] cannot get encoded Sequence Header %d, %d",
                         rBufInfo.fgBSStatus, rBufInfo.fgIsConfigData);

            // report bitstream corrupt error
            mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                   mAppData,
                                   OMX_EventError,
                                   OMX_ErrorStreamCorrupt,
                                   (OMX_U32)NULL,
                                   NULL);

            pOutputBuf->nFilledLen = 0;
            pOutputBuf->nTimeStamp = 0;

            EncHandleEmptyBufferDone(pInputBuf);
            HandleFillBufferDone(pOutputBuf);

            return;
        }

        if (mDumpDLBS)
        {
            MTK_OMX_LOGD("Sequence header = 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
                         *((VAL_UINT8_T *)(mBitStreamBuf.u4BSStartVA + 0)),
                         *((VAL_UINT8_T *)(mBitStreamBuf.u4BSStartVA + 1)),
                         *((VAL_UINT8_T *)(mBitStreamBuf.u4BSStartVA + 2)),
                         *((VAL_UINT8_T *)(mBitStreamBuf.u4BSStartVA + 3)),
                         *((VAL_UINT8_T *)(mBitStreamBuf.u4BSStartVA + 4)),
                         *((VAL_UINT8_T *)(mBitStreamBuf.u4BSStartVA + 5)),
                         *((VAL_UINT8_T *)(mBitStreamBuf.u4BSStartVA + 6)),
                         *((VAL_UINT8_T *)(mBitStreamBuf.u4BSStartVA + 7)),
                         *((VAL_UINT8_T *)(mBitStreamBuf.u4BSStartVA + 8)),
                         *((VAL_UINT8_T *)(mBitStreamBuf.u4BSStartVA + 9))
                        );
        }

        MTK_OMX_LOGD("Sequence header size = %d", rBufInfo.u4BSSize);

        pOutputBuf->nFlags |= OMX_BUFFERFLAG_CODECCONFIG;
        pOutputBuf->nFilledLen = rBufInfo.u4BSSize;

#if 1 // dump bs
        {
            VAL_BOOL_T bDumpLog = (VAL_BOOL_T) MtkVenc::DumpLog();

            if (bDumpLog)
            {
                char buf[255];
                sprintf(buf, "/sdcard/DLvenc%d.bs", gettid());
                FILE *fp = fopen(buf, "ab");
                if (fp)
                {
                    fwrite((void *)mBitStreamBuf.u4BSStartVA, 1, mBitStreamBuf.u4BSSize, fp);
                    fclose(fp);
                }
            }
        }
#endif

        EncHandleEmptyBufferDone(pInputBuf);
        HandleFillBufferDone(pOutputBuf);

        mSeqHdrEncoded = OMX_TRUE;

        return;
    }

    if (pInputBuf->nFlags & OMX_BUFFERFLAG_EOS)    // EOS frame
    {
        MTK_OMX_LOGD("DL Enc EOS received, TS=%lld", pInputBuf->nTimeStamp);

        pOutputBuf->nFlags |= OMX_BUFFERFLAG_EOS;
        pOutputBuf->nTimeStamp = mLastFrameTimeStamp;
        pOutputBuf->nFilledLen = 0;

        EncHandleEmptyBufferDone(pInputBuf);
        HandleFillBufferDone(pOutputBuf);
    }
    else    // encode normal frame
    {
        // assign timestamp
        pOutputBuf->nTimeStamp = pInputBuf->nTimeStamp;

        // get mBitStreamBuf from meta buffer handle
        GetVEncDrvBSBuffer(aOutputBuf, aOutputSize);

        // get mFrameBuf from meta buffer handle
        GetVEncDrvFrmBuffer(aInputBuf, aInputSize);

        mFrameBuf.rTimeStamp.u4TimeStamp[0] = (VAL_UINT32_T)(pInputBuf->nTimeStamp >> 32);
        mFrameBuf.rTimeStamp.u4TimeStamp[1] = (VAL_UINT32_T)(pInputBuf->nTimeStamp &  0xFFFFFFFF);

        DLMetaModeParseandSendBufInfo(&rBufInfo);

        if (VAL_TRUE != rBufInfo.fgBSStatus)
        {
            MTK_OMX_LOGE("## DL ENCODE ERROR !!!");

            // report bitstream corrupt error
            mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                   mAppData,
                                   OMX_EventError,
                                   OMX_ErrorStreamCorrupt,
                                   (OMX_U32)NULL,
                                   NULL);
        }
        else
        {
            MTK_OMX_LOGD("ENCODE OK IsKey(%d), Size(%d)", rBufInfo.fgIsKeyFrame, rBufInfo.u4BSSize);

            if (mDumpDLBS)
            {
                MTK_OMX_LOGD("DL BS = 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
                             *((VAL_UINT8_T *)(mBitStreamBuf.u4BSStartVA + 0)),
                             *((VAL_UINT8_T *)(mBitStreamBuf.u4BSStartVA + 1)),
                             *((VAL_UINT8_T *)(mBitStreamBuf.u4BSStartVA + 2)),
                             *((VAL_UINT8_T *)(mBitStreamBuf.u4BSStartVA + 3)),
                             *((VAL_UINT8_T *)(mBitStreamBuf.u4BSStartVA + 4)),
                             *((VAL_UINT8_T *)(mBitStreamBuf.u4BSStartVA + 5)),
                             *((VAL_UINT8_T *)(mBitStreamBuf.u4BSStartVA + 6)),
                             *((VAL_UINT8_T *)(mBitStreamBuf.u4BSStartVA + 7)),
                             *((VAL_UINT8_T *)(mBitStreamBuf.u4BSStartVA + 8)),
                             *((VAL_UINT8_T *)(mBitStreamBuf.u4BSStartVA + 9))
                            );
            }

#if 1 // dump bs
            {
                VAL_BOOL_T bDumpLog = MtkVenc::DumpLog();

                if (bDumpLog)
                {
                    char buf[255];
                    sprintf(buf, "/sdcard/DLvenc%d.bs", gettid());
                    FILE *fp = fopen(buf, "ab");
                    if (fp)
                    {
                        fwrite((void *)mBitStreamBuf.u4BSStartVA, 1, mBitStreamBuf.u4BSSize, fp);
                        fclose(fp);
                    }
                }
            }
#endif
        }

        _end_tick = getTickCountMs();
        MTK_OMX_LOGD("EncTime=%lld", _end_tick - _start_tick);

        pOutputBuf->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;

        if (VAL_TRUE == rBufInfo.fgIsKeyFrame)
        {
            pOutputBuf->nFlags |= OMX_BUFFERFLAG_SYNCFRAME;
        }

        mLastFrameTimeStamp = pOutputBuf->nTimeStamp;

        pOutputBuf->nFilledLen = rBufInfo.u4BSSize;

        VENC_DRV_MRESULT_T mReturn = VENC_DRV_MRESULT_OK;
        OMX_U32 uIsHEVCHW = 0; // todo: VENC_DRV_GET_TYPE_IS_HEVC_HW_PLATFORM V4L2 solution
        //mReturn = eVEncDrvGetParam((VAL_HANDLE_T)NULL, VENC_DRV_GET_TYPE_IS_HEVC_HW_PLATFORM, (VAL_VOID_T *)&mChipName, (VAL_VOID_T *)&uIsHEVCHW);
        //MTK_OMX_LOGD("uIsHEVCHW %d", uIsHEVCHW);
        if ((mCodecId == MTK_VENC_CODEC_ID_HEVC) && uIsHEVCHW)
        {
            //for HEVC HW solution
            pOutputBuf->nOffset = 3;   // skip NAL prefix from HEVC Encoder
            pOutputBuf->nFilledLen -= 3;
        }

        mFrameCount++;

        EncHandleEmptyBufferDone(pInputBuf);
        HandleFillBufferDone(pOutputBuf);

    }

    return;
}
/* for DirectLink Meta Mode - */

//-------- switch case for common code --------
VENC_DRV_VIDEO_FORMAT_T MtkOmxVenc::drvCodecId()
{
    switch (mCodecId)
    {
        case MTK_VENC_CODEC_ID_AVC:
            //if MT6577 define MTK_HYBRID_MULTI_THREAD :
            //if (mInputPortDef.format.video.nFrameWidth * mInputPortDef.format.video.nFrameHeight) > (640 * 480))
            //return VENC_DRV_VIDEO_FORMAT_H264_VGA;
            //MTK_OMX_LOGE("drvCodecId mIsSecureInst %d\n",mIsSecureInst);
            if (OMX_TRUE == mIsSecureInst/* && (OMX_TRUE == mSetWFDMode || OMX_TRUE == mWFDMode)*/)
            {
                MTK_OMX_LOGD("set to sec driver \n");
                if(mTeeEncType == INHOUSE_TEE)
                {
                    MTK_OMX_LOGE("set to VENC_DRV_VIDEO_FORMAT_INHOUSE_H264SEC driver \n");
                    return VENC_DRV_VIDEO_FORMAT_INHOUSE_H264SEC;
                }
                else
            {
                return VENC_DRV_VIDEO_FORMAT_H264SEC;
            }
            }
            else
            {
                //MTK_OMX_LOGE("set to normal driver \n");
                return VENC_DRV_VIDEO_FORMAT_H264;
            }
            break;
        case MTK_VENC_CODEC_ID_AVC_VGA:
        {
            return VENC_DRV_VIDEO_FORMAT_H264_VGA;
        }
        break;
        case MTK_VENC_CODEC_ID_MPEG4:
        case MTK_VENC_CODEC_ID_MPEG4_1080P:
            //if MT6577 define MTK_HYBRID_MULTI_THREAD :
            //if (mInputPortDef.format.video.nFrameWidth * mInputPortDef.format.video.nFrameHeight) > (1280* 720))
            //return VENC_DRV_VIDEO_FORMAT_MPEG4_1080P;
            return VENC_DRV_VIDEO_FORMAT_MPEG4;
        case MTK_VENC_CODEC_ID_MPEG4_SHORT:
            return VENC_DRV_VIDEO_FORMAT_H263;
        case MTK_VENC_CODEC_ID_H263_VT:
            return VENC_DRV_VIDEO_FORMAT_H263VT;
        case MTK_VENC_CODEC_ID_HEVC:
            return VENC_DRV_VIDEO_FORMAT_HEVC;
        case MTK_VENC_CODEC_ID_VP8:
            return VENC_DRV_VIDEO_FORMAT_VP8;
        default:
            MTK_OMX_LOGE("unsupported codec %d", mCodecId);
            return VENC_DRV_VIDEO_FORMAT_MAX;
    }
}

const char *MtkOmxVenc::codecName()
{
    switch (mCodecId)
    {
        case MTK_VENC_CODEC_ID_AVC:
        case MTK_VENC_CODEC_ID_AVC_VGA:
            return "AVC";
        case MTK_VENC_CODEC_ID_MPEG4:
        case MTK_VENC_CODEC_ID_MPEG4_SHORT:
        case MTK_VENC_CODEC_ID_MPEG4_1080P:
        case MTK_VENC_CODEC_ID_H263_VT:
            return "MP4";
        case MTK_VENC_CODEC_ID_HEVC:
            return "HEVC";
        case MTK_VENC_CODEC_ID_VP8:
            return "VP8";
        default:
            MTK_OMX_LOGE("unsupported codec %d", mCodecId);
            return "NON";
    }
}

VAL_BOOL_T MtkOmxVenc::QueryDriverEnc()
{
    switch (mCodecId)
    {
        case MTK_VENC_CODEC_ID_AVC:
        case MTK_VENC_CODEC_ID_AVC_VGA:
            return QueryDriverH264Enc();
        case MTK_VENC_CODEC_ID_MPEG4:
        case MTK_VENC_CODEC_ID_MPEG4_SHORT:
        case MTK_VENC_CODEC_ID_MPEG4_1080P:
        case MTK_VENC_CODEC_ID_H263_VT:
            return QueryDriverMPEG4Enc();
        case MTK_VENC_CODEC_ID_HEVC:
            return QueryDriverHEVCEnc();
        case MTK_VENC_CODEC_ID_VP8:
            return QueryDriverVP8Enc();
        default:
            MTK_OMX_LOGE("unsupported codec %d", mCodecId);
            return VAL_FALSE;
    }
}

//-------- encode drv query --------

VAL_BOOL_T MtkOmxVenc::QueryDriverVP8Enc()
{
    VENC_DRV_QUERY_VIDEO_FORMAT_T tQuery;
    VENC_DRV_QUERY_VIDEO_FORMAT_T *pQueryInfo = &tQuery;

    pQueryInfo->eVideoFormat = VENC_DRV_VIDEO_FORMAT_VP8;
    pQueryInfo->u4Profile = VENC_DRV_MS_VIDEO_PROFILE_MAX;
    pQueryInfo->eLevel = VENC_DRV_VIDEO_LEVEL_MAX;
    pQueryInfo->eResolution = VENC_DRV_RESOLUTION_SUPPORT_1080P;
    if (mInputScalingMode)
    {
        pQueryInfo->u4Width = mScaledWidth;
        pQueryInfo->u4Height = mScaledHeight;
    }
    else
    {
        pQueryInfo->u4Width = mInputPortDef.format.video.nFrameWidth;
        pQueryInfo->u4Height = mInputPortDef.format.video.nFrameHeight;
    }
    pQueryInfo->u4Bitrate = 0;
#if 0
    // todo
    if (VENC_DRV_MRESULT_FAIL == eVEncDrvQueryCapability(VENC_DRV_QUERY_TYPE_VIDEO_FORMAT, pQueryInfo, VAL_NULL))
    {
        MTK_OMX_LOGE("[ERROR] cannot support VP8 encoder");
        return VAL_FALSE;
    }
#endif
    MTK_OMX_LOGD("VAL_CHIP_ID: %d", mChipName);

    return VAL_TRUE;
}

//-------- encode drv setting --------
void MtkOmxVenc::EncSettingDrvResolution(void)
{
    mEncDrvSetting.eVEncFormat = CheckFormatToDrv(); // MtkOmxVenc:CheckFormatToDrv

    if (mInputScalingMode)
    {
        mEncDrvSetting.u4Width = mScaledWidth;
        mEncDrvSetting.u4Height = mScaledHeight;
    }
    else
    {
        mEncDrvSetting.u4Width = mInputPortDef.format.video.nFrameWidth;
        mEncDrvSetting.u4Height = mInputPortDef.format.video.nFrameHeight;
    }
    //if need color convert we can decide the buffer size, after concider the GPU limitation.
    if (NeedConversion())
    {
        if (mInputScalingMode)
        {
            mEncDrvSetting.u4BufWidth = VENC_ROUND_N(mScaledWidth, 16);
            mEncDrvSetting.u4BufHeight = VENC_ROUND_N(mScaledHeight, 16);
        }
        else
        {
            mEncDrvSetting.u4BufWidth = VENC_ROUND_N(mInputPortDef.format.video.nFrameWidth, 16);
            if (mEncDrvSetting.u4BufWidth < mGrallocWStride)
            {
                mEncDrvSetting.u4BufWidth = mGrallocWStride;
            }
            mEncDrvSetting.u4BufHeight = VENC_ROUND_N(mInputPortDef.format.video.nFrameHeight, 16);
        }
        //always give even height input to driver
        mEncDrvSetting.u4Height &= (~0x01);
    }
    else
    {
        //if meta mode the stride is decided by handle, not user
        mEncDrvSetting.u4BufWidth = (mStoreMetaDataInBuffers) ? mGrallocWStride : mInputPortDef.format.video.nStride;
        mEncDrvSetting.u4BufHeight = mInputPortDef.format.video.nSliceHeight;
    }

    return;
}

//-------- map omx profile & level to drv profile & level --------
VAL_UINT32_T MtkOmxVenc::Omx2DriverH264ProfileMap(OMX_VIDEO_AVCPROFILETYPE eProfile)
{
    switch (eProfile)
    {
        case OMX_VIDEO_AVCProfileBaseline:
            return VENC_DRV_H264_VIDEO_PROFILE_BASELINE;
            break;
        case OMX_VIDEO_AVCProfileMain:
            return VENC_DRV_H264_VIDEO_PROFILE_MAIN;
            break;
        case OMX_VIDEO_AVCProfileExtended:
            return VENC_DRV_H264_VIDEO_PROFILE_EXTENDED;
            break;
        case OMX_VIDEO_AVCProfileHigh:
            return VENC_DRV_H264_VIDEO_PROFILE_HIGH;
            break;
        case OMX_VIDEO_AVCProfileHigh10:
            return VENC_DRV_H264_VIDEO_PROFILE_HIGH_10;
            break;
        case OMX_VIDEO_AVCProfileHigh422:
            return VENC_DRV_H264_VIDEO_PROFILE_HIGH422;
            break;
        case OMX_VIDEO_AVCProfileHigh444:
            return VENC_DRV_H264_VIDEO_PROFILE_HIGH444;
            break;
        default:
            MTK_OMX_LOGE("[ERROR] Unsupported H264 Profile");
            return VENC_DRV_H264_VIDEO_PROFILE_BASELINE;
            break;
    }
}

VAL_UINT32_T MtkOmxVenc::Omx2DriverH264LevelMap(OMX_VIDEO_AVCLEVELTYPE eLevel)
{
    switch (eLevel)
    {
        case OMX_VIDEO_AVCLevel1:
            return VENC_DRV_VIDEO_LEVEL_1;
            break;
        case OMX_VIDEO_AVCLevel1b:
            return VENC_DRV_VIDEO_LEVEL_1b;
            break;
        case OMX_VIDEO_AVCLevel11:
            return VENC_DRV_VIDEO_LEVEL_1_1;
            break;
        case OMX_VIDEO_AVCLevel12:
            return VENC_DRV_VIDEO_LEVEL_1_2;
            break;
        case OMX_VIDEO_AVCLevel13:
            return VENC_DRV_VIDEO_LEVEL_1_3;
            break;
        case OMX_VIDEO_AVCLevel2:
            return VENC_DRV_VIDEO_LEVEL_2;
            break;
        case OMX_VIDEO_AVCLevel21:
            return VENC_DRV_VIDEO_LEVEL_2_1;
            break;
        case OMX_VIDEO_AVCLevel22:
            return VENC_DRV_VIDEO_LEVEL_2_2;
            break;
        case OMX_VIDEO_AVCLevel3:
            return VENC_DRV_VIDEO_LEVEL_3;
            break;
        case OMX_VIDEO_AVCLevel31:
            return VENC_DRV_VIDEO_LEVEL_3_1;
            break;
        case OMX_VIDEO_AVCLevel32:
            return VENC_DRV_VIDEO_LEVEL_3_2;
            break;
        case OMX_VIDEO_AVCLevel4:
            return VENC_DRV_VIDEO_LEVEL_4;
            break;
        case OMX_VIDEO_AVCLevel41:
            return VENC_DRV_VIDEO_LEVEL_4_1;
            break;
        case OMX_VIDEO_AVCLevel42:
            return VENC_DRV_VIDEO_LEVEL_4_2;
            break;
        case OMX_VIDEO_AVCLevel5:
            return VENC_DRV_VIDEO_LEVEL_5;
            break;
        case OMX_VIDEO_AVCLevel51:
            return VENC_DRV_VIDEO_LEVEL_5_1;
            break;
        default:
            MTK_OMX_LOGE("[ERROR] Unsupported H264 Level");
            return VENC_DRV_VIDEO_LEVEL_3_1;
            break;
    }
}

VAL_UINT32_T MtkOmxVenc::Omx2DriverMPEG4ProfileMap(OMX_VIDEO_MPEG4PROFILETYPE eProfile)
{
    switch (eProfile)
    {
        case OMX_VIDEO_MPEG4ProfileSimple:
            return VENC_DRV_MPEG_VIDEO_PROFILE_MPEG4_SIMPLE;
            break;
        default:
            MTK_OMX_LOGE("[ERROR] Unsupported MPEG4 Profile");
            return VENC_DRV_MPEG_VIDEO_PROFILE_MPEG4_SIMPLE;
            break;
    }
}

VAL_UINT32_T MtkOmxVenc::Omx2DriverMPEG4LevelMap(OMX_VIDEO_MPEG4LEVELTYPE eLevel)
{
    switch (eLevel)
    {
        case OMX_VIDEO_MPEG4Level0:
            return VENC_DRV_VIDEO_LEVEL_0;
            break;
        case OMX_VIDEO_MPEG4Level0b:
            return VENC_DRV_VIDEO_LEVEL_0;
            break;
        case OMX_VIDEO_MPEG4Level1:
            return VENC_DRV_VIDEO_LEVEL_1;
            break;
        case OMX_VIDEO_MPEG4Level2:
            return VENC_DRV_VIDEO_LEVEL_2;
            break;
        case OMX_VIDEO_MPEG4Level3:
            return VENC_DRV_VIDEO_LEVEL_3;
            break;
        case OMX_VIDEO_MPEG4Level4:
            return VENC_DRV_VIDEO_LEVEL_4;
            break;
        case OMX_VIDEO_MPEG4Level4a:
            return VENC_DRV_VIDEO_LEVEL_4;
            break;
        case OMX_VIDEO_MPEG4Level5:
            return VENC_DRV_VIDEO_LEVEL_5;
            break;
        default:
            MTK_OMX_LOGE("[ERROR] Unsupported MPEG4 Level");
            return VENC_DRV_VIDEO_LEVEL_4;
            break;
    }
}

VAL_UINT32_T MtkOmxVenc::Omx2DriverH263ProfileMap(OMX_VIDEO_H263PROFILETYPE eProfile)
{
    switch (eProfile)
    {
        case OMX_VIDEO_H263ProfileBaseline:
            return VENC_DRV_MPEG_VIDEO_PROFILE_H263_0;
            break;
        default:
            MTK_OMX_LOGE("[ERROR] Unsupported H263 Profile");
            return VENC_DRV_MPEG_VIDEO_PROFILE_H263_0;
            break;
    }
}

VAL_UINT32_T MtkOmxVenc::Omx2DriverH263LevelMap(OMX_VIDEO_H263LEVELTYPE eLevel)
{
    switch (eLevel)
    {
        case OMX_VIDEO_H263Level10:
            return VENC_DRV_VIDEO_LEVEL_1;
            break;
        case OMX_VIDEO_H263Level20:
            return VENC_DRV_VIDEO_LEVEL_2;
            break;
        case OMX_VIDEO_H263Level30:
            return VENC_DRV_VIDEO_LEVEL_3;
            break;
        case OMX_VIDEO_H263Level40:
            return VENC_DRV_VIDEO_LEVEL_4;
            break;
        case OMX_VIDEO_H263Level50:
            return VENC_DRV_VIDEO_LEVEL_5;
            break;
        case OMX_VIDEO_H263Level60:
            return VENC_DRV_VIDEO_LEVEL_6;
            break;
        case OMX_VIDEO_H263Level70:
            return VENC_DRV_VIDEO_LEVEL_7;
            break;
        default:
            MTK_OMX_LOGE("[ERROR] Unsupported H263 Level");
            return VENC_DRV_VIDEO_LEVEL_1;
            break;
    }
}

VAL_UINT32_T MtkOmxVenc::Omx2DriverHEVCProfileMap(OMX_VIDEO_HEVCPROFILETYPE eProfile)
{
    VENC_DRV_MRESULT_T mReturn = VENC_DRV_MRESULT_OK;
    OMX_U32 uIsHEVCSWPlatform = 1; // todo: VENC_DRV_GET_TYPE_IS_HEVC_SW_SPECIFIC_PLATFORM v4l2
    //mReturn = eVEncDrvGetParam((VAL_HANDLE_T)NULL, VENC_DRV_GET_TYPE_IS_HEVC_SW_SPECIFIC_PLATFORM, (VAL_VOID_T *)&mChipName, (VAL_VOID_T *)&uIsHEVCSWPlatform);
    MTK_OMX_LOGD("uIsHEVCSWPlatform %d", uIsHEVCSWPlatform);

    if (uIsHEVCSWPlatform)
    {
        return VENC_DRV_MPEG_VIDEO_PROFILE_MPEG4_SIMPLE;

    }
    else
    {
        switch (eProfile)
        {
            case OMX_VIDEO_HEVCProfileMain:
                return VENC_DRV_HEVC_VIDEO_PROFILE_MAIN;
                break;
            default:
                MTK_OMX_LOGE("[ERROR] Unsupported HEVC Profile, %x", eProfile);
                return OMX_VIDEO_HEVCProfileMain;
                break;
        }
    }
}

VAL_UINT32_T MtkOmxVenc::Omx2DriverHEVCLevelMap(OMX_VIDEO_HEVCLEVELTYPE eLevel)
{
    switch (eLevel)
    {
        case OMX_VIDEO_HEVCMainTierLevel1:
            return VENC_DRV_VIDEO_LEVEL_1;
            break;
        case OMX_VIDEO_HEVCHighTierLevel1:
            return VENC_DRV_VIDEO_LEVEL_1;
            break;
        case OMX_VIDEO_HEVCMainTierLevel2:
            return VENC_DRV_VIDEO_LEVEL_2;
            break;
        case OMX_VIDEO_HEVCHighTierLevel2:
            return VENC_DRV_VIDEO_LEVEL_2;
            break;
        case OMX_VIDEO_HEVCMainTierLevel21:
            return VENC_DRV_VIDEO_LEVEL_2_1;
            break;
        case OMX_VIDEO_HEVCHighTierLevel21:
            return VENC_DRV_VIDEO_LEVEL_2_1;
            break;
        case OMX_VIDEO_HEVCMainTierLevel3:
            return VENC_DRV_VIDEO_LEVEL_3;
            break;
        case OMX_VIDEO_HEVCHighTierLevel3:
            return VENC_DRV_VIDEO_LEVEL_3;
            break;
        case OMX_VIDEO_HEVCMainTierLevel31:
            return VENC_DRV_VIDEO_LEVEL_3_1;
            break;
        case OMX_VIDEO_HEVCHighTierLevel31:
            return VENC_DRV_VIDEO_LEVEL_3_1;
            break;
        case OMX_VIDEO_HEVCMainTierLevel4:
            return VENC_DRV_VIDEO_LEVEL_4;
            break;
        case OMX_VIDEO_HEVCHighTierLevel4:
            return VENC_DRV_VIDEO_LEVEL_4;
            break;
        case OMX_VIDEO_HEVCMainTierLevel41:
            return VENC_DRV_VIDEO_LEVEL_4_1;
            break;
        case OMX_VIDEO_HEVCHighTierLevel41:
            return VENC_DRV_VIDEO_LEVEL_4_1;
            break;
        case OMX_VIDEO_HEVCMainTierLevel5:
            return VENC_DRV_VIDEO_LEVEL_5;
            break;
        case OMX_VIDEO_HEVCHighTierLevel5:
            return VENC_DRV_VIDEO_LEVEL_5;
            break;

        default:
            MTK_OMX_LOGE("[ERROR] Unsupported HEVC Level, %x", eLevel);
            return VENC_DRV_VIDEO_LEVEL_5;
            break;
    }
}

//-------- init omx encode parameters --------
OMX_BOOL MtkOmxVenc::InitEncParams()
{
    //strcpy((char *)mCompRole, "video_encoder.h263");//add in InitXXXEncParas()
    // init input port format
    mInputPortFormat.nPortIndex         = MTK_OMX_INPUT_PORT;
    mInputPortFormat.nIndex             = 0;
    mInputPortFormat.eCompressionFormat = OMX_VIDEO_CodingUnused;
    mInputPortFormat.eColorFormat       = OMX_MTK_COLOR_FormatYV12;
    mInputPortFormat.xFramerate         = (30 << 16);

    // init output port format
    mOutputPortFormat.nPortIndex    = MTK_OMX_OUTPUT_PORT;
    mOutputPortFormat.nIndex        = 0;
    //mOutputPortFormat.eCompressionFormat = OMX_VIDEO_CodingH263;//add in InitXXXEncParas()
    mOutputPortFormat.eColorFormat  = OMX_COLOR_FormatUnused;
    mOutputPortFormat.xFramerate    = (30 << 16);

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
    mInputPortDef.format.video.xFramerate               = (30 << 16);
    mInputPortDef.format.video.cMIMEType                = (OMX_STRING)"raw";
    mInputPortDef.format.video.bFlagErrorConcealment    = OMX_FALSE;
    mInputPortDef.format.video.eCompressionFormat       = OMX_VIDEO_CodingUnused;
    mInputPortDef.format.video.eColorFormat             = OMX_MTK_COLOR_FormatYV12;

    mInputPortDef.nBufferCountActual                    = MTK_VENC_DEFAULT_INPUT_BUFFER_COUNT;
    mInputPortDef.nBufferCountMin                       = 1;
    mInputPortDef.nBufferSize                           = MTK_VENC_DEFAULT_INPUT_BUFFER_SIZE;
    mInputPortDef.bEnabled                              = OMX_TRUE;
    mInputPortDef.bPopulated                            = OMX_FALSE;

    // init output port definition
    mOutputPortDef.nPortIndex                           = MTK_OMX_OUTPUT_PORT;
    mOutputPortDef.eDomain                              = OMX_PortDomainVideo;
    //mOutputPortDef.format.video.cMIMEType = (OMX_STRING)"video/h263";
    mOutputPortDef.format.video.pNativeRender           = 0;
    mOutputPortDef.format.video.bFlagErrorConcealment   = OMX_FALSE;
    //mOutputPortDef.format.video.eCompressionFormat = OMX_VIDEO_CodingH263;
    mOutputPortDef.format.video.eColorFormat            = OMX_COLOR_FormatUnused;
    mOutputPortDef.format.video.nFrameWidth             = 176;
    mOutputPortDef.format.video.nStride                 = 176;
    mOutputPortDef.format.video.nFrameHeight            = 144;
    mOutputPortDef.format.video.nSliceHeight            = 144;
    mOutputPortDef.format.video.nBitrate                = MTK_VENC_DEFAULT_OUTPUT_BITRATE;
    mOutputPortDef.format.video.xFramerate              = (15 << 16);
    mOutputPortDef.eDir                                 = OMX_DirOutput;

    mOutputPortDef.nBufferCountActual                   = MtkVenc::BufferCountActual("8");
    MTK_OMX_LOGD_ENG("[TEST] mtk.omx.venc.bufcount set count to %d", mOutputPortDef.nBufferCountActual);
    //mOutputPortDef.nBufferCountActual                   = MTK_VENC_DEFAULT_OUTPUT_BUFFER_COUNT;
    mOutputPortDef.nBufferCountMin                      = 1;
    mOutputPortDef.nBufferSize                          = MTK_VENC_DEFAULT_OUTPUT_BUFFER_SIZE_MPEG4;
    mOutputPortDef.bEnabled                             = OMX_TRUE;
    mOutputPortDef.bPopulated                           = OMX_FALSE;

    // init bitrate type
    mBitrateType.nPortIndex     = MTK_OMX_OUTPUT_PORT;
    mBitrateType.eControlRate   = OMX_Video_ControlRateConstant;
    mBitrateType.nTargetBitrate = 64000;

    // init frame rate type
    mFrameRateType.nPortIndex       = MTK_OMX_OUTPUT_PORT;
    mFrameRateType.xEncodeFramerate = (30 << 16);

    //init quantization type
    mQuantizationType.nPortIndex    = MTK_OMX_OUTPUT_PORT;
    mQuantizationType.nQpI          = 0;
    mQuantizationType.nQpP          = 0;
    mQuantizationType.nQpB          = 0;

    // init macro block size
    MTK_OMX_MEMSET(&mVbsmcType, 0x00, sizeof(OMX_VIDEO_PARAM_VBSMCTYPE));
    mVbsmcType.nPortIndex   = MTK_OMX_OUTPUT_PORT;
    mVbsmcType.b16x16       = OMX_TRUE;

    // init motion vector type
    mMvType.nPortIndex      = MTK_OMX_OUTPUT_PORT;
    mMvType.eAccuracy       = OMX_Video_MotionVectorQuarterPel;
    mMvType.bUnrestrictedMVs = OMX_TRUE;
    mMvType.sXSearchRange   = 16;
    mMvType.sYSearchRange   = 16;

    // init intra refresh type
    mIntraRefreshType.nPortIndex    = MTK_OMX_OUTPUT_PORT;
    mIntraRefreshType.eRefreshMode  = OMX_VIDEO_IntraRefreshCyclic;
    mIntraRefreshType.nCirMBs       = 0;

    // init error correction type
    mErrorCorrectionType.nPortIndex             = MTK_OMX_OUTPUT_PORT;
    mErrorCorrectionType.bEnableDataPartitioning = OMX_FALSE;

    // init bitrate config
    mConfigBitrate.nPortIndex       = MTK_OMX_OUTPUT_PORT;
    mConfigBitrate.nEncodeBitrate   = MTK_VENC_DEFAULT_OUTPUT_BITRATE;

    // init intra refresh config
    mConfigIntraRefreshVopType.nPortIndex       = MTK_OMX_OUTPUT_PORT;
    mConfigIntraRefreshVopType.IntraRefreshVOP  = OMX_FALSE;

    // allocate input buffer headers address array
    mInputBufferHdrs = (OMX_BUFFERHEADERTYPE **)MTK_OMX_ALLOC(
                           sizeof(OMX_BUFFERHEADERTYPE *)*mInputPortDef.nBufferCountActual);
    MTK_OMX_MEMSET(mInputBufferHdrs, 0x00, sizeof(OMX_BUFFERHEADERTYPE *)*mInputPortDef.nBufferCountActual);

    // allocate output buffer headers address array
    mOutputBufferHdrs = (OMX_BUFFERHEADERTYPE **)MTK_OMX_ALLOC(
                            sizeof(OMX_BUFFERHEADERTYPE *)*mOutputPortDef.nBufferCountActual);
    MTK_OMX_MEMSET(mOutputBufferHdrs, 0x00, sizeof(OMX_BUFFERHEADERTYPE *)*mOutputPortDef.nBufferCountActual);

    return OMX_TRUE;
}

OMX_BOOL MtkOmxVenc::InitH263EncParams()
{
    // init common part
    InitEncParams();

    strncpy((char *)mCompRole, "video_encoder.h263", sizeof(mCompRole));

    // init output port format
    mOutputPortFormat.eCompressionFormat = OMX_VIDEO_CodingH263;

    // init output port definition
    mOutputPortDef.format.video.cMIMEType           = (OMX_STRING)"video/h263";
    mOutputPortDef.format.video.eCompressionFormat  = OMX_VIDEO_CodingH263;

    // init h263 type
    MTK_OMX_MEMSET(&mH263Type, 0x00, sizeof(OMX_VIDEO_PARAM_H263TYPE));
    mH263Type.nPortIndex                = MTK_OMX_OUTPUT_PORT;
    mH263Type.eProfile                  = OMX_VIDEO_H263ProfileBaseline;
    mH263Type.eLevel                    = OMX_VIDEO_H263Level10;
    mH263Type.bPLUSPTYPEAllowed         = OMX_FALSE;
    mH263Type.nAllowedPictureTypes      = OMX_VIDEO_PictureTypeI | OMX_VIDEO_PictureTypeP;
    mH263Type.bForceRoundingTypeToZero  = OMX_TRUE;
    mH263Type.nPictureHeaderRepetition  = 0;
    mH263Type.nGOBHeaderInterval        = 0;

    // init profile level type
    mProfileLevelType.nPortIndex    = MTK_OMX_OUTPUT_PORT;
    mProfileLevelType.nProfileIndex = 0;
    mProfileLevelType.eProfile      = OMX_VIDEO_H263ProfileBaseline;
    mProfileLevelType.eLevel        = OMX_VIDEO_H263Level10;

    return OMX_TRUE;
}

OMX_BOOL MtkOmxVenc::InitMpeg4EncParams()
{
    // init common part
    InitEncParams();

    strncpy((char *)mCompRole, "video_encoder.mpeg4", sizeof(mCompRole));

    // init output port format
    mOutputPortFormat.eCompressionFormat = OMX_VIDEO_CodingMPEG4;

    // init output port definition
    mOutputPortDef.format.video.cMIMEType = (OMX_STRING)"video/mpeg4";
    mOutputPortDef.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4;

    // init mpeg4 type
    MTK_OMX_MEMSET(&mMpeg4Type, 0x00, sizeof(OMX_VIDEO_PARAM_MPEG4TYPE));
    mMpeg4Type.nPortIndex           = MTK_OMX_OUTPUT_PORT;
    mMpeg4Type.eProfile             = OMX_VIDEO_MPEG4ProfileSimple;
    mMpeg4Type.eLevel               = OMX_VIDEO_MPEG4Level3;
    mMpeg4Type.nPFrames             = 10;
    mMpeg4Type.nBFrames             = 0;
    mMpeg4Type.nMaxPacketSize       = 255;
    mMpeg4Type.nAllowedPictureTypes = OMX_VIDEO_PictureTypeI | OMX_VIDEO_PictureTypeP;
    mMpeg4Type.bGov                 = OMX_FALSE;

    // init profile level type
    mProfileLevelType.nPortIndex    = MTK_OMX_OUTPUT_PORT;
    mProfileLevelType.nProfileIndex = 0;
    mProfileLevelType.eProfile      = OMX_VIDEO_MPEG4ProfileSimple;
    mProfileLevelType.eLevel        = OMX_VIDEO_MPEG4Level3;

    return OMX_TRUE;
}

OMX_BOOL MtkOmxVenc::InitAvcEncParams()
{
    // init common part
    InitEncParams();

    strncpy((char *)mCompRole, "video_encoder.avc", sizeof(mCompRole));

    // init output port format
    mOutputPortFormat.eCompressionFormat = OMX_VIDEO_CodingAVC;

    // init output port definition
    mOutputPortDef.format.video.cMIMEType = (OMX_STRING)"video/avc";
    mOutputPortDef.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;

    //mOutputPortDef.nBufferSize = MTK_VENC_DEFAULT_OUTPUT_BUFFER_SIZE_AVC;
    {
        //static bool bDump = true;
        int bufK = MtkVenc::BufferSize();
        MTK_OMX_LOGE("bufk = %d", bufK);
        if (bufK == 0)
        {
            //property_get("ro.mtk_config_max_dram_size", value, "0x40000000");
            if (MtkVenc::MaxDramSize() == 0x20000000)
            {
                mOutputPortDef.nBufferSize = MTK_VENC_DEFAULT_OUTPUT_BUFFER_SIZE_AVC / 2; // For LCA project
            }
            else
            {
                mOutputPortDef.nBufferSize = MTK_VENC_DEFAULT_OUTPUT_BUFFER_SIZE_AVC;
            }
        }
        else
        {
            mOutputPortDef.nBufferSize = bufK * 1024;
        }
    }

    // init avc type
    MTK_OMX_MEMSET(&mAvcType, 0x00, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
    mAvcType.nPortIndex             = MTK_OMX_OUTPUT_PORT;
    mAvcType.eProfile               = defaultAvcProfile(mChipName);
    mAvcType.eLevel                 = defaultAvcLevel(mChipName);
    mAvcType.nPFrames               = 0xFFFFFFFF; //Default value
    mAvcType.nBFrames               = 0; // B frames are not supported
    mAvcType.nAllowedPictureTypes   = OMX_VIDEO_PictureTypeI | OMX_VIDEO_PictureTypeP;
    mAvcType.nRefFrames             = 1;
    mAvcType.eLoopFilterMode        = OMX_VIDEO_AVCLoopFilterEnable;
    mAvcType.bEnableFMO             = OMX_FALSE;
    mAvcType.bFrameMBsOnly          = OMX_TRUE;
    mAvcType.bMBAFF                 = OMX_FALSE;
    mAvcType.bEntropyCodingCABAC    = OMX_FALSE;
    mAvcType.bWeightedPPrediction   = OMX_FALSE;
    mAvcType.bDirect8x8Inference    = OMX_FALSE;

    // init profile level type
    mProfileLevelType.nPortIndex    = MTK_OMX_OUTPUT_PORT;
    mProfileLevelType.nProfileIndex = 0;
    mProfileLevelType.eProfile      = defaultAvcProfile(mChipName);
    mProfileLevelType.eLevel        = defaultAvcLevel(mChipName);

    //for SECURE VIDEO PATH
    InitSecEncParams();

    return OMX_TRUE;
}

OMX_BOOL MtkOmxVenc::InitHevcEncParams()
{
    // init common part
    InitEncParams();

    strncpy((char *)mCompRole, "video_encoder.hevc", sizeof(mCompRole));

    // init output port format
    mOutputPortFormat.eCompressionFormat = OMX_VIDEO_CodingHEVC;

    // init output port definition
    mOutputPortDef.format.video.cMIMEType           = (OMX_STRING)"video/hevc";
    mOutputPortDef.format.video.eCompressionFormat  = OMX_VIDEO_CodingHEVC;

    //mOutputPortDef.nBufferSize = MTK_VENC_DEFAULT_OUTPUT_BUFFER_SIZE_HEVC;
    {
        //static bool bDump = true;
        int bufK = MtkVenc::BufferSize();
        MTK_OMX_LOGE("bufk = %d", bufK);
        if (bufK == 0)
        {
            mOutputPortDef.nBufferSize = MTK_VENC_DEFAULT_OUTPUT_BUFFER_SIZE_HEVC;
        }
        else
        {
            mOutputPortDef.nBufferSize = bufK * 1024;
        }
    }

    // init hevc type
    MTK_OMX_MEMSET(&mHevcType, 0x00, sizeof(OMX_VIDEO_PARAM_HEVCTYPE));
    mHevcType.nPortIndex = MTK_OMX_OUTPUT_PORT;
    mHevcType.eProfile  = defaultHevcProfile(mChipName);
    mHevcType.eLevel    = defaultHevcLevel(mChipName);

    // init profile level type
    mProfileLevelType.nPortIndex    = MTK_OMX_OUTPUT_PORT;
    mProfileLevelType.nProfileIndex = 0;
    mProfileLevelType.eProfile      = defaultHevcProfile(mChipName);
    mProfileLevelType.eLevel        = defaultHevcLevel(mChipName);

    return OMX_TRUE;
}

OMX_BOOL MtkOmxVenc::InitVP8EncParams()
{
    // init common part
    InitEncParams();

    strncpy((char *)mCompRole, "video_encoder.vp8", sizeof(mCompRole));

    // init output port format
    mOutputPortFormat.eCompressionFormat = OMX_VIDEO_CodingVP8;

    // init output port definition
    mOutputPortDef.format.video.cMIMEType           = (OMX_STRING)"video/x-vnd.on2.vp8";
    mOutputPortDef.format.video.eCompressionFormat  = OMX_VIDEO_CodingVP8;

    //mOutputPortDef.nBufferSize = MTK_VENC_DEFAULT_OUTPUT_BUFFER_SIZE_HEVC;
    {
        //static bool bDump = true;
        int bufK = MtkVenc::BufferSize();
        MTK_OMX_LOGE("bufk = %d", bufK);
        if (bufK == 0)
        {
            mOutputPortDef.nBufferSize = MTK_VENC_DEFAULT_OUTPUT_BUFFER_SIZE_VP8;
        }
        else
        {
            mOutputPortDef.nBufferSize = bufK * 1024;
        }
    }

    // init VP8 type
    MTK_OMX_MEMSET(&mVp8Type, 0x00, sizeof(OMX_VIDEO_PARAM_VP8TYPE));
    mVp8Type.nPortIndex           = MTK_OMX_OUTPUT_PORT;
    mVp8Type.eProfile             = OMX_VIDEO_VP8ProfileUnknown;
    mVp8Type.eLevel               = OMX_VIDEO_VP8LevelUnknown;

    // init profile level type
    mProfileLevelType.nPortIndex    = MTK_OMX_OUTPUT_PORT;
    mProfileLevelType.nProfileIndex = 0;
    mProfileLevelType.eProfile      = OMX_VIDEO_VP8ProfileUnknown;
    mProfileLevelType.eLevel        = OMX_VIDEO_VP8LevelUnknown;

    return OMX_TRUE;
}

OMX_BOOL MtkOmxVenc::InitSecEncParams(void)
{
    // check secure environment [
    int svp = MtkVenc::SvpSupport();
    mTeeEncType = NONE_TEE;
    if (svp)
    {
        //MTK_OMX_LOGE("%s ro.mtk_trustonic_tee_support %s",__FUNCTION__,value);
        if (MtkVenc::TrustTonicTeeSupport())
        {
            mTeeEncType = TRUSTONIC_TEE;
        }
        else
        {
            VENC_DRV_MRESULT_T mReturn = VENC_DRV_MRESULT_OK;
            OMX_U32 uIsInHouseReady = 0; // todo: VENC_DRV_GET_TYPE_IS_INHOUSE_TEE v4l2
            //mReturn = eVEncDrvGetParam((VAL_HANDLE_T)NULL, VENC_DRV_GET_TYPE_IS_INHOUSE_TEE, (VAL_VOID_T *)&mChipName, (VAL_VOID_T *)&uIsInHouseReady);
            //MTK_OMX_LOGD("uIsInHouseReady %d", uIsInHouseReady);

           //if(mChipName == VAL_CHIP_NAME_MT8163 || mChipName == VAL_CHIP_NAME_MT8173 || mChipName == VAL_CHIP_NAME_MT8167)
           if(uIsInHouseReady)
           {
               if (MtkVenc::InHouseReady())
               {
                    MTK_OMX_LOGE("%s ro.mtk_venc_inhouse_ready",__FUNCTION__);
                    mTeeEncType = INHOUSE_TEE;
               }
           }
           else
           {
               if (MtkVenc::MicroTrustTeeSupport())
               {//Microtrust TEE
                   mTeeEncType = MICROTRUST_TEE;
               }
           }
        }
    }

    mIsSecureInst = (svp && mTeeEncType != NONE_TEE) ? OMX_TRUE : OMX_FALSE;
    // ] check secure environment
    MTK_OMX_LOGD("%s svp support: %d, type: %d", __FUNCTION__, svp, mTeeEncType);

    // setup basic secure variables.
    if (OMX_TRUE == mIsSecureInst)
    {
        if (mTeeEncType == TRUSTONIC_TEE || mTeeEncType == MICROTRUST_TEE)
        {
            memset(&tlc, 0, sizeof(tlc));
            int supportGP = MtkVenc::TeeGpSupport();
            if (1 == supportGP){
                tlc.tlcLib = dlopen("/vendor/lib/AVCSecureVencCA.so", RTLD_LAZY);
            }
            else{
                tlc.tlcLib = dlopen("/vendor/lib/libMtkH264SecVencTLCLib.so", RTLD_LAZY);
            }

            if (tlc.tlcLib != NULL) {
                tlc.tlcHandleCreate = (void *(*)())dlsym(tlc.tlcLib, "MtkH264SecVenc_tlcHandleCreate");
                tlc.tlcHandleRelease = (void (*)(void *))dlsym(tlc.tlcLib, "MtkH264SecVenc_tlcHandleRelease");

                tlc.tlcDumpSecMem = (int (*)(void *, uint32_t, void *, uint32_t))dlsym(tlc.tlcLib,
                                                                                       "MtkH264SecVenc_tlcDumpSecMem");
                tlc.tlcFillSecMem = (int (*)(void *, void *, uint32_t, uint32_t))dlsym(tlc.tlcLib,
                                                                                       "MtkH264SecVenc_tlcFillSecMem");
                bHasSecTlc = true;
            }
            else {
                MTK_OMX_LOGE("[ERROR] don't get secure lib %d", supportGP);
                bHasSecTlc = false;
            }
        }
        //else if (mTeeEncType == INHOUSE_TEE)//...
    }

    return mIsSecureInst;
}

OMX_BOOL MtkOmxVenc::DeInitSecEncParams(void)
{
    //TODO: free mTestSecInput ?

    DeInitSecCnvtBuffer();

    if (bHasSecTlc)
    {
        dlclose(tlc.tlcLib);
    }
    return OMX_TRUE;
}

bool MtkOmxVenc::postHdrProcess(OMX_U8 *aOutputBuf, OMX_U32 aOutputSize, VENC_DRV_DONE_RESULT_T *rEncResult)
{
    (void)(aOutputSize);
    if (MTK_VENC_CODEC_ID_AVC == mCodecId || MTK_VENC_CODEC_ID_AVC_VGA == mCodecId)
    {
        //for MTK_CAM_STEREO_CAMERA_SUPPORT
        OMX_U32 SideBySideSEI[4] = { 0x01000000, 0x81072d06, 0x03000081, 0x80200100 };
        OMX_U32 TopAndBottomSEI[4] = { 0x01000000, 0x82072d06, 0x03000001, 0x80200100 };
        switch (m3DVideoRecordMode)
        {
            case OMX_VIDEO_H264FPA_FRAMESEQUENCE:
                MTK_OMX_LOGE("Frame sequence of FPA is not support\n");
                break;
            case OMX_VIDEO_H264FPA_SIDEBYSIDE:
                memcpy(aOutputBuf + rEncResult->prBSBuf->u4BSSize, SideBySideSEI, sizeof(OMX_U32) * 4);
                rEncResult->prBSBuf->u4BSSize += sizeof(OMX_U32) * 4;
                break;
            case OMX_VIDEO_H264FPA_TOPANDBOTTOM:
                memcpy(aOutputBuf + rEncResult->prBSBuf->u4BSSize, TopAndBottomSEI, sizeof(OMX_U32) * 4);
                rEncResult->prBSBuf->u4BSSize += sizeof(OMX_U32) * 4;
                break;
            case OMX_VIDEO_H264FPA_NONE:
            default:
                break;
        }
    }
    return true;
}

bool MtkOmxVenc::preEncProcess(void)
{
    if (OMX_TRUE == mIsSecureInst && mIsSecureSrc)
    {
        if ((mDrawStripe == true) || (mDrawBlack == OMX_TRUE))
        {
            MTK_OMX_LOGD("Cannot modify frame buffer data when secure path enabled\n");
        }
    }
    else
    {
        if (mDrawStripe == true)
        {
            DrawYUVStripeLine((OMX_U8 *)mFrameBuf.rFrmBufAddr.u4VA);
        }

        if (mDrawBlack == OMX_TRUE)//for Miracast test case SIGMA 5.1.11 workaround
        {
            int YSize = mInputPortDef.format.video.nStride * mInputPortDef.format.video.nSliceHeight;
            memset((unsigned char *)mFrameBuf.rFrmBufAddr.u4VA, 0, YSize);
            memset((unsigned char *)((VAL_UINT32_T)mFrameBuf.rFrmBufAddr.u4VA + YSize),
                   0x80, mFrameBuf.rFrmBufAddr.u4Size - YSize);
        }
    }
    return true;
}

bool MtkOmxVenc::setDynamicFrameRate(OMX_BUFFERHEADERTYPE *pInputBuf)
{
    VAL_UINT32_T u4TimeDiff;
    VAL_UINT32_T u4AdjustedFrameRate;
    VAL_UINT32_T i,u4Count,u4Sum;
    VAL_UINT32_T u4FrameRateUpperBound = 30;
    VAL_UINT32_T u4FrameRateLowerBound = 10;

    VENC_DRV_MRESULT_T  rRet = VENC_DRV_MRESULT_OK;

    if (!mIsViLTE)
    {
        return false;
    }

    if (mLastFrameTimeStamp == 0)
    {
        return false;
    }

    if (pInputBuf->nTimeStamp < mLastFrameTimeStamp)
    {
        MTK_OMX_LOGE("setDynamicFrameRate invalid time stamp sequence\n");
        return false;
    }

    if (pInputBuf->nFlags & OMX_BUFFERFLAG_CAMERASWITCH)
    {
        for (i = 0 ;i < MTK_VENC_TIMESTAMP_WINDOW_SIZE ;i++)
        {
            mInputTimeStampDiffs[i] = 0;
        }
        MTK_OMX_LOGD("setDynamicFrameRate camera switch event\n");
        return false;
    }
    //MTK_OMX_LOGD("setDynamicFrameRate timeStamp %lld lastframe timestamp %lld\n",pInputBuf->nTimeStamp,mLastFrameTimeStamp);
    u4TimeDiff = pInputBuf->nTimeStamp - mLastFrameTimeStamp;

    if (u4TimeDiff > 100000)
    {
        MTK_OMX_LOGE("setDynamicFrameRate obnormal time diff timeStamp %lld lastframe timestamp %lld\n",pInputBuf->nTimeStamp,mLastFrameTimeStamp);
    }

    //MTK_OMX_LOGD("setDynamicFrameRate u4TimeDiff %d\n",u4TimeDiff);

    for (i =(MTK_VENC_TIMESTAMP_WINDOW_SIZE - 1) ;i>0;i--)
    {
        mInputTimeStampDiffs[i] = mInputTimeStampDiffs[i-1];
    }
    mInputTimeStampDiffs[0] = u4TimeDiff;


    u4Sum = 0;
    u4Count = 0;
    for (i = 0 ;i < MTK_VENC_TIMESTAMP_WINDOW_SIZE ;i++)
    {
        if (mInputTimeStampDiffs[i] !=0)
        {
            u4Sum += mInputTimeStampDiffs[i];
            u4Count++;

        }
    }

    if ((u4Sum == 0) || (u4Count == 0))
    {
        MTK_OMX_LOGE("setDynamicFrameRate invalid time stamp record\n");
        return false;
    }
    u4AdjustedFrameRate = (1000000 + ((u4Sum/u4Count)/2))/(u4Sum/u4Count);

    if (u4AdjustedFrameRate > u4FrameRateUpperBound)
    {
        u4AdjustedFrameRate = u4FrameRateUpperBound;
        MTK_OMX_LOGD("setDynamicFrameRate limit frame rate to upper bound\n");
    }

    if (u4AdjustedFrameRate < u4FrameRateLowerBound)
    {
        u4AdjustedFrameRate = u4FrameRateLowerBound;
        MTK_OMX_LOGD("setDynamicFrameRate limit frame rate to lowner bound\n");
    }

    MTK_OMX_LOGD("setDynamicFrameRate frame rate %d\n",u4AdjustedFrameRate);

    // !todo VENC_DRV_SET_TYPE_DETECTED_FRAMERATE v4l2
    /*rRet = eVEncDrvSetParam(mDrvHandle, VENC_DRV_SET_TYPE_DETECTED_FRAMERATE,
                                (VAL_VOID_T *)&u4AdjustedFrameRate, VAL_NULL);
    */
    if (VENC_DRV_MRESULT_FAIL == rRet)
    {
        MTK_OMX_LOGE("[ERROR] setDynamicFrameRate cannot set param framerate");
    }

    return true;
}

void MtkOmxVenc::normalSecureSwitchHndling(OMX_BUFFERHEADERTYPE *pInputBuf, OMX_BUFFERHEADERTYPE *pOutputBuf)
{
    mIsSecureSrc = (OMX_TRUE == mIsSecureSrc) ? OMX_FALSE : OMX_TRUE;
    MTK_OMX_LOGD("set secure src:%d", mIsSecureSrc);

    //Notify Encode Thread wait PortReconfig
    mPortReconfigInProgress = OMX_TRUE; //Encode Thread need to wait Port Re-config

    //Put current frame back to Queue
    pOutputBuf->nTimeStamp = mLastFrameTimeStamp;
    pOutputBuf->nFilledLen = 0;
    if (mDoConvertPipeline)
    {
        EncHandleEmptyBufferDone(pInputBuf);
    }
    else
    {
        QueueBufferAdvance(mpVencInputBufQ, pInputBuf);
    }
    HandleFillBufferDone(pOutputBuf);
    if (mDummyIdx >= 0)
    {
        QueueOutputBuffer(mDummyIdx);//the VencOutBufQ MUST be REAL OutBufQ!!!
        mDummyIdx = -1;
    }

    if (mDoConvertPipeline)  //switch internal bufQ to secure or normal
    {
        //clear C-out Q
        LOCK(mpConvertOutputBufQ->mBufQLock);
        mpConvertOutputBufQ->Clear();
        mpConvertOutputBufQ->mPendingNum = 0;
        UNLOCK(mpConvertOutputBufQ->mBufQLock);
        //both C-out & E-in bufHdrQ should be EMPTY!!
        MTK_OMX_LOGD("C-out:%d, E-in:%d", mpConvertOutputBufQ->Size(), mpVencInputBufQ->Size());
        int i;
        if (mIsSecureSrc)
        {
            // check there are secure convert buffers, if no, allocate here.
            if (checkSecCnvtBufferNum() <= 0)
            {
                InitSecCnvtBuffer(CONVERT_MAX_BUFFER);
            }
            //normal to secure
            for (i = 0; i < CONVERT_MAX_BUFFER; ++i)
            {
                //memset(mConvertOutputBufferHdrs[i], 0, sizeof(OMX_BUFFERHEADERTYPE));
                *(buffer_handle_t *)(mConvertOutputBufferHdrs[i]->pBuffer + 4) = mSecConvertBufs[i]->handle;
                mConvertOutputBufferHdrs[i]->nAllocLen      = 8;
                mConvertOutputBufferHdrs[i]->nFlags         = 0;
                mConvertOutputBufferHdrs[i]->nTimeStamp     = 0;
                mConvertOutputBufferHdrs[i]->nFilledLen     = 0;
                mConvertOutputBufferHdrs[i]->nOffset        = 0;
                mConvertOutputBufferHdrs[i]->pMarkData      = 0;


                //memset(mVencInputBufferHdrs[i], 0, sizeof(OMX_BUFFERHEADERTYPE));
                *(buffer_handle_t *)(mVencInputBufferHdrs[i]->pBuffer + 4) = mSecConvertBufs[i]->handle;
                mVencInputBufferHdrs[i]->nAllocLen      = 8;
                mVencInputBufferHdrs[i]->nFlags         = 0;
                mVencInputBufferHdrs[i]->nTimeStamp     = 0;
                mVencInputBufferHdrs[i]->nFilledLen     = 0;
                mVencInputBufferHdrs[i]->nOffset        = 0;
                mVencInputBufferHdrs[i]->pMarkData      = 0;
            }
        }
        else
        {
            //secure to normal
            for (i = 0; i < CONVERT_MAX_BUFFER; ++i)
            {
                //memset(mConvertOutputBufferHdrs[i], 0, sizeof(OMX_BUFFERHEADERTYPE));
                mConvertOutputBufferHdrs[i]->nAllocLen      = mCnvtBufferSize;
                mConvertOutputBufferHdrs[i]->nFlags         = 0;
                mConvertOutputBufferHdrs[i]->nTimeStamp     = 0;
                mConvertOutputBufferHdrs[i]->nFilledLen     = 0;
                mConvertOutputBufferHdrs[i]->nOffset        = 0;
                mConvertOutputBufferHdrs[i]->pMarkData      = 0;

                //memset(mVencInputBufferHdrs[i], 0, sizeof(OMX_BUFFERHEADERTYPE));
                mVencInputBufferHdrs[i]->nAllocLen      = mCnvtBufferSize;
                mVencInputBufferHdrs[i]->nFlags         = 0;
                mVencInputBufferHdrs[i]->nTimeStamp     = 0;
                mVencInputBufferHdrs[i]->nFilledLen     = 0;
                mVencInputBufferHdrs[i]->nOffset        = 0;
                mVencInputBufferHdrs[i]->pMarkData      = 0;
            }
        }
        LOCK(mpConvertOutputBufQ->mBufQLock);
        //fill convert output bufq
        for (i = 0; i < CONVERT_MAX_BUFFER; ++i)
        {
            mpConvertOutputBufQ->Push(i);
            ++mpConvertOutputBufQ->mPendingNum;
        }
        UNLOCK(mpConvertOutputBufQ->mBufQLock);
    }
    else//reset convert buffer
    {
        //convert buffer for secure path is not required now
#if 0
        if (mIsSecureSrc)
        {
            // check there are secure convert buffers, if no, allocate here.
            if (checkSecCnvtBufferNum() <= 0)
            {
                InitSecCnvtBuffer(1);
            }
            //normal to secure, set handle to buffer
            *(buffer_handle_t *)(mCnvtBuffer + 4) = mSecConvertBufs[0]->handle;
        }
#endif
    }
    //Notify that Output buffers' format goes to reconfig
    MTK_OMX_LOGD("--- OMX_EventPortSettingsChanged ---");
    mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                           mAppData,
                           OMX_EventPortSettingsChanged,
                           MTK_OMX_OUTPUT_PORT,
                           OMX_IndexVendorMtkOmxVencSwitchWFDSecureOut,
                           NULL);
    ++mReconfigCount;
    mFrameCount = 0;
    return;
}

bool MtkOmxVenc::checkMBAFF(void)
{
    OMX_U32 u4MBAFF = (OMX_U32) MtkVenc::MBAFF();
    MTK_OMX_LOGD("MBAFF 0x%x", u4MBAFF);
    if (u4MBAFF != 2)
    {
        return u4MBAFF;
    }
    else
    {
        bool fgMBAFF = mAvcType.bMBAFF;
        if (!fgMBAFF ||
            mEncDrvSetting.u4Profile == VENC_DRV_H264_VIDEO_PROFILE_BASELINE ||
            mWFDMode == OMX_TRUE)
        {
            return false;
        }

        // Chip list
        if (0)
        {
            if (mEncDrvSetting.u4BufWidth * mEncDrvSetting.u4BufHeight >= 1920 * 1088 &&
                mEncDrvSetting.u4BufHeight % 32 == 0)
            {
                return true;
            }
        }
    }
    return false;
}

bool MtkOmxVenc::checkH264MultiSlice(VAL_UINT32_T u4FrameWidth, VAL_UINT32_T u4FrameHeight)
{
    VENC_DRV_MRESULT_T mReturn = VENC_DRV_MRESULT_OK;
    OMX_U32 uGetMultiSliceType = 0; // todo: VENC_DRV_GET_TYPE_MULTISLICE_TYPE v4l2
    //mReturn = eVEncDrvGetParam((VAL_HANDLE_T)NULL, VENC_DRV_GET_TYPE_MULTISLICE_TYPE, (VAL_VOID_T *)&mChipName, (VAL_VOID_T *)&uGetMultiSliceType);
    MTK_OMX_LOGD("uGetMultiSliceType %d", uGetMultiSliceType);

    switch (uGetMultiSliceType)
    {
        // sw encoder
        //D2
        case VENC_DRV_MULTISLICE_TYPE_ONE:
        {
            return mIsViLTE;
        }
        break;
        //80
        case VENC_DRV_MULTISLICE_TYPE_TWO:
        {
            // Enable multi-slice encoding only when the recorded size >= 720p.
            return (u4FrameWidth * u4FrameHeight >= 1280 * 720) ? true : false;
        }
        break;
        //70
        case VENC_DRV_MULTISLICE_TYPE_THREE:
        {
            // Enable multi-slice encoding only when the recorded size >= HVGA
            return (u4FrameWidth * u4FrameHeight >= 480 * 320) ? true : false;
        }
        break;
        default:
            break;
    }
    return false;
}

OMX_BOOL MtkOmxVenc::IsConstantBitrate(void)
{
    if (mSetConstantBitrateMode == OMX_TRUE)
    {

        VENC_DRV_MRESULT_T mReturn = VENC_DRV_MRESULT_OK;
        OMX_U32 uIsDenaliPlatform = 0; // todo VENC_DRV_GET_TYPE_IS_DENALI_PLATFORM v4l2
        //mReturn = eVEncDrvGetParam((VAL_HANDLE_T)NULL, VENC_DRV_GET_TYPE_IS_DENALI_PLATFORM, (VAL_VOID_T *)&mChipName, (VAL_VOID_T *)&uIsDenaliPlatform);
        MTK_OMX_LOGD("uIsDenaliPlatform %d", uIsDenaliPlatform);

        if(uIsDenaliPlatform)
        {
                return OMX_TRUE;
        }
        switch (mChipName)
        {
            case VAL_CHIP_NAME_DENALI_1:
            case VAL_CHIP_NAME_DENALI_2:
            case VAL_CHIP_NAME_DENALI_3:
                return OMX_TRUE;
            default:
                break;
        }
    }
    return OMX_FALSE;
}

#define CASE_RET_ENUM_WORD(X) \
    case X: return #X

const char* MtkOmxVenc::indexType(OMX_INDEXTYPE index)
{
    switch(index){

        CASE_RET_ENUM_WORD(OMX_IndexComponentStartUnused);
        CASE_RET_ENUM_WORD(OMX_IndexParamPriorityMgmt);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioInit);
        CASE_RET_ENUM_WORD(OMX_IndexParamImageInit);
        CASE_RET_ENUM_WORD(OMX_IndexParamVideoInit);
        CASE_RET_ENUM_WORD(OMX_IndexParamOtherInit);
        CASE_RET_ENUM_WORD(OMX_IndexParamNumAvailableStreams);
        CASE_RET_ENUM_WORD(OMX_IndexParamActiveStream);
        CASE_RET_ENUM_WORD(OMX_IndexParamSuspensionPolicy);
        CASE_RET_ENUM_WORD(OMX_IndexParamComponentSuspended);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCapturing);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCaptureMode);
        CASE_RET_ENUM_WORD(OMX_IndexAutoPauseAfterCapture);
        CASE_RET_ENUM_WORD(OMX_IndexParamContentURI);
        CASE_RET_ENUM_WORD(OMX_IndexParamCustomContentPipe);
        CASE_RET_ENUM_WORD(OMX_IndexParamDisableResourceConcealment);
        CASE_RET_ENUM_WORD(OMX_IndexConfigMetadataItemCount);
        CASE_RET_ENUM_WORD(OMX_IndexConfigContainerNodeCount);
        CASE_RET_ENUM_WORD(OMX_IndexConfigMetadataItem);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCounterNodeID);
        CASE_RET_ENUM_WORD(OMX_IndexParamMetadataFilterType);
        CASE_RET_ENUM_WORD(OMX_IndexParamMetadataKeyFilter);
        CASE_RET_ENUM_WORD(OMX_IndexConfigPriorityMgmt);
        CASE_RET_ENUM_WORD(OMX_IndexParamStandardComponentRole);
        CASE_RET_ENUM_WORD(OMX_IndexPortStartUnused);
        CASE_RET_ENUM_WORD(OMX_IndexParamPortDefinition);
        CASE_RET_ENUM_WORD(OMX_IndexParamCompBufferSupplier);
        CASE_RET_ENUM_WORD(OMX_IndexReservedStartUnused);
        CASE_RET_ENUM_WORD(OMX_IndexAudioStartUnused);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioPortFormat);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioPcm);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioAac);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioRa);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioMp3);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioAdpcm);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioG723);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioG729);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioAmr);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioWma);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioSbc);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioMidi);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioGsm_FR);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioMidiLoadUserSound);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioG726);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioGsm_EFR);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioGsm_HR);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioPdc_FR);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioPdc_EFR);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioPdc_HR);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioTdma_FR);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioTdma_EFR);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioQcelp8);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioQcelp13);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioEvrc);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioSmv);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioVorbis);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioFlac);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioApe);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioRaw);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioWmaProfile);
        CASE_RET_ENUM_WORD(OMX_IndexParamAudioAlac);
        CASE_RET_ENUM_WORD(OMX_IndexConfigAudioMidiImmediateEvent);
        CASE_RET_ENUM_WORD(OMX_IndexConfigAudioMidiControl);
        CASE_RET_ENUM_WORD(OMX_IndexConfigAudioMidiSoundBankProgram);
        CASE_RET_ENUM_WORD(OMX_IndexConfigAudioMidiStatus);
        CASE_RET_ENUM_WORD(OMX_IndexConfigAudioMidiMetaEvent);
        CASE_RET_ENUM_WORD(OMX_IndexConfigAudioMidiMetaEventData);
        CASE_RET_ENUM_WORD(OMX_IndexConfigAudioVolume);
        CASE_RET_ENUM_WORD(OMX_IndexConfigAudioBalance);
        CASE_RET_ENUM_WORD(OMX_IndexConfigAudioChannelMute);
        CASE_RET_ENUM_WORD(OMX_IndexConfigAudioMute);
        CASE_RET_ENUM_WORD(OMX_IndexConfigAudioLoudness);
        CASE_RET_ENUM_WORD(OMX_IndexConfigAudioEchoCancelation);
        CASE_RET_ENUM_WORD(OMX_IndexConfigAudioNoiseReduction);
        CASE_RET_ENUM_WORD(OMX_IndexConfigAudioBass);
        CASE_RET_ENUM_WORD(OMX_IndexConfigAudioTreble);
        CASE_RET_ENUM_WORD(OMX_IndexConfigAudioStereoWidening);
        CASE_RET_ENUM_WORD(OMX_IndexConfigAudioChorus);
        CASE_RET_ENUM_WORD(OMX_IndexConfigAudioEqualizer);
        CASE_RET_ENUM_WORD(OMX_IndexConfigAudioReverberation);
        CASE_RET_ENUM_WORD(OMX_IndexConfigAudioChannelVolume);
        CASE_RET_ENUM_WORD(OMX_IndexImageStartUnused);
        CASE_RET_ENUM_WORD(OMX_IndexParamImagePortFormat);
        CASE_RET_ENUM_WORD(OMX_IndexParamFlashControl);
        CASE_RET_ENUM_WORD(OMX_IndexConfigFocusControl);
        CASE_RET_ENUM_WORD(OMX_IndexParamQFactor);
        CASE_RET_ENUM_WORD(OMX_IndexParamQuantizationTable);
        CASE_RET_ENUM_WORD(OMX_IndexParamHuffmanTable);
        CASE_RET_ENUM_WORD(OMX_IndexConfigFlashControl);
        CASE_RET_ENUM_WORD(OMX_IndexVideoStartUnused);
        CASE_RET_ENUM_WORD(OMX_IndexParamVideoPortFormat);
        CASE_RET_ENUM_WORD(OMX_IndexParamVideoQuantization);
        CASE_RET_ENUM_WORD(OMX_IndexParamVideoFastUpdate);
        CASE_RET_ENUM_WORD(OMX_IndexParamVideoBitrate);
        CASE_RET_ENUM_WORD(OMX_IndexParamVideoMotionVector);
        CASE_RET_ENUM_WORD(OMX_IndexParamVideoIntraRefresh);
        CASE_RET_ENUM_WORD(OMX_IndexParamVideoErrorCorrection);
        CASE_RET_ENUM_WORD(OMX_IndexParamVideoVBSMC);
        CASE_RET_ENUM_WORD(OMX_IndexParamVideoMpeg2);
        CASE_RET_ENUM_WORD(OMX_IndexParamVideoMpeg4);
        CASE_RET_ENUM_WORD(OMX_IndexParamVideoWmv);
        CASE_RET_ENUM_WORD(OMX_IndexParamVideoRv);
        CASE_RET_ENUM_WORD(OMX_IndexParamVideoAvc);
        CASE_RET_ENUM_WORD(OMX_IndexParamVideoH263);
        CASE_RET_ENUM_WORD(OMX_IndexParamVideoProfileLevelQuerySupported);
        CASE_RET_ENUM_WORD(OMX_IndexParamVideoProfileLevelCurrent);
        CASE_RET_ENUM_WORD(OMX_IndexConfigVideoBitrate);
        CASE_RET_ENUM_WORD(OMX_IndexConfigVideoFramerate);
        CASE_RET_ENUM_WORD(OMX_IndexConfigVideoIntraVOPRefresh);
        CASE_RET_ENUM_WORD(OMX_IndexConfigVideoIntraMBRefresh);
        CASE_RET_ENUM_WORD(OMX_IndexConfigVideoMBErrorReporting);
        CASE_RET_ENUM_WORD(OMX_IndexParamVideoMacroblocksPerFrame);
        CASE_RET_ENUM_WORD(OMX_IndexConfigVideoMacroBlockErrorMap);
        CASE_RET_ENUM_WORD(OMX_IndexParamVideoSliceFMO);
        CASE_RET_ENUM_WORD(OMX_IndexConfigVideoAVCIntraPeriod);
        CASE_RET_ENUM_WORD(OMX_IndexConfigVideoNalSize);
        CASE_RET_ENUM_WORD(OMX_IndexCommonStartUnused);
        CASE_RET_ENUM_WORD(OMX_IndexParamCommonDeblocking);
        CASE_RET_ENUM_WORD(OMX_IndexParamCommonSensorMode);
        CASE_RET_ENUM_WORD(OMX_IndexParamCommonInterleave);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonColorFormatConversion);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonScale);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonImageFilter);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonColorEnhancement);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonColorKey);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonColorBlend);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonFrameStabilisation);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonRotate);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonMirror);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonOutputPosition);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonInputCrop);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonOutputCrop);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonDigitalZoom);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonOpticalZoom);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonWhiteBalance);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonExposure);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonContrast);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonBrightness);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonBacklight);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonGamma);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonSaturation);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonLightness);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonExclusionRect);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonDithering);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonPlaneBlend);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonExposureValue);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonOutputSize);
        CASE_RET_ENUM_WORD(OMX_IndexParamCommonExtraQuantData);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonFocusRegion);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonFocusStatus);
        CASE_RET_ENUM_WORD(OMX_IndexConfigCommonTransitionEffect);
        CASE_RET_ENUM_WORD(OMX_IndexOtherStartUnused);
        CASE_RET_ENUM_WORD(OMX_IndexParamOtherPortFormat);
        CASE_RET_ENUM_WORD(OMX_IndexConfigOtherPower);
        CASE_RET_ENUM_WORD(OMX_IndexConfigOtherStats);
        CASE_RET_ENUM_WORD(OMX_IndexTimeStartUnused);
        CASE_RET_ENUM_WORD(OMX_IndexConfigTimeScale);
        CASE_RET_ENUM_WORD(OMX_IndexConfigTimeClockState);
        CASE_RET_ENUM_WORD(OMX_IndexConfigTimeActiveRefClock);
        CASE_RET_ENUM_WORD(OMX_IndexConfigTimeCurrentMediaTime);
        CASE_RET_ENUM_WORD(OMX_IndexConfigTimeCurrentWallTime);
        CASE_RET_ENUM_WORD(OMX_IndexConfigTimeCurrentAudioReference);
        CASE_RET_ENUM_WORD(OMX_IndexConfigTimeCurrentVideoReference);
        CASE_RET_ENUM_WORD(OMX_IndexConfigTimeMediaTimeRequest);
        CASE_RET_ENUM_WORD(OMX_IndexConfigTimeClientStartTime);
        CASE_RET_ENUM_WORD(OMX_IndexConfigTimePosition);
        CASE_RET_ENUM_WORD(OMX_IndexConfigTimeSeekMode);
        CASE_RET_ENUM_WORD(OMX_IndexKhronosExtensions);
        CASE_RET_ENUM_WORD(OMX_IndexVendorStartUnused);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecTimeSource);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecPriorityAdjustment);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecVideoSpecQuerySupported);
        CASE_RET_ENUM_WORD(OMX_GoogleAndroidIndexEnableAndroidNativeBuffers);
        CASE_RET_ENUM_WORD(OMX_GoogleAndroidIndexUseAndroidNativeBuffer);
        CASE_RET_ENUM_WORD(OMX_GoogleAndroidIndexGetAndroidNativeBufferUsage);
        CASE_RET_ENUM_WORD(OMX_GoogleAndroidIndexStoreMetaDataInBuffers);
        CASE_RET_ENUM_WORD(OMX_GoogleAndroidIndexstoreANWBufferInMetadata);
        CASE_RET_ENUM_WORD(OMX_GoogleAndroidIndexPrepareForAdaptivePlayback);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxPartialFrameQuerySupported);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecThumbnailMode);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecSeekMode);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecSwitchBwTVout);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecUseClearMotion);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecGetMinUndequeuedBufs);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecUsePostProcessingFw);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecEnablePostProcessingModule);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVencSetForceIframe);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVencSetTimelapseMode);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVencSetWhiteboardEffectMode);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVencSetMCIMode);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecConcealmentLevel);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecStreamingMode);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkMP3Decode);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdec3DVideoPlayback);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVenc3DVideoRecode);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecGetAspectRatio);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecGetCropInfo);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVideoUseIonBuffer);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVencSetIInterval);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVencSkipFrame);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecFixedMaxBuffer);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVencDrawBlack);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecNoReorderMode);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVideoSetClientLocally);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecSkipReferenceCheckMode);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVencSetScenario);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVencPrependSPSPPS);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVencSwitchWFDSecureOut);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecComponentColorConvert);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecSetScenario);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecTaskGroup);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecANW_HWComposer);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVencNonRefPOp);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecLowLatencyDecode);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecACodecColorConvertMode);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecACodecEncodeRGB2YUVMode);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxAIVConfigSurface);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxAIVEncrypMeta);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxAIVAC3Param);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxAIVExtraDataFlag);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxAIVEXTIndexObject);
        CASE_RET_ENUM_WORD(OMX_IndexVendMtkOmxUpdateColorFormat);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecGetColorFormat);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecUse16xSlowMotion);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecSlowMotionSpeed);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecSlowMotionSection);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecAVSyncTime);
        CASE_RET_ENUM_WORD(OMX_GoogleAndroidIndexDescribeColorFormat);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxHandle);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkQueryDriverVersion);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkConfigQP);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVdecQueryCodecsSizes);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVencQueryCodecsSizes);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxSliceLossIndication);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVencSetForceFullIframe);
        CASE_RET_ENUM_WORD(OMX_GoogleAndroidIndexDescribeColorFormat2);
        CASE_RET_ENUM_WORD(OMX_GoogleAndroidIndexEnableAndroidNativeHandle);
        CASE_RET_ENUM_WORD(OMX_GoogleAndroidIndexDescribeColorAspects);
        CASE_RET_ENUM_WORD(OMX_GoogleAndroidIndexDescribeHDRStaticInfo);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVencSeResolutionChange);
        CASE_RET_ENUM_WORD(OMX_IndexVendorMtkOmxVencInputScaling);

        default: return "UNSUPPORTED";
    }
}

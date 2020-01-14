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
#include "MtkOmx2V4L2.h"

#ifdef V4L2

OMX_BOOL MtkOmxVenc::InitVideoEncodeHW()
{
    IN_FUNC();
    VAL_BOOL_T  bRet = VAL_FALSE;
    VENC_DRV_MRESULT_T  rRet = VENC_DRV_MRESULT_OK;

    mV4L2fd = device_open("/dev/video1");
    if(mV4L2fd == -1)
    {
        MTK_OMX_LOGE("[ERROR] Device open fail");
        return OMX_FALSE;
    }

    /* creates an "eventfd object" that can be used as an event
       wait/notify mechanism by user-space applications, and by the kernel
       to notify user-space applications of events */
    mdevice_poll_interrupt_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (mdevice_poll_interrupt_fd == -1) {
        MTK_OMX_LOGE("Initialize(): open device_poll_interrupt_fd_ fail:\n");
        return OMX_FALSE;
    }

    // encoding settings.
    // log keyword: " Encoding: Format "
    bRet = EncSettingEnc(); // MtkOmxVencDrv:EncSettingEnc -> MtkOmx2V4L2:EncSettingH264Enc
    if (VAL_FALSE == bRet)
    {
        MTK_OMX_LOGE("[ERROR] EncSettingEnc fail");
        return OMX_FALSE;
    }

    // query driver capability
    bRet = QueryDriverEnc();
    if (VAL_FALSE == bRet)
    {
        MTK_OMX_LOGE("[ERROR] QueryDriverEnc fail");
        return OMX_FALSE;
    }

    ioctl_set_param(&mEncParam); // Stream Parameters, type=V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, denominator=framerate, numerator=1
    ioctl_set_fmt(&mEncParam); // output format=V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, input format=V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE
    ioctl_set_fix_scale(&mEncParam); // don't allow kernel change output bs frame size
    //ioctl_set_crop(&mEncParam); // set crop width, height

    // init buffer
    ioctl_req_bufs(); // request buffers (+ tell buffer count, type, buffer memory)
    ioctl_query_in_dmabuf(); // query if OUTPUT_MPLANE frame buffer allocated
    ioctl_query_out_dmabuf(); // query if INPUT_MPLANE bs buffer allocated

    OUT_FUNC();
    return OMX_TRUE;
 }


OMX_BOOL MtkOmxVenc::DeInitVideoEncodeHW()
{
    IN_FUNC();
    if (mEncoderInitCompleteFlag == OMX_TRUE)
    {
        if (mdevice_poll_interrupt_fd  != 0) {
            close(mdevice_poll_interrupt_fd);
        }
        device_close(mV4L2fd);
        mEncoderInitCompleteFlag = OMX_FALSE;
    }
    OUT_FUNC();
    return OMX_TRUE;
}


void MtkOmxVenc::EncodeFunc(OMX_BUFFERHEADERTYPE *pInputBuf, OMX_BUFFERHEADERTYPE *pOutputBuf)
{
#if PROFILING
    m_start_tick = 0;
    m_end_tick = 0;

    m_trans_start_tick = 0;
    m_trans_end_tick = 0;

    m_start_tick = getTickCountMs();
#endif
    IN_FUNC();
    ATRACE_CALL();

    if (pInputBuf->nOffset > pInputBuf->nFilledLen) //check the incorrect access
    {
        MTK_OMX_LOGE("[ERROR] incorrect buffer access");
        return;
    }

    VAL_BOOL_T  bRet = VAL_FALSE;
    VENC_DRV_MRESULT_T  rRet = VENC_DRV_MRESULT_OK;
    VENC_DRV_DONE_RESULT_T  rEncResult;
    rEncResult.eMessage = VENC_DRV_MESSAGE_OK;

    OMX_U8 *aInputBuf   = pInputBuf->pBuffer + pInputBuf->nOffset;
    OMX_U8 *aOutputBuf = pOutputBuf->pBuffer + pOutputBuf->nOffset;
    OMX_U32 aInputSize  = pInputBuf->nFilledLen;
    OMX_U32 aOutputSize = pOutputBuf->nAllocLen;

    if (OMX_FALSE == mEncoderInitCompleteFlag)
    {
        // put input buffer back bufferQ
        QueueBufferAdvance(mpVencInputBufQ, pInputBuf);

#if 0 // TODO: DummyBuffer
        if (mDummyIdx >= 0)
        {
            QueueOutputBuffer(mDummyIdx);//the VencOutBufQ MUST be REAL OutBufQ!!!
            mDummyIdx = -1;
        }
#endif
        bRet = InitVideoEncodeHW(); // open device + allocate buffer
        if (OMX_FALSE == bRet)
        {
            MTK_OMX_LOGE("[ERROR] cannot init encode driver");
            // report bitstream corrupt error
            mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                   mAppData,
                                   OMX_EventError,
                                   OMX_ErrorBadParameter,
                                   (OMX_U32)NULL,
                                   NULL);
            pOutputBuf->nFilledLen = 0;
            pOutputBuf->nTimeStamp = 0;
            HandleFillBufferDone(pOutputBuf);
            OUT_FUNC();
            return;
        }

        setDrvParamBeforeHdr(); // set scenario, ioctl_set_ctrl

        // assign timestamp
        pOutputBuf->nTimeStamp = pInputBuf->nTimeStamp;

        // get output buffer mva (getOmxMVAFromVAToVencBS)
        GetVEncDrvBSBuffer(aOutputBuf, aOutputSize); // aOutputBuf -> mBitStreamBuf

        // get intput buffer mva (getOmxMVAFromVAToVencFrm)
        GetVEncDrvFrmBuffer(aInputBuf, aInputSize); // aInputBuf -> mFrameBuf
        MTK_OMX_LOGD("EncodeFunc: input fd:%d, outputfd: %d", mFrameBuf.u4IonShareFd, mBitStreamBuf.u4IonShareFd);

        // encode sequence header.
        ioctl_q_in_buf(mFrameBuf.index, mFrameBuf.u4IonShareFd); // ioctl Q FrameBuf
        ioctl_q_out_buf(mBitStreamBuf.index, mBitStreamBuf.u4IonShareFd); // ioctl Q BS Buf
        ioctl_stream_on(); // stream on

        int ret = -1;
        int dq_buf_idx;
        int bs_size;
        int flags = 0;

        // wait for 1 frame, first output is seq header
        if(IoctlPoll() == 0) {
            ret = ioctl_dq_out_buf(&dq_buf_idx, &bs_size, &flags); // ioctl DQ BS Buf
        }
        if (0 != ret)
        {
            MTK_OMX_LOGE("[ERROR] cannot encode Sequence Header");
            // report bitstream corrupt error
            mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                   mAppData,
                                   OMX_EventError,
                                   OMX_ErrorStreamCorrupt,
                                   (OMX_U32)NULL,
                                   NULL);
            pOutputBuf->nFilledLen = 0;
            pOutputBuf->nTimeStamp = 0;
            HandleFillBufferDone(pOutputBuf);
            OUT_FUNC();
            return;
        }
        if (dq_buf_idx != mBitStreamBuf.index)
        {
            MTK_OMX_LOGD("dq buffer index isn't expected: %d != %d", dq_buf_idx, mBitStreamBuf.index);
        }

        MTK_OMX_LOGD("Sequence header size = %lu", bs_size);

        mHeaderLen = bs_size;
        // Don't show any config info. to upstream
        // Some Tier-1 customers will ask about this.
        if (OMX_FALSE == mStoreMetaDataInOutBuffers)
        {
            pOutputBuf->nFlags |= OMX_BUFFERFLAG_CODECCONFIG;
        }
        pOutputBuf->nFilledLen = mHeaderLen;

        //if (OMX_TRUE == mIsMultiSlice)
        //{
        //    pOutputBuf->nFlags |= OMX_BUFFERFLAG_MULTISLICE;
        //}
        //MTK_OMX_LOGD("mIsMultiSlice = %d", mIsMultiSlice);

        dumpOutputBuffer(pOutputBuf, aOutputBuf, pOutputBuf->nFilledLen);

        HandleFillBufferDone(pOutputBuf); // FBD

        mEncoderInitCompleteFlag = OMX_TRUE;
        OUT_FUNC();
        return;
    }

    if ((pInputBuf->nFlags & OMX_BUFFERFLAG_EOS) && pInputBuf->nFilledLen == 0)    // EOS frame
    {
        MTK_OMX_LOGD("Enc EOS received, TS=%lld, nFilledLen %lu", pInputBuf->nTimeStamp, pOutputBuf->nFilledLen);
        pOutputBuf->nFlags |= OMX_BUFFERFLAG_EOS;
        pOutputBuf->nTimeStamp = pInputBuf->nTimeStamp;
        pOutputBuf->nFilledLen = 0;

        EncHandleEmptyBufferDone(pInputBuf);
        HandleFillBufferDone(pOutputBuf);

#if 0 // TODO: DummyBuffer
        if (mDummyIdx >= 0)
        {
            QueueOutputBuffer(mDummyIdx);//the VencOutBufQ MUST be REAL OutBufQ!!!
            mDummyIdx = -1;
        }
#endif
    }
    else    // encode normal frame
    {
        if (OMX_TRUE == NeedConversion() && false == mDoConvertPipeline && mPartNum == 0)
        {
            //do color convert
#ifdef PROFILING
            m_trans_start_tick = getTickCountMs();
#endif
            if (colorConvert(aInputBuf, aInputSize, mCnvtBuffer, mCnvtBufferSize) <= 0)
            {
                MTK_OMX_LOGE("Color Convert fail!!");
            }

#ifdef PROFILING
            m_trans_end_tick = getTickCountMs();
#endif
        }
#ifdef PROFILING
        else
        {
            m_trans_start_tick = 0;
            m_trans_end_tick = 0;
        }
#endif//PROFILING
        getLatencyToken(pInputBuf, aInputBuf);//for WFD Latency profiling

        pOutputBuf->nTimeStamp = pInputBuf->nTimeStamp;

        preEncProcess(); // mDrawStripe, mDrawBlack

        // skip frame mode, runtime bitrate/frame rate/I-interval adjust, force I 1st frame
        // dependent var: mForceIFrame, mBitRateUpdated, mFrameRateUpdated, mSetIInterval,
        // dependent var: mSkipFrame, mPrependSPSPPSToIDRFrames, mSetQP, mGotSLI
        setDrvParamBeforeEnc();

        if (mFrameCount > 0) // not first frame (first frame was stored in mFrameBuf & queued kernel during Init, less 1 ioctl)
        {
            GetVEncDrvFrmBuffer(aInputBuf, aInputSize);
            ioctl_q_in_buf(mFrameBuf.index, mFrameBuf.u4IonShareFd);
        }

        GetVEncDrvBSBuffer(aOutputBuf, aOutputSize);
        ioctl_q_out_buf(mBitStreamBuf.index, mBitStreamBuf.u4IonShareFd);

        dumpInputBuffer(pInputBuf, (OMX_U8 *)mFrameBuf.rFrmBufAddr.u4VA,
                        mEncParam.buf_w * mEncParam.buf_h * 3 / 2);
        //                mEncDrvSetting.u4BufWidth * mEncDrvSetting.u4BufHeight * 3 / 2);

        int dq_in_buf_idx, dq_out_buf_idx;
        int bs_size;
        int flags = 0;
        int in_ret = -1, out_ret = -1, pollret = -1;

        do
        {
            pollret = IoctlPoll();
            if(pollret == -1) break;

            if (out_ret != 0)
            {
                out_ret = ioctl_dq_out_buf(&dq_out_buf_idx, &bs_size, &flags);
            }
            if (in_ret != 0)
            {
                in_ret = ioctl_dq_in_buf(&dq_in_buf_idx);
            }
            if (!out_ret && !in_ret) break;
        } while(1);
        //} while(out_ret || in_ret);

        if (0 != pollret)
        {
            MTK_OMX_LOGE("## ENCODE ERROR !!!");
            // report bitstream corrupt error
            mCallback.EventHandler((OMX_HANDLETYPE)&mCompHandle,
                                   mAppData,
                                   OMX_EventError,
                                   OMX_ErrorStreamCorrupt,
                                   (OMX_U32)NULL,
                                   NULL);
            goto ERR_JUMPOUT;
        }

        MTK_OMX_LOGD("EncodeFunc Done, in_index=%d, out_index=%d", dq_in_buf_idx, dq_out_buf_idx);
        setDrvParamAfterEnc(); // set Force-I, skip mode to 0

#if PROFILING
        m_end_tick = getTickCountMs();
        {
            MTK_OMX_LOGD("%s EncTime=%lld ms, RGB_2_YUV=%lld, FrameCount=%d, buf timestamp=%lld (%lld) IsKey(%d), Size(%lu) : "
                     "in VA=0x%08X, offset=0x%08x, t=%lld, len=%d, flags=0x%08x : out VA=0x%08X, offset=0x%08x",
                     codecName(), m_end_tick - m_start_tick, m_trans_end_tick - m_trans_start_tick, mFrameCount,
                     pInputBuf->nTimeStamp / 1000, pInputBuf->nTimeStamp, (flags & V4L2_BUF_FLAG_KEYFRAME),
                     bs_size, (unsigned int)pInputBuf->pBuffer, pInputBuf->nOffset,
                     pInputBuf->nTimeStamp, (int)pInputBuf->nFilledLen, (unsigned int)pInputBuf->nFlags,
                     pOutputBuf->pBuffer, pOutputBuf->nOffset);
        }
#endif

#if 0 // TODO:
        if (rEncResult.eMessage == VENC_DRV_MESSAGE_PARTIAL)
        {
            //MTK_OMX_LOGD("Get partial frame: %d", mPartNum);//do nothing
            ++mPartNum;
        }
        else if (rEncResult.eMessage == VENC_DRV_MESSAGE_TIMEOUT)
        {
            EncHandleEmptyBufferDone(pInputBuf);

            pOutputBuf->nFilledLen = 0;
            pOutputBuf->nOffset = 0;
            pOutputBuf->nFlags = 0;
            QueueBufferAdvance(mpVencOutputBufQ, pOutputBuf);
            if (mDummyIdx >= 0)
            {
                QueueOutputBuffer(mDummyIdx);//the VencOutBufQ MUST be REAL OutBufQ!!!
                mDummyIdx = -1;
            }
            MTK_OMX_LOGD("Enc Time Out");
            //aee_system_warning("264enc",NULL,DB_OPT_DEFAULT,"timeout");
            goto ERR_JUMPOUT;
        }
        else
#endif
        {
            //default
            pOutputBuf->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;
            if (pInputBuf->nFlags & OMX_BUFFERFLAG_EOS)
            {
                P_VENC_DRV_PARAM_BS_BUF_T prBSBuf = (P_VENC_DRV_PARAM_BS_BUF_T)rEncResult.prBSBuf;
                pOutputBuf->nFlags |= OMX_BUFFERFLAG_EOS;
                MTK_OMX_LOGD("Enc EOS received, TS=%lld", pInputBuf->nTimeStamp);
            }
            mPartNum = 0;
 #if 0 // TODO:
            if (mWFDMode == OMX_TRUE && mEnableDummy == OMX_TRUE)
            {
                mSendDummyNAL = true;
            }
 #endif
            pOutputBuf->nTickCount = pInputBuf->nTickCount;
        }

        if (flags & V4L2_BUF_FLAG_KEYFRAME)
        {
            pOutputBuf->nFlags |= OMX_BUFFERFLAG_SYNCFRAME;

#if 0 // TODO: put force full I frame at runtime_config
            if (OMX_TRUE == mForceFullIFrame)
            {
                mForceFullIFrame = OMX_FALSE;
                VAL_UINT32_T enable = 0;
                if (VENC_DRV_MRESULT_FAIL == eVEncDrvSetParam(mDrvHandle, VENC_DRV_SET_TYPE_PREPEND_HEADER, &enable, VAL_NULL))
                {
                    MTK_OMX_LOGE("[ERROR] set prepend header fail");
                }
            }
#endif
        }

        mLastFrameTimeStamp = pOutputBuf->nTimeStamp;
        pOutputBuf->nFilledLen = bs_size;

        dumpOutputBuffer(pOutputBuf, aOutputBuf, pOutputBuf->nFilledLen);

        mFrameCount++;

        if (rEncResult.eMessage == VENC_DRV_MESSAGE_PARTIAL)
        {
            //QueueInputBuffer(findBufferHeaderIndex(MTK_OMX_INPUT_PORT, pInputBuf));
            QueueBufferAdvance(mpVencInputBufQ, pInputBuf); // queue back input buffer
        }
        else
        {
            EncHandleEmptyBufferDone(pInputBuf);
        }
        HandleFillBufferDone(pOutputBuf);

#if 0 // TODO: DummyBuffer
        if (mSendDummyNAL == true && mDummyIdx >= 0)
        {
            OMX_BUFFERHEADERTYPE    *pDummyOutputBufHdr = mOutputBufferHdrs[mDummyIdx];
            OMX_U8                  *pDummyOutputBuf = pDummyOutputBufHdr->pBuffer + pDummyOutputBufHdr->nOffset;

            // encode sequence header.
            GetVEncDrvBSBuffer(pDummyOutputBuf, pDummyOutputBufHdr->nAllocLen);

            //TODO : h264 only give pps
            if (VENC_DRV_MRESULT_FAIL == eVEncDrvEncode(mDrvHandle,
                                                        VENC_DRV_START_OPT_ENCODE_SEQUENCE_HEADER_H264_PPS,
                                                        VAL_NULL, &mBitStreamBuf, &rEncResult))
            {
                MTK_OMX_LOGE("[ERROR] cannot encode Sequence Header");
                HandleFillBufferDone(pDummyOutputBufHdr);
                mSendDummyNAL = false;
                return;
            }
            pDummyOutputBufHdr->nTimeStamp = mLastFrameTimeStamp;
            pDummyOutputBufHdr->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;
            pDummyOutputBufHdr->nFlags |= OMX_BUFFERFLAG_DUMMY_NALU;

            pDummyOutputBufHdr->nFilledLen = rEncResult.prBSBuf->u4BSSize;
            pDummyOutputBufHdr->nTickCount = pInputBuf->nTickCount;

            HandleFillBufferDone(pDummyOutputBufHdr);
            mDummyIdx = -1;

            mSendDummyNAL = false;
            return;
        }
#endif
        if (mWaitPart == 1 && mPartNum == 0)
        {
            SIGNAL_COND(mPartCond);
        }
    }
    OUT_FUNC();
    return;
ERR_JUMPOUT:
#if 0 // TODO: DummyBuffer
    if (mDummyIdx >= 0)
    {
        QueueOutputBuffer(mDummyIdx);//the VencOutBufQ MUST be REAL OutBufQ!!!
        mDummyIdx = -1;
    }
#endif
    EncHandleEmptyBufferDone(pInputBuf);
    HandleFillBufferDone(pOutputBuf);
    if (mWaitPart == 1 && mPartNum == 0)
    {
        SIGNAL_COND(mPartCond);
    }
    OUT_FUNC();
    return;
}

int MtkOmxVenc::CheckFormatToV4L2()
{
    IN_FUNC();
    int ret = V4L2_PIX_FMT_YVU420M;
    int line = __LINE__;

    if (mStoreMetaDataInBuffers)//if meta mode
    {
        switch (mInputMetaDataFormat)
        {
            case HAL_PIXEL_FORMAT_RGBA_8888:
            case HAL_PIXEL_FORMAT_RGBX_8888:
            case HAL_PIXEL_FORMAT_BGRA_8888:
            case HAL_PIXEL_FORMAT_IMG1_BGRX_8888:
                //drv format is the format after color converting
                line = __LINE__;
                ret = V4L2_PIX_FMT_YVU420M;
                break;
            case HAL_PIXEL_FORMAT_YV12:
                //only support YV12 (16/16/16) right now
                line = __LINE__;
                ret = V4L2_PIX_FMT_YVU420M;
                break;
            default:
                //MTK_OMX_LOGD("unsupported format:0x%x %s", mInputMetaDataFormat,
                //             PixelFormatToString(mInputMetaDataFormat));
                line = __LINE__;
                ret = V4L2_PIX_FMT_YVU420M;
                break;
        }
    }
    else
    {
        switch (mInputPortFormat.eColorFormat)
        {
            case OMX_COLOR_FormatYUV420Planar:
            case OMX_COLOR_FormatYUV420Flexible:
            case OMX_COLOR_FormatYUV420PackedPlanar:
                line = __LINE__;
                ret = V4L2_PIX_FMT_YUV420M;
                break;

            case OMX_MTK_COLOR_FormatYV12:
                line = __LINE__;
                ret = V4L2_PIX_FMT_YVU420M;
                break;

            case OMX_COLOR_FormatAndroidOpaque:
                //should not be here, metaMode MUST on when format is AndroidQpaque...
                line = __LINE__;
                ret = V4L2_PIX_FMT_YUV420M;
                break;

            // Gary Wu add for MediaCodec encode with input data format is RGB
            case OMX_COLOR_Format16bitRGB565:
            case OMX_COLOR_Format24bitRGB888:
            case OMX_COLOR_Format32bitARGB8888:
            case OMX_COLOR_Format32bitBGRA8888:
                line = __LINE__;
                ret = V4L2_PIX_FMT_YUV420M;
                break;

            case OMX_COLOR_FormatYUV420SemiPlanar:
            case OMX_COLOR_FormatYUV420PackedSemiPlanar:
                line = __LINE__;
                ret = V4L2_PIX_FMT_NV12M;
                break;

            default:
                MTK_OMX_LOGE("[ERROR][EncSettingCodec] ColorFormat = 0x%X, not supported ?\n",
                             mInputPortFormat.eColorFormat);
                line = __LINE__;
                ret = V4L2_PIX_FMT_YVU420M;
                break;
        }
    }
    MTK_OMX_LOGD("[EncSettingCodec] Input Format = 0x%x, ColorFormat = 0x%x @ Line %d\n", mInputPortFormat.eColorFormat, ret, line);
    OUT_FUNC();
    return ret;
}

VAL_BOOL_T MtkOmxVenc::EncSettingEnc()
{
    EncSettingDrvResolution();

    EncSettingEncCommon();

    switch (mCodecId)
    {
        case MTK_VENC_CODEC_ID_AVC:
        case MTK_VENC_CODEC_ID_AVC_VGA:
            return EncSettingH264Enc();
        case MTK_VENC_CODEC_ID_MPEG4:
        case MTK_VENC_CODEC_ID_MPEG4_SHORT:
        case MTK_VENC_CODEC_ID_MPEG4_1080P:
        case MTK_VENC_CODEC_ID_H263_VT:
            return EncSettingMPEG4Enc();
        case MTK_VENC_CODEC_ID_HEVC:
            return EncSettingHEVCEnc();
        case MTK_VENC_CODEC_ID_VP8:
            return EncSettingVP8Enc();
        default:
            MTK_OMX_LOGE("unsupported codec %d", mCodecId);
            return VAL_FALSE;
    }
}

VAL_BOOL_T MtkOmxVenc::EncSettingEncCommon()
{
    mEncParam.format = CheckFormatToV4L2(); //out_fourcc
    mEncParam.hdr = 1;
    mEncParam.bitrate = mOutputPortDef.format.video.nBitrate;
    mEncParam.framerate = mInputPortDef.format.video.xFramerate >> 16;

    mEncParam.width = mInputPortDef.format.video.nFrameWidth;
    mEncParam.height = mInputPortDef.format.video.nFrameHeight;

    mEncParam.buf_w = mEncDrvSetting.u4BufWidth;
    mEncParam.buf_h = mEncDrvSetting.u4BufHeight;

    return VAL_TRUE;
}

VAL_BOOL_T MtkOmxVenc::EncSettingVP8Enc()
{
    return VAL_TRUE;
}

bool MtkOmxVenc::setDrvParamBeforeHdr(void)
{
    IN_FUNC();
    int nRet = 0;
    VAL_BOOL_T bRet = VAL_FALSE;

    if (mSetWFDMode == OMX_TRUE)
    {
        mSetWFDMode = OMX_FALSE;
        nRet = ioctl_runtime_config(V4L2_CID_MPEG_MTK_ENCODE_SCENARIO, 1);
        if (-1 == nRet)
        {
            MTK_OMX_LOGE("[ERROR] set WFD mode fail");
        }
        mWFDMode = OMX_TRUE;
    }

    if (mSetStreamingMode == OMX_TRUE)
    {
        mSetStreamingMode = OMX_FALSE;
        nRet = ioctl_runtime_config(V4L2_CID_MPEG_MTK_ENCODE_SCENARIO, 1);
        if (-1 == nRet)
        {
            MTK_OMX_LOGE("[ERROR] set WFD mode fail");
        }
    }

    if (mIsLivePhoto == OMX_TRUE)
    {
        int nRet = ioctl_runtime_config(V4L2_CID_MPEG_MTK_ENCODE_SCENARIO, 4);
        if (-1 == nRet)
        {
            MTK_OMX_LOGE("[ERROR] set venc scenario fail");
        }
    }

    if (mEnableNonRefP == OMX_TRUE)
    {
        int enable = 1;
        nRet = ioctl_runtime_config(V4L2_CID_MPEG_MTK_ENCODE_NONREFP, enable);
        if (-1 == nRet)
        {
            MTK_OMX_LOGE("[ERROR] enable non ref p fail");
        }
    }

    // if VILTE
    if (mIsViLTE)
    {
        nRet = ioctl_runtime_config(V4L2_CID_MPEG_MTK_ENCODE_SCENARIO, 5);
        if (-1 == nRet)
        {
            MTK_OMX_LOGE("[ERROR] set venc vilte scenario fail");
        }

        mWFDMode = OMX_FALSE;
        mEnableDummy = OMX_FALSE;
    }

    // ioctl set PROFILE, LEVEL, HEADER MODE, BITRATE, GOP
    ioctl_set_ctrl(&mEncParam);

    OUT_FUNC();
    return true;
}


bool MtkOmxVenc::setDrvParamBeforeEnc(void)
{
    IN_FUNC();
    int nRet;

    // For Force Intra (Begin)
    if (OMX_TRUE == mForceIFrame || OMX_TRUE == mForceFullIFrame)
    {
        mForceIFrame = OMX_FALSE;
        nRet = ioctl_runtime_config(V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME, 1);
        if (-1 == nRet)
        {
            MTK_OMX_LOGE("[ERROR] cannot set forceI");
        }
    }

    // Dynamic bitrate adjustment
    if (OMX_TRUE == mBitRateUpdated)
    {
        VENC_DRV_PARAM_ENC_EXTRA_T rEncoderExtraConfig = VENC_DRV_PARAM_ENC_EXTRA_T();
        mBitRateUpdated = OMX_FALSE;
        nRet = ioctl_runtime_config(V4L2_CID_MPEG_VIDEO_BITRATE, mConfigBitrate.nEncodeBitrate);
        if (-1 == nRet)
        {
            MTK_OMX_LOGE("[ERROR] cannot set param bitrate");
        }
    }

    // Dynamic framerate adjustment
    if (OMX_TRUE == mFrameRateUpdated)
    {
        int frameRate = 30;
        mFrameRateUpdated = OMX_FALSE;
        frameRate = mFrameRateType.xEncodeFramerate >> 16; //only support int

        nRet = ioctl_runtime_config(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, frameRate);
        if (-1 == nRet)
        {
            MTK_OMX_LOGE("[ERROR] cannot set param framerate");
        }
    }

    if (OMX_TRUE == mSetIInterval)
    {
        mSetIInterval = OMX_FALSE;
        int gopSize = mIInterval * (mFrameRateType.xEncodeFramerate >> 16);
        nRet = ioctl_runtime_config(V4L2_CID_MPEG_VIDEO_GOP_SIZE, gopSize);
        if (-1 == nRet)
        {
            MTK_OMX_LOGE("[ERROR] cannot set param I interval");
        }
    }

    if (OMX_TRUE == mSetIDRInterval)
    {
        mSetIDRInterval = OMX_FALSE;
        nRet = ioctl_runtime_config(V4L2_CID_MPEG_VIDEO_H264_I_PERIOD, mIDRInterval);
        if(-1 == nRet)
        {
            MTK_OMX_LOGE("[ERROR] cannot set param IDR interval");
        }
    }

    if (mSkipFrame)
    {
        mSkipFrame = 0;
        nRet = ioctl_runtime_config(V4L2_CID_MPEG_MFC51_VIDEO_FRAME_SKIP_MODE, 1);
        if (-1 == nRet)
        {
            MTK_OMX_LOGE("[ERROR] cannot skip frame");
        }
    }

    if (OMX_TRUE == mPrependSPSPPSToIDRFramesNotify)
    {
        mPrependSPSPPSToIDRFramesNotify = OMX_FALSE;
        VAL_UINT32_T enable = mPrependSPSPPSToIDRFrames;
    }

// TODO:
#if 0
    if (OMX_TRUE == mSetQP)
    {
        mSetQP = OMX_FALSE;
        if (VENC_DRV_MRESULT_FAIL == eVEncDrvSetParam(mDrvHandle, VENC_DRV_SET_TYPE_CONFIG_QP, (VAL_VOID_T *)&mQP, VAL_NULL))
        {
            MTK_OMX_LOGE("[ERROR] set qp %u fail", mQP);
        }
    }
#endif

    if (OMX_TRUE == mGotSLI)
    {
        mGotSLI = OMX_FALSE;
        OMX_U32 RFS_ErrFrm = mSLI.SliceLoss[0] & 0x3F;
    }

    OUT_FUNC();
    return true;
}
bool MtkOmxVenc::setDrvParamAfterEnc(void)
{
    IN_FUNC();

    int nRet = ioctl_runtime_config(V4L2_CID_MPEG_MFC51_VIDEO_FRAME_SKIP_MODE, 0);
    if (-1 == nRet)
    {
        MTK_OMX_LOGE("[ERROR] cannot skip frame");
    }

    OUT_FUNC();
    return true;
}

// V4L2
int MtkOmxVenc::device_open(const char* kDevice)
{
    IN_FUNC();
    //const char kDevice[] = "/dev/video1";
    int device_fd;

    device_fd  = open(kDevice, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (device_fd == -1) {
        return -1;
    }

    OUT_FUNC();
    return device_fd;
}

void MtkOmxVenc::device_close(int fd)
{
    IN_FUNC();

    if (fd != -1) {
        close(fd);
    }
    OUT_FUNC();
}

int MtkOmxVenc::ioctl_set_param(video_encode_param *param)
{
    struct v4l2_streamparm streamparm;
    int fd = mV4L2fd;

    IN_FUNC();

    memset(&streamparm, 0, sizeof(streamparm));

    streamparm.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    streamparm.parm.output.timeperframe.denominator = param->framerate;
    streamparm.parm.output.timeperframe.numerator = 1;
    IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_S_PARM, &streamparm, -1);

    OUT_FUNC();
    return 0;
}

int MtkOmxVenc::ioctl_set_fmt(video_encode_param *param)
{
    /* output format has to be setup before streaming starts. */
    struct v4l2_format format;
    int fd = mV4L2fd;

    IN_FUNC();
    /* output format */
    video_encode_reset_out_fmt_pix_mp(&format, &mEncParam);

    if (mOutputPortDef.nBufferSize) {
        format.fmt.pix_mp.plane_fmt[0].sizeimage = mOutputPortDef.nBufferSize;
    }
    v4l2_dump_fmt(&format);
    IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_TRY_FMT, &format, -1);
    v4l2_dump_fmt(&format);
    IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_S_FMT, &format, -1);
    v4l2_dump_fmt(&format);

    mOutputPortDef.nBufferSize = format.fmt.pix_mp.plane_fmt[0].sizeimage;

    IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_G_FMT, &format, -1);
    v4l2_dump_fmt(&format);

    /* input format */

    video_encode_reset_in_fmt_pix_mp(&format, &mEncParam);
    v4l2_dump_fmt(&format);

    IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_TRY_FMT, &format, -1);
    //v4l2_dump_fmt(&format);

    format.fmt.pix_mp.width = mEncParam.width;
    format.fmt.pix_mp.height = mEncParam.height;
    v4l2_dump_fmt(&format);

    IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_S_FMT, &format, -1);
    v4l2_dump_fmt(&format);

    //mInputPortDef.nBufferSize = format.fmt.pix_mp.plane_fmt[0].sizeimage; // maximun image size

    IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_G_FMT, &format, -1);
    v4l2_dump_fmt(&format);

    OUT_FUNC();
    return 0;
}

int MtkOmxVenc::ioctl_set_fix_scale(video_encode_param *param)
{
    IN_FUNC();

    int fd = mV4L2fd;

    struct v4l2_selection sel;

    v4l2_reset_dma_in_fix_scale(&sel, mEncParam.width, mEncParam.height);

    v4l2_dump_select(&sel);

    IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_S_SELECTION, &sel, -1);

    OUT_FUNC();
    return 0;
}

int MtkOmxVenc::ioctl_set_crop(video_encode_param *param)
{
    IN_FUNC();
    struct v4l2_crop crop;
    int fd = mV4L2fd;

    memset(&crop, 0, sizeof(crop));

    crop.c.width = param->width;
    crop.c.height = param->height;

    v412_dump_crop(&crop);

    IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_S_CROP, &crop, -1);

    OUT_FUNC();
    return 0;
}

int MtkOmxVenc::profile_id_ioctl_set_ctrl() {
    int ret = V4L2_CID_MPEG_VIDEO_H264_PROFILE;
    switch(mEncParam.codec) {
        case V4L2_PIX_FMT_H264:
            ret = V4L2_CID_MPEG_VIDEO_H264_PROFILE;
            break;
        case V4L2_PIX_FMT_H265:
            ret = V4L2_CID_MPEG_VIDEO_H265_PROFILE;
            break;
        case V4L2_PIX_FMT_DIVX3:
        case V4L2_PIX_FMT_S263:
        case V4L2_PIX_FMT_MPEG4:
            ret = V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE;
            break;
        default: ;
    }

    return ret;
}

int MtkOmxVenc::level_id_ioctl_set_ctrl() {
    int ret = V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE;
    switch(mEncParam.codec) {
        case V4L2_PIX_FMT_H264:
            ret = V4L2_CID_MPEG_VIDEO_H264_LEVEL;
            break;
        case V4L2_PIX_FMT_H265:
            ret = V4L2_CID_MPEG_VIDEO_H265_TIER_LEVEL;
            break;
        case V4L2_PIX_FMT_DIVX3:
        case V4L2_PIX_FMT_S263:
        default:
            ret = V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL;
            break;
    }
    return ret;
}

int MtkOmxVenc::ioctl_set_ctrl(video_encode_param *param) // param = this->mEncParam
{
    struct v4l2_control v4l2_ctrl;
    int fd = mV4L2fd;

    IN_FUNC();

    memset(&v4l2_ctrl, 0, sizeof(v4l2_ctrl));

    // PROFILE
    v4l2_ctrl.id = profile_id_ioctl_set_ctrl();
    v4l2_ctrl.value = param->profile;
    IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_S_CTRL, &v4l2_ctrl, -1);

    // LEVEL
    v4l2_ctrl.id = level_id_ioctl_set_ctrl();
    v4l2_ctrl.value = param->level;
    IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_S_CTRL, &v4l2_ctrl, -1);

    // HEADER MODE
    v4l2_ctrl.id = V4L2_CID_MPEG_VIDEO_HEADER_MODE;
    switch (param->prepend_hdr) {
    case 0:
        v4l2_ctrl.value = V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE;
        break;
    case 1:
        v4l2_ctrl.value = V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_1ST_FRAME;
        break;
    }
    IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_S_CTRL, &v4l2_ctrl, -1);

    // BITRATE
    v4l2_ctrl.id = V4L2_CID_MPEG_VIDEO_BITRATE;
    v4l2_ctrl.value = param->bitrate;
    IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_S_CTRL, &v4l2_ctrl, -1);

    // GOP
    v4l2_ctrl.id = V4L2_CID_MPEG_VIDEO_GOP_SIZE;
    v4l2_ctrl.value = param->gop;
    IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_S_CTRL, &v4l2_ctrl, -1);

    OUT_FUNC();
    return 0;
}

int MtkOmxVenc::ioctl_req_bufs()
{
    struct v4l2_requestbuffers reqbufs;
    int fd = mV4L2fd;

    IN_FUNC();

    // input buffer
    memset(&reqbufs, 0, sizeof(reqbufs));
    reqbufs.count  = mInputPortDef.nBufferCountActual;
    reqbufs.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    reqbufs.memory = V4L2_MEMORY_DMABUF;
    IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_REQBUFS, &reqbufs, -1);

    // output buffer
    memset(&reqbufs, 0, sizeof(reqbufs));
    reqbufs.count  = mOutputPortDef.nBufferCountActual;
    reqbufs.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    reqbufs.memory = V4L2_MEMORY_DMABUF;
    IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_REQBUFS, &reqbufs, -1);

    OUT_FUNC();
    return 0;
}

int MtkOmxVenc::ioctl_query_in_dmabuf()
{
    int i, j;
    int fd = mV4L2fd;
    int dim = v4l2_get_nplane_by_pixelformat(mEncParam.format);
    IN_FUNC();
    MTK_OMX_LOGD(" -- Query Input Buffer --\n");
    for (i = 0; i < mInputPortDef.nBufferCountActual; i++) {
        // Query for the MEMORY_MMAP pointer
        struct v4l2_plane planes[VIDEO_MAX_PLANES];
        struct v4l2_buffer buffer;

        v4l2_reset_dma_in_buf(&buffer, planes, i, dim);

        v4l2_dump_qbuf(&buffer);

        IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_QUERYBUF, &buffer, -1);

        v4l2_dump_qbuf(&buffer);
    }

    OUT_FUNC();
    return 0;
}

int MtkOmxVenc::ioctl_query_out_dmabuf()
{
    IN_FUNC();
    int i,j;
    int fd = mV4L2fd;
    unsigned int address;

    MTK_OMX_LOGD(" -- Query Output Buffer --\n");
    for (i = 0; i < mOutputPortDef.nBufferCountActual; i++) {
        // Query for the MEMORY_MMAP pointer.
        struct v4l2_plane planes[VIDEO_MAX_PLANES];
        struct v4l2_buffer buffer;

        v4l2_reset_dma_out_buf(&buffer, planes, i, 1);

        v4l2_dump_qbuf(&buffer);

        IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_QUERYBUF, &buffer, -1);

        v4l2_dump_qbuf(&buffer);
    }

    OUT_FUNC();
    return 0;
}

int MtkOmxVenc::ioctl_q_in_buf(int idx, int dmabuf_fd)
{
    struct v4l2_plane planes[VIDEO_MAX_PLANES];
    struct v4l2_buffer qbuf;
    int fd = mV4L2fd;
    int dim = v4l2_get_nplane_by_pixelformat(mEncParam.format);
    idx = idx % mInputPortDef.nBufferCountActual;

    MTK_OMX_LOGD("+ %s() idx %d\n", __func__, idx);

    v4l2_reset_dma_in_buf(&qbuf, planes, idx, dim);

    v4l2_dump_qbuf(&qbuf);

    IOCTL_OR_ERROR_LOG(fd, VIDIOC_QUERYBUF, &qbuf);

    v4l2_dump_qbuf(&qbuf);

    v4l2_set_dma_in_memory(&qbuf, &mEncParam, dmabuf_fd);

    v4l2_dump_qbuf(&qbuf);

    IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_QBUF, &qbuf, -1);

    OUT_FUNC();
    return 0;
}

int MtkOmxVenc::ioctl_q_out_buf(int idx, int dmabuf_fd)
{
    struct v4l2_plane planes[VIDEO_MAX_PLANES];
    struct v4l2_buffer qbuf;
    int fd = mV4L2fd;

    idx = idx % mOutputPortDef.nBufferCountActual;

    MTK_OMX_LOGD("+ %s() idx %d\n", __func__, idx);

    // this contiguous chunk is too small, but no error
    v4l2_reset_dma_out_buf(&qbuf, planes, idx, 1);

    IOCTL_OR_ERROR_LOG(fd, VIDIOC_QUERYBUF, &qbuf);

    v4l2_set_dma_out_memory(&qbuf, &mEncParam, dmabuf_fd);

    v4l2_dump_qbuf(&qbuf);

    IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_QBUF, &qbuf, -1);

    OUT_FUNC();
    return 0;
}

int MtkOmxVenc::ioctl_dq_in_buf(int *pdq_in_buf_idx)
{
    struct v4l2_plane planes[VIDEO_MAX_PLANES];
    struct v4l2_buffer dqbuf;
    int fd = mV4L2fd;
    int dim = v4l2_get_nplane_by_pixelformat(mEncParam.format);

    IN_FUNC();

    // input buffer container
    v4l2_reset_dma_in_buf(&dqbuf, planes, 0, dim);

    IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_DQBUF, &dqbuf, -1);

    v4l2_dump_qbuf(&dqbuf);

    *pdq_in_buf_idx = dqbuf.index;

    OUT_FUNC();
    return 0;
}

int MtkOmxVenc::ioctl_dq_out_buf(int *pdq_out_buf_idx, int *bs_size, int *pflags)
{
    struct v4l2_plane planes[VIDEO_MAX_PLANES];
    struct v4l2_buffer dqbuf;
    int fd = mV4L2fd;

    IN_FUNC();

    // output buffer container
    v4l2_reset_dma_out_buf(&dqbuf, planes, 0, 1); // fd = 0, don't care

    IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_DQBUF, &dqbuf, -1);
    *pdq_out_buf_idx = dqbuf.index;
    *bs_size = dqbuf.m.planes[0].bytesused;
    *pflags = dqbuf.flags;

    v4l2_dump_qbuf(&dqbuf);

    OUT_FUNC();
    return 0;
}

int MtkOmxVenc::IoctlPoll() {
    IN_FUNC();

    struct pollfd pollfds[2];
    nfds_t nfds;
    int pollfd = -1;

    pollfds[0].fd = mdevice_poll_interrupt_fd;
    pollfds[0].events = POLLIN | POLLERR;
    nfds = 1;

    if (1) {
        //V4L2_INFO(1, "Poll(): adding device fd to poll() set\n");
        pollfds[nfds].fd = mV4L2fd;
        pollfds[nfds].events = POLLIN | POLLOUT | POLLERR | POLLPRI;
        pollfd = nfds;
        nfds++;
    }

    if (poll(pollfds, nfds, 5000) == -1) {
        MTK_OMX_LOGE("poll() failed\n");
        OUT_FUNC();
        return -1;
    }
    OUT_FUNC();
    return 0;
}

int MtkOmxVenc::ioctl_stream_on() {
    __u32 type;
    int fd = mV4L2fd;

    IN_FUNC();

    type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_STREAMON, &type, -1);

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_STREAMON, &type, -1);

    OUT_FUNC();
    return 0;
}

/*
* Call this will do mem unmap to all frames/bs buffers by VPUD
*/
int MtkOmxVenc::ioctl_stream_off() {
    __u32 type;
    int fd = mV4L2fd;

    IN_FUNC();

    type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_STREAMOFF, &type, -1);

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_STREAMOFF, &type, -1);

    OUT_FUNC();
    return 0;
}

int MtkOmxVenc::ioctl_runtime_config(int change_to_mode, int change_to_value) {
    IN_FUNC();
    int fd = mV4L2fd;
    struct v4l2_ext_control ext_ctrl;
    struct v4l2_ext_controls ext_ctrls;
    struct v4l2_streamparm parms;

    MTK_OMX_LOGD("change %d to value %d\n", change_to_mode, change_to_value);

    memset(&ext_ctrl, 0, sizeof(ext_ctrl));
    memset(&ext_ctrls, 0, sizeof(ext_ctrls));
    ext_ctrls.ctrl_class = V4L2_CTRL_CLASS_MPEG;
    ext_ctrls.count = 1;
    ext_ctrls.controls = &ext_ctrl;
    memset(&parms, 0, sizeof(parms));
    switch (change_to_mode) {
    case V4L2_CID_MPEG_VIDEO_BITRATE:
        ext_ctrl.id = V4L2_CID_MPEG_VIDEO_BITRATE;
        ext_ctrl.value = change_to_value;
        IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_S_EXT_CTRLS, &ext_ctrls, -1);
        break;
    case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
        parms.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        parms.parm.output.timeperframe.numerator = 1;
        parms.parm.output.timeperframe.denominator = change_to_value;
        IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_S_PARM, &parms, -1);
        break;
    case V4L2_CID_MPEG_VIDEO_GOP_SIZE:
        ext_ctrl.id = V4L2_CID_MPEG_VIDEO_GOP_SIZE;
        ext_ctrl.value = change_to_value;
        IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_S_EXT_CTRLS, &ext_ctrls, -1);
        break;
    case V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME:
        ext_ctrl.id = V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME;
        ext_ctrl.value = change_to_value;
        if(change_to_value != 0)
            IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_S_EXT_CTRLS, &ext_ctrls, -1);
        break;
    case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_SKIP_MODE:
        ext_ctrl.id = V4L2_CID_MPEG_MFC51_VIDEO_FRAME_SKIP_MODE;
        ext_ctrl.value = change_to_value ?
                         V4L2_MPEG_MFC51_VIDEO_FRAME_SKIP_MODE_BUF_LIMIT :
                         V4L2_MPEG_MFC51_VIDEO_FRAME_SKIP_MODE_DISABLED;
        IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_S_EXT_CTRLS, &ext_ctrls, -1);
        break;
    case V4L2_CID_MPEG_MTK_ENCODE_SCENARIO:
        ext_ctrl.id = V4L2_CID_MPEG_MTK_ENCODE_SCENARIO;
        ext_ctrl.value = change_to_value;
        IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_S_EXT_CTRLS, &ext_ctrls, -1);
        break;
    case V4L2_CID_MPEG_MTK_ENCODE_NONREFP:
        ext_ctrl.id = V4L2_CID_MPEG_MTK_ENCODE_NONREFP;
        ext_ctrl.value = change_to_value;
        IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_S_EXT_CTRLS, &ext_ctrls, -1);
        break;
    case V4L2_CID_MPEG_VIDEO_H264_I_PERIOD:
        ext_ctrl.id = V4L2_CID_MPEG_VIDEO_H264_I_PERIOD;
        ext_ctrl.value = change_to_value;
        IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_S_EXT_CTRLS, &ext_ctrls, -1);
        break;
    default:
        MTK_OMX_LOGE("unknown mode %d to change to\n",
                 change_to_mode);
        OUT_FUNC();
        return -1;
    }

    OUT_FUNC();
    return 0;
}

int MtkOmxVenc::queryCapability(int format, int profile, int level, int width, int height)
{
    IN_FUNC();
    int fd = mV4L2fd;
    long minResolution, maxResolution, resolution;

    struct v4l2_frmsizeenum frmsizeenum;
    frmsizeenum.index = 0;
    frmsizeenum.pixel_format = format;

    IOCTL_OR_ERROR_RETURN_VALUE(fd, VIDIOC_ENUM_FRAMESIZES, &frmsizeenum, VAL_FALSE);
    v4l2_dump_frmsizeenum(&frmsizeenum);

    if(frmsizeenum.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
        if(width < frmsizeenum.stepwise.min_width || width > frmsizeenum.stepwise.max_width) {
            MTK_OMX_LOGE("width not supported %d (%d, %d)", width, frmsizeenum.stepwise.min_width, frmsizeenum.stepwise.max_width);
            OUT_FUNC();
            return VAL_FALSE;
        }
        minResolution = frmsizeenum.stepwise.min_width * frmsizeenum.stepwise.min_height;
        maxResolution = frmsizeenum.stepwise.max_width * frmsizeenum.stepwise.max_height;
        resolution = width * height;

        if(resolution < minResolution || resolution > maxResolution) {
            MTK_OMX_LOGW("resolution not supported %d (%d, %d)", resolution, minResolution, maxResolution);
            OUT_FUNC();
            return VAL_FALSE;
        }
    }

    if(profile > frmsizeenum.reserved[0]) {
        MTK_OMX_LOGE("profile not supported %d > %d", profile, frmsizeenum.reserved[0]);
        OUT_FUNC();
        return VAL_FALSE;
    }
    if(level > frmsizeenum.reserved[1]) {
        MTK_OMX_LOGE("level not supported %d > %d", level, frmsizeenum.reserved[1]);
        OUT_FUNC();
        return VAL_FALSE;
    }

    OUT_FUNC();
    return VAL_TRUE;
}

int v4l2_get_nplane_by_pixelformat(__u32 pixelformat)
{
    switch(pixelformat) {
        case V4L2_PIX_FMT_YUV420M:
        case V4L2_PIX_FMT_YVU420M:
            return 3;
        case V4L2_PIX_FMT_NV12M:
        case V4L2_PIX_FMT_NV21M:
            return 2;
        case V4L2_PIX_FMT_YUV420:
        case V4L2_PIX_FMT_YVU420:
        case V4L2_PIX_FMT_NV12:
        case V4L2_PIX_FMT_NV21:
        default:
            return 1;
    }
}

void video_encode_reset_in_fmt_pix_mp(struct v4l2_format* format, video_encode_param* encParam)
{
    memset(format, 0, sizeof(struct v4l2_format));

    format->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

    struct v4l2_pix_format_mplane* pix_mp = &format->fmt.pix_mp;
    pix_mp->pixelformat = encParam->format;
    pix_mp->width = encParam->width;
    pix_mp->height = encParam->height;

    pix_mp->num_planes = v4l2_get_nplane_by_pixelformat(pix_mp->pixelformat);

    switch(pix_mp->pixelformat) {
        case V4L2_PIX_FMT_YUV420M:
        case V4L2_PIX_FMT_YVU420M:
            pix_mp->plane_fmt[0].bytesperline = encParam->buf_w;
            pix_mp->plane_fmt[1].bytesperline = encParam->buf_w/2;
            pix_mp->plane_fmt[2].bytesperline = encParam->buf_w/2;
            pix_mp->plane_fmt[0].sizeimage = encParam->buf_w * encParam->buf_h;
            pix_mp->plane_fmt[1].sizeimage = encParam->buf_w * encParam->buf_h / 4;
            pix_mp->plane_fmt[2].sizeimage = encParam->buf_w * encParam->buf_h / 4;

            break;
        case V4L2_PIX_FMT_NV12M:
        case V4L2_PIX_FMT_NV21M:
            pix_mp->plane_fmt[0].bytesperline = encParam->buf_w;
            pix_mp->plane_fmt[1].bytesperline = encParam->buf_w;
            pix_mp->plane_fmt[0].sizeimage = encParam->buf_w * encParam->buf_h;
            pix_mp->plane_fmt[1].sizeimage = encParam->buf_w * encParam->buf_h / 2;
            break;
        case V4L2_PIX_FMT_YUV420:
        case V4L2_PIX_FMT_YVU420:
        case V4L2_PIX_FMT_NV12:
        case V4L2_PIX_FMT_NV21:
        default:
            pix_mp->plane_fmt[0].bytesperline = encParam->buf_w;
            pix_mp->plane_fmt[0].sizeimage = encParam->buf_w * encParam->buf_h * 3 / 2;
    }
}

void video_encode_reset_out_fmt_pix_mp(struct v4l2_format *format, video_encode_param* encParam)
{
    memset(format, 0, sizeof(struct v4l2_format));

    format->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    struct v4l2_pix_format_mplane* pix_mp = &format->fmt.pix_mp;
    pix_mp->pixelformat = encParam->codec;
    pix_mp->width = encParam->width;
    pix_mp->height = encParam->height;

    pix_mp->num_planes = 1;

    pix_mp->plane_fmt[0].bytesperline = 0;
    pix_mp->plane_fmt[0].sizeimage = encParam->width * encParam->height * 3 / 2; // maximum size required
}

void v4l2_set_dma_in_memory(struct v4l2_buffer* qbuf, video_encode_param* encParam, int dmabuf_fd)
{
    int i;
    int lumaSize = encParam->width * encParam->height;

    for(i=0; i<qbuf->length; i++) {
        qbuf->m.planes[i].m.fd = dmabuf_fd;
        qbuf->m.planes[i].length = lumaSize + lumaSize/2;
        qbuf->m.planes[i].bytesused = lumaSize + lumaSize/2;
    }

    qbuf->m.planes[0].data_offset = 0;
    qbuf->m.planes[1].data_offset = lumaSize;
    qbuf->m.planes[2].data_offset = lumaSize + lumaSize/4;

    /*
    qbuf->length = 1;
    qbuf->m.planes[0].length = mplane_len;
    qbuf->m.planes[0].bytesused = mplane_bu;
    qbuf->m.planes[0].data_offset = 0;
    qbuf->m.planes[0].m.fd = dmabuf_fd;

    qbuf->m.planes[0].m.fd = dmabuf_fd;
    qbuf->m.planes[0].data_offset = 0;

    qbuf->m.planes[0].bytesused = qbuf.m.planes[0].length;*/

}

void v4l2_set_dma_out_memory(struct v4l2_buffer* qbuf, video_encode_param* encParam, int dmabuf_fd)
{
    qbuf->length = 1;
    qbuf->m.planes[0].m.fd = dmabuf_fd;
    qbuf->m.planes[0].data_offset = 0;
}

void v412_dump_crop(struct v4l2_crop *crop)
{
    ALOGD("crop.c.left: %d, crop.c.top: %d, crop.c.width: %d, crop.c.height: %d",
        crop->c.left, crop->c.top, crop->c.width, crop->c.height);
}

void v4l2_dump_qbuf(struct v4l2_buffer* qbuf)
{
    char bytesused_buf[64] = {0};
    char length_buf[64] = {0};
    char data_offset_buf[64] = {0};
    char fd_buf[64] = {0};

    int i;
    for(i=0; i<qbuf->length; i++) {
        sprintf(bytesused_buf, "%s,%d", bytesused_buf, qbuf->m.planes[i].bytesused);
        sprintf(length_buf, "%s,%d", length_buf, qbuf->m.planes[i].length);
        sprintf(data_offset_buf, "%s,%d", data_offset_buf, qbuf->m.planes[i].data_offset);
        sprintf(fd_buf, "%s,%d", fd_buf, qbuf->m.planes[i].m.fd);
    }

    ALOGD("index: %d, type: %d, memory: %d, length: %d, byteused: %d, flag: 0x%x, planes_bytesused:[%s], planes_length:[%s], planes_data_offset:[%s], planes_m_fd[%s]",
        qbuf->index, qbuf->type, qbuf->memory, qbuf->length, qbuf->bytesused, qbuf->flags,
        bytesused_buf+1, length_buf+1, data_offset_buf+1, fd_buf+1);
}

void v4l2_dump_fmt(struct v4l2_format *format)
{
    struct v4l2_pix_format_mplane *pix_mp = &format->fmt.pix_mp;

    char format_cc[5] = {0};
    memcpy(format_cc, &pix_mp->pixelformat, 4);

    char sizeimage_buf[64] = {0};
    char stride_buf[64]={0};

    int i;
    for(i=0; i<pix_mp->num_planes; i++) {
        sprintf(sizeimage_buf, "%s,%d", sizeimage_buf, pix_mp->plane_fmt[i].sizeimage);
        sprintf(stride_buf, "%s,%d", stride_buf, pix_mp->plane_fmt[i].bytesperline);
    }

    // ===== DEBUG =====
    ALOGD("format: %s, width: %d, height: %d, pix_mp->num_planes: %d, plane_fmd.stride:[%s], plane_fmd.sizeimage= [%s]",
        format_cc, pix_mp->width, pix_mp->height, pix_mp->num_planes, stride_buf+1,
        sizeimage_buf+1);
}

void v4l2_dump_select(struct v4l2_selection* sel)
{
    ALOGD("type: 0x%x, target: 0x%x, flags: 0x%x, rect(l,t,w,h): (%d,%d,%d,%d)",
        sel->type, sel->target, sel->flags, sel->r.left, sel->r.top, sel->r.width, sel->r.height);
}

void v4l2_dump_frmsizeenum(struct v4l2_frmsizeenum* fse)
{
    char format_cc[5] = {0};
    memcpy(format_cc, &fse->pixel_format, 4);

    char frmsize[512] = {0};

    switch(fse->type)
    {
        case V4L2_FRMSIZE_TYPE_STEPWISE:
            sprintf(frmsize, "w %d-%d step %d, h %d-%d step %d",
                fse->stepwise.min_width, fse->stepwise.max_width, fse->stepwise.step_width,
                fse->stepwise.min_height, fse->stepwise.max_height, fse->stepwise.step_height);
            break;
        case V4L2_FRMSIZE_TYPE_DISCRETE:
            sprintf(frmsize, "w %d, h %d",
                fse->discrete.width, fse->discrete.height);
            break;
        default: ;
    }

    ALOGD("index: %d, pixel_format: %s, type:%d, framesize:{%s}, profile: %d, level: %d",
        fse->index, format_cc, fse->type, frmsize, fse->reserved[0], fse->reserved[1]);
}

/*
* DMA 1-plane in buffer
*/
void v4l2_reset_dma_in_buf(struct v4l2_buffer* qbuf, struct v4l2_plane* qbuf_planes, int idx, int dim)
{
    memset(qbuf, 0, sizeof(struct v4l2_buffer));
    memset(qbuf_planes, 0, sizeof(struct v4l2_plane)*VIDEO_MAX_PLANES);

    qbuf->index = idx;
    qbuf->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    qbuf->memory = V4L2_MEMORY_DMABUF;
    qbuf->length = dim;
    qbuf->m.planes = qbuf_planes;
}

/*
* DMA 1-plane out buffer
*/
void v4l2_reset_dma_out_buf(struct v4l2_buffer* qbuf, struct v4l2_plane* qbuf_planes, int idx, int dim)
{
    v4l2_reset_dma_in_buf(qbuf, qbuf_planes, idx, dim);

    qbuf->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
}

void v4l2_reset_dma_in_fix_scale(struct v4l2_selection* sel, int w, int h)
{
    sel->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    sel->target = V4L2_SEL_TGT_COMPOSE;
    sel->flags = V4L2_SEL_FLAG_GE | V4L2_SEL_FLAG_LE; //fixed

    v4l2_reset_rect(&sel->r, w, h);
}

void v4l2_reset_dma_out_fix_scale(struct v4l2_selection* sel, int w, int h)
{
    v4l2_reset_dma_in_fix_scale(sel, w, h);
    sel->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
}

/*
* rect reset
*/
void v4l2_reset_rect(struct v4l2_rect* rect, int w, int h)
{
    rect->left=0;
    rect->top=0;
    rect->width=w;
    rect->height=h;
}

#endif
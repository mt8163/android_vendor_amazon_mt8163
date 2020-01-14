#include <signal.h>
#include <cutils/log.h>

#include <utils/Trace.h>
#include <utils/AndroidThreads.h>

#include "osal_utils.h"
#include "MtkOmxVenc.h"

#include <cutils/properties.h>
#include <sched.h>

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

#define IN_FUNC() \
    MTK_OMX_LOGD("+ %s():%d\n", __func__, __LINE__)

#define OUT_FUNC() \
    MTK_OMX_LOGD("- %s():%d\n", __func__, __LINE__)

#define PROP() \
    MTK_OMX_LOGD(" --> %s : %d\n", __func__, __LINE__)

extern const char *PixelFormatToString(unsigned int nPixelFormat);

OMX_ERRORTYPE MtkOmxVenc::CheckInputBufferPortAvailbility()
{
    if (OMX_FALSE == mInputPortDef.bEnabled)
    {
        return OMX_ErrorIncorrectStateOperation;
    }

    if (OMX_TRUE == mInputPortDef.bPopulated)
    {
        MTK_OMX_LOGE("Error input port already populated, LINE:%d", __LINE__);
        return OMX_ErrorBadParameter;
    }

    return OMX_ErrorNone;
}
OMX_ERRORTYPE MtkOmxVenc::CheckOutputBufferPortAvailbility()
{
    if (OMX_FALSE == mOutputPortDef.bEnabled)
    {
        return OMX_ErrorIncorrectStateOperation;
    }

    if (OMX_TRUE == mOutputPortDef.bPopulated)
    {
        MTK_OMX_LOGE("Error output port already populated, LINE:%d", __LINE__);
        return OMX_ErrorBadParameter;
    }

    return OMX_ErrorNone;
}

void MtkOmxVenc::HandleInputPortPopulated()
{
    if (mInputBufferPopulatedCnt == mInputPortDef.nBufferCountActual)
    {
        mInputPortDef.bPopulated = OMX_TRUE;

        if (IS_PENDING(MTK_OMX_IDLE_PENDING))
        {
            SIGNAL(mInPortAllocDoneSem);
            MTK_OMX_LOGD_ENG("signal mInPortAllocDoneSem (%d)", get_sem_value(&mInPortAllocDoneSem));
        }

        if (IS_PENDING(MTK_OMX_IN_PORT_ENABLE_PENDING))
        {
            SIGNAL(mInPortAllocDoneSem);
            MTK_OMX_LOGD_ENG("signal mInPortAllocDoneSem (%d)", get_sem_value(&mInPortAllocDoneSem));
        }
    }

    MTK_OMX_LOGD_ENG("input port populated");
}

void MtkOmxVenc::HandleOutputPortPopulated()
{
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
}

OMX_ERRORTYPE MtkOmxVenc::InputBufferHeaderAllocate(
    OMX_BUFFERHEADERTYPE **ppBufferHdr, OMX_U32 nPortIndex, OMX_PTR pAppPrivate, OMX_U32 nSizeBytes)
{
    mInputAllocateBuffer = OMX_TRUE;

    *ppBufferHdr = mInputBufferHdrs[mInputBufferPopulatedCnt] =
                       (OMX_BUFFERHEADERTYPE *)MTK_OMX_ALLOC(sizeof(OMX_BUFFERHEADERTYPE));

    if (OMX_TRUE == mStoreMetaDataInBuffers)
    {
        // For meta mode, allocate input from dram,
        // And we don't need to map this VA for MVA.
        (*ppBufferHdr)->pBuffer = (OMX_U8 *)MTK_OMX_MEMALIGN(MEM_ALIGN_32, nSizeBytes);
    }
    else
    {
        // If not meta mode, we will allocate VA and map MVA
        mInputMVAMgr->newOmxMVAandVA(MEM_ALIGN_512, (int)nSizeBytes,
                                     (void *)*ppBufferHdr, (void **)(&(*ppBufferHdr)->pBuffer));
    }
    (*ppBufferHdr)->nAllocLen = nSizeBytes;
    (*ppBufferHdr)->pAppPrivate = pAppPrivate;
    (*ppBufferHdr)->pMarkData = NULL;
    (*ppBufferHdr)->nInputPortIndex  = MTK_OMX_INPUT_PORT;
    (*ppBufferHdr)->nOutputPortIndex = MTK_OMX_INVALID_PORT;
    //(*ppBufferHdr)->pInputPortPrivate = NULL; // TBD
    *((OMX_U32 *)(*ppBufferHdr)->pBuffer) = kMetadataBufferTypeInvalid;

    MTK_OMX_LOGD_ENG("MtkOmxVenc::AllocateBuffer port_idx(0x%X), idx[%d],"
        "nSizeBytes %d, pBuffHead(0x%08X), pBuffer(0x%08X)",
        (unsigned int)nPortIndex, (int)mInputBufferPopulatedCnt, nSizeBytes,
        (unsigned int)mInputBufferHdrs[mInputBufferPopulatedCnt], (unsigned int)((*ppBufferHdr)->pBuffer));

    mInputBufferPopulatedCnt++;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MtkOmxVenc::EpilogueInputBufferHeaderAllocate()
{
    HandleInputPortPopulated();

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MtkOmxVenc::OutputBufferHeaderAllocate(
    OMX_BUFFERHEADERTYPE **ppBufferHdr, OMX_U32 nPortIndex, OMX_PTR pAppPrivate, OMX_U32 nSizeBytes)
{
    mOutputAllocateBuffer = OMX_TRUE;

    *ppBufferHdr = mOutputBufferHdrs[mOutputBufferPopulatedCnt] =
        (OMX_BUFFERHEADERTYPE *)MTK_OMX_ALLOC(sizeof(OMX_BUFFERHEADERTYPE));

    OMX_ERRORTYPE err = OMX_ErrorNone;
    if (OMX_TRUE == mStoreMetaDataInOutBuffers)
    {
        uint32_t offset =9;
        if(mOutputPortDef.nBufferSize >= MTK_VENC_DEFAULT_OUTPUT_BUFFER_SIZE_HEVC_4K)
        {
            offset = 11;
        }
        else if((mOutputPortDef.nBufferSize >= MTK_VENC_DEFAULT_OUTPUT_BUFFER_SIZE_VP8_1080P) ||
        (mOutputPortDef.nBufferSize >= MTK_VENC_DEFAULT_OUTPUT_BUFFER_SIZE_AVC_1080P))
        {
            offset = 10;
        }
#ifdef SUPPORT_NATIVE_HANDLE
        if (OMX_FALSE == mIsAllocateOutputNativeBuffers)
        {
            (*ppBufferHdr)->pBuffer = (OMX_U8 *)MTK_OMX_MEMALIGN(MEM_ALIGN_32, nSizeBytes);
            memset((*ppBufferHdr)->pBuffer, 0x0, nSizeBytes);
        }
#endif
#ifdef SUPPORT_NATIVE_HANDLE
        size_t bistreamBufferSize = (1 << offset) * (1 << 9);

        int bsSize = MtkVenc::BsSize();
        if (0 != bsSize)
        {
            MTK_OMX_LOGD("mtk.omx.venc.bssize set buffer size to %d", bsSize);
            bistreamBufferSize = bsSize;
        }
        else
        {
            MTK_OMX_LOGD("no mtk.omx.venc.bssize, buffer size is %d", bistreamBufferSize);
        }
        if (OMX_TRUE == mIsAllocateOutputNativeBuffers)
        {
            native_handle_t* native_handle = NULL;
            ion_user_handle_t ion_handle;
            int ion_share_fd = -1;
            mIonDevFd = ((mIonDevFd < 0) ? mt_ion_open("MtkOmxVenc_Sec"): mIonDevFd);
            if (OMX_TRUE == mIsSecureSrc)
            {
                bistreamBufferSize = 1024*1024;
                MTK_OMX_LOGD("Set bistreamBufferSize to %d for secure buffer", bistreamBufferSize);
                OMX_U32 flags = ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC | ION_FLAG_MM_HEAP_INIT_ZERO;
                if (mIonDevFd < 0)
                {
                    MTK_OMX_LOGE("[ERROR] cannot open ION device. LINE:%d", __LINE__);
                    mIonDevFd = -1;
                    return OMX_ErrorUndefined;
                }
                if (0 != ion_alloc(mIonDevFd, bistreamBufferSize, 1024, ION_HEAP_MULTIMEDIA_SEC_MASK, flags, &ion_handle))
                {
                    MTK_OMX_LOGE("[ERROR] Failed to ion_alloc (%d) from mIonDevFd(%d)!\n", bistreamBufferSize, mIonDevFd);
                    return OMX_ErrorInsufficientResources;
                }
                int ret = ion_share( mIonDevFd, ion_handle, &ion_share_fd );
                if (0 != ret)
                {
                    MTK_OMX_LOGE("[ERROR] ion_share(ion fd = %d) failed(%d), LINE:%d",mIonDevFd, ret, __LINE__);
                    return OMX_ErrorUndefined;
                }
                struct ion_mm_data mm_data;
                mm_data.mm_cmd = ION_MM_CONFIG_BUFFER;
                mm_data.config_buffer_param.handle = ion_handle;
                mm_data.config_buffer_param.eModuleID = 1;
                mm_data.config_buffer_param.security = 1;
                mm_data.config_buffer_param.coherent = 1;
                if (ion_custom_ioctl(mIonDevFd, ION_CMD_MULTIMEDIA, &mm_data))
                {
                    MTK_OMX_LOGE("IOCTL[ION_IOC_CUSTOM] Config Buffer failed! LINE(%d)", __LINE__);
                }
                struct ion_mm_data set_debug_info_mm_data;
                set_debug_info_mm_data.mm_cmd = ION_MM_SET_DEBUG_INFO;
                set_debug_info_mm_data.buf_debug_info_param.handle = ion_handle;
                set_debug_info_mm_data.buf_debug_info_param.value1 = 1;//security
                set_debug_info_mm_data.buf_debug_info_param.value2 = bistreamBufferSize;//buffersize
                set_debug_info_mm_data.buf_debug_info_param.value3 = 0;
                set_debug_info_mm_data.buf_debug_info_param.value4 = 0;
                if (ion_custom_ioctl(mIonDevFd, ION_CMD_MULTIMEDIA, &set_debug_info_mm_data)) {
                    MTK_OMX_LOGE("IOCTL[ION_IOC_CUSTOM] Config Buffer failed! LINE(%d)", __LINE__);
                }
                struct ion_sys_data sys_data;
                sys_data.sys_cmd = ION_SYS_GET_PHYS;
                sys_data.get_phys_param.handle = ion_handle;
                if (ion_custom_ioctl(mIonDevFd, ION_CMD_SYSTEM, &sys_data))
                {
                    MTK_OMX_LOGE("IOCTL[ION_IOC_CUSTOM] Get Phys failed! line(%d)", __LINE__);
                }
                if (OMX_FALSE == SetDbgInfo2Ion(
                        ion_handle, ion_share_fd, 0, sys_data.get_phys_param.phy_addr,
                        bistreamBufferSize, ion_share_fd, ion_handle, 0,
                        ion_handle, ion_share_fd, 0, sys_data.get_phys_param.phy_addr,
                        bistreamBufferSize, ion_share_fd, ion_handle, 0))
                {
                    MTK_OMX_LOGE("IOCTL[ION_IOC_CUSTOM] set buffer debug info!\n");
                    return OMX_ErrorUndefined;
                }
                native_handle = (native_handle_t*)MTK_OMX_ALLOC(sizeof(native_handle_t) + sizeof(int) * 12);
                native_handle->version  = sizeof(native_handle_t);
                native_handle->numFds   = 1;
                native_handle->numInts  = 4;
#ifdef SECURE_BUF_HINT
                native_handle->data[0]  = (ion_share_fd | (1<<31));  // ION fd for output buffer
                MTK_OMX_LOGD("[Test] Set data[0] to %x for ion fd %x",native_handle->data[0], ion_share_fd );
#else
                native_handle->data[0]  = ion_share_fd;  // ION fd for output buffer
#endif
                native_handle->data[1]  = 0;             // rangeOffset
                native_handle->data[2]  = 0;             // rangeLength
                native_handle->data[3]  = 0;
                native_handle->data[4]  = 1;             // 1: secure buffer; 0: normal buffer
                native_handle->data[5]  = 0;             // secure PA [63:32]
                native_handle->data[6]  = 0;             // secure PA [31: 0]
                native_handle->data[7]  = sys_data.get_phys_param.phy_addr;
                native_handle->data[8]  = bistreamBufferSize;
                native_handle->data[9]  = (uint32_t)ion_handle;
                native_handle->data[10]  = sys_data.get_phys_param.len;
                (*ppBufferHdr)->pBuffer = (OMX_U8 *)native_handle;
                MTK_OMX_LOGD("[secure]native_handle %p, ion fd %d, buffer size %d",
                    native_handle, ion_share_fd, bistreamBufferSize);
                mStoreMetaOutNativeHandle.push(native_handle);
            }
            else
            {
                uint32_t flags = 0;
#ifdef ALLOC_CONTIG_PHY_ADDR
#ifdef ALLOC_FROM_MM_CONTIG
                int ret = ion_alloc(mIonDevFd, bistreamBufferSize, 0, ION_HEAP_MULTIMEDIA_CONTIG_MASK, flags, &ion_handle);
                MTK_OMX_LOGD("[DBG]Allocated from ION_HEAP_MULTIMEDIA_CONTIG");
#else
                int ret = ion_alloc(mIonDevFd, bistreamBufferSize, 0, ION_HEAP_DMA_RESERVED_MASK, flags, &ion_handle);
                MTK_OMX_LOGD_ENG("[DBG]Allocated from ION_HEAP_DMA_RESERVED");
#endif
                if (0 != ret)
                {
                    MTK_OMX_LOGE("[ERROR] ion_alloc failed (%d), size(%d), LINE:%d",
                        ret, bistreamBufferSize, __LINE__);
                    return OMX_ErrorUndefined;
                }
#else //ALLOC_CONTIG_PHY_ADDR
                int ret = ion_alloc_mm(mIonDevFd, bistreamBufferSize, 0, flags, &ion_handle);
                if (0 != ret)
                {
                    MTK_OMX_LOGE("[ERROR] ion_alloc_mm failed (%d), LINE:%d", ret, __LINE__);
                    return OMX_ErrorUndefined;
                }
#endif
                if (ion_share(mIonDevFd, ion_handle, &ion_share_fd))
                {
                    MTK_OMX_LOGE("[ERROR] ion_share failed, LINE:%d", __LINE__);
                    return OMX_ErrorUndefined;
                }
                void* va = ion_mmap(mIonDevFd, NULL, bistreamBufferSize,
                    PROT_READ | PROT_WRITE, MAP_SHARED, ion_share_fd, 0);
                if ((0 == va)|| (0xffffffff == (uint32_t)va))
                {
                    MTK_OMX_LOGE("[ERROR] ion_mmap failed, LINE:%d", __LINE__);
                    close(ion_share_fd);
                    return OMX_ErrorUndefined;
                }
                MTK_OMX_LOGD("VA %p", va);
#ifdef CHECK_OUTPUT_CONSISTENCY
                MTK_OMX_LOGD("Initialize VA %p len %d to 0", va, bistreamBufferSize);
                memset(va, 0, bistreamBufferSize);
#endif
#ifdef ALLOC_CONTIG_PHY_ADDR
#else
                {
                    struct ion_mm_data mm_data;
                    mm_data.mm_cmd = ION_MM_CONFIG_BUFFER;
                    mm_data.config_buffer_param.handle = ion_handle;
                    mm_data.config_buffer_param.eModuleID = eVideoGetM4UModuleID(VAL_MEM_CODEC_FOR_VENC);
                    mm_data.config_buffer_param.security = 0;
                    mm_data.config_buffer_param.coherent = 1;
                    if (ion_custom_ioctl(mIonDevFd, ION_CMD_MULTIMEDIA, &mm_data))
                    {
                        MTK_OMX_LOGE("[ERROR]IOCTL[ION_IOC_CUSTOM] Config Buffer failed!\n");
                    }
                    struct ion_mm_data set_debug_info_mm_data;
                    set_debug_info_mm_data.mm_cmd = ION_MM_SET_DEBUG_INFO;
                    set_debug_info_mm_data.buf_debug_info_param.handle = ion_handle;
                    set_debug_info_mm_data.buf_debug_info_param.value1 = 0;//1: secure buffer; 0: normal buffer
                    set_debug_info_mm_data.buf_debug_info_param.value2 = bistreamBufferSize;//buffersize
                    set_debug_info_mm_data.buf_debug_info_param.value3 = 0;
                    set_debug_info_mm_data.buf_debug_info_param.value4 = 0;
                    if (ion_custom_ioctl(mIonDevFd, ION_CMD_MULTIMEDIA, &set_debug_info_mm_data)) {
                        MTK_OMX_LOGE("IOCTL[ION_IOC_CUSTOM] Config Buffer failed! LINE(%d)", __LINE__);
                    }
                }
#endif
                unsigned int mva = 0;
                struct ion_sys_data sys_data;
#ifdef ALLOC_CONTIG_PHY_ADDR
                sys_data.sys_cmd = ION_SYS_GET_IOVA;
#else
                sys_data.sys_cmd = ION_SYS_GET_PHYS;
#endif
                sys_data.get_phys_param.handle = ion_handle;
                if (ion_custom_ioctl(mIonDevFd, ION_CMD_SYSTEM, &sys_data))
                {
                    MTK_OMX_LOGE("[ERROR] cannot get buffer physical address");
                    return OMX_ErrorUndefined;
                }
                mva = sys_data.get_phys_param.phy_addr;
                MTK_OMX_LOGD_ENG("MVA 0x%x", mva);
#ifdef COPY_2_CONTIG
                sys_data.sys_cmd = ION_SYS_GET_PHYS;
                sys_data.get_phys_param.handle = ion_handle;
                if (ion_custom_ioctl(mIonDevFd, ION_CMD_SYSTEM, &sys_data))
                {
                    MTK_OMX_LOGE("[ERROR] cannot get buffer physical address");
                    return OMX_ErrorUndefined;
                }
                MTK_OMX_LOGD_ENG("PA 0x%x", sys_data.get_phys_param.phy_addr);
#endif
#ifdef COPY_2_CONTIG
                ion_user_handle_t ion_handle_4_enc;
                ret = ion_alloc_mm(mIonDevFd, bistreamBufferSize, 0, flags, &ion_handle_4_enc);
                if (0 != ret)
                {
                    MTK_OMX_LOGE("[ERROR] ion_alloc_mm failed (%d), LINE:%d", ret, __LINE__);
                    return OMX_ErrorUndefined;
                }
                int ion_share_fd_4_enc = -1;
                if (ion_share(mIonDevFd, ion_handle_4_enc, &ion_share_fd_4_enc))
                {
                    MTK_OMX_LOGE("[ERROR] ion_share failed, LINE:%d", __LINE__);
                    return OMX_ErrorUndefined;
                }
                void* va_4_enc = ion_mmap(mIonDevFd, NULL, bistreamBufferSize,
                    PROT_READ | PROT_WRITE, MAP_SHARED, ion_share_fd_4_enc, 0);
                if ((0 == va_4_enc)|| (0xffffffff == (uint32_t)va_4_enc))
                {
                    MTK_OMX_LOGE("[ERROR] ion_mmap failed, LINE:%d", __LINE__);
                    close(ion_share_fd);
                    return OMX_ErrorUndefined;
                }
                MTK_OMX_LOGD("va_4_enc %p", va_4_enc);
                struct ion_mm_data mm_data;
                mm_data.mm_cmd = ION_MM_CONFIG_BUFFER;
                mm_data.config_buffer_param.handle = ion_handle_4_enc;
                mm_data.config_buffer_param.eModuleID = eVideoGetM4UModuleID(VAL_MEM_CODEC_FOR_VENC);
                mm_data.config_buffer_param.security = 0;
                mm_data.config_buffer_param.coherent = 1;
                if (ion_custom_ioctl(mIonDevFd, ION_CMD_MULTIMEDIA, &mm_data))
                {
                    MTK_OMX_LOGE("[ERROR]IOCTL[ION_IOC_CUSTOM] Config Buffer failed!\n");
                }
                unsigned int mva_4_enc = 0;
                sys_data.sys_cmd = ION_SYS_GET_PHYS;
                sys_data.get_phys_param.handle = ion_handle_4_enc;
                if (ion_custom_ioctl(mIonDevFd, ION_CMD_SYSTEM, &sys_data))
                {
                    MTK_OMX_LOGE("[ERROR] cannot get buffer physical address");
                    return OMX_ErrorUndefined;
                }
                mva_4_enc = sys_data.get_phys_param.phy_addr;
                MTK_OMX_LOGD("MVA_4_enc 0x%x", mva_4_enc);
#else
#ifdef CHECK_OUTPUT_CONSISTENCY
                ion_user_handle_t ion_handle_4_enc;
                ret = ion_alloc_mm(mIonDevFd, bistreamBufferSize, 0, flags, &ion_handle_4_enc);
                if (0 != ret)
                {
                    MTK_OMX_LOGE("[ERROR] ion_alloc_mm failed (%d), LINE:%d", ret, __LINE__);
                    return OMX_ErrorUndefined;
                }
                int ion_share_fd_4_enc = -1;
                if (ion_share(mIonDevFd, ion_handle_4_enc, &ion_share_fd_4_enc))
                {
                    MTK_OMX_LOGE("[ERROR] ion_share failed, LINE:%d", __LINE__);
                    return OMX_ErrorUndefined;
                }
                void* va_4_enc = ion_mmap(mIonDevFd, NULL, bistreamBufferSize,
                    PROT_READ | PROT_WRITE, MAP_SHARED, ion_share_fd_4_enc, 0);
                if ((0 == va)|| (0xffffffff == (uint32_t)va))
                {
                    MTK_OMX_LOGE("[ERROR] ion_mmap failed, LINE:%d", __LINE__);
                    close(ion_share_fd);
                    return OMX_ErrorInsufficientResources;
                }
                MTK_OMX_LOGD("va_4_enc %p", va_4_enc);
                memset(va_4_enc, 0, bistreamBufferSize);
                struct ion_mm_data mm_data;
                mm_data.mm_cmd = ION_MM_CONFIG_BUFFER;
                mm_data.config_buffer_param.handle = ion_handle_4_enc;
                mm_data.config_buffer_param.eModuleID = eVideoGetM4UModuleID(VAL_MEM_CODEC_FOR_VENC);
                mm_data.config_buffer_param.security = 0;
                mm_data.config_buffer_param.coherent = 1;
                if (ion_custom_ioctl(mIonDevFd, ION_CMD_MULTIMEDIA, &mm_data))
                {
                    MTK_OMX_LOGE("[ERROR]IOCTL[ION_IOC_CUSTOM] Config Buffer failed!\n");
                }
                unsigned int mva_4_enc = 0;
                sys_data.sys_cmd = ION_SYS_GET_PHYS;
                sys_data.get_phys_param.handle = ion_handle_4_enc;
                if (ion_custom_ioctl(mIonDevFd, ION_CMD_SYSTEM, &sys_data))
                {
                    MTK_OMX_LOGE("[ERROR] cannot get buffer physical address");
                    return OMX_ErrorUndefined;
                }
                mva_4_enc = sys_data.get_phys_param.phy_addr;
                MTK_OMX_LOGD("MVA_4_enc 0x%x", mva_4_enc);
#else
                ion_user_handle_t ion_handle_4_enc;
                int ion_share_fd_4_enc = -1;
                void* va_4_enc = NULL;
                unsigned int mva_4_enc = 0;
#endif
#endif
                if (OMX_FALSE == SetDbgInfo2Ion(
                        ion_handle, ion_share_fd, va, 0,
                        bistreamBufferSize, ion_share_fd, (uint32_t)va, mva,
                        ion_handle_4_enc, ion_share_fd_4_enc, va_4_enc, 0,
                        bistreamBufferSize, ion_share_fd_4_enc, (uint32_t)va_4_enc, mva_4_enc))
                {
                    MTK_OMX_LOGE("IOCTL[ION_IOC_CUSTOM] set buffer debug info!\n");
                    return OMX_ErrorUndefined;
                }
                native_handle = (native_handle_t*)MTK_OMX_ALLOC(sizeof(native_handle_t) + sizeof(int) * 12);
                MTK_OMX_LOGD_ENG("Allocated %d bytes for a native handle", sizeof(native_handle_t) + sizeof(int) * 9);
                native_handle->version  = sizeof(native_handle_t);
                native_handle->numFds   = 1;
                native_handle->numInts  = 4;
                native_handle->data[0]  = ion_share_fd;  // ION fd for output buffer
                native_handle->data[1]  = bistreamBufferSize;             // rangeOffset
                native_handle->data[2]  = 0;             // rangeLength
                native_handle->data[3]  = (uint32_t)va;
                native_handle->data[4]  = 0;             // 1: secure buffer; 0: normal buffer
                native_handle->data[5]  = 0;             // secure PA [63:32]
                native_handle->data[6]  = 0;             // secure PA [31: 0]
                native_handle->data[7]  = (uint32_t)va_4_enc;
                native_handle->data[8]  = bistreamBufferSize;
                native_handle->data[9]  = (uint32_t)ion_handle;
                native_handle->data[10]  = (uint32_t)ion_handle_4_enc;
                native_handle->data[11]  = ion_share_fd_4_enc;
                (*ppBufferHdr)->pBuffer = (OMX_U8 *)native_handle;
                MTK_OMX_LOGD("[normal]native_handle %p, va %p, ion fd %d, buffer size %d",
                    native_handle, va, ion_share_fd, bistreamBufferSize);
                mStoreMetaOutNativeHandle.push(native_handle);
            }
        }
        else //OMX_FALSE == mIsAllocateOutputNativeBuffers
#endif // SUPPORT_NATIVE_HANDLE
        {
            (*ppBufferHdr)->pBuffer = (OMX_U8 *)MTK_OMX_MEMALIGN(MEM_ALIGN_32, nSizeBytes);
            memset((*ppBufferHdr)->pBuffer, 0x0, nSizeBytes);
            //When output buffers are meta mode, the handles are allocated in venc
            if (mIsSecureSrc)
            {
                uint32_t flags = GRALLOC_USAGE_HW_VIDEO_ENCODER | GRALLOC_USAGE_SECURE  | GRALLOC_USAGE_SW_READ_OFTEN;
                //allocate secure buffer by GraphicBuffer,because only WFD use Secure buffer and wfd output is less than  1M
                mStoreMetaOutHandle.push(
                    new GraphicBuffer(
                        1 << offset,
                        1 << 9,
                        PIXEL_FORMAT_RGBA_8888,
                        flags)
                );
                if (NULL == mStoreMetaOutHandle.top()->handle)
                {
                    err = OMX_ErrorInsufficientResources;
                }
                //Don't map MVA when secure buffer, we will query for each frame.
            }
            else
            {
                //allocate normal buffer by GraphicBuffer,
                uint32_t flags = GRALLOC_USAGE_HW_VIDEO_ENCODER | GRALLOC_USAGE_SW_READ_OFTEN;
                mStoreMetaOutHandle.push(
                    new GraphicBuffer(
                        1 << offset,
                        1 << 9,
                        PIXEL_FORMAT_RGBA_8888,
                        flags)
                );

                //int ionfd, size;
                buffer_handle_t _handle = (buffer_handle_t)mStoreMetaOutHandle.top()->handle;
                mOutputMVAMgr->newOmxMVAwithHndl((void *)_handle, (void *)(*ppBufferHdr));
            }
            //assign handle to real buffer
            struct VideoGrallocMetadata *metadata = (struct VideoGrallocMetadata *)(*ppBufferHdr)->pBuffer;
            metadata->eType = kMetadataBufferTypeGrallocSource;
            metadata->pHandle = mStoreMetaOutHandle.top()->handle;
            //struct VideoNativeHandleMetadata *metadata = (struct VideoNativeHandleMetadata*)(*ppBufferHdr)->pBuffer;
            //metadata->eType = kMetadataBufferTypeNativeHandleSource;
            //metadata->pHandle = (native_handle_t*)mStoreMetaOutHandle.top()->handle;
            int *bitstreamLen = (int*)((*ppBufferHdr)->pBuffer + sizeof(struct VideoNativeHandleMetadata));
            *bitstreamLen = 0;

            MTK_OMX_LOGD_ENG("GB(0x%08x), GB->handle(0x%08x), meta type:%d, bitstreamLen:0x%x",
                         (unsigned int)mStoreMetaOutHandle.top().get(),
                         (unsigned int)mStoreMetaOutHandle.top()->handle,
                         metadata->eType, (unsigned int)bitstreamLen);
        }
    }
    else // OMX_TRUE != mStoreMetaDataInOutBuffers
    {
        // If not meta mode, we will allocate VA and map MVA.
        mOutputMVAMgr->newOmxMVAandVA(MEM_ALIGN_512, (int)nSizeBytes,
                                      (void *)*ppBufferHdr, (void **)(&(*ppBufferHdr)->pBuffer));
    }

    (*ppBufferHdr)->nAllocLen = nSizeBytes;
    (*ppBufferHdr)->pAppPrivate = pAppPrivate;
    (*ppBufferHdr)->pMarkData = NULL;
    (*ppBufferHdr)->nInputPortIndex  = MTK_OMX_INVALID_PORT;
    (*ppBufferHdr)->nOutputPortIndex = MTK_OMX_OUTPUT_PORT;
    //(*ppBufferHdr)->pOutputPortPrivate = NULL; // TBD

    MTK_OMX_LOGD_ENG("MtkOmxVenc::AllocateBuffer port_idx(0x%X), idx[%d], pBuffHead(0x%08X), pBuffer(0x%08X)",
                 (unsigned int)nPortIndex, (int)mOutputBufferPopulatedCnt,
                 (unsigned int)mOutputBufferHdrs[mOutputBufferPopulatedCnt],
                 (unsigned int)((*ppBufferHdr)->pBuffer));

    mOutputBufferPopulatedCnt++;

    return err;
}

OMX_ERRORTYPE MtkOmxVenc::EpilogueOutputBufferHeaderAllocate()
{
    if (mOutputBufferPopulatedCnt == mOutputPortDef.nBufferCountActual)
    {
        HandleOutputPortPopulated();

        MTK_OMX_LOGD_ENG("output port populated");
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MtkOmxVenc::InputBufferHeaderUse(
    OMX_BUFFERHEADERTYPE **ppBufferHdr, OMX_U32 nPortIndex, OMX_PTR pAppPrivate, OMX_U32 nSizeBytes, OMX_U8 *pBuffer)
{
    *ppBufferHdr = mInputBufferHdrs[mInputBufferPopulatedCnt] =
                       (OMX_BUFFERHEADERTYPE *)MTK_OMX_ALLOC(sizeof(OMX_BUFFERHEADERTYPE));
    (*ppBufferHdr)->pBuffer = pBuffer;
    (*ppBufferHdr)->nAllocLen = nSizeBytes;
    (*ppBufferHdr)->pAppPrivate = pAppPrivate;
    (*ppBufferHdr)->pMarkData = NULL;
    (*ppBufferHdr)->nInputPortIndex  = MTK_OMX_INPUT_PORT;
    (*ppBufferHdr)->nOutputPortIndex = MTK_OMX_INVALID_PORT;
    //(*ppBufferHdr)->pInputPortPrivate = NULL; // TBD

    if (OMX_FALSE == mStoreMetaDataInBuffers)
    {
        // If meta mode, we will map handle & ion MVA dynamically at GetVEncDrvFrmBuffer()
        // If not meta mode,
        // UseBuffer doesn't support mapping MVA by ION.
        // This should be M4U :
        if (strncmp("m4u", mInputMVAMgr->getType(), strlen("m4u")))
        {
            //if not m4u map
            delete mInputMVAMgr;
            mInputMVAMgr = new OmxMVAManager("m4u");
        }
        mInputMVAMgr->newOmxMVAwithVA((void *)pBuffer, (int)nSizeBytes, (void *)(*ppBufferHdr));
    }

    MTK_OMX_LOGD_ENG("MtkOmxVenc::UseBuffer port_idx(0x%X), idx[%d], pBuffHead(0x%08X), pBuffer(0x%08X), mapType:%s",
                 (unsigned int)nPortIndex, (int)mInputBufferPopulatedCnt,
                 (unsigned int)mInputBufferHdrs[mInputBufferPopulatedCnt],
                 (unsigned int)pBuffer, mInputMVAMgr->getType());

    mInputBufferPopulatedCnt++;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MtkOmxVenc::EpilogueInputBufferHeaderUse()
{
    HandleInputPortPopulated();

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MtkOmxVenc::OutputBufferHeaderUse(
    OMX_BUFFERHEADERTYPE **ppBufferHdr, OMX_U32 nPortIndex, OMX_PTR pAppPrivate, OMX_U32 nSizeBytes, OMX_U8 *pBuffer)
{
    *ppBufferHdr = mOutputBufferHdrs[mOutputBufferPopulatedCnt] =
                       (OMX_BUFFERHEADERTYPE *)MTK_OMX_ALLOC(sizeof(OMX_BUFFERHEADERTYPE));
    (*ppBufferHdr)->pBuffer = pBuffer;
    (*ppBufferHdr)->nAllocLen = nSizeBytes;
    (*ppBufferHdr)->pAppPrivate = pAppPrivate;
    (*ppBufferHdr)->pMarkData = NULL;
    (*ppBufferHdr)->nInputPortIndex  = MTK_OMX_INVALID_PORT;
    (*ppBufferHdr)->nOutputPortIndex = MTK_OMX_OUTPUT_PORT;
    //(*ppBufferHdr)->pOutputPortPrivate = NULL; // TBD

    if (OMX_FALSE == mStoreMetaDataInOutBuffers)
    {
        // If not meta mode,
        // UseBuffer doesn't support mapping MVA by ION.
        // This should be M4U :
        if (strncmp("m4u", mOutputMVAMgr->getType(), strlen("m4u")))
        {
            //if not m4u map
            delete mOutputMVAMgr;
            mOutputMVAMgr = new OmxMVAManager("m4u");
        }
        mOutputMVAMgr->newOmxMVAwithVA((void *)pBuffer, (int)nSizeBytes, (void *)(*ppBufferHdr));
    }

    MTK_OMX_LOGD_ENG("MtkOmxVenc::UseBuffer port_idx(0x%X), idx[%d], pBuffHead(0x%08X), pBuffer(0x%08X), mapType:%s",
                 (unsigned int)nPortIndex, (int)mOutputBufferPopulatedCnt,
                 (unsigned int)mOutputBufferHdrs[mOutputBufferPopulatedCnt],
                 (unsigned int)pBuffer, mOutputMVAMgr->getType());

    mOutputBufferPopulatedCnt++;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MtkOmxVenc::EpilogueOutputBufferHeaderUse()
{
    HandleOutputPortPopulated();

    return OMX_ErrorNone;
}

OMX_BOOL MtkOmxVenc::AllowToFreeBuffer(OMX_U32 nPortIndex, OMX_STATETYPE state)
{
    if (((nPortIndex == MTK_OMX_INPUT_PORT) && (mInputPortDef.bEnabled == OMX_FALSE)) ||
        ((nPortIndex == MTK_OMX_OUTPUT_PORT) && (mOutputPortDef.bEnabled == OMX_FALSE)))
        // in port disabled case, p.99
    {
        if (bufferReadyState(state))
            return OMX_TRUE;
    }

    if ((IS_PENDING(MTK_OMX_LOADED_PENDING)))
        // de-initialization, p.128
    {
        if(state == OMX_StateIdle)
            return OMX_TRUE;
    }

    return OMX_FALSE;
}

bool MtkOmxVenc::bufferReadyState(OMX_STATETYPE state)
{
    return (state == OMX_StateExecuting || state == OMX_StateIdle || state == OMX_StatePause);
}

OMX_ERRORTYPE MtkOmxVenc::UnmapInputMemory(OMX_BUFFERHEADERTYPE *pBuffHead)
{
    if (OMX_FALSE == mStoreMetaDataInBuffers)
    {
        mInputMVAMgr->freeOmxMVAByVa((void *)pBuffHead->pBuffer);
    }
    else//if meta mode
    {
        //buffer_handle_t _handle = *((buffer_handle_t *)(pBuffHead->pBuffer + 4));
        OMX_U32 _handle = 0;
        GetMetaHandleFromOmxHeader(pBuffHead, &_handle);
        mInputMVAMgr->freeOmxMVAByHndl((void *)_handle);
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MtkOmxVenc::FreeInputBuffers(OMX_BUFFERHEADERTYPE *pBuffHead)
{
    // free input buffers
    for (OMX_U32 i = 0 ; i < mInputPortDef.nBufferCountActual ; i++)
    {
        if (pBuffHead == mInputBufferHdrs[i])
        {
            if (pBuffHead != NULL)
            {
                MTK_OMX_LOGD_ENG("MtkOmxVenc::FreeBuffer input hdr (0x%08X), buf (0x%08X)",
                        (unsigned int)pBuffHead, (unsigned int)pBuffHead->pBuffer);
            }
            // do this only when input is meta mode, and allocateBuffer
            if ((mStoreMetaDataInBuffers == OMX_TRUE) && (mInputAllocateBuffer == OMX_TRUE) &&
                (pBuffHead->pBuffer != NULL))
            {
                MTK_OMX_FREE(pBuffHead->pBuffer);
            }
            MTK_OMX_FREE(mInputBufferHdrs[i]);
            mInputBufferHdrs[i] = NULL;
            mInputBufferPopulatedCnt--;
        }
    }

    if (mInputBufferPopulatedCnt == 0)       // all input buffers have been freed
    {
        mInputPortDef.bPopulated = OMX_FALSE;
        SIGNAL(mInPortFreeDoneSem);
        MTK_OMX_LOGD_ENG("MtkOmxVenc::FreeBuffer all input buffers have been freed!!! signal mInPortFreeDoneSem(%d)",
                     get_sem_value(&mInPortFreeDoneSem));
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MtkOmxVenc::releaseOutputNativeHandle(OMX_BUFFERHEADERTYPE *pBuffHead)
{
    native_handle* pHandle = (native_handle*)(pBuffHead->pBuffer + pBuffHead->nOffset);
    OMX_BOOL foundNativeHandle = OMX_FALSE;
    for (int i = 0; i < mStoreMetaOutNativeHandle.size(); ++i)
    {
        if (pHandle == mStoreMetaOutNativeHandle[i])
        {
            foundNativeHandle = OMX_TRUE;
            OMX_BOOL foundIONInfo = OMX_FALSE;
            for (int i = 0; i < mIonBufferInfo.size(); ++i)
            {
                if (pHandle->data[0] == mIonBufferInfo[i].ion_share_fd)
                {
                    foundIONInfo = OMX_TRUE;
                    if (0 == mIonBufferInfo[i].secure_handle)
                    {
                        if (NULL != mIonBufferInfo[i].va)
                        {
                            MTK_OMX_LOGD_ENG("ion_munmap %p w/ size %d",
                                mIonBufferInfo[i].va,
                                mIonBufferInfo[i].value[0]);
                            ion_munmap(mIonDevFd, mIonBufferInfo[i].va, mIonBufferInfo[i].value[0]);
#ifdef COPY_2_CONTIG
                            MTK_OMX_LOGD_ENG("ion_munmap %p w/ size %d",
                                mIonBufferInfo[i].va_4_enc,
                                mIonBufferInfo[i].value[0]);
                            ion_munmap(mIonDevFd, mIonBufferInfo[i].va_4_enc, mIonBufferInfo[i].value[0]);
#endif
                        }
                    }
                    ion_share_close(mIonDevFd, mIonBufferInfo[i].ion_share_fd);
                    if (ion_free(mIonDevFd, mIonBufferInfo[i].ion_handle))
                    {
                        MTK_OMX_LOGE("[ERROR] ion_free %d failed", mIonBufferInfo[i].ion_handle);
                    }
#ifdef COPY_2_CONTIG
                    ion_share_close(mIonDevFd, mIonBufferInfo[i].ion_share_fd_4_enc);
                    if (ion_free(mIonDevFd, mIonBufferInfo[i].ion_handle_4_enc))
                    {
                        MTK_OMX_LOGE("[ERROR] ion_free %d failed", mIonBufferInfo[i].ion_handle_4_enc);
                    }
#endif
                    mIonBufferInfo.removeAt(i);
                    break;
                }
            }
            if (OMX_TRUE == foundIONInfo)
            {
                MTK_OMX_LOGD_ENG("[Output][FreeBuffer] Buffer Header = %p, handle=%p, c:%d",
                             pBuffHead, pHandle, mStoreMetaOutNativeHandle.size());
                MTK_OMX_FREE(mStoreMetaOutNativeHandle[i]);
                mStoreMetaOutNativeHandle.removeAt(i);
            }
            else
            {
                MTK_OMX_LOGE("[Output][FreeBuffer][ERR] No matched ION info for ion fd %d",
                    pHandle->data[0]);
                for (int i = 0; i < mIonBufferInfo.size(); ++i)
                {
                    MTK_OMX_LOGE("mIonBufferInfo[%d].ion_share_fd: %d", i, mIonBufferInfo[i].ion_share_fd);
                }
            }
            break;
        }
    }
    if (OMX_FALSE == foundNativeHandle)
    {
        MTK_OMX_LOGE("[Output][FreeBuffer][ERR] No matched native handle for %p", pHandle);
        for (int i = 0; i < mStoreMetaOutNativeHandle.size(); ++i)
        {
            MTK_OMX_LOGE("mStoreMetaOutNativeHandle[%d]: %d", i, mStoreMetaOutNativeHandle[i]);
        }
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MtkOmxVenc::UnmapOutputMemory(OMX_BUFFERHEADERTYPE *pBuffHead)
{
    if (OMX_TRUE == mStoreMetaDataInOutBuffers)
    {
#ifdef SUPPORT_NATIVE_HANDLE
        if (OMX_TRUE == mIsAllocateOutputNativeBuffers)
        {
            releaseOutputNativeHandle(pBuffHead);
        }
        else
#endif
        {
            VAL_UINT32_T u4I;
            OMX_U32 _handle = 0;
            GetMetaHandleFromOmxHeader(pBuffHead, &_handle);
            // When output meta mode, the handles are created by venc, release handles here.
            for (u4I = 0; u4I < mStoreMetaOutHandle.size(); ++u4I)
            {
                if ((void *)mStoreMetaOutHandle[u4I]->handle == (void *)_handle)
                {
                    //unmap MVA here
                    mOutputMVAMgr->freeOmxMVAByHndl((void *)_handle);
                    MTK_OMX_LOGD("[Output][FreeBuffer] Buffer Header = 0x%u, handle=%u, c:%d",
                                 (unsigned int)pBuffHead,
                                 (unsigned int)mStoreMetaOutHandle[u4I]->handle,
                                 mStoreMetaOutHandle.size());
                    mStoreMetaOutHandle.removeAt(u4I);
                    break;
                }
            }
        }
    }
    else
    {
        mOutputMVAMgr->freeOmxMVAByVa((void *)pBuffHead->pBuffer);
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MtkOmxVenc::FreeOutputBuffers(OMX_BUFFERHEADERTYPE *pBuffHead)
{
    // free output buffers
    for (OMX_U32 i = 0 ; i < mOutputPortDef.nBufferCountActual ; i++)
    {
        if (pBuffHead == mOutputBufferHdrs[i])
        {
            MTK_OMX_LOGD("MtkOmxVenc::FreeBuffer output hdr (0x%08X), buf (0x%08X)",
                         (unsigned int)pBuffHead, (unsigned int)pBuffHead->pBuffer);
            // do this only when output is meta mode, and allocateBuffer
            if ((mStoreMetaDataInOutBuffers == OMX_TRUE) && (mOutputAllocateBuffer == OMX_TRUE) &&
                (pBuffHead->pBuffer != NULL))
            {
#ifdef SUPPORT_NATIVE_HANDLE
                if (OMX_FALSE == mIsAllocateOutputNativeBuffers)
                {
                    MTK_OMX_FREE(pBuffHead->pBuffer);
                }
#endif
            }
            MTK_OMX_FREE(mOutputBufferHdrs[i]);
            mOutputBufferHdrs[i] = NULL;
            mOutputBufferPopulatedCnt--;
        }
    }

    if (mOutputBufferPopulatedCnt == 0)      // all output buffers have been freed
    {
        mOutputPortDef.bPopulated = OMX_FALSE;
        SIGNAL(mOutPortFreeDoneSem);
        MTK_OMX_LOGD("MtkOmxVenc::FreeBuffer all output buffers have been freed!!! signal mOutPortFreeDoneSem(%d)",
                     get_sem_value(&mOutPortFreeDoneSem));
    }

    return OMX_ErrorNone;
}

int MtkOmxVenc::findBufferHeaderIndexAdvance(int iBufQId, OMX_BUFFERHEADERTYPE *pBuffHdr)
{
    OMX_BUFFERHEADERTYPE **pBufHdrPool = NULL;
    MtkOmxBufQ::MtkOmxBufQId    id = (MtkOmxBufQ::MtkOmxBufQId)iBufQId;
    int bufCount;

    if (id == MtkOmxBufQ::MTK_OMX_VENC_BUFQ_INPUT)
    {
        pBufHdrPool = mInputBufferHdrs;
        bufCount = mInputPortDef.nBufferCountActual;
    }
    else if (id == MtkOmxBufQ::MTK_OMX_VENC_BUFQ_OUTPUT)
    {
        pBufHdrPool = mOutputBufferHdrs;
        bufCount = mOutputPortDef.nBufferCountActual;
    }
    else if (id == MtkOmxBufQ::MTK_OMX_VENC_BUFQ_CONVERT_OUTPUT)
    {
        pBufHdrPool = mConvertOutputBufferHdrs;
        bufCount = CONVERT_MAX_BUFFER;
    }
    else if (id == MtkOmxBufQ::MTK_OMX_VENC_BUFQ_VENC_INPUT)
    {
        pBufHdrPool = mVencInputBufferHdrs;
        bufCount = CONVERT_MAX_BUFFER;
    }
    else
    {
        MTK_OMX_LOGE("[ERROR] findBufferHeaderIndex invalid index(%d)", (unsigned int)iBufQId);
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

    return -1; // nothing found
}

//find the BufferHdr index of OutBufQ which has the same buffer with pBufferHdr in InBufQ
int MtkOmxVenc::findBufferHeaderIndexAdvance(int iOutBufQId, int iInBufQId, OMX_BUFFERHEADERTYPE *pBuffHdr)
{
    (void)(iInBufQId);
    OMX_BUFFERHEADERTYPE **pBufHdrPool = NULL;
    MtkOmxBufQ::MtkOmxBufQId    id = (MtkOmxBufQ::MtkOmxBufQId)iOutBufQId;
    int bufCount;

    if (id == MtkOmxBufQ::MTK_OMX_VENC_BUFQ_CONVERT_OUTPUT)
    {
        pBufHdrPool = mConvertOutputBufferHdrs;
        bufCount = CONVERT_MAX_BUFFER;
    }
    else if (id == MtkOmxBufQ::MTK_OMX_VENC_BUFQ_VENC_INPUT)
    {
        pBufHdrPool = mVencInputBufferHdrs;
        bufCount = CONVERT_MAX_BUFFER;
    }
    else
    {
        MTK_OMX_LOGE("[ERROR] findBufferHeaderIndex invalid index(%d)", (unsigned int)iOutBufQId);
        return -1;
    }

    for (int i = 0 ; i < bufCount ; i++)
    {
        if (pBuffHdr->pBuffer == pBufHdrPool[i]->pBuffer)
        {
            // index found
            return i;
        }
    }

    return -1; // nothing found
}

OMX_BOOL MtkOmxVenc::GetMetaHandleFromOmxHeader(OMX_BUFFERHEADERTYPE *pBufHdr, OMX_U32 *pBufferHandle)
{
    if( (NULL == pBufHdr) || (NULL == pBufHdr->pBuffer) )
    {
        MTK_OMX_LOGD("Warning: BufferHdr is NULL !!!! LINE: %d", __LINE__);
        return OMX_FALSE;
    }
    OMX_U32 bufferType = *((OMX_U32 *)pBufHdr->pBuffer);
    // check buffer type
    //MTK_OMX_LOGD("bufferType %d", bufferType);
    if((kMetadataBufferTypeNativeHandleSource == bufferType)||(kMetadataBufferTypeGrallocSource == bufferType))
    {
        *pBufferHandle = *((OMX_U32 *)(pBufHdr->pBuffer + 4));
    }
    else if(kMetadataBufferTypeANWBuffer == bufferType)
    {
        ANativeWindowBuffer* pNWBuffer = *((ANativeWindowBuffer**)(pBufHdr->pBuffer + 4));

        if( NULL == pNWBuffer )
        {
            MTK_OMX_LOGD("Warning: pNWBuffer is NULL !!!! LINE: %d", __LINE__);
            return OMX_FALSE;
        }
        *pBufferHandle = (OMX_U32)pNWBuffer->handle;
    }
    else
    {
        MTK_OMX_LOGD("Warning: BufferType is not Gralloc Source !!!! LINE: %d", __LINE__);
        return OMX_FALSE;
    }
    return OMX_TRUE;
}

OMX_BOOL MtkOmxVenc::GetMetaHandleFromBufferPtr(OMX_U8 *pBuffer, OMX_U32 *pBufferHandle)
{
    if( NULL == pBuffer )
    {
        MTK_OMX_LOGE("Warning: Buffer is NULL !!!! LINE: %d", __LINE__);
        return OMX_FALSE;
    }
    OMX_U32 bufferType = *((OMX_U32 *)pBuffer);
    // check buffer type
    //MTK_OMX_LOGD("bufferType %d", bufferType);
    if((kMetadataBufferTypeNativeHandleSource == bufferType)||(kMetadataBufferTypeGrallocSource == bufferType))
    {
        *pBufferHandle = *((OMX_U32 *)(pBuffer + 4));
    }
    else if(kMetadataBufferTypeANWBuffer == bufferType)
    {
        ANativeWindowBuffer* pNWBuffer = *((ANativeWindowBuffer**)(pBuffer + 4));

        if( NULL == pNWBuffer )
        {
            MTK_OMX_LOGD("Warning: pNWBuffer is NULL !!!! LINE: %d", __LINE__);
            return OMX_FALSE;
        }
        *pBufferHandle = (OMX_U32)pNWBuffer->handle;
    }
    else
    {
        MTK_OMX_LOGD("Warning: BufferType is not Gralloc Source !!!! LINE: %d", __LINE__);
        return OMX_FALSE;
    }

    return OMX_TRUE;
}

void MtkOmxVenc::ReturnPendingInternalBuffers()
{
    OMX_BUFFERHEADERTYPE *pBuffHdr;
    MTK_OMX_LOGD("ReturnPending_I_Buffers");
    while (mpVencInputBufQ->Size() > 0)
    {
        LOCK(mpVencInputBufQ->mBufQLock);
        --mpVencInputBufQ->mPendingNum;
        UNLOCK(mpVencInputBufQ->mBufQLock);

        int input_idx = DequeueBufferAdvance(mpVencInputBufQ);
        pBuffHdr = (OMX_BUFFERHEADERTYPE *)mVencInputBufferHdrs[input_idx];
        MTK_OMX_LOGD("%06x VENC_p pBuffHdr(0x%08X) pBuffer(0x%08X) pMarkData(0x%08X) flag(0x%08X), mNum(%d)", (unsigned int)this, (unsigned int)pBuffHdr,
                     (unsigned int)pBuffHdr->pBuffer, (unsigned int)pBuffHdr->pMarkData, (unsigned int)pBuffHdr->nFlags, mpVencInputBufQ->mPendingNum);

        if (((pBuffHdr->nFlags&OMX_BUFFERFLAG_COLORCONVERT_NEEDRETURN) != 0) && pBuffHdr->pMarkData != 0) {
            pBuffHdr = (OMX_BUFFERHEADERTYPE *)mVencInputBufferHdrs[input_idx]->pMarkData;
            HandleEmptyBufferDone(pBuffHdr);
        }
    }
}

void MtkOmxVenc::InitConvertBuffer()
{
    OMX_U32 iWidth  = mOutputPortDef.format.video.nFrameWidth;
    OMX_U32 iHeight = mOutputPortDef.format.video.nFrameHeight;

    if (mInputScalingMode)
    {
        iWidth = mScaledWidth;
        iHeight = mScaledHeight;
    }

    if (mCnvtMVAMgr->count() == 0 && (NeedConversion() || mIsSecureInst))
    {
        int u4PicAllocSize = VENC_ROUND_N(iWidth, 32) * VENC_ROUND_N(iHeight, 32);
        //when secure instance, prepare convert buffer at first.
        mCnvtBufferSize = (u4PicAllocSize * 3) >> 1;
        mCnvtBufferSize = getHWLimitSize(mCnvtBufferSize);
        MTK_OMX_LOGD("InitConvertBuffer Cnvt Buffer:w=%lu, h=%lu, size=%u",
                     VENC_ROUND_N(iWidth, 32), VENC_ROUND_N(iHeight, 32), (unsigned int)mCnvtBufferSize);

        mCnvtMVAMgr->newOmxMVAandVA(MEM_ALIGN_512, mCnvtBufferSize, NULL, (void **)(&mCnvtBuffer));

        // ALPS03676091 Clear convert dst buffer to black
        setBufferBlack(mCnvtBuffer, u4PicAllocSize);

        mCnvtMVAMgr->syncBufferCacheFrm(mCnvtBuffer);
    }
}


int MtkOmxVenc::findBufferHeaderIndex(OMX_U32 nPortIndex, OMX_BUFFERHEADERTYPE *pBuffHdr)
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

    return -1; // nothing found
}

OMX_BOOL MtkOmxVenc::CheckBufferAvailabilityAdvance(MtkOmxBufQ *pvInputBufQ, MtkOmxBufQ *pvOutputBufQ)
{
    //MTK_OMX_LOGD("CheckBufferAvailabilityAdvance 0x%X=%d 0x%X=%d",
    //pvInputBufQ, pvInputBufQ->mId, pvOutputBufQ, pvOutputBufQ->mId);
    if (pvInputBufQ->IsEmpty() || pvOutputBufQ->IsEmpty())
    {
        return OMX_FALSE;
    }
#ifdef OMX_CHECK_DUMMY
    else if(CheckNeedOutDummy())
    {
        if(pvOutputBufQ->Size()<2)
        {
            //MTK_OMX_LOGD("CheckBufferAvailabilityAdvance pvOutputBufQ->Size=%d\n", pvOutputBufQ->Size());
            return OMX_FALSE;
        }
        return OMX_TRUE;
    }
#endif
    return OMX_TRUE;
}

int MtkOmxVenc::DequeueBufferAdvance(MtkOmxBufQ *pvBufQ)
{
    //MTK_OMX_LOGD("DequeueBufferAdvance 0x%X=%d", pvBufQ, pvBufQ->mId);
    return pvBufQ->DequeueBuffer();
}
void MtkOmxVenc::QueueBufferAdvance(MtkOmxBufQ *pvBufQ, OMX_BUFFERHEADERTYPE *pBuffHdr)
{
    if (MtkOmxBufQ::MTK_OMX_VENC_BUFQ_INPUT == pvBufQ->mId)
    {
        QueueInputBuffer(findBufferHeaderIndex(MTK_OMX_INPUT_PORT, pBuffHdr));
    }
    else if (MtkOmxBufQ::MTK_OMX_VENC_BUFQ_OUTPUT == pvBufQ->mId)
    {
        QueueOutputBuffer(findBufferHeaderIndex(MTK_OMX_OUTPUT_PORT, pBuffHdr));
    }
    else if (MtkOmxBufQ::MTK_OMX_VENC_BUFQ_CONVERT_OUTPUT == pvBufQ->mId)
    {
        pvBufQ->QueueBufferBack(findBufferHeaderIndexAdvance((int)pvBufQ->mId, pBuffHdr));
    }
    else if (MtkOmxBufQ::MTK_OMX_VENC_BUFQ_VENC_INPUT == pvBufQ->mId)
    {
        pvBufQ->QueueBufferFront(findBufferHeaderIndexAdvance((int)pvBufQ->mId, pBuffHdr));
    }
    else
    {
        MTK_OMX_LOGE("[ERROR] Unknown bufQ ID!!");
    }
}

OMX_BOOL MtkOmxVenc::GetVEncDrvBSBuffer(OMX_U8 *aOutputBuf, OMX_U32 aOutputSize)
{
    IN_FUNC();
    //(void)(aOutputSize);
    if (OMX_TRUE == mStoreMetaDataInOutBuffers)
    {
#ifdef SUPPORT_NATIVE_HANDLE
        if (OMX_TRUE == mIsAllocateOutputNativeBuffers)
        {
            native_handle* pHandle =  (native_handle*)aOutputBuf;
            ion_user_handle_t handle = pHandle->data[9];
            if (OMX_TRUE == mIsSecureSrc)
            {
                mBitStreamBuf.rBSAddr.u4VA      = 0;
                mBitStreamBuf.rBSAddr.u4PA      = 0;
                mBitStreamBuf.rBSAddr.u4Size    = pHandle->data[10];
                mBitStreamBuf.u4BSStartVA       = 0;
                mBitStreamBuf.rSecMemHandle     = pHandle->data[7];
                mBitStreamBuf.pIonBufhandle     = 0;
                MTK_OMX_LOGD_ENG("Get BS buffer secure handle 0x%x, size %d aOutputSize %d",
                    mBitStreamBuf.rSecMemHandle, mBitStreamBuf.rBSAddr.u4Size, aOutputSize);
              }
            else
            {
                OMX_BOOL foundIonInfo = OMX_FALSE;
                for (int i = 0; i < mIonBufferInfo.size(); ++i)
                {
                    if (handle == mIonBufferInfo[i].ion_handle)
                    {
                        foundIonInfo = OMX_TRUE;
#ifdef COPY_2_CONTIG
#ifdef CHECK_OVERFLOW
                        MTK_OMX_LOGE("Initialize %p to 0, size %d",
                            mIonBufferInfo[i].va_4_enc, mIonBufferInfo[i].value_4_enc[0]);
                        memset(mIonBufferInfo[i].va_4_enc, TITAN_CHECK_PATTERN, mIonBufferInfo[i].value_4_enc[0]);
#endif
                        mBitStreamBuf.rBSAddr.u4VA = (uint32_t)mIonBufferInfo[i].va_4_enc + TITAN_BS_SHIFT;
                        mBitStreamBuf.rBSAddr.u4PA = mIonBufferInfo[i].value_4_enc[3] + TITAN_BS_SHIFT;
                        mBitStreamBuf.rBSAddr.u4Size = mIonBufferInfo[i].value_4_enc[0] - TITAN_BS_SHIFT;
                        mBitStreamBuf.u4BSStartVA = (uint32_t)mIonBufferInfo[i].va_4_enc + TITAN_BS_SHIFT;
                        mBitStreamBuf.pIonBufhandle = mIonBufferInfo[i].ion_handle_4_enc;
#else
#ifdef CHECK_OVERFLOW
                        MTK_OMX_LOGE("Initialize %p to 0, size %d",
                            mIonBufferInfo[i].va, mIonBufferInfo[i].value[0]);
                        memset(mIonBufferInfo[i].va, TITAN_CHECK_PATTERN, mIonBufferInfo[i].value[0]);
#endif
                        mBitStreamBuf.rBSAddr.u4VA =  (uint32_t)mIonBufferInfo[i].va + BS_SHIFT;
                        mBitStreamBuf.rBSAddr.u4PA = mIonBufferInfo[i].value[3] + BS_SHIFT;
                        mBitStreamBuf.rBSAddr.u4Size = mIonBufferInfo[i].value[0] - BS_SHIFT;
                        mBitStreamBuf.u4BSStartVA =  (uint32_t)mIonBufferInfo[i].va + BS_SHIFT;
                        mBitStreamBuf.rSecMemHandle = 0;
                        mBitStreamBuf.pIonBufhandle = handle;
#endif
                        break;
                    }
                }
                if (OMX_TRUE == foundIonInfo)
                {
                    MTK_OMX_LOGD("Get BS buffer va 0x%x, size %d mva 0x%x",
                        mBitStreamBuf.rBSAddr.u4VA, mBitStreamBuf.rBSAddr.u4Size, mBitStreamBuf.rBSAddr.u4PA);
                }
                else
                {
                    MTK_OMX_LOGE("Didn't find handle %p", handle);
                    for (int i = 0; i < mIonBufferInfo.size(); ++i)
                    {
                        MTK_OMX_LOGE("handle[%d] = %p", i, mIonBufferInfo[i].ion_handle);
#ifdef COPY_2_CONTIG
                        MTK_OMX_LOGE("handle_4_enc[%d] = %p", i, mIonBufferInfo[i].ion_handle_4_enc);
#endif
                    }
                }
            }
        }
        else
#endif
        {
        (void)(aOutputSize);
        OMX_U32 _handle = 0;
        GetMetaHandleFromBufferPtr(aOutputBuf, &_handle);
        //output is meta mode
        if (mIsSecureSrc)
        {
            PROP();
            int hSecHandle = 0, size = 0;

            //gralloc_extra_query(_handle, GRALLOC_EXTRA_GET_SECURE_HANDLE_HWC, &hSecHandle);
            //use GET_SECURE_HANDLE for buffer not pass HWC
            gralloc_extra_query((buffer_handle_t)_handle, GRALLOC_EXTRA_GET_SECURE_HANDLE, &hSecHandle);
            gralloc_extra_query((buffer_handle_t)_handle, GRALLOC_EXTRA_GET_ALLOC_SIZE, &size);
            mBitStreamBuf.rBSAddr.u4VA = 0;
            mBitStreamBuf.rBSAddr.u4PA = 0;
            mBitStreamBuf.rBSAddr.u4Size = size;
            mBitStreamBuf.u4BSStartVA = 0;
            mBitStreamBuf.rSecMemHandle = hSecHandle;
            mBitStreamBuf.pIonBufhandle = 0;
            mBitStreamBuf.u4IonDevFd = 0;
            //mBitStreamBuf.u4Flags = FRM_BUF_FLAG_SECURE;  //for in-house tee
        }
        else
        {
            PROP();
            //MTK_OMX_LOGD("get normal buf");
            mOutputMVAMgr->getOmxMVAFromHndlToVencBS((void *)_handle, &mBitStreamBuf);
        }
        }
    }
    else //if not mStoreMetaDataInOutBuffers
    {
        PROP();
        if (mOutputMVAMgr->getOmxMVAFromVAToVencBS((void *)aOutputBuf, &mBitStreamBuf) < 0)
        {
            MTK_OMX_LOGE("[ERROR][Output][VideoEncode]\n");
            OUT_FUNC();
            return OMX_FALSE;
        }
    }
    OUT_FUNC();
    return OMX_TRUE;
}

OMX_BOOL MtkOmxVenc::GetVEncDrvFrmBuffer(OMX_U8 *aInputBuf, OMX_U32 aInputSize)
{
    IN_FUNC();
    if (MtkOmxBufQ::MTK_OMX_VENC_BUFQ_VENC_INPUT == mpVencInputBufQ->mId)//convert pipeline output
    {
        PROP();

        if (OMX_FALSE == NeedConversion())
        {
            PROP();
            // when pipeline without convert, input handle should passed here.
            if (OMX_FALSE == mStoreMetaDataInBuffers)
            {
                MTK_OMX_LOGE("[ERROR] MUST be meta mode!!");
                abort();
            }
            OMX_U32 _handle = 0;
            GetMetaHandleFromBufferPtr(aInputBuf, &_handle);

            int ionfd, bufSize;
            gralloc_extra_query((buffer_handle_t)_handle, GRALLOC_EXTRA_GET_ION_FD, &ionfd);
            gralloc_extra_query((buffer_handle_t)_handle, GRALLOC_EXTRA_GET_ALLOC_SIZE, &bufSize);

            // free MVA and query it again
            mInputMVAMgr->freeOmxMVAByHndl((void *)_handle);

            mInputMVAMgr->newOmxMVAwithHndl((void *)_handle, NULL);
            mInputMVAMgr->getOmxMVAFromHndlToVencFrm((void *)_handle, &mFrameBuf);

            int format;
            gralloc_extra_query((buffer_handle_t)_handle, GRALLOC_EXTRA_GET_FORMAT, &format);
            MTK_OMX_LOGD_ENG("FrameBuf : handle = 0x%x, VA = 0x%x, PA = 0x%x, format=%s(0x%x), ion=%d",
                         (unsigned int)_handle, mFrameBuf.rFrmBufAddr.u4VA, mFrameBuf.rFrmBufAddr.u4PA,
                         PixelFormatToString((unsigned int)format), (unsigned int)format, ionfd);

            //for dump counting
            if (mYV12State != 1)//YV12
            {
                mYV12State = 1;
                ++mYV12Switch;
            }
        }
        else    //pipeline and after convert
        {
            PROP();
            if (mCnvtMVAMgr->getOmxMVAFromVAToVencFrm(aInputBuf, &mFrameBuf) < 0)
            {
                MTK_OMX_LOGE("[ERROR] Can't find Frm in Cnvt MVA");
                OUT_FUNC();
                return OMX_FALSE;
            }
            MTK_OMX_LOGD_ENG("FrameBuf : VA = 0x%x, PA = 0x%x, format = not YV12",
                         mFrameBuf.rFrmBufAddr.u4VA, mFrameBuf.rFrmBufAddr.u4PA);

            //for dump counting
            if (mYV12State != 0)//after convert (RGB)
            {
                mYV12State = 0;
                ++mYV12Switch;
            }
        }

    }
    else if (MtkOmxBufQ::MTK_OMX_VENC_BUFQ_INPUT == mpVencInputBufQ->mId)//input
    {
        PROP();
        if (OMX_TRUE == mStoreMetaDataInBuffers)//if meta mode
        {
            if (mIsSecureSrc)
            {
                PROP();
                int hSecHandle = 0, size = 0;
                OMX_U32 _handle = 0;
                if (OMX_TRUE == NeedConversion())
                {
                    PROP();
                    //when secure color convert, the input handle is from mCnvtBuffer
                    GetMetaHandleFromBufferPtr(mCnvtBuffer, &_handle);
                }
                else
                {
                    PROP();
                    GetMetaHandleFromBufferPtr(aInputBuf, &_handle);
                }
                gralloc_extra_query((buffer_handle_t)_handle, GRALLOC_EXTRA_GET_SECURE_HANDLE_HWC, &hSecHandle);
                gralloc_extra_query((buffer_handle_t)_handle, GRALLOC_EXTRA_GET_ALLOC_SIZE, &size);

                mFrameBuf.rFrmBufAddr.u4VA = 0;
                mFrameBuf.rFrmBufAddr.u4PA = 0;
                mFrameBuf.rFrmBufAddr.u4Size = size;
                mFrameBuf.rSecMemHandle = (VAL_UINT32_T)hSecHandle;
            }
            else//non-secure buffer
            {
                PROP();
                //meta mode and normal buffer
                int ionfd, size, format;
                OMX_U32 _handle = 0;
                GetMetaHandleFromBufferPtr(aInputBuf, &_handle);
                if (NeedConversion())
                {
                    PROP();
                    mCnvtMVAMgr->getOmxMVAFromVAToVencFrm(mCnvtBuffer, &mFrameBuf);
                    //for dump counting
                    if (mYV12State != 0)
                    {
                        mYV12State = 0;
                        ++mYV12Switch;
                    }
                    gralloc_extra_query((buffer_handle_t)_handle, GRALLOC_EXTRA_GET_FORMAT, &format);
                    MTK_OMX_LOGD_ENG("FrameBuf : handle = 0x%x, VA = 0x%x, PA = 0x%x, format=%s(0x%x)",
                                 (unsigned int)_handle, mFrameBuf.rFrmBufAddr.u4VA, mFrameBuf.rFrmBufAddr.u4PA,
                                 PixelFormatToString((unsigned int)format), (unsigned int)format);
                }
                else // no need conversion, non secure, meta mode
                {
                    PROP();
                    gralloc_extra_query((buffer_handle_t)_handle, GRALLOC_EXTRA_GET_ION_FD, &ionfd);
                    gralloc_extra_query((buffer_handle_t)_handle, GRALLOC_EXTRA_GET_ALLOC_SIZE, &size);

                    // free MVA and query it again
                    mInputMVAMgr->freeOmxMVAByHndl((void *)_handle);

                    mInputMVAMgr->newOmxMVAwithHndl((void *)_handle, NULL);
                    mInputMVAMgr->getOmxMVAFromHndlToVencFrm((void *)_handle, &mFrameBuf);

                    gralloc_extra_query((buffer_handle_t)_handle, GRALLOC_EXTRA_GET_FORMAT, &format);
                    MTK_OMX_LOGD_ENG("FrameBuf : handle = 0x%x, VA = 0x%x, PA = 0x%x, frmBuf.size=%d, portdef.size=%d, grallocsize=%d, arg_InputSize=%d, format=%s(0x%x), ion=%d",
                                 (unsigned int)_handle, mFrameBuf.rFrmBufAddr.u4VA,
                                 mFrameBuf.rFrmBufAddr.u4PA, mFrameBuf.rFrmBufAddr.u4Size,
                                 mInputPortDef.nBufferSize, size, aInputSize,
                                 PixelFormatToString((unsigned int)format), (unsigned int)format, ionfd);
                    //for dump counting
                    if (mYV12State != 1)
                    {
                        mYV12State = 1;
                        ++mYV12Switch;
                    }
                }
                mFrameBuf.rSecMemHandle = 0;
            }
            //MTK_OMX_LOGD ("@@@ mFrameBuf.rFrmBufAddr.u4VA = 0x%08X, mFrameBuf.rFrmBufAddr.u4PA = 0x%08X, "
            //"mFrameBuf.rFrmBufAddr.u4Size = %d",
            //mFrameBuf.rFrmBufAddr.u4VA, mFrameBuf.rFrmBufAddr.u4PA, mFrameBuf.rFrmBufAddr.u4Size);
            OUT_FUNC();
            return OMX_TRUE;
        }
        else//not meta mode
        {
            PROP();
            //don't have secure buffers in non meta mode
            if (NeedConversion())
            {
                PROP();
                mCnvtMVAMgr->getOmxMVAFromVAToVencFrm(mCnvtBuffer, &mFrameBuf);
                //for dump counting
                if (mYV12State != 0)
                {
                    mYV12State = 0;
                    ++mYV12Switch;
                }
                MTK_OMX_LOGD("FrameBuf : VA = 0x%x, PA = 0x%x, format=(0x%x)",
                             mFrameBuf.rFrmBufAddr.u4VA, mFrameBuf.rFrmBufAddr.u4PA,
                             mInputPortDef.format.video.eColorFormat);
            }
            else
            {
                PROP();
                if (mInputMVAMgr->getOmxMVAFromVAToVencFrm((void *)aInputBuf, &mFrameBuf) < 0)
                {
                    MTK_OMX_LOGE("[ERROR][Input][VideoEncode]\n");
                    OUT_FUNC();
                    return OMX_FALSE;
                }
            }
            OUT_FUNC();
            return OMX_TRUE;
        }
    }
    mFrameBuf.rFrmBufAddr.u4Size = (VAL_UINT32_T)aInputSize;

    OUT_FUNC();
    return OMX_TRUE;
}

OMX_BOOL MtkOmxVenc::SetVEncDrvFrmBufferFlag(OMX_BUFFERHEADERTYPE *pInputBuf)
{
    mFrameBuf.u4InputFlag &= ~VENC_DRV_INPUT_BUF_CAMERASWITCH;
    if (pInputBuf->nFlags & OMX_BUFFERFLAG_CAMERASWITCH)
    {
        mFrameBuf.u4InputFlag |= VENC_DRV_INPUT_BUF_CAMERASWITCH;
        MTK_OMX_LOGD("SetVEncDrvFrmBufferFlag VENC_DRV_INPUT_BUF_CAMERASWITCH\n");
    }

    return OMX_TRUE;
}

void MtkOmxVenc::InitPipelineBuffer()
{
    int i, u4PicAllocSize;
    OMX_U32 iWidth = 0;
    OMX_U32 iHeight = 0;

    if (mInputScalingMode)
    {
        iWidth = mScaledWidth;
        iHeight = mScaledHeight;
    }
    else
    {
        iWidth = mOutputPortDef.format.video.nFrameWidth;
        iHeight = mOutputPortDef.format.video.nFrameHeight;
    }

    u4PicAllocSize = VENC_ROUND_N(iWidth, 32) * VENC_ROUND_N(iHeight, 32);
    mCnvtBufferSize = (u4PicAllocSize* 3) >> 1;
    mCnvtBufferSize = getHWLimitSize(mCnvtBufferSize);
    MTK_OMX_LOGD("Cnvt Buffer:w=%lu, h=%lu, size=%u",
                 VENC_ROUND_N(iWidth, 32), VENC_ROUND_N(iHeight, 32), (unsigned int)mCnvtBufferSize);

    if (mInitPipelineBuffer == true)
    {
        return;
    }

    //allocate convert buffer headers
    // allocate convert output buffer headers address array
    mConvertOutputBufferHdrs = (OMX_BUFFERHEADERTYPE **)MTK_OMX_ALLOC(sizeof(OMX_BUFFERHEADERTYPE *) *
                                                                      CONVERT_MAX_BUFFER);
    MTK_OMX_MEMSET(mConvertOutputBufferHdrs, 0x00, sizeof(OMX_BUFFERHEADERTYPE *)*CONVERT_MAX_BUFFER);
    // allocate venc input buffer headers address array
    mVencInputBufferHdrs = (OMX_BUFFERHEADERTYPE **)MTK_OMX_ALLOC(sizeof(OMX_BUFFERHEADERTYPE *) * CONVERT_MAX_BUFFER);
    MTK_OMX_MEMSET(mVencInputBufferHdrs, 0x00, sizeof(OMX_BUFFERHEADERTYPE *)*CONVERT_MAX_BUFFER);

    for (i = 0; i < CONVERT_MAX_BUFFER; ++i)
    {
        void *CnvtBuf = NULL;
        mCnvtMVAMgr->newOmxMVAandVA(MEM_ALIGN_512, mCnvtBufferSize, NULL, &CnvtBuf);

        mConvertOutputBufferHdrs[i] = (OMX_BUFFERHEADERTYPE *)MTK_OMX_ALLOC(sizeof(OMX_BUFFERHEADERTYPE));
        memset(mConvertOutputBufferHdrs[i], 0, sizeof(OMX_BUFFERHEADERTYPE));
        mConvertOutputBufferHdrs[i]->pBuffer    = (OMX_U8 *)CnvtBuf;
        mConvertOutputBufferHdrs[i]->nAllocLen  = mCnvtBufferSize;

        mVencInputBufferHdrs[i] = (OMX_BUFFERHEADERTYPE *)MTK_OMX_ALLOC(sizeof(OMX_BUFFERHEADERTYPE));
        memset(mVencInputBufferHdrs[i], 0, sizeof(OMX_BUFFERHEADERTYPE));
        mVencInputBufferHdrs[i]->pBuffer        = (OMX_U8 *)CnvtBuf;
        mVencInputBufferHdrs[i]->nAllocLen      = mCnvtBufferSize;

        // ALPS03676091 Clear convert dst buffer to black
        setBufferBlack((OMX_U8 *)CnvtBuf, u4PicAllocSize);

        mCnvtMVAMgr->syncBufferCacheFrm((OMX_U8 *)CnvtBuf);
    }

    LOCK(mpConvertOutputBufQ->mBufQLock);
    //fill convert output bufq
    for (i = 0; i < CONVERT_MAX_BUFFER; ++i)
    {
        mpConvertOutputBufQ->Push(i);
        ++mpConvertOutputBufQ->mPendingNum;
    }
    UNLOCK(mpConvertOutputBufQ->mBufQLock);

    mInitPipelineBuffer = true;
}

void MtkOmxVenc::DeinitPipelineBuffer()
{
    int i;

    if (mInitPipelineBuffer == false)
    {
        return;
    }

    //deallocate convert buffer headers
    if (mVencInputBufferHdrs != NULL)
    {
        for (i = 0; i < CONVERT_MAX_BUFFER; ++i)
        {
            if (mVencInputBufferHdrs[i] != NULL)
            {
                // unamp only once
                mCnvtMVAMgr->freeOmxMVAByVa(mVencInputBufferHdrs[i]->pBuffer);
                MTK_OMX_FREE(mVencInputBufferHdrs[i]);
            }
        }
        MTK_OMX_FREE(mVencInputBufferHdrs);
        mVencInputBufferHdrs = NULL;
    }
    if (mConvertOutputBufferHdrs != NULL)
    {
        for (i = 0; i < CONVERT_MAX_BUFFER; ++i)
        {
            if (mConvertOutputBufferHdrs[i] != NULL)
            {
                // don't unmap here...
                MTK_OMX_FREE(mConvertOutputBufferHdrs[i]);
            }
        }
        MTK_OMX_FREE(mConvertOutputBufferHdrs);
        mConvertOutputBufferHdrs = NULL;
    }

    mInitPipelineBuffer = false;
}

void MtkOmxVenc::DeinitConvertBuffer()
{
    if (mCnvtMVAMgr->count() > 0)
    {
        mCnvtMVAMgr->freeOmxMVAByVa((void *)mCnvtBuffer);
        mCnvtBuffer = NULL;
    }
}

#ifdef SUPPORT_NATIVE_HANDLE
OMX_BOOL MtkOmxVenc::SetDbgInfo2Ion(
    ion_user_handle_t& ion_handle,
    int& ion_share_fd,
    void* va,
    int secure_handle,
    int value1,
    int value2,
    int value3,
    int value4,
    ion_user_handle_t& ion_handle_4_enc,
    int& ion_share_fd_4_enc,
    void* va_4_enc,
    int secure_handle_4_enc,
    int value5,
    int value6,
    int value7,
    int value8
    )
{
    MtkVencIonBufferInfo rIonBufferInfo;
    rIonBufferInfo.ion_handle = ion_handle;
    rIonBufferInfo.ion_share_fd = ion_share_fd;
    rIonBufferInfo.va = va;
    rIonBufferInfo.secure_handle = secure_handle;
    rIonBufferInfo.value[0] = value1;
    rIonBufferInfo.value[1] = value2;
    rIonBufferInfo.value[2] = value3;
    rIonBufferInfo.value[3] = value4;
#ifdef COPY_2_CONTIG
    rIonBufferInfo.ion_handle_4_enc = ion_handle_4_enc;
    rIonBufferInfo.ion_share_fd_4_enc= ion_share_fd_4_enc;
    rIonBufferInfo.va_4_enc = va_4_enc;
    rIonBufferInfo.secure_handle = secure_handle_4_enc;
    rIonBufferInfo.value_4_enc[0] = value5;
    rIonBufferInfo.value_4_enc[1] = value6;
    rIonBufferInfo.value_4_enc[2] = value7;
    rIonBufferInfo.value_4_enc[3] = value8;
    MTK_OMX_LOGD_ENG("[4enc]ion_handle %x, fd %d, va %p", ion_handle_4_enc, ion_share_fd_4_enc, va_4_enc);
#endif
    mIonBufferInfo.push(rIonBufferInfo);
    return OMX_TRUE;
}
#endif

OMX_BOOL MtkOmxVenc::isBufferSec(OMX_U8 *aInputBuf, OMX_U32 aInputSize, int *aBufferType)
{
    int sec_buffer_type = 0;
    OMX_U32 _handle = 0;
    GetMetaHandleFromBufferPtr(aInputBuf, &_handle);

    gralloc_extra_ion_sf_info_t sf_info;
    gralloc_extra_query((buffer_handle_t)_handle, GRALLOC_EXTRA_GET_IOCTL_ION_SF_INFO, &sf_info);
    sec_buffer_type = ((sf_info. status & GRALLOC_EXTRA_MASK_SECURE) == GRALLOC_EXTRA_BIT_SECURE);

    //gralloc_extra_getSecureBuffer(_handle, &sec_buffer_type, &sec_handle);
    *aBufferType = sec_buffer_type;
    MTK_OMX_LOGD_ENG("isBufferSec %d 0x%8x\n",sec_buffer_type, sf_info.status);
    return OMX_TRUE;
}

OMX_BOOL MtkOmxVenc::InitSecCnvtBuffer(int num)
{
    int count = 0, i;
    if (num > CONVERT_MAX_BUFFER)
    {
        count = CONVERT_MAX_BUFFER;
    }
    else if (num > 0)
    {
        count = num;
    }
    //else//num <= 0
    OMX_U32 iWidth  = mOutputPortDef.format.video.nFrameWidth;
    OMX_U32 iHeight = mOutputPortDef.format.video.nFrameHeight;
    uint32_t flags = GRALLOC_USAGE_HW_VIDEO_ENCODER | GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_SECURE;
    for (i = 0; i < count; ++i)
    {
        mSecConvertBufs.push(new GraphicBuffer(VENC_ROUND_N(iWidth, 32), VENC_ROUND_N(iHeight, 32),
                                               HAL_PIXEL_FORMAT_YV12, flags));
        MTK_OMX_LOGD("mSecConvertBufs[%d] h=0x%x", i, (unsigned int)mSecConvertBufs[0]->handle);
    }
    return OMX_TRUE;
}

OMX_U32 MtkOmxVenc::CheckGrallocWStride(OMX_BUFFERHEADERTYPE *pInputBuf)
{
    OMX_U8 *aInputBuf = pInputBuf->pBuffer + pInputBuf->nOffset;
    OMX_U32 _handle = 0;
    unsigned int stride = 0;

    if (pInputBuf->nFilledLen == 0 || OMX_TRUE != mStoreMetaDataInBuffers)
    {
        //if buffer is empty or not meta mode
        //if ((pInputBuf->nFlags & OMX_BUFFERFLAG_EOS) && (0xCDCDCDCD == (OMX_U32)_handle))
        if ((pInputBuf->nFlags & OMX_BUFFERFLAG_EOS) && (pInputBuf->nFilledLen == 0)
            &&(OMX_TRUE == mStoreMetaDataInBuffers)){
                MTK_OMX_LOGE("CheckGrallocWStride nStride=%d\n",mInputPortDef.format.video.nStride);
                return mInputPortDef.format.video.nStride;
        }
        return 0;
    }
    else
    {
        GetMetaHandleFromBufferPtr(aInputBuf, &_handle);
        gralloc_extra_query((buffer_handle_t)_handle, GRALLOC_EXTRA_GET_STRIDE, &stride);
        MTK_OMX_LOGE("CheckGrallocWStride: %d", stride);
        return (uint32_t)stride;
    }
}

unsigned int MtkOmxVenc::getLatencyToken(OMX_BUFFERHEADERTYPE *pInputBufHdr, OMX_U8 *pInputBuf)
{
    if (OMX_TRUE == mStoreMetaDataInBuffers && OMX_TRUE == mWFDMode)//WFD mode
    {
        OMX_U32 _handle = 0;
        GetMetaHandleFromBufferPtr(pInputBuf, &_handle);

        gralloc_extra_ion_sf_info_t ext_info;
        gralloc_extra_query((buffer_handle_t)_handle, GRALLOC_EXTRA_GET_IOCTL_ION_SF_INFO, &ext_info);
        //MTK_OMX_LOGD("get sequence info: %u", ext_info.sequence);
        pInputBufHdr->nTickCount = (OMX_U32)ext_info.sequence;
        return ext_info.sequence;
    }
    else
    {
        pInputBufHdr->nTickCount = (OMX_U32)mFrameCount;
        return 0;
    }
}

int MtkOmxVenc::checkSecSwitchInEnc(OMX_BUFFERHEADERTYPE *pInputBuf, OMX_BUFFERHEADERTYPE *pOutputBuf)
{
    int inputbuffertype = 0;
    if (OMX_TRUE == mStoreMetaDataInOutBuffers)
    {
        if ((OMX_TRUE == mIsSecureInst) && mDoConvertPipeline)
        {
            //inputbuffertype = ((unsigned int)pInputBuf->pMarkData == 0xc07f1900) ? 1 : 0;
            inputbuffertype = ((pInputBuf->nFlags & OMX_BUFFERFLAG_SECUREBUF) != 0);
            MTK_OMX_LOGD("get convert in: %d, 0x%x, 0x%x",
                         inputbuffertype, pInputBuf->pMarkData, pInputBuf->nFlags);
        }
        else
        {
            if ((OMX_TRUE == mIsSecureInst) && (OMX_FALSE == isBufferSec(pInputBuf->pBuffer + pInputBuf->nOffset, pInputBuf->nFilledLen, &inputbuffertype)))
            {
                MTK_OMX_LOGE("[ERROR] Input frame buffer type is not normal nor secure\n");
                return -1;
            }
        }
    }

    //Src Buffer Type = 0 Normal Buffer
    //Src Buffer Type = 1 Secure Buffer
    //Src Buffer Type = 2 Secure FD, not support
    //mIsSecureSrc = FALSE current Working buffers for Normal Path
    //mIsSecureSrc = TRUE current Working buffers for Secure Path
    // for TRUSTONIC TEE SUPPORT only [
    if (mIsSecureSrc && mTeeEncType == TRUSTONIC_TEE && !bHasSecTlc)
    {
        MTK_OMX_LOGE("[ERROR] Don't support secure input!\n");
        return -1;
    }
    // ]

    if (((1 == inputbuffertype) && (OMX_FALSE == mIsSecureSrc)) ||
        ((0 == inputbuffertype) && (OMX_TRUE == mIsSecureSrc)))
    {
        normalSecureSwitchHndling(pInputBuf, pOutputBuf);
        return 1;//return switching happen
    }
    return 0;
}

int MtkOmxVenc::checkSecSwitchInCC(OMX_BUFFERHEADERTYPE *pInputBuf, OMX_BUFFERHEADERTYPE *pOutputBuf)
{
    return 0;
}

/* Debug Util */
bool MtkOmxVenc::dumpInputBuffer(OMX_BUFFERHEADERTYPE *pInputBuf, OMX_U8 *aInputBuf, OMX_U32 aInputSize)
{
    (void)(pInputBuf);

    MTK_OMX_LOGD("dumpInputBuffer: 0x%x [%d]", aInputBuf, aInputSize);

    char name[128];
    int width = mInputPortDef.format.video.nFrameWidth;
    int height = mInputPortDef.format.video.nFrameHeight;
    int size = 0;
    if (OMX_TRUE == mIsSecureSrc)
    {
        if (OMX_TRUE == mDumpInputFrame)
        {
            if (OMX_FALSE == NeedConversion())
            {
                int hSecHandle;
                buffer_handle_t _handle = *((buffer_handle_t *)(aInputBuf + 4));
                gralloc_extra_query(_handle, GRALLOC_EXTRA_GET_SECURE_HANDLE_HWC, &hSecHandle);

                sprintf(name, "/sdcard/input_%u_%u_%d_s.yuv", width, height, gettid());
                size = width * height * 3 / 2;
                dumpSecBuffer(name, hSecHandle, size);
            }
            //MTK_OMX_LOGD("Cannot dump frame buffer data when secure path enabled\n");
        }
    }
    else
    {
        //if(1)// (mRTDumpInputFrame == OMX_TRUE)
        mDumpInputFrame = (OMX_BOOL) MtkVenc::DumpInputFrame();
        if (OMX_TRUE == mDumpInputFrame)
        {
            if (OMX_FALSE == mDumpCts) {
                sprintf(name, "/sdcard/input_%u_%u_%u_%d.yuv", width, height, aInputSize, gettid());
            } else {
                sprintf(name, "/sdcard/vdump/Venc_input_%ld_%ld_%05d.yuv", width, height, gettid());
            }

            dumpBuffer(name, (unsigned char *)aInputBuf, aInputSize);
        }
    }
    return true;
}

bool MtkOmxVenc::dumpOutputBuffer(OMX_BUFFERHEADERTYPE *pOutputBuf, OMX_U8 *aOutputBuf, OMX_U32 aOutputSize)
{
    MTK_OMX_LOGD("dumpOutputBuffer: 0x%x [%d]", aOutputBuf, aOutputSize);

    char name[128];
    if (mIsSecureSrc == OMX_TRUE && (mDumpFlag & DUMP_SECURE_OUTPUT_Flag) && bHasSecTlc)
    {
        int hSecHandle = 0;
        buffer_handle_t _handle = *((buffer_handle_t *)(aOutputBuf + 4));
        gralloc_extra_query(_handle, GRALLOC_EXTRA_GET_SECURE_HANDLE, &hSecHandle);

        sprintf(name, "/sdcard/enc_dump_%d_s.h264", gettid());
        dumpSecBuffer(name, hSecHandle, mBitStreamBuf.u4BSSize);
    }
    else
    {
        VAL_BOOL_T bDumpLog = MtkVenc::DumpBs();

        if (bDumpLog)
        {
            sprintf(name, "/sdcard/enc_dump_bitstream%d.bin", gettid());
            if (mIsSecureSrc == OMX_FALSE)
            {
                dumpBuffer(name, (unsigned char *)aOutputBuf, aOutputSize);
            }
       #ifdef MTK_DUM_SEC_ENC
            else if(INHOUSE_TEE == mTeeEncType)
            {
                  int hSecHandle = 0;
                  buffer_handle_t _handle = *((buffer_handle_t *)(aOutputBuf + 4));
                  gralloc_extra_query(_handle, GRALLOC_EXTRA_GET_SECURE_HANDLE, &hSecHandle);
                  dumpSecBuffer(name, hSecHandle, aOutputSize);
            }
       #endif
        }

        if (mIsViLTE)
        {
            VAL_BOOL_T bDump = MtkVenc::DumpVtBs();
            if (bDump)
            {
                sprintf(name, "/sdcard/vilte_venc_bs_%d.bin", gettid());
                dumpBuffer(name, (unsigned char *)aOutputBuf, aOutputSize);
            }
        }
    }
    return true;
}

bool MtkOmxVenc::dumpSecBuffer(char *name, int hSecHandle, int size)
{
    int i, tmpSize = 0;
    unsigned char *buf, *tmp;
    if (mTeeEncType == TRUSTONIC_TEE)
    {
        tmpSize = 64 << 10;//64K
        buf = (unsigned char *)memalign(512, size);
        tmp = buf;
        void *tlcHandle = tlc.tlcHandleCreate();
        for (i = 0; i < (size / tmpSize); ++i)
        {
            tlc.tlcDumpSecMem(tlcHandle, ((uint32_t)hSecHandle + (i * tmpSize)),
                              tmp + tmpSize, tmpSize);
        }
        if (size  % tmpSize)
        {
            tlc.tlcDumpSecMem(tlcHandle, ((uint32_t)hSecHandle + (i * tmpSize)),
                              tmp + (i * tmpSize), size % tmpSize);
        }
        dumpBuffer(name, buf, size);
        free(buf);
        tlc.tlcHandleRelease(tlcHandle);
    }
#ifdef MTK_DUM_SEC_ENC
    else if(INHOUSE_TEE == mTeeEncType)
    {
         MTK_OMX_LOGD("INHOUSE dump");
         MTK_OMX_LOGE(" %s line %d \n", __FUNCTION__,__LINE__);
         if(pTmp_buf == 0)
         {
            pTmp_buf =(unsigned char *)memalign(512, size);
            if(pTmp_buf)
            Tmp_bufsz = size;
         }
         else if(size > Tmp_bufsz)
         {
             free(pTmp_buf);
             pTmp_buf =(unsigned char *)memalign(512, size);
             if(pTmp_buf)
             Tmp_bufsz = size;
         }
         MTK_OMX_LOGE(" %s line %d,szie %d \n", __FUNCTION__,__LINE__,size);
         Dump_buf_sec_and_normal(hSecHandle, pTmp_buf, size, 0);
         dumpBuffer(name, pTmp_buf, size);
    }
#endif
    else
    {
        MTK_OMX_LOGD("Don't support dump input");
    }
    return true;
}

bool MtkOmxVenc::dumpBuffer(char *name, unsigned char *data, int size)
{
    MTK_OMX_LOGD("dumpBuffer: %s, 0x%x[%d]", name, data, size);

    FILE *fp = fopen(name, "ab");
    char newFile[256]={0};
    if(!fp)
    {
         char *ptsrc =NULL;
         if(0==memcmp("/data/",name, 6))
         {
            ptsrc =(char *)(name +6);
            memcpy(newFile,"/sdcard/",8);
            memcpy(newFile+8,ptsrc, strlen(ptsrc));
         }
         else  if(0==memcmp("/sdcard/",name, 8))
         {
            ptsrc =(char *)(name +8);
            memcpy(newFile,"/data/",6);
            memcpy(newFile+6,ptsrc, strlen(ptsrc));
         }
         else
         {
             MTK_OMX_LOGE("open file %s fail: %d %s,and new file not support", name, errno, strerror(errno));
             return true;
         }
         MTK_OMX_LOGE("open file %s fail,and switch to %s\n", name, newFile);
         fp = fopen(newFile, "ab");
    }

    if (fp)
    {
        fwrite((void *)data, 1, size, fp);
        fclose(fp);
    }
    else
    {
        MTK_OMX_LOGE("open file %s fail: %d %s", name, errno, strerror(errno));
    }
    return true;
}

bool MtkOmxVenc::dumpYUVBuffer(char *name, unsigned char *y, unsigned char *u, unsigned char *v,
                               int width, int height)
{
    int ySize = width * height;
    int uvSize = ySize >> 2;
    FILE *fp = fopen(name, "ab");
    if (fp)
    {
        fwrite((void *)y, 1, ySize, fp);
        fwrite((void *)u, 1, uvSize, fp);
        fwrite((void *)v, 1, uvSize, fp);
        fclose(fp);
    }
    else
    {
        MTK_OMX_LOGE("open file %s fail: %d %s", name, errno, strerror(errno));
    }
    return true;
}

#ifdef MTK_DUM_SEC_ENC
/* -------------------------------------------------------------------------- */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <uree/system.h>
#include <uree/mem.h>
#include <tz_cross/ta_test.h>
#include <tz_cross/ta_mem.h>

VAL_UINT32_T MtkOmxVenc::Dump_buf_sec_and_normal(OMX_U32 hSec_buf, OMX_U8* pTemp_Buf,OMX_U32 ui4_sz, unsigned  dir)
{
    TZ_RESULT ret;
    UREE_SESSION_HANDLE uree_mem_session;
    UREE_SESSION_HANDLE uree_dump_ta_test;
    UREE_SHAREDMEM_PARAM  shm_param;
    UREE_SHAREDMEM_HANDLE shm_handle_src;
    unsigned int cmd;
    MTEEC_PARAM param[4];

    /* check input parameters */
    if( NULL == pTemp_Buf )
    {
        ALOGE("MtkOmxVenc ERROR: NULL pointer for normal buffer\n");
        return -1;
    }
        /* create memory and mtee img prot inf gen sessions */
    ret = UREE_CreateSession(TZ_TA_MEM_UUID, &uree_mem_session);
    if (ret != TZ_RESULT_SUCCESS)
    {
        ALOGE("MtkOmxVenc ERROR:fail to creat memory session (%s)\n", TZ_GetErrorString(ret));
        return -2;
    }
    ret = UREE_CreateSession(TZ_TA_TEST_UUID, &uree_dump_ta_test);
    if (ret != TZ_RESULT_SUCCESS)
    {
        ALOGE("MtkOmxVenc ERROR: fail to creat test ta session (%s)\n", TZ_GetErrorString(ret));

        ret = UREE_CloseSession(uree_mem_session);
        return -3;
    }

    /* register share memory handles */
    shm_param.buffer = (void *) pTemp_Buf;
    shm_param.size = ui4_sz;
    ret = UREE_RegisterSharedmem(uree_mem_session, &shm_handle_src, &shm_param);
    if (ret != TZ_RESULT_SUCCESS)
    {
        ALOGE("MtkOmxVenc ERROR: fail to register share memory for normal buffer (%s)\n", TZ_GetErrorString(ret));
        ret = UREE_CloseSession(uree_dump_ta_test);
        ret = UREE_CloseSession(uree_mem_session);
        return -4;
    }

    /* perform operation */
    cmd = ( dir == 0 )? TZCMD_TEST_CP_SBUF2NBUF : TZCMD_TEST_CP_NBUF2SBUF;
    param[0].value.a = hSec_buf;
    param[0].value.b = 0;
    param[1].memref.handle = (uint32_t) shm_handle_src;
    param[1].memref.offset = 0;
    param[1].memref.size = ui4_sz;
    param[2].value.a = ui4_sz;
    param[2].value.b = 0;
    ret = UREE_TeeServiceCall( uree_dump_ta_test,
                               cmd,
                               TZ_ParamTypes3(TZPT_VALUE_INPUT, TZPT_MEMREF_INOUT, TZPT_VALUE_INPUT),
                               param );
    if (ret != TZ_RESULT_SUCCESS)
    {
         ALOGE("MtkOmxVenc ERROR: fail to invoke function for test ta (%s)\n", TZ_GetErrorString(ret));
         ret = UREE_UnregisterSharedmem(uree_mem_session, shm_handle_src);
         ret = UREE_CloseSession(uree_dump_ta_test);
         ret = UREE_CloseSession(uree_mem_session);
         return -5;
    }

    /* un-register share memory handles */
    ret = UREE_UnregisterSharedmem(uree_mem_session, shm_handle_src);
    if (ret != TZ_RESULT_SUCCESS)
    {
        ALOGE("MtkOmxVenc ERROR: fail to un-register share memory for normal buffer (%s)\n", TZ_GetErrorString(ret));
        ret = UREE_CloseSession(uree_dump_ta_test);
        ret = UREE_CloseSession(uree_mem_session);
        return -6;
    }

    MTK_OMX_LOGE("MtkOmxVenc :Free sec dump tmp handle\n");
    ret = UREE_CloseSession(uree_dump_ta_test);
    if (ret != TZ_RESULT_SUCCESS)
    {
        ALOGE("MtkOmxVenc ERROR: fail to close test ta session (%d)\n", ret);
        ret = UREE_CloseSession(uree_mem_session);
        return -7;
    }

    MTK_OMX_LOGE("MtkOmxVenc :Free sec dump tmp session\n");
    ret = UREE_CloseSession(uree_mem_session);
    if (ret != TZ_RESULT_SUCCESS)
    {
        ALOGE("MtkOmxVenc ERROR: fail to close memory session (%d)\n", ret);
        return -8;
    }
    return 0;
}
#endif

#ifdef SUPPORT_NATIVE_HANDLE
void MtkOmxVenc::DumpCorruptedMem(char* name, char* startAddr, int size)
{
    if (NULL == startAddr )
    {
        MTK_OMX_LOGE("NULL startAddr for size %d", size);
        return;
    }
    static int fileNumPostFix = 0;
    char buf[255];
    sprintf(buf, "/sdcard/venc/venc_file_%d_%s_size_%d.h264", fileNumPostFix, name, size);
    FILE *fp = fopen(buf, "wb");
    if (fp)
    {
        MTK_OMX_LOGE("Dump to %s", buf);
        fwrite((void *)startAddr, 1, size, fp);
        fclose(fp);
    }
    else
    {
        MTK_OMX_LOGE("Cannot open %s", buf);
    }
    ++fileNumPostFix;
}
void MtkOmxVenc::checkMemory(char* startAddr, int bufferSize, char* name)
{
    char* ptemp = startAddr;
    int correuptedSize = 0;
    OMX_BOOL corrupting = OMX_FALSE;
    char* myStartAddr = NULL;
    MTK_OMX_LOGE("%s Check start from %p, size %d", name, ptemp, bufferSize);
#if 1
    OMX_BOOL isDirty = OMX_FALSE;
    for (int i = 0; i < bufferSize; ++i)
    {
        if (*ptemp != CHECK_PATTERN)
        {
            isDirty = OMX_TRUE;
            break;
        }
         ++ptemp;
    }
    if (OMX_TRUE == isDirty)
    {
        MTK_OMX_LOGE("Buffer is dirty, dump %d bytes to check", bufferSize);
        DumpCorruptedMem(name, startAddr, bufferSize);
    }
#else
    for (int i = 0; i < bufferSize; ++i)
    {
        if (*ptemp != 0)
        {
            if (OMX_FALSE == corrupting)
            {
                correuptedSize = 1;
                corrupting = OMX_TRUE;
                myStartAddr = ptemp;
            }
            else
            {
                correuptedSize += 1;
            }
        }
        else
        {
            if (OMX_TRUE == corrupting)
            {
                corrupting = OMX_FALSE;
                MTK_OMX_LOGE("Continuous correupted size: %d bytes", correuptedSize);
                DumpCorruptedMem(name, myStartAddr, correuptedSize);
                myStartAddr = NULL;
            }
        }
        ++ptemp;
    }
    if (OMX_TRUE == corrupting)
    {
        MTK_OMX_LOGE("Continuous correupted size: %d bytes", correuptedSize);
        DumpCorruptedMem(name, myStartAddr, correuptedSize);
    }
#endif
    MTK_OMX_LOGE("%s Check end %p", name, ptemp);
    return;
}
void MtkOmxVenc::DumpBitstream(char* name, char* startAddr, int size)
{
    if (NULL == startAddr )
    {
        MTK_OMX_LOGE("NULL startAddr for size %d", size);
        return;
    }
    static int fileNumPostFix = 0;
    char buf[255];
    sprintf(buf, "/sdcard/venc_recorded_bs/bs_%d.h264", fileNumPostFix);
    FILE *fp = fopen(buf, "wb");
    if (fp)
    {
        MTK_OMX_LOGD("Dump to %s", buf);
        fwrite((void *)startAddr, 1, size, fp);
        fclose(fp);
    }
    else
    {
        MTK_OMX_LOGE("Cannot open %s", buf);
    }
    ++fileNumPostFix;
}
void MtkOmxVenc::SetBitstreamSize4Framework(
    OMX_BUFFERHEADERTYPE *pOutputBuf,
    OMX_U8 *aOutputBuf,
    OMX_U32 bistreamSize
    )
{
    //pOutputBuf->nFilledLen = 8;
    pOutputBuf->nFilledLen = bistreamSize;
    native_handle_t *handle = (native_handle_t *)(aOutputBuf);
    handle->data[2] = bistreamSize;
#ifdef COPY_2_CONTIG
#ifdef CHECK_OVERFLOW
    checkMemory((char*)handle->data[7], TITAN_BS_SHIFT, "[HEAD]");
    checkMemory((char*)handle->data[7] + TITAN_BS_SHIFT + bistreamSize, 2 * 1024, "[TAIL]");
    checkMemory((char*)handle->data[7] + handle->data[8] - 1 - 512, 512, "[OUTBOUND]");
#endif
    MTK_OMX_LOGD("Copy from %p to %p", handle->data[7], handle->data[3]);
    memcpy((void*)handle->data[3], (void*)(handle->data[7] + TITAN_BS_SHIFT), bistreamSize);
    MTK_OMX_LOGD("Copy done");
#endif
#ifdef CHECK_OVERFLOW
    checkMemory((char*)handle->data[3] + TITAN_BS_SHIFT - 4 * 1024, 4 * 1024, "[HEAD]");
    checkMemory((char*)handle->data[3] + TITAN_BS_SHIFT + bistreamSize + 128, 2 * 1024, "[TAIL]");
    checkMemory((char*)handle->data[3] + handle->data[8] - 1 - 512, 512, "[OUTBOUND]");
    MTK_OMX_LOGD("Copy from %p to %p", handle->data[3] + TITAN_BS_SHIFT, handle->data[3]);
    memcpy((void*)(handle->data[3] + TITAN_BS_SHIFT), (void*)handle->data[3], bistreamSize);
#endif
#ifdef CHECK_OUTPUT_CONSISTENCY
    MTK_OMX_LOGD("Copy %d bytes from %p to %p", bistreamSize, handle->data[3], handle->data[7]);
    memcpy((void*)handle->data[7], (void*)handle->data[3], bistreamSize);
#endif
    if (0 != mRecordBitstream)
    {
        DumpBitstream("dummy", (char*)handle->data[3], bistreamSize);
    }
    if (0 != mWFDLoopbackMode)
    {
        static int fileNumPostFix = 0;
        char buf[255];
        int bufferSize = 256*1024*1024;
        char* dataBuffer = (char*)MTK_OMX_ALLOC(bufferSize);
        sprintf(buf, "/sdcard/venc_recorded_bs/bs_%d.h264", fileNumPostFix);
        MTK_OMX_LOGD("[Loopback] copy from pre-dump %s", buf);
        FILE *fp = fopen(buf, "rb");
        if (fp)
        {
            size_t readSize = fread(dataBuffer, 1, bufferSize, fp);
            MTK_OMX_LOGD("[Loopback]readSize %d", readSize);
            handle->data[2] = readSize;
            memcpy((void*)handle->data[3], (void*)dataBuffer, readSize);
            fclose(fp);
            ++fileNumPostFix;
        }
        else
        {
            MTK_OMX_LOGD("[Loopback] Cannot open file %s", buf);
            fileNumPostFix = 0;
        }
        MTK_OMX_FREE(dataBuffer);
    }
    MTK_OMX_LOGD("handle->data[2](bistream size) is %d", handle->data[2]);
}
#endif

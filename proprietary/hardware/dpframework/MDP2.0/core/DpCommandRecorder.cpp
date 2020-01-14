#include "DpCommandRecorder.h"
#include "DpEngineType.h"
#include "DpPathBase.h"

#include "m4u_port_map.h"
#include "mdp_reg_rdma.h"
#include "mdp_reg_wdma.h"
#include "mdp_reg_wrot.h"
#include "DpPlatform.h"
#if CONFIG_FOR_OS_ANDROID
#ifndef BASIC_PACKAGE
#include "PQSessionManager.h"
#endif //BASIC_PACKAGE
#endif // CONFIG_FOR_OS_ANDROID
#include <string.h>

#define UNIT_COMMAND_BLOCK_SIZE (32 * 1024)
#define BASE_COMMAND_BLOCK_SIZE (UNIT_COMMAND_BLOCK_SIZE * 5)

#define CMDQ_SUBSYS_INVALID     (-1)
#define CMDQ_SUBSYS_UNDEFINED   (99)
#define CMDQ_ADDR_MASK          0xFFFFFFFC
#define CMDQ_ADDR_MASK_16       0xFFFC


static inline int32_t get_cmdq_subsys(uint32_t argA)
{
    int32_t subsys = (argA & 0xffff0000) >> 16;

    switch (subsys) {
#define DECLARE_CMDQ_SUBSYS(msb, id, grp, base) case msb: return id;
#include "cmdq_subsys.h"
#undef DECLARE_CMDQ_SUBSYS
        default:
            // White list
            if (0x1502 == subsys || 0x1501 == subsys || 0x1500 == subsys)// ISP DIP WPE CQ
            {
                return CMDQ_SUBSYS_UNDEFINED;
            }
            else
            {
                DPLOGE("DpCommandRecorder: unknown cmdq subsys %#06x", subsys);
            }
    }
    return CMDQ_SUBSYS_INVALID;
}


DpCommandRecorder::DpCommandRecorder(DpPathBase *path)
    : m_pPath(path)
    , m_pBackBuffer(NULL)
    , m_pFrontBuffer(NULL)
    , m_pExtBuffer(NULL)
    , m_backBufferSize(0)
    , m_frontBufferSize(0)
    , m_extBufferSize(0)
    , m_backLength(0)
    , m_frontLength(0)
    , m_extLength(0)
    , m_pCurCommand(NULL)
    , m_blockType(NONE_BLOCK)
    , m_tileID(0)
    , m_pBackLabels(NULL)
    , m_pFrontLabels(NULL)
    , m_backLabelCount(0)
    , m_frontLabelCount(0)
    , m_pCurLabel(NULL)
    , m_pLabelCommand(NULL)
    , m_pLastCommand(NULL)
    , m_nextLabel(0)
    , m_dumpOrder(0)
    , m_maxX(0)
    , m_maxY(0)
    , m_secureMode(DP_SECURE_NONE)
    , m_secureInfoCount(0)
    , m_addrListLength(0)
    , m_srcFormat(DP_COLOR_UNKNOWN)
    , m_srcWidth(0)
    , m_srcHeight(0)
    , m_srcYPitch(0)
    , m_srcUVPitch(0)
    , m_srcSecMode(DP_SECURE_NONE)
    , m_regDstNum(0)
    , m_numReadbackRegs(0)
    , m_ISPDebugDumpRegs(0)
    , m_hasIspSecMeta(false)
{
    DPLOGI("DpCommandRecorder: create DpCommandRecorder\n");

    memset(m_blockOffset, -1, sizeof(m_blockOffset));
    memset(m_blockSize, 0, sizeof(m_blockSize));
    memset(m_tileOffset, -1, sizeof(m_tileOffset));
    memset(m_tileSize, 0, sizeof(m_tileSize));

    memset(m_frontBlockOffset, -1, sizeof(m_frontBlockOffset));
    memset(m_frontBlockSize, 0, sizeof(m_frontBlockSize));

    memset(m_labelIndex, -1, sizeof(m_labelIndex));
    memset(m_frontLabelIndex, -1, sizeof(m_frontLabelIndex));

    memset(m_secureInfo, 0x0, sizeof(m_secureInfo));
    memset(m_secureAddrMD, 0x0, sizeof(m_secureAddrMD));
#ifdef CMDQ_V3
    memset(&m_ispMeta, 0x0, sizeof(m_ispMeta));
#endif

    memset(m_srcMemAddr, 0x0, sizeof(m_srcMemAddr));
    memset(m_srcMemSize, 0x0, sizeof(m_srcMemSize));
    memset(m_dstFormat, 0x0, sizeof(m_dstFormat));
    memset(m_dstWidth, 0x0, sizeof(m_dstWidth));
    memset(m_dstHeight, 0x0, sizeof(m_dstHeight));
    memset(m_dstYPitch, 0x0, sizeof(m_dstYPitch));
    memset(m_dstUVPitch, 0x0, sizeof(m_dstUVPitch));
    memset(m_dstMemAddr, 0x0, sizeof(m_dstMemAddr));
    memset(m_dstMemSize, 0x0, sizeof(m_dstMemSize));
    memset(m_dstSecMode, 0x0, sizeof(m_dstSecMode));

    memset(m_readbackRegs, 0, sizeof(m_readbackRegs));
    memset(m_readbackValues, 0, sizeof(m_readbackValues));
    memset(m_frameInfoToCMDQ, 0, sizeof(m_frameInfoToCMDQ));
    memset(&m_mdp_pmqos, 0, sizeof(m_mdp_pmqos));
}


DpCommandRecorder::~DpCommandRecorder()
{
    DPLOGI("DpCommandRecorder: destroy DpCommandRecorder\n");

    free(m_pBackBuffer);
    m_pBackBuffer = NULL;

    free(m_pFrontBuffer);
    m_pFrontBuffer = NULL;

    free(m_pExtBuffer);
    m_pExtBuffer = NULL;

    free(m_pBackLabels);
    m_pBackLabels = NULL;

    free(m_pFrontLabels);
    m_pFrontLabels = NULL;
}


uint32_t DpCommandRecorder::getScenario()
{
    DP_STATUS_ENUM   status;
    STREAM_TYPE_ENUM type;

    status = m_pPath->getScenario(&type);
#if !CONFIG_FOR_VERIFY_FPGA
    assert(DP_STATUS_RETURN_SUCCESS == status);
#endif

    return (uint32_t)type;
}


bool DpCommandRecorder::getPQReadback()
{
    DP_STATUS_ENUM status;
    bool           readback;

    status = m_pPath->getPQReadback(&readback);
#if !CONFIG_FOR_VERIFY_FPGA
    assert(DP_STATUS_RETURN_SUCCESS == status);
#endif

    return readback;
}


bool DpCommandRecorder::getHDRReadback()
{
    DP_STATUS_ENUM status;
    bool           readback;

    status = m_pPath->getHDRReadback(&readback);
#if !CONFIG_FOR_VERIFY_FPGA
    assert(DP_STATUS_RETURN_SUCCESS == status);
#endif

    return readback;
}

int32_t DpCommandRecorder::getDREReadback()
{
    DP_STATUS_ENUM status;
    int32_t        readback;

    status = m_pPath->getDREReadback(&readback);
#if !CONFIG_FOR_VERIFY_FPGA
    assert(DP_STATUS_RETURN_SUCCESS == status);
#endif

    return readback;
}

uint32_t DpCommandRecorder::getPriority()
{
    DP_STATUS_ENUM status;
    int32_t        priority;

    status = m_pPath->getPriority(&priority);
#if !CONFIG_FOR_VERIFY_FPGA
    assert(DP_STATUS_RETURN_SUCCESS == status);
#endif

    return priority;
}


uint32_t DpCommandRecorder::getEngineFlag()
{
    DP_STATUS_ENUM   status;
    STREAM_TYPE_ENUM type;
    int32_t          flag = 0;

    status = m_pPath->getPathFlag(&flag);
#if !CONFIG_FOR_VERIFY_FPGA
    assert(DP_STATUS_RETURN_SUCCESS == status);
#endif

    status = m_pPath->getScenario(&type);
#if !CONFIG_FOR_VERIFY_FPGA
    assert(DP_STATUS_RETURN_SUCCESS == status);
#endif

#if 0
    if (STREAM_FRAG_JPEGDEC == type) // embedded HW JPEG DEC mode for FragStream
    {
        flag |= tJPEGDEC;
    }
#endif

    return flag;
}


void* DpCommandRecorder::getBlockBaseSW()
{
    return m_pFrontBuffer;
}


uint32_t DpCommandRecorder::getBlockSize()
{
    return m_frontLength;
}


void* DpCommandRecorder::getSecureAddrMD()
{
    DPLOGI("m_secureAddrMD addr: %p\n", m_secureAddrMD);
    return m_secureAddrMD;
}


uint32_t DpCommandRecorder::getSecureAddrCount()
{
    DPLOGI("m_addrListLength: %d\n", m_addrListLength);
    return m_addrListLength;
}

uint32_t DpCommandRecorder::getSecurePortFlag()
{
    uint32_t rtn = 0;
    for (int i = 0; i < MAX_SECURE_INFO_COUNT; i++)
    {
        if (m_secureInfo[i].securePortFlag == 0)
            break;
        rtn |= m_secureInfo[i].securePortFlag;
    }
    DPLOGI("DpCommandRecorder::getSecurePortFlag%x", rtn);
    return rtn;
}

void DpCommandRecorder::setSecureMode(DpEngineType type,
                                      uint32_t     flag,
                                      uint32_t     secureRegAddr[3],
                                      DpSecure     secMode,
                                      uint32_t     handle,
                                      uint32_t     offset[3],
                                      uint32_t     memSize[3],
                                      uint32_t     planeOffset[3])
{
    int i;
    if(m_secureInfoCount >= MAX_SECURE_INFO_COUNT)
    {
        DPLOGE("Secure Info overflow, current count: %d", m_secureInfoCount);
        return;
    }

    m_secureMode = secMode;

    DPLOGI("Secure mode %d Engine %d Flag 0x%08x\n", secMode, type, flag);
    DPLOGI("Hand 0x%08x\n", handle);
    DPLOGI("Addr 0x%08x 0x%08x 0x%08x\n", secureRegAddr[0], secureRegAddr[1], secureRegAddr[2]);
    DPLOGI("Offs 0x%08x 0x%08x 0x%08x\n", offset[0], offset[1], offset[2]);
    DPLOGI("Size 0x%08x 0x%08x 0x%08x\n", memSize[0], memSize[1], memSize[2]);
    DPLOGI("POff 0x%08x 0x%08x 0x%08x\n", planeOffset[0], planeOffset[1], planeOffset[2]);

    if (DP_SECURE_NONE != secMode)
    {
        for (i = 0; i < m_secureInfoCount; i++)
        {
             if(m_secureInfo[i].secureRegAddr[0] == secureRegAddr[0])
                 break;
        }

        m_secureInfo[i].securePortFlag = flag;
        m_secureInfo[i].secureMode = secMode;

        m_secureInfo[i].secureRegAddr[0] = secureRegAddr[0];
        m_secureInfo[i].secureRegAddr[1] = secureRegAddr[1];
        m_secureInfo[i].secureRegAddr[2] = secureRegAddr[2];

        m_secureInfo[i].secureHandle[0] = handle;
        m_secureInfo[i].secureHandle[1] = handle;
        m_secureInfo[i].secureHandle[2] = handle;

        m_secureInfo[i].secureOffsetList[0] = offset[0];
        m_secureInfo[i].secureOffsetList[1] = offset[1];
        m_secureInfo[i].secureOffsetList[2] = offset[2];

        m_secureInfo[i].secureBlockOffsetList[0] = planeOffset[0];
        m_secureInfo[i].secureBlockOffsetList[1] = planeOffset[1];
        m_secureInfo[i].secureBlockOffsetList[2] = planeOffset[2];

        m_secureInfo[i].secureSizeList[0] = memSize[0];
        m_secureInfo[i].secureSizeList[1] = memSize[1];
        m_secureInfo[i].secureSizeList[2] = memSize[2];

        m_secureInfo[i].securePortList[0] = convertPort(type, 0);
        m_secureInfo[i].securePortList[1] = convertPort(type, 1);
        m_secureInfo[i].securePortList[2] = convertPort(type, 2);

        if (i >= m_secureInfoCount)
        m_secureInfoCount++;
    }
}


void DpCommandRecorder::setSecureMetaData(uint32_t regAddr, uint32_t memAddr)
{
    int32_t index, sub_index;

    if (DP_SECURE_NONE != m_secureMode)
    {
        if (0 != memAddr)
        {
            for (index = 0; index < m_secureInfoCount; index++)
            {
                if (regAddr == m_secureInfo[index].secureRegAddr[0])
                {
                    sub_index = 0;
                    break;
                }
                else if (regAddr == m_secureInfo[index].secureRegAddr[1])
            {
                    sub_index = 1;
                    break;
                }
                else if(regAddr == m_secureInfo[index].secureRegAddr[2])
                {
                    sub_index = 2;
                    break;
                }
            }

            if (index >= m_secureInfoCount)
            {
                return;
            }
            if(m_addrListLength >= 30)
            {
                DPLOGE("secureAddr list overflow! addr %08x, index %d", m_secureInfo[index].secureRegAddr[sub_index],  ((unsigned long)m_pCurCommand - (unsigned long)m_pBackBuffer) >> 3);
                return;
            }

            m_secureAddrMD[m_addrListLength].baseHandle = m_secureInfo[index].secureHandle[sub_index];
            m_secureAddrMD[m_addrListLength].instrIndex = ((unsigned long)m_pCurCommand - (unsigned long)m_pBackBuffer) >> 3;
            m_secureAddrMD[m_addrListLength].offset = m_secureInfo[index].secureOffsetList[sub_index];
            m_secureAddrMD[m_addrListLength].blockOffset = m_secureInfo[index].secureBlockOffsetList[sub_index];
#ifdef CMDQ_V3
            if(m_secureInfo[index].secureMode == DP_SECURE_PROTECTED)
                m_secureAddrMD[m_addrListLength].type = CMDQ_SAM_PH_2_MVA;
            else
#endif
                m_secureAddrMD[m_addrListLength].type = CMDQ_SAM_H_2_MVA;
            m_secureAddrMD[m_addrListLength].size = m_secureInfo[index].secureSizeList[sub_index];
            m_secureAddrMD[m_addrListLength].port = m_secureInfo[index].securePortList[sub_index];

            DPLOGI("index %d  m_secureAddrMD[%d] addr %08x, index %d  handle %016llx  offset %08x  size %08x  port %d type %d\n",
                index, m_addrListLength,
                m_secureInfo[index].secureRegAddr[sub_index],
                m_secureAddrMD[m_addrListLength].instrIndex,
                m_secureAddrMD[m_addrListLength].baseHandle,
                m_secureAddrMD[m_addrListLength].offset,
                m_secureAddrMD[m_addrListLength].size,
                m_secureAddrMD[m_addrListLength].port,
                m_secureAddrMD[m_addrListLength].type);

            m_addrListLength++;
        }
    }
}

void DpCommandRecorder::setFrameSrcInfo(DpColorFormat format,
                                        int32_t       width,
                                        int32_t       height,
                                        int32_t       YPitch,
                                        int32_t       UVPitch,
                                        uint32_t      memAddr[3],
                                        uint32_t      memSize[3],
                                        DpSecure      secMode)
{
    m_srcFormat = format;
    m_srcWidth = width;
    m_srcHeight = height;
    m_srcYPitch = YPitch;
    m_srcUVPitch = UVPitch;
    m_srcMemAddr[0] = memAddr[0];
    m_srcMemAddr[1] = memAddr[1];
    m_srcMemAddr[2] = memAddr[2];
    m_srcMemSize[0] = memSize[0];
    m_srcMemSize[1] = memSize[1];
    m_srcMemSize[2] = memSize[2];
    m_srcSecMode = secMode;
}

void DpCommandRecorder::setFrameDstInfo(int32_t       portIndex,
                                        DpColorFormat format,
                                        int32_t       width,
                                        int32_t       height,
                                        int32_t       YPitch,
                                        int32_t       UVPitch,
                                        uint32_t      memAddr[3],
                                        uint32_t      memSize[3],
                                        DpSecure      secMode)
{
    m_dstFormat[portIndex] = format;
    m_dstWidth[portIndex] = width;
    m_dstHeight[portIndex] = height;
    m_dstYPitch[portIndex] = YPitch;
    m_dstUVPitch[portIndex] = UVPitch;
    m_dstMemAddr[portIndex][0] = memAddr[0];
    m_dstMemAddr[portIndex][1] = memAddr[1];
    m_dstMemAddr[portIndex][2] = memAddr[2];
    m_dstMemSize[portIndex][0] = memSize[0];
    m_dstMemSize[portIndex][1] = memSize[1];
    m_dstMemSize[portIndex][2] = memSize[2];
    m_dstSecMode[portIndex] = secMode;
}

DpFrameInfo DpCommandRecorder::getFrameInfo()
{
    DpFrameInfo frameInfo;

    frameInfo.m_srcFormat = m_srcFormat;
    frameInfo.m_srcWidth = m_srcWidth;
    frameInfo.m_srcHeight = m_srcHeight;
    frameInfo.m_srcYPitch = m_srcYPitch;
    frameInfo.m_srcUVPitch = m_srcUVPitch;
    frameInfo.m_srcMemAddr[0] = m_srcMemAddr[0];
    frameInfo.m_srcMemAddr[1] = m_srcMemAddr[1];
    frameInfo.m_srcMemAddr[2] = m_srcMemAddr[2];
    frameInfo.m_srcMemSize[0] = m_srcMemSize[0];
    frameInfo.m_srcMemSize[1] = m_srcMemSize[1];
    frameInfo.m_srcMemSize[2] = m_srcMemSize[2];
    frameInfo.m_srcSecMode = m_srcSecMode;

    for (int index = 0 ; index < ISP_MAX_OUTPUT_PORT_NUM ; index++)
    {
        frameInfo.m_dstFormat[index] = m_dstFormat[index];
        frameInfo.m_dstWidth[index] = m_dstWidth[index];
        frameInfo.m_dstHeight[index] = m_dstHeight[index];
        frameInfo.m_dstYPitch[index] = m_dstYPitch[index];
        frameInfo.m_dstUVPitch[index] = m_dstUVPitch[index];
        frameInfo.m_dstMemAddr[index][0] = m_dstMemAddr[index][0];
        frameInfo.m_dstMemAddr[index][1] = m_dstMemAddr[index][1];
        frameInfo.m_dstMemAddr[index][2] = m_dstMemAddr[index][2];
        frameInfo.m_dstMemSize[index][0] = m_dstMemSize[index][0];
        frameInfo.m_dstMemSize[index][1] = m_dstMemSize[index][1];
        frameInfo.m_dstMemSize[index][2] = m_dstMemSize[index][2];
        frameInfo.m_dstSecMode[index] = m_dstSecMode[index];
    }

    return frameInfo;
}

void DpCommandRecorder::setRegDstNum(int32_t regNum)
{
    m_regDstNum = regNum;
}

int32_t DpCommandRecorder::getRegDstNum()
{
    return m_regDstNum;
}


void DpCommandRecorder::markRecord(BlockType type)
{
    switch (m_blockType)
    {
        case FRAME_BLOCK:
        case TILE_BLOCK:
            m_tileSize[m_tileID]     = m_backLength - m_tileOffset[m_tileID];
            m_blockSize[m_blockType] = m_backLength - m_blockOffset[m_blockType];
            break;
        case EXT_FRAME_BLOCK:
            m_blockSize[m_blockType] = m_extLength - m_blockOffset[m_blockType];
            break;
        default:
            break;
    }

    if (type != m_blockType)
    {
        switch (type)
        {
            case FRAME_BLOCK:
                //m_tileID = 0;
                m_tileOffset[0] = m_backLength;
                m_tileSize[0]   = 0;
                /* fall through */
            case TILE_BLOCK:
                m_blockOffset[type] = m_backLength; // Store current offset
                m_blockSize[type]   = 0;
                break;
            case EXT_FRAME_BLOCK:
                // For additional frame setting at last
                if (NULL == m_pExtBuffer)
                {
                    m_extBufferSize = BASE_COMMAND_BLOCK_SIZE;
                    m_pExtBuffer    = (uint32_t*)malloc(m_extBufferSize);
                }

                m_pCurCommand = m_pExtBuffer;
                m_extLength   = 0;

                m_blockOffset[type] = 0;
                m_blockSize[type]   = 0;
                assert(0);
                break;
            default:
                break;
        }

        m_blockType = type;
        //appendCommand(CMDQ_OP_JUMP, 0, 8);
    }
}


void DpCommandRecorder::markRecord(BlockType type, uint32_t x, uint32_t y)
{
    if (TILE_BLOCK == type)
    {
        uint32_t preTileID = m_tileID;
        //DPLOGD("DpCommandRecorder: %d, %#x\n", preTileID, m_tileOffset[preTileID]);

        m_tileID = toTileID(x, y);
        assert(m_tileID < MAX_TILE_NUM);

        m_tileSize[preTileID]  = m_backLength - m_tileOffset[preTileID];
        m_tileOffset[m_tileID] = m_backLength;
        m_tileSize[m_tileID]   = 0;

        m_maxX = MAX(m_maxX, x);
        m_maxY = MAX(m_maxY, y);
    }
}


void DpCommandRecorder::reorder()
{
    uint32_t i, j;
    uint32_t *tempPtr = NULL;

    if (m_dumpOrder)
    {
        // use front buffer as temp buffer
        m_frontBufferSize = m_backBufferSize;
        m_pFrontBuffer    = (uint32_t*)realloc(m_pFrontBuffer, m_backBufferSize);

        m_frontLength = 0;
        tempPtr       = m_pFrontBuffer;

        // copy frame setting
        //DPLOGD("frame: %#x, %#x\n", m_tileOffset[0], m_tileSize[0]);
        memcpy(tempPtr, (uint8_t*)m_pBackBuffer + m_tileOffset[0], m_tileSize[0]);
        m_frontLength += m_tileSize[0];
        tempPtr = (uint32_t*)((uint8_t*)m_pFrontBuffer + m_frontLength);

        DPLOGD("Only apply in JPEG direclink mode m_dumpOrder: %#x\n", m_dumpOrder);
    }

    if ((m_dumpOrder & (TILE_ORDER_Y_FIRST | TILE_ORDER_RIGHT_TO_LEFT)) == (TILE_ORDER_Y_FIRST | TILE_ORDER_RIGHT_TO_LEFT)) // 270
    {
        for (i = m_maxX; i >= 0; i--)
        {
            for (j = 0; j <= m_maxY; j++)
            {
                tempPtr = copyTile(tempPtr, i, j);
            }
        }
    }
    else if ((m_dumpOrder & (TILE_ORDER_BOTTOM_TO_TOP | TILE_ORDER_RIGHT_TO_LEFT)) == (TILE_ORDER_BOTTOM_TO_TOP | TILE_ORDER_RIGHT_TO_LEFT)) // 180
    {
        for (j = 0; j <= m_maxY; j++)
        {
            for (i = m_maxX; i >= 0 ; i--)
            {
                tempPtr = copyTile(tempPtr, i, j);
            }
        }
    }
    else if ((m_dumpOrder & (TILE_ORDER_Y_FIRST | TILE_ORDER_BOTTOM_TO_TOP)) == (TILE_ORDER_Y_FIRST | TILE_ORDER_BOTTOM_TO_TOP)) // 90
    {
        for (i = 0; i <= m_maxX; i++)
        {
            for (j = 0; j <= m_maxY; j++)
            {
                tempPtr = copyTile(tempPtr, i, j);
            }
        }
    }
    else if ((m_dumpOrder & (TILE_ORDER_RIGHT_TO_LEFT)) == (TILE_ORDER_RIGHT_TO_LEFT)) // Left to Right
    {
        for (j = 0; j <= m_maxY; j++)
        {
            for (i = m_maxX; i >= 0; i--)
            {
                tempPtr = copyTile(tempPtr, i, j);
            }
        }
    }
    else
    {
        // do nothing
    }

    if (m_dumpOrder)
    {
        m_pCurCommand = tempPtr;

        // swap buffer
        tempPtr        = m_pBackBuffer;
        m_pBackBuffer  = m_pFrontBuffer;
        m_pFrontBuffer = tempPtr;

        m_backLength  = m_frontLength;
        m_frontLength = 0;
    }
}


uint32_t *DpCommandRecorder::copyTile(uint32_t *dst, uint32_t x, uint32_t y)
{
    uint32_t i;
    uint32_t indexTile;

    indexTile = toTileID(x, y);
    //DPLOGD("x:%d y:%d, %x, %d, %d\n", x, y, indexTile, m_tileOffset[indexTile], m_tileSize[indexTile]);

    for (i = 0; i < m_backLabelCount; i++)
    {
        if (m_pBackLabels[i].type == TILE_BLOCK && m_pBackLabels[i].tileID == indexTile)
        {
            m_pBackLabels[i].offset += m_frontLength - m_tileOffset[indexTile];
        }
    }

    memcpy(dst, (uint8_t*)m_pBackBuffer + m_tileOffset[indexTile], m_tileSize[indexTile]);
    m_frontLength += m_tileSize[indexTile];
    return (uint32_t*)((uint8_t*)m_pFrontBuffer + m_frontLength);
}


void DpCommandRecorder::dupRecord(BlockType type)
{
    // Ring buffer mode would have this to duplicate the frame setting
    if (m_frontBlockOffset[type] < 0)
    {
        assert(0);
        return;
    }

    switch (m_blockType)
    {
        case FRAME_BLOCK:
        case TILE_BLOCK:
            m_tileSize[m_tileID]     = m_backLength - m_tileOffset[m_tileID];
            m_blockSize[m_blockType] = m_backLength - m_blockOffset[m_blockType];
            break;
        case EXT_FRAME_BLOCK:
            m_blockSize[m_blockType] = m_extLength - m_blockOffset[m_blockType];
            break;
        default:
            break;
    }

    if ((m_backLength + m_frontBlockSize[type]) >= m_backBufferSize)
    {
        m_backBufferSize += ((m_frontBlockSize[type] / UNIT_COMMAND_BLOCK_SIZE) + 1) * UNIT_COMMAND_BLOCK_SIZE;
        m_pBackBuffer = (uint32_t*)realloc(m_pBackBuffer, m_backBufferSize);

        m_pCurCommand = (uint32_t*)((uint8_t*)m_pBackBuffer + m_backLength);
    }

    switch (type)
    {
        case FRAME_BLOCK:
            //m_tileID = 0;
            m_tileOffset[0] = m_backLength;
            m_tileSize[0]   = m_frontBlockSize[type];
            /* fall through */
        case TILE_BLOCK:
            m_blockOffset[type] = m_backLength;
            m_blockSize[type]   = m_frontBlockSize[type];

            // copy from front
            memcpy(m_pCurCommand, (uint8_t*)m_pFrontBuffer + m_frontBlockOffset[type], m_frontBlockSize[type]);
            m_backLength += m_frontBlockSize[type];
            m_pCurCommand = (uint32_t*)((uint8_t*)m_pBackBuffer + m_backLength);

            for (uint32_t index = 0; index < m_frontLabelCount; index++)
            {
                if (m_pFrontLabels[index].type == type)
                {
                    dupLabel(index);
                }
            }
            break;
        default:
            assert(0);
            break;
    }

    m_blockType = type;
}


void DpCommandRecorder::initRecord()
{
    resetRecord();

    memset(m_blockOffset, -1, sizeof(m_blockOffset));
    memset(m_blockSize, 0, sizeof(m_blockSize));
    memset(m_tileOffset, -1, sizeof(m_tileOffset));
    memset(m_tileSize, 0, sizeof(m_tileSize));

    memset(m_secureInfo, 0x0, sizeof(m_secureInfo));
    memset(m_secureAddrMD, 0x0, sizeof(m_secureAddrMD));
#ifdef CMDQ_V3
    memset(&m_ispMeta, 0x0, sizeof(m_ispMeta));
#endif
}


void DpCommandRecorder::stopRecord()
{
    markRecord(NONE_BLOCK);
    appendCommand(CMDQ_OP_EOC, 0, 1);
}


void DpCommandRecorder::swapRecord()
{
    // swap buffer
    uint32_t *tempPtr = m_pFrontBuffer;
    uint32_t tempSize = m_frontBufferSize;

    m_pFrontBuffer    = m_pBackBuffer;
    m_frontBufferSize = m_backBufferSize;

    m_pBackBuffer     = tempPtr;
    m_backBufferSize  = tempSize;

    m_frontLength = m_backLength;
    m_backLength  = 0;

    memcpy(m_frontBlockOffset, m_blockOffset, sizeof(m_frontBlockOffset));
    memcpy(m_frontBlockSize, m_blockSize, sizeof(m_frontBlockSize));

    // swap label
    LabelInfo *temp = m_pFrontLabels;
    m_pFrontLabels  = m_pBackLabels;
    m_pBackLabels   = temp;

    m_frontLabelCount = m_backLabelCount;
    m_backLabelCount  = 0;

    if (m_frontLabelCount > 0) // copy on used
    {
        memcpy(m_frontLabelIndex, m_labelIndex, sizeof(m_frontLabelIndex));
    }

    //flushRecord();
}


void DpCommandRecorder::resetRecord()
{
    m_blockType = NONE_BLOCK;
    m_tileID = 0;

    m_addrListLength = 0;

    if (NULL == m_pBackBuffer)
    {
        m_backBufferSize = BASE_COMMAND_BLOCK_SIZE;
        m_pBackBuffer    = (uint32_t*)malloc(m_backBufferSize);
    }

    m_nextLabel = 0;

    m_maxX = 0;
    m_maxY = 0;

    m_pCurCommand = m_pBackBuffer;
    m_backLength  = 0;

    m_secureInfoCount = 0;
    m_hasIspSecMeta = false;

#ifdef CONFIG_FOR_SOURCE_PQ
    if (STREAM_COLOR_BITBLT == getScenario())
    {
        DPLOGI("DpCommandRecorder: COLOR_BITBLT reset Record !!!!\n");
        memset(m_blockSize, 0, sizeof(m_blockSize));
    }
#endif
}

#define CMDQ_GET_ARG_B(arg)         (((arg) & 0xffff0000) >> 16)
#define CMDQ_GET_ARG_C(arg)         ((arg) & 0xffff)
#define CMDQ_IMMEDIATE_VALUE        0
#define CMDQ_REG_TYPE               1
#define CMDQ_GET_ADDR_HIGH(addr)    ((uint32_t)((addr >> 16) & 0xffffffff))
#define CMDQ_ADDR_LOW_BIT           (0x2)
#define CMDQ_GET_ADDR_LOW(addr)     ((uint16_t)(addr & 0xffff) | CMDQ_ADDR_LOW_BIT)
#define CMDQ_SPR_FOR_TEMP           (0)

void DpCommandRecorder::encodeCommand(uint16_t arg_c, uint16_t arg_b,
    uint16_t arg_a, uint8_t s_op, uint8_t arg_c_type, uint8_t arg_b_type, uint8_t arg_a_type, uint8_t op)
{
    struct cmdq_instruction *cmdq_inst = (struct cmdq_instruction *)m_pCurCommand;

    cmdq_inst->op = op;
    cmdq_inst->arg_a_type = arg_a_type;
    cmdq_inst->arg_b_type = arg_b_type;
    cmdq_inst->arg_c_type = arg_c_type;
    cmdq_inst->s_op = s_op;
    cmdq_inst->arg_a = arg_a;
    cmdq_inst->arg_b = arg_b;
    cmdq_inst->arg_c = arg_c;

    m_pCurCommand += 2;
}

void DpCommandRecorder::recReadReg(uint8_t subsys, uint16_t offset,
    uint16_t dst_reg_idx)
{
    encodeCommand(0, offset, dst_reg_idx, subsys,
        CMDQ_IMMEDIATE_VALUE, CMDQ_IMMEDIATE_VALUE, CMDQ_REG_TYPE,
        CMDQ_OP_READ_S);
}

void DpCommandRecorder::recReadAddr(uint64_t addr, uint16_t dst_reg_idx)
{
    const uint16_t src_reg_idx = CMDQ_SPR_FOR_TEMP;

    recAssign(src_reg_idx, CMDQ_GET_ADDR_HIGH(addr));
    encodeCommand(0, CMDQ_GET_ADDR_LOW(addr), dst_reg_idx, src_reg_idx,
        CMDQ_IMMEDIATE_VALUE, CMDQ_IMMEDIATE_VALUE, CMDQ_REG_TYPE,
        CMDQ_OP_READ_S);
}

void DpCommandRecorder::recWriteReg(uint8_t subsys,
    uint16_t offset, uint16_t src_reg_idx, bool mask)
{
    encodeCommand(0, src_reg_idx, offset, subsys,
        CMDQ_IMMEDIATE_VALUE, CMDQ_REG_TYPE, CMDQ_IMMEDIATE_VALUE,
        mask ? CMDQ_OP_WRITE_S_W_MASK : CMDQ_OP_WRITE_S);
}

void DpCommandRecorder::recWriteRegAddr(uint64_t addr,
    uint16_t src_reg_idx, bool mask)
{
    const uint16_t dst_reg_idx = CMDQ_SPR_FOR_TEMP;

    recAssign(dst_reg_idx, CMDQ_GET_ADDR_HIGH(addr));
    recStoreValueReg(dst_reg_idx, CMDQ_GET_ADDR_LOW(addr), src_reg_idx, mask);
}

void DpCommandRecorder::recWriteValueAddr(uint64_t addr,
    uint32_t value, bool mask)
{
    const uint16_t dst_reg_idx = CMDQ_SPR_FOR_TEMP;

    recAssign(dst_reg_idx, CMDQ_GET_ADDR_HIGH(addr));
    recStoreValue(dst_reg_idx, CMDQ_GET_ADDR_LOW(addr), value, mask);
}

void DpCommandRecorder::recStoreValue(uint16_t indirect_dst_reg_idx,
    uint16_t dst_addr_low, uint32_t value, bool mask)
{
    encodeCommand(CMDQ_GET_ARG_C(value),
        CMDQ_GET_ARG_B(value), dst_addr_low, indirect_dst_reg_idx,
        CMDQ_IMMEDIATE_VALUE, CMDQ_IMMEDIATE_VALUE, CMDQ_IMMEDIATE_VALUE,
        mask ? CMDQ_OP_WRITE_S_W_MASK : CMDQ_OP_WRITE_S);
}

void DpCommandRecorder::recStoreValueReg(uint16_t indirect_dst_reg_idx,
    uint16_t dst_addr_low, uint16_t indirect_src_reg_idx, bool mask)
{
    return encodeCommand(0, dst_addr_low, indirect_dst_reg_idx, indirect_src_reg_idx,
        CMDQ_IMMEDIATE_VALUE, CMDQ_IMMEDIATE_VALUE, CMDQ_REG_TYPE,
        mask ? CMDQ_OP_WRITE_S_W_MASK : CMDQ_OP_WRITE_S);
}

void DpCommandRecorder::recAssign(uint16_t reg_idx, uint32_t value)
{
    encodeCommand(CMDQ_GET_ARG_C(value), CMDQ_GET_ARG_B(value), reg_idx,
        CMDQ_LOGIC_ASSIGN,
        CMDQ_IMMEDIATE_VALUE, CMDQ_IMMEDIATE_VALUE, CMDQ_REG_TYPE,
        CMDQ_OP_LOGIC);
}

void DpCommandRecorder::appendCommand(COMMAND_TYPE_ENUM code, uint32_t argA, uint32_t argB)
{
    int32_t subsys, subsysB;

    if ((m_pLastCommand == NULL) &&
        (m_backLength + UNIT_COMMAND_BLOCK_SIZE) >= m_backBufferSize)
    {
        m_backBufferSize += UNIT_COMMAND_BLOCK_SIZE;
        m_pBackBuffer = (uint32_t*)realloc(m_pBackBuffer, m_backBufferSize);

        m_pCurCommand = (uint32_t*)((uint8_t*)m_pBackBuffer + m_backLength);
    }

    switch (code)
    {
        case CMDQ_OP_MOVE:
            *m_pCurCommand++ = argB;
            *m_pCurCommand++ = CMDQ_OP_MOVE << 24;
            break;
        case CMDQ_OP_READ:
            subsys = get_cmdq_subsys(argA);
            if (subsys == CMDQ_SUBSYS_UNDEFINED)
            {
#ifdef MDP_ENABLE_SPR
                recReadAddr(argA, GCE_SPR_DATA);
#else
                //appendCommand(CMDQ_OP_WFE, 0, DpCommand::SYNC_TOKEN_GPR_READ);
                *m_pCurCommand++ = (1 << 31) | (1 << 15) | 1;
                *m_pCurCommand++ = (CMDQ_OP_WFE << 24) | DpCommand::SYNC_TOKEN_GPR_WRITE;

                *m_pCurCommand++ = argA & ~0x1;
                *m_pCurCommand++ = (CMDQ_OP_MOVE << 24) | (0 & 0xffff) | ((GPR_WRITE & 0x1f) << 16) | (4 << 21); // 1 0 0

                *m_pCurCommand++ = GPR_READ;
                *m_pCurCommand++ = (CMDQ_OP_READ << 24) | (argA & 0x1) | ((GPR_WRITE & 0x1f) << 16) | (6 << 21); // 11 0

                //appendCommand(CMDQ_OP_SET_TOKEN, 0, DpCommand::SYNC_TOKEN_GPR_READ);
                *m_pCurCommand++ = (1 << 31) | (1 << 16);
                *m_pCurCommand++ = (CMDQ_OP_WFE << 24) | DpCommand::SYNC_TOKEN_GPR_WRITE;
#endif // MDP_ENABLE_SPR
            }
            else
            {
#ifdef MDP_ENABLE_SPR
                recReadReg(subsys, argA & 0xffff, GCE_SPR_DATA);
#else
                *m_pCurCommand++ = GPR_READ;
                *m_pCurCommand++ = (CMDQ_OP_READ << 24) | (argA & 0xffff) | ((subsys & 0x1f) << 16) | (2 << 21); // 0 1 0
#endif // MDP_ENABLE_SPR
            }
#ifdef MDP_ENABLE_SPR
            recWriteRegAddr(argB, GCE_SPR_DATA, false);
#else
            *m_pCurCommand++ = argB;
            *m_pCurCommand++ = (CMDQ_OP_MOVE << 24) | (0 & 0xffff) | ((GPR_READ_DST & 0x1f) << 16) | (4 << 21); // 1 0 0

            *m_pCurCommand++ = GPR_READ;
            *m_pCurCommand++ = (CMDQ_OP_WRITE << 24) | (0 & 0xffff) | ((GPR_READ_DST & 0x1f) << 16) | (6 << 21); // 1 1 0
#endif // MDP_ENABLE_SPR
            break;
        case CMDQ_OP_WRITE:
            setSecureMetaData(argA, argB);

            subsys = get_cmdq_subsys(argA);

            if (subsys == CMDQ_SUBSYS_UNDEFINED)
            {
#ifdef MDP_ENABLE_SPR
                recWriteValueAddr(argA & CMDQ_ADDR_MASK, argB, bool(argA & 0x1));
#else
                //appendCommand(CMDQ_OP_WFE, 0, DpCommand::SYNC_TOKEN_GPR_WRITE);
                *m_pCurCommand++ = (1 << 31) | (1 << 15) | 1;
                *m_pCurCommand++ = (CMDQ_OP_WFE << 24) | DpCommand::SYNC_TOKEN_GPR_WRITE;

                *m_pCurCommand++ = argA & ~0x1;
                *m_pCurCommand++ = (CMDQ_OP_MOVE << 24) | (0 & 0xffff) | ((GPR_WRITE & 0x1f) << 16) | (4 << 21); // 1 0 0

                *m_pCurCommand++ = argB;
                *m_pCurCommand++ = (CMDQ_OP_WRITE << 24) | (argA & 0x1) | ((GPR_WRITE & 0x1f) << 16) | (4 << 21); // 1 0 0

                //appendCommand(CMDQ_OP_SET_TOKEN, 0, DpCommand::SYNC_TOKEN_GPR_WRITE);
                *m_pCurCommand++ = (1 << 31) | (1 << 16);
                *m_pCurCommand++ = (CMDQ_OP_WFE << 24) | DpCommand::SYNC_TOKEN_GPR_WRITE;
#endif // MDP_ENABLE_SPR
            }
            else
            {
                *m_pCurCommand++ = argB;
                *m_pCurCommand++ = (CMDQ_OP_WRITE << 24) | (argA & 0xffff) | ((subsys & 0x1f) << 16);
            }
            break;
        case CMDQ_OP_WRITE_FROM_MEM:
            subsys = get_cmdq_subsys(argA);
#ifdef MDP_ENABLE_SPR
            recReadAddr(argA, GCE_SPR_DATA);
            recWriteReg(subsys, argA & CMDQ_ADDR_MASK_16, GCE_SPR_DATA, bool(argA & 0x1));
#else
            *m_pCurCommand++ = argB;
            *m_pCurCommand++ = (CMDQ_OP_MOVE << 24) | (0 & 0xffff) | ((GPR_WRITE_FROM_MEM_DST & 0x1f) << 16) | (4 << 21); // 1 0 0

            *m_pCurCommand++ = GPR_WRITE_FROM_MEM;
            *m_pCurCommand++ = (CMDQ_OP_READ << 24) | (0 & 0xffff) | ((GPR_WRITE_FROM_MEM_DST & 0x1f) << 16) | (6 << 21); // 1 1 0

            *m_pCurCommand++ = GPR_WRITE_FROM_MEM;
            *m_pCurCommand++ = (CMDQ_OP_WRITE << 24) | (argA & 0xffff) | ((subsys & 0x1f) << 16) | (2 << 21); // 0 1 0
#endif // MDP_ENABLE_SPR
            break;
        case CMDQ_OP_WRITE_FROM_REG:
            subsys = get_cmdq_subsys(argA);
            subsysB = get_cmdq_subsys(argB);

#ifdef MDP_ENABLE_SPR
            recReadReg(subsys, argB & 0xffff, GCE_SPR_DATA);
            recWriteReg(subsys, argA & CMDQ_ADDR_MASK_16, GCE_SPR_DATA, bool(argA & 0x1));
#else
            // read regB value to GPR
            *m_pCurCommand++ = GPR_WRITE_FROM_REG;
            *m_pCurCommand++ = (CMDQ_OP_READ << 24) | (argB & 0xffff) | ((subsysB & 0x1f) << 16) | (2 << 21); // 0 1 0

            // write GPR value to regA
            *m_pCurCommand++ = GPR_WRITE_FROM_REG;
            *m_pCurCommand++ = (CMDQ_OP_WRITE << 24) | (argA & 0xffff) | ((subsys & 0x1f) << 16) | (2 << 21); // 0 1 0
#endif // MDP_ENABLE_SPR
            break;
        case CMDQ_OP_POLL:
            subsys = get_cmdq_subsys(argA);

#if CONFIG_FOR_DEBUG_POLLING
            if (subsys == CMDQ_SUBSYS_UNDEFINED)
            {
                //appendCommand(CMDQ_OP_WFE, 0, DpCommand::SYNC_TOKEN_GPR_POLL);
                *m_pCurCommand++ = (1 << 31) | (1 << 15) | 1;
                *m_pCurCommand++ = (CMDQ_OP_WFE << 24) | DpCommand::SYNC_TOKEN_GPR_POLL;

                *m_pCurCommand++ = argA & ~0x1;
                *m_pCurCommand++ = (CMDQ_OP_MOVE << 24) | (0 & 0xffff) | ((GPR_POLL & 0x1f) << 16) | (4 << 21); // 1 0 0

                *m_pCurCommand++ = argB;
                *m_pCurCommand++ = (CMDQ_OP_POLL << 24) | (argA & 0x1) | ((GPR_POLL & 0x1f) << 16) | (4 << 21); // 1 0 0

                //appendCommand(CMDQ_OP_SET_TOKEN, 0, DpCommand::SYNC_TOKEN_GPR_POLL);
                *m_pCurCommand++ = (1 << 31) | (1 << 16);
                *m_pCurCommand++ = (CMDQ_OP_WFE << 24) | DpCommand::SYNC_TOKEN_GPR_POLL;
            }
            else
#endif
            {
                *m_pCurCommand++ = argB;
                *m_pCurCommand++ = (CMDQ_OP_POLL << 24) | (argA & 0xffff) | ((subsys & 0x1f) << 16);
            }
            break;
        case CMDQ_OP_JUMP:
#ifdef GCE_ADDR_SUPPORT_35BIT
            *m_pCurCommand++ = argB >> 3;
#else
            *m_pCurCommand++ = argB;
#endif
            *m_pCurCommand++ = (CMDQ_OP_JUMP << 24) | (argA & 0xffffff);
            break;
        case CMDQ_OP_WFE:
            // this is actually WFE(SYNC) but with different parameter
            // interpretation
            // bit 15: to_wait, true
            // bit 31: to_update, true
            // bit 16-27: update_value, 0
            *m_pCurCommand++ = (1 << 31) | (1 << 15) | 1;
            *m_pCurCommand++ = (CMDQ_OP_WFE << 24) | argB;
            break;
        case CMDQ_OP_SET_TOKEN:
            // this is actually WFE(SYNC) but with different parameter
            // interpretation
            // bit 15: to_wait, false
            // bit 31: to_update, true
            // bit 16-27: update_value, 1
            *m_pCurCommand++ = (1 << 31) | (1 << 16);
            *m_pCurCommand++ = (CMDQ_OP_WFE << 24) | argB;
            break;
        case CMDQ_OP_CLEAR_TOKEN:
            // this is actually WFE(SYNC) but with different parameter
            // interpretation
            // bit 15: to_wait, false
            // bit 31: to_update, true
            // bit 16-27: update_value, 0
            *m_pCurCommand++ = (1 << 31) | (0 << 16);
            *m_pCurCommand++ = (CMDQ_OP_WFE << 24) | argB;
            break;
        case CMDQ_OP_WAIT_NO_CLEAR:
            // this is actually WFE(SYNC) but with different parameter
            // interpretation
            // bit 15: to_wait, true
            // bit 31: to_update, false
            // bit 16-27: update_value, 0
            *m_pCurCommand++ = (0 << 31) | (1 << 15) | 1;
            *m_pCurCommand++ = (CMDQ_OP_WFE << 24) | argB;
            break;
        case CMDQ_OP_EOC:
            *m_pCurCommand++ = argB;
            *m_pCurCommand++ = (CMDQ_OP_EOC << 24) | (argA & 0xffffff);

            *m_pCurCommand++ = 8;
            *m_pCurCommand++ = (CMDQ_OP_JUMP << 24) | (0 & 0xffffff); //JUMP: NOP
            break;
        default:
            DPLOGE("DpCommandRecorder: unknown command %d\n", code);
            assert(0);
    }

    if (m_pLastCommand == NULL)
    {
        m_backLength = (unsigned long)m_pCurCommand - (unsigned long)m_pBackBuffer;
    }
}


void DpCommandRecorder::beginLabel()
{
    if (m_pCurLabel != NULL)
    {
        return;
    }

    if (m_pLabelCommand != NULL)
    {
        DPLOGE("DpCommandRecorder: unexpected label begin\n");
        return;
    }

    m_pLabelCommand = m_pCurCommand;
}


int32_t DpCommandRecorder::endLabel()
{
    if (m_pCurLabel != NULL)
    {
        return m_pCurLabel->label;
    }

    if (m_pLabelCommand == NULL)
    {
        DPLOGE("DpCommandRecorder: possible double or dummy label end\n");
        return -1;
    }

    LabelInfo *pLabel = addLabel();

    m_pLabelCommand = NULL;

    if (pLabel == NULL)
    {
        DPLOGE("DpCommandRecorder: out of label\n");
        return -1;
    }
    return pLabel->label;
}


DpCommandRecorder::LabelInfo *DpCommandRecorder::addLabel()
{
    int32_t label;
    LabelInfo *pLabel;

    if (NULL == m_pBackLabels)
    {
        m_pBackLabels = (LabelInfo*)malloc(MAX_TILE_NUM * sizeof(LabelInfo));
        m_pFrontLabels = (LabelInfo*)malloc(MAX_TILE_NUM * sizeof(LabelInfo));
    }
    if (m_backLabelCount == 0) // reset on first use
    {
        memset(m_labelIndex, -1, sizeof(m_labelIndex));
        if (m_frontLabelCount == 0)
        {
            memset(m_frontLabelIndex, -1, sizeof(m_frontLabelIndex));
        }
    }

    for (label = m_nextLabel; label < MAX_TILE_NUM; label++)
    {
        if (m_frontLabelIndex[label] == -1)
        {
            break;
        }
    }
    if (label == MAX_TILE_NUM) // out of label
    {
        m_nextLabel = MAX_TILE_NUM;
        return NULL;
    }

    m_nextLabel = label + 1;

    pLabel = &m_pBackLabels[m_backLabelCount];
    pLabel->label  = label;
    pLabel->type   = m_blockType;
    pLabel->tileID = m_tileID;
    pLabel->offset = (unsigned long)m_pLabelCommand - (unsigned long)m_pBackBuffer - m_blockOffset[m_blockType];
    pLabel->length = (unsigned long)m_pCurCommand - (unsigned long)m_pLabelCommand;

    m_labelIndex[label] = m_backLabelCount;
    m_backLabelCount ++;

    return pLabel;
}


DpCommandRecorder::LabelInfo *DpCommandRecorder::findLabel(int32_t label)
{
    if (label < 0 || label >= MAX_TILE_NUM)
    {
        return NULL;
    }

    int32_t index = m_labelIndex[label];
    if (index < 0 || (uint32_t)index >= m_backLabelCount || m_pBackLabels[index].label != label)
    {
        return NULL;
    }

    return &m_pBackLabels[index];
}


void DpCommandRecorder::dupLabel(uint32_t index)
{
    int32_t label = m_pFrontLabels[index].label;

    if (m_backLabelCount == MAX_TILE_NUM) // out of label
    {
        DPLOGE("DpCommandRecorder: out of label\n");
        return;
    }
    if (m_backLabelCount == 0) // reset on first use
    {
        memset(m_labelIndex, -1, sizeof(m_labelIndex));
    }

    m_pBackLabels[m_backLabelCount] = m_pFrontLabels[index];
    m_labelIndex[label] = m_backLabelCount;
    m_backLabelCount ++;
}


void DpCommandRecorder::beginOverwrite(int32_t label)
{
    if (m_pCurLabel != NULL)
    {
        DPLOGE("DpCommandRecorder: unexpected overwrite begin\n");
        return;
    }

    m_pCurLabel = findLabel(label);
    if (m_pCurLabel == NULL)
    {
        DPLOGE("DpCommandRecorder: unexpected overwrite label %d\n", label);
        return;
    }

    m_pLabelCommand = (uint32_t*)((uint8_t*)m_pBackBuffer + m_blockOffset[m_pCurLabel->type] + m_pCurLabel->offset);
    m_pLastCommand = m_pCurCommand;
    m_pCurCommand = m_pLabelCommand;
}


void DpCommandRecorder::endOverwrite()
{
    if (m_pCurLabel == NULL)
    {
        DPLOGE("DpCommandRecorder: possible double or dummy overwrite end\n");
        return;
    }

    if ((unsigned long)m_pCurCommand - (unsigned long)m_pLabelCommand != m_pCurLabel->length)
    {
        DPLOGE("DpCommandRecorder: overwrite command length mismatch\n");
    }

    m_pCurCommand = m_pLastCommand;
    m_pLastCommand = NULL;
    m_pLabelCommand = NULL;

    m_pCurLabel = NULL;
}


void DpCommandRecorder::flushRecord()
{
    //m_blockOffset[TILE_BLOCK] = -1;
}


void DpCommandRecorder::dumpRecord()
{
    FILE *pFile;
#if CONFIG_FOR_OS_ANDROID
    pFile = fopen("/data/command.bin", "wb");
#else
    pFile = fopen("./out/command.bin", "wb");
#endif
    if (NULL != pFile)
    {
        fwrite(m_pFrontBuffer, m_frontLength, 1, pFile);
        fclose(pFile);
    }

    DPLOGD("command block: start %p, size %#x, engine %#010x\n", getBlockBaseSW(), getBlockSize(), getEngineFlag());
    DPLOGD("command block: back %p size %#x, frame %#x %#x, tile %#x %#x\n", m_pBackBuffer, m_backLength,
        m_blockOffset[FRAME_BLOCK], m_blockSize[FRAME_BLOCK], m_blockOffset[TILE_BLOCK], m_blockSize[TILE_BLOCK]);
}


void DpCommandRecorder::dumpRegister(uint64_t pqSessionId)
{
#ifndef BASIC_PACKAGE
#ifdef DEBUG_DUMP_REG
    PQSession* pPQSession = PQSessionManager::getInstance()->getPQSession(pqSessionId);
    DpPqParam param;
    FILE *pFile;
    char name[256] = {0};
    int32_t DumpReg = DpDriver::getInstance()->getEnableDumpRegister();

    if (pPQSession != NULL)
    {
        pPQSession->getPQParam(&param);

        if (param.scenario == MEDIA_ISP_PREVIEW || param.scenario == MEDIA_ISP_CAPTURE)
        {
            if (((DumpReg == DUMP_ISP_PRV && param.scenario == MEDIA_ISP_PREVIEW) ||
                (DumpReg == DUMP_ISP_CAP && param.scenario == MEDIA_ISP_CAPTURE) ||
                (DumpReg == DUMP_ISP_PRV_CAP)) &&
                param.u.isp.timestamp != 0xFFFFFFFF)
                sprintf(name, "/data/vendor/camera_dump/%09d-%04d-%04d-MDP-%s-%d-%s.mdp",
                    param.u.isp.timestamp,
                    param.u.isp.frameNo,
                    param.u.isp.requestNo,
                    (param.scenario == MEDIA_ISP_PREVIEW) ? "Prv" : "CAP",
                    param.u.isp.lensId,
                    param.u.isp.userString);
            else
                return;
        }

#if CONFIG_FOR_OS_ANDROID
        pFile = fopen(name, "ab");

        if (NULL != pFile)
        {
            fwrite(m_pFrontBuffer, m_frontLength, 1, pFile);
            fclose(pFile);
            DPLOGD("Dump register to %s\n", name);
        }
        else
        {
            DPLOGD("Open %s failed\n", name);
        }
#endif
    }
#endif
#endif
}


uint32_t* DpCommandRecorder::getReadbackRegs(uint32_t& numReadRegs)
{
    numReadRegs = m_numReadbackRegs;
    return m_readbackRegs;
}


uint32_t* DpCommandRecorder::getReadbackValues(uint32_t& numValues)
{
    numValues = m_numReadbackRegs;
    return m_readbackValues;
}


uint32_t DpCommandRecorder::getISPDebugDumpRegs()
{
    return m_ISPDebugDumpRegs;
}


char* DpCommandRecorder::getFrameInfoToCMDQ()
{
    return m_frameInfoToCMDQ;
}

bool DpCommandRecorder::getSyncMode()
{
    DP_STATUS_ENUM status;
    bool           syncMode;

    status = m_pPath->getSyncMode(&syncMode);
#if !CONFIG_FOR_VERIFY_FPGA
    assert(DP_STATUS_RETURN_SUCCESS == status);
#endif

    return syncMode;
}

uint32_t* DpCommandRecorder::getReadbackPABuffer(uint32_t& readbackPABufferIndex)
{
    return m_pPath->getReadbackPABuffer(readbackPABufferIndex);
}

DP_STATUS_ENUM DpCommandRecorder::setNumReadbackPABuffer(uint32_t numReadbackPABuffer)
{
    return m_pPath->setNumReadbackPABuffer(numReadbackPABuffer);
}

mdp_pmqos* DpCommandRecorder::getMdpPmqos() {
    int32_t enableMet = DpDriver::getInstance()->getEnableMet();

    m_pPath->getPMQOS(&m_mdp_pmqos);

    DPLOGI("m_mdp_pmqos.mdp_total_datasize %d\n", m_mdp_pmqos.mdp_total_datasize);
    DPLOGI("m_mdp_pmqos.mdp_total_pixel %d\n", m_mdp_pmqos.mdp_total_pixel);

    DPLOGI("m_mdp_pmqos.isp_total_datasize %d\n", m_mdp_pmqos.isp_total_datasize);
    DPLOGI("m_mdp_pmqos.isp_total_pixel %d\n", m_mdp_pmqos.isp_total_pixel);

    DPLOGI("m_mdp_pmqos.tv_sec %lld ms\n", m_mdp_pmqos.tv_sec);
    DPLOGI("m_mdp_pmqos.tv_usec %lld ms\n", m_mdp_pmqos.tv_usec);

    m_mdp_pmqos.ispMetString = 0;
    m_mdp_pmqos.ispMetStringSize = 0;
    m_mdp_pmqos.mdpMetString = 0;
    m_mdp_pmqos.mdpMetStringSize = 0;
    DPLOGI("m_mdp_pmqos size %d\n", sizeof(m_mdp_pmqos));
    if (m_ISP_MET_size> 0 && enableMet) {
        m_mdp_pmqos.ispMetString = (unsigned long)m_ISP_MET_info;
        m_mdp_pmqos.ispMetStringSize = m_ISP_MET_size;
        DPLOGI("DpCommandRecorder::isp met log (%d) %s", m_ISP_MET_size, m_ISP_MET_info);
    }
    if (m_MET_size > 0 && enableMet) {
        m_mdp_pmqos.mdpMetString = (unsigned long)m_MET_info;
        m_mdp_pmqos.mdpMetStringSize = m_MET_size+1;
        DPLOGI("DpCommandRecorder::mdp met log (%d) %s", m_MET_size, m_MET_info);
    }

    return &m_mdp_pmqos;
}

void DpCommandRecorder::setSubtaskId(uint32_t id){
    const char *subtask = "subtask";
    int32_t enableMet = DpDriver::getInstance()->getEnableMet();

    if (!enableMet)
        return;

    if (strstr(m_MET_info, subtask)!= NULL){
        sprintf(strstr(m_MET_info, subtask), "subtask=%d", id%10);
        if (m_MET_size > strlen(m_MET_info)){
            m_MET_info[strlen(m_MET_info)] = ',';
        }
    }
    else if (m_MET_size + 20 > MAX_MET_INFO)
    {
        DPLOGE("MET buffer size too small");
        return;
    }
    else{
       m_MET_size += sprintf(m_MET_info + m_MET_size, ",subtask=%d", id);
    }
}

void DpCommandRecorder::addMetLog(const char *name, uint32_t value)
{
    int32_t enableMet = DpDriver::getInstance()->getEnableMet();

    if (!enableMet)
        return;
    if (name == NULL)
        return;
    if (m_MET_size + 30 > MAX_MET_INFO)
    {
        DPLOGE("MET buffer size too small");
        return;
    }
    if (m_MET_size > 0)
       m_MET_size += sprintf(m_MET_info + m_MET_size, ",%s=%d", name, value);
    else
       m_MET_size += sprintf(m_MET_info + m_MET_size, "%s=%d", name, value);

}

void DpCommandRecorder::addIspMetLog(char *log, uint32_t size)
{
    DPLOGI("DpCommandRecorder::addIspMetLog (%p) (%d)", log, size);
    if (log == NULL || size == 0)
        return ;
    m_ISP_MET_info = log;
    m_ISP_MET_size = size;
}

#ifdef CMDQ_V3
bool DpCommandRecorder::hasIspSecMeta()
{
    return m_hasIspSecMeta;
}

void DpCommandRecorder::addIspSecureMeta(cmdqSecIspMeta ispMeta)
{
    m_hasIspSecMeta = true;
    m_ispMeta = ispMeta;
}

cmdqSecIspMeta DpCommandRecorder::getSecIspMeta()
{
    return m_ispMeta;
}
#endif

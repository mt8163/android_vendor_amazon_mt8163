#include "DpCommandQueue.h"
#include "DpEngineType.h"
#include "DpEngineBase.h"
#include "mmsys_reg_base.h"
#ifdef CONFIG_FOR_SOURCE_PQ
#include "mdp_reg_colorex.h"
#endif
#include "DpTileEngine.h"
#if CONFIG_FOR_OS_ANDROID
#ifndef BASIC_PACKAGE
#include "PQSessionManager.h"
#endif //BASIC_PACKAGE
#endif // CONFIG_FOR_OS_ANDROID

#define DUMP_REGISTER 1


/*
 * Factory method to create command queue object
 */
DpCommand* DpCommand::Factory(DpPathBase *path, uint32_t venc_flag)
{
    return new DpCommandQueue(path, venc_flag);
}

////////////////////////////////////////////////////

DpCommandQueue::DpCommandQueue(DpPathBase *path, uint32_t venc_flag)
    : m_recorder(path),
      m_VEncFlag(venc_flag),
      m_parTDSHP(0),
      m_PqSessionID(0)
{
    m_recorder.initRecord();
}


DpCommandQueue::~DpCommandQueue()
{
}


DP_STATUS_ENUM DpCommandQueue::init()
{
    m_recorder.initRecord();
    return DP_STATUS_RETURN_SUCCESS;
}


DP_STATUS_ENUM DpCommandQueue::reset()
{
    m_recorder.resetRecord();
    return DP_STATUS_RETURN_SUCCESS;
}

DP_STATUS_ENUM DpCommandQueue::secure(DpEngineType type,
                                      uint32_t      flag,
                                      uint32_t     secureRegAddr[3],
                                      DpSecure     secMode,
                                      uint32_t     handle,
                                      uint32_t     offset[3],
                                      uint32_t     memSize[3],
                                      uint32_t     planeOffset[3])
{
    m_recorder.setSecureMode(type,
                             flag,
                             secureRegAddr,
                             secMode,
                             handle,
                             offset,
                             memSize,
                             planeOffset);

    return DP_STATUS_RETURN_SUCCESS;
}

DP_STATUS_ENUM DpCommandQueue::mark(BLOCK_TYPE_ENUM type)
{
    m_recorder.markRecord((DpCommandRecorder::BlockType)type);
    return DP_STATUS_RETURN_SUCCESS;
}


DP_STATUS_ENUM DpCommandQueue::mark(BLOCK_TYPE_ENUM, uint32_t x, uint32_t y)
{
    m_recorder.markRecord(DpCommandRecorder::TILE_BLOCK, x, y);
    return DP_STATUS_RETURN_SUCCESS;
}


DP_STATUS_ENUM DpCommandQueue::dup(BLOCK_TYPE_ENUM type)
{
    DPLOGI("DpCommandQueue: dup block type %d\n", type);

    m_recorder.dupRecord((DpCommandRecorder::BlockType)type);
    return DP_STATUS_RETURN_SUCCESS;
}


DP_STATUS_ENUM DpCommandQueue::write(uint32_t addr,
                                     uint32_t value,
                                     uint32_t mask)
{
    if (0xFFFFFFFF != mask)
    {
        m_recorder.appendCommand(DpCommandRecorder::CMDQ_OP_MOVE, 0, ~mask);
        addr = addr | 0x1;
    }
    m_recorder.appendCommand(DpCommandRecorder::CMDQ_OP_WRITE, addr, value);
    return DP_STATUS_RETURN_SUCCESS;
}


DP_STATUS_ENUM DpCommandQueue::write(uint32_t addr,
                                     uint32_t value,
                                     uint32_t mask,
                                     int32_t  *pLabel,
                                     int32_t  label)
{
    if (label != -1)
    {
        m_recorder.beginOverwrite(label);
    }
    if (pLabel != NULL)
    {
        m_recorder.beginLabel();
    }

    if (0xFFFFFFFF != mask)
    {
        m_recorder.appendCommand(DpCommandRecorder::CMDQ_OP_MOVE, 0, ~mask);
        addr = addr | 0x1;
    }
    m_recorder.appendCommand(DpCommandRecorder::CMDQ_OP_WRITE, addr, value);

    if (pLabel != NULL)
    {
        *pLabel = m_recorder.endLabel();
    }
    if (label != -1)
    {
        m_recorder.endOverwrite();
    }

    return DP_STATUS_RETURN_SUCCESS;
}


DP_STATUS_ENUM DpCommandQueue::writeFromMem(uint32_t addr,
                                            uint32_t value,
                                            uint32_t mask)
{
    if (0xFFFFFFFF != mask)
    {
        m_recorder.appendCommand(DpCommandRecorder::CMDQ_OP_MOVE, 0, ~mask);
        addr = addr | 0x1;
    }
    m_recorder.appendCommand(DpCommandRecorder::CMDQ_OP_WRITE_FROM_MEM, addr, value);
    return DP_STATUS_RETURN_SUCCESS;
}


DP_STATUS_ENUM DpCommandQueue::writeFromReg(uint32_t addr,
                                            uint32_t value,
                                            uint32_t mask)
{
    if (0xFFFFFFFF != mask)
    {
        m_recorder.appendCommand(DpCommandRecorder::CMDQ_OP_MOVE, 0, ~mask);
        addr = addr | 0x1;
    }
    m_recorder.appendCommand(DpCommandRecorder::CMDQ_OP_WRITE_FROM_REG, addr, value);
    return DP_STATUS_RETURN_SUCCESS;
}


DP_STATUS_ENUM DpCommandQueue::read(uint32_t addr, uint32_t value, uint32_t)
{
    m_recorder.appendCommand(DpCommandRecorder::CMDQ_OP_READ, addr, value);
    return DP_STATUS_RETURN_SUCCESS;
}


DP_STATUS_ENUM DpCommandQueue::read(uint32_t addr,
                                     uint32_t value,
                                     int32_t  *pLabel,
                                     int32_t  label)
{
    if (label != -1)
    {
        m_recorder.beginOverwrite(label);
    }
    if (pLabel != NULL)
    {
        m_recorder.beginLabel();
    }

    m_recorder.appendCommand(DpCommandRecorder::CMDQ_OP_READ, addr, value);

    if (pLabel != NULL)
    {
        *pLabel = m_recorder.endLabel();
    }
    if (label != -1)
    {
        m_recorder.endOverwrite();
    }

    return DP_STATUS_RETURN_SUCCESS;
}


DP_STATUS_ENUM DpCommandQueue::poll(uint32_t addr,
                                    uint32_t value,
                                    uint32_t mask)
{
    m_recorder.appendCommand(DpCommandRecorder::CMDQ_OP_MOVE, 0, ~mask);
    m_recorder.appendCommand(DpCommandRecorder::CMDQ_OP_POLL, (addr | 0x1), value);
    return DP_STATUS_RETURN_SUCCESS;
}


DP_STATUS_ENUM DpCommandQueue::wait(EVENT_TYPE_ENUM event)
{
    m_recorder.appendCommand(DpCommandRecorder::CMDQ_OP_WFE, 0, DpDriver::getInstance()->getEventValue(event));
    return DP_STATUS_RETURN_SUCCESS;
}


DP_STATUS_ENUM DpCommandQueue::waitNoClear(EVENT_TYPE_ENUM event)
{
    m_recorder.appendCommand(DpCommandRecorder::CMDQ_OP_WAIT_NO_CLEAR, 0, DpDriver::getInstance()->getEventValue(event));
    return DP_STATUS_RETURN_SUCCESS;
}


DP_STATUS_ENUM DpCommandQueue::clear(EVENT_TYPE_ENUM event)
{
    m_recorder.appendCommand(DpCommandRecorder::CMDQ_OP_CLEAR_TOKEN, 0, DpDriver::getInstance()->getEventValue(event));
    return DP_STATUS_RETURN_SUCCESS;
}


DP_STATUS_ENUM DpCommandQueue::setEvent(EVENT_TYPE_ENUM event)
{
    m_recorder.appendCommand(DpCommandRecorder::CMDQ_OP_SET_TOKEN, 0, DpDriver::getInstance()->getEventValue(event));
    return DP_STATUS_RETURN_SUCCESS;
}

DP_STATUS_ENUM DpCommandQueue::setDumpOrder(uint32_t dumpOrder)
{
    m_recorder.setDumpOrder(dumpOrder);
    return DP_STATUS_RETURN_SUCCESS;
}


DP_STATUS_ENUM DpCommandQueue::stop(bool reorder)
{
    if (reorder)
    {
        m_recorder.reorder();
    }

    m_recorder.stopRecord();
    return DP_STATUS_RETURN_SUCCESS;
}

DP_STATUS_ENUM DpCommandQueue::swap()
{
    int32_t enableDumpReg = DpDriver::getInstance()->getEnableDumpRegister();

    m_recorder.swapRecord();

#if CONFIG_FOR_DUMP_COMMAND
    m_recorder.dumpRecord();
#endif  // CONFIG_FOR_DUMP_COMMAND

#if DUMP_REGISTER
    if (enableDumpReg)
    {
        m_recorder.dumpRegister(m_PqSessionID);
    }
#endif
    return DP_STATUS_RETURN_SUCCESS;
}

DP_STATUS_ENUM DpCommandQueue::flush(bool waitDone, DpJobID* pRet, char** pFrameInfo)
{
    DP_TRACE_CALL();
    DP_STATUS_ENUM status = DP_STATUS_RETURN_SUCCESS;
    int32_t        retry = 1;

    DPLOGI("DpCommandQueue:: flush command begin\n");

    // Dispatch commands
    if (true == waitDone)
    {
        DP_PROFILER_HW_EXECUTION_BEGIN();
        do
        {
            status = DpDriver::getInstance()->execCommand(m_recorder);
            retry--;
        } while ((status != DP_STATUS_RETURN_SUCCESS) && (retry > 0));
        DP_PROFILER_HW_EXECUTION_END();
#if CONFIG_FOR_OS_ANDROID
        {
            uint32_t *pRegVal;
            uint32_t counter;
            bool     pq_readback;
            bool     hdr_readback;
            uint32_t engineFlag;
            //uint32_t index, jpegFilesize;

            pq_readback = m_recorder.getPQReadback();
            hdr_readback = m_recorder.getHDRReadback();
            engineFlag  = m_recorder.getEngineFlag();
            pRegVal     = m_recorder.getReadbackValues(counter);

#ifndef BASIC_PACKAGE
            int32_t pqSupport;
            pqSupport = DpDriver::getInstance()->getPQSupport();
            if (0 != pqSupport) {
                if (pq_readback)
                {
                    DPLOGI("DpCommandQueue:: pq_readback, m_PqSessionID %d\n",m_PqSessionID);

                    PQSession* pPQSession = PQSessionManager::getInstance()->getPQSession(m_PqSessionID);

                    if (pPQSession != NULL)
                    {
                        pPQSession->setHistogram(pRegVal, counter);
                    }
                }
#ifdef SUPPORT_HDR
                if (hdr_readback)
                {
                    DPLOGI("DpCommandQueue:: hdr_readback, m_PqSessionID %d\n",m_PqSessionID);

                    PQSession* pPQSession = PQSessionManager::getInstance()->getPQSession(m_PqSessionID);

                    if (pPQSession != NULL)
                    {
                        pPQSession->setHDRRegInfo(pRegVal, counter);
                    }
                }
#endif
            }
#endif // BASIC_PACKAGE
#if 0
            else if ((engineFlag >> tJPEGENC) & 0x1)
            {
                for (index = 0; index < counter; index++)
                {
                    DPLOGI("Index:%d Val: %X\n", index, pRegVal[index]);
                }
                //Update result to the DpStream by m_recorder.m_readbackValues[0]
                jpegFilesize = (pRegVal[0] + 0x200) - (pRegVal[1] + pRegVal[2]);
                pRegVal[0] = jpegFilesize;
                m_recorder.setNumReadbackRegs(1);
            }
#endif
        }
#endif // CONFIG_FOR_OS_ANDROID
    }
    else
    {
        DP_PROFILER_HW_EXECUTION_BEGIN();
        do
        {
            DPLOGI("DpCommandQueue::flush() m_VEncFlag is : %x\n",m_VEncFlag);
            status = DpDriver::getInstance()->submitCommand(m_recorder, pRet, m_VEncFlag, pFrameInfo);
            retry--;
        } while((status != DP_STATUS_RETURN_SUCCESS) && (retry > 0));
        DP_PROFILER_HW_EXECUTION_END();
    }

    if (DP_STATUS_RETURN_SUCCESS != status)
    {
#if CONFIG_FOR_DUMP_COMMAND
        m_recorder.dumpRecord();
#endif  // CONFIG_FOR_DUMP_COMMAND
        DPLOGE("DpCommandQueue:: execute command failed\n");
        return status;
    }

    DPLOGI("DpCommandQueue:: flush command end\n");
    return DP_STATUS_RETURN_SUCCESS;
}

DP_STATUS_ENUM DpCommandQueue::setISPDebugDumpRegs(uint32_t DumpRegs)
{
    DPLOGI("setISPDebugDumpRegs: %d \n",DumpRegs);
    m_recorder.setISPDebugDumpRegs(DumpRegs);

    return DP_STATUS_RETURN_SUCCESS;
}

DP_STATUS_ENUM DpCommandQueue::setReadbackRegs()
{
    uint32_t index;
    bool     pq_readback;
    bool     hdr_readback;
    int32_t  dre_readback;
    uint32_t engineFlag;
    uint32_t *pReadRegs = NULL;
#ifdef SUPPORT_HDR
    uint32_t *pReadHDRRegs = NULL;
#endif
#ifdef SUPPORT_DRE
    uint32_t *pReadDRERegs = NULL;
#endif
    uint32_t numReadRegs = 0;
    uint32_t counter = 0;

    pq_readback  = m_recorder.getPQReadback();
    hdr_readback = m_recorder.getHDRReadback();
    dre_readback = m_recorder.getDREReadback();
    engineFlag   = m_recorder.getEngineFlag();
    pReadRegs    = m_recorder.getReadbackRegs(numReadRegs);

    DPLOGI("DpCommandQueue::setReadbackReg: readback:%d, engineFlag:%#010x\n", pq_readback, engineFlag);

    //TBD: init the Readback register to all zero !!!

    if (pq_readback)
    {
        uint32_t iTDSHPBase = /*((engineFlag >> tTDSHP0) & 0x1) ?*/ MDP_TDSHP0_BASE /*: MDP_TDSHP1_BASE*/;

        counter = 0;

        for (index = 0x6C; index <= 0xB4; index += 4)
        {
            if (index == 0x88)
                continue;

            pReadRegs[counter++] = iTDSHPBase + index;
        }

        //For shit usage to read color info by Algo's requests
        #ifdef CONFIG_FOR_SOURCE_PQ
                pReadRegs[counter++] = COLOREX_COLOR_TWO_D_W1_RESULT;
        #else
            #ifdef CMD_GPR_R32
                pReadRegs[counter++] = CMD_GPR_R32(CMDQ_DATA_REG_PQ_COLOR);
            #else
                pReadRegs[counter++] = iTDSHPBase + 0x264;
            #endif
        #endif

#if DYN_CONTRAST_VERSION == 2
        for (index = 0x3DC; index <= 0x41C; index += 4)
        {
            pReadRegs[counter++] = iTDSHPBase + index;
        }
#endif
    }
#if 0
    else if ((engineFlag >> tJPEGENC) & 0x1)
    {
        pReadRegs[0] = REG_ADDR_JPEG_ENC_PASS2_DMA_ADDR;
        pReadRegs[1] = REG_ADDR_JPEG_ENC_PASS2_DST_BASE;
        pReadRegs[2] = REG_ADDR_JPEG_ENC_PASS2_DST_OFFSET;
        counter = 3;
    }
#endif

#ifdef SUPPORT_HDR
    if (hdr_readback)
    {
        DPLOGI("DpCommandQueue::setReadbackRegs:hdr_readback\n");
        pReadHDRRegs = &(pReadRegs[counter]);
#if defined(HDR_IN_RDMA)
        for (int i = 0; i < 57; i++)
        {
            pReadRegs[counter++] = MDP_HDR0_BASE + 0x88;
        }
        pReadRegs[counter++] = MDP_HDR0_BASE + 0xA4;
#else
        for (int i = 0; i < 57; i++)
        {
            pReadRegs[counter++] = MDP_HDR0_BASE + 0xC8;
        }
        pReadRegs[counter++] = MDP_HDR0_BASE + 0xE4;
#endif
    }
#endif
#ifdef SUPPORT_DRE
    if (dre_readback != DpDREParam::DRESRAM::SRAMDefault)
    {
        DPLOGI("DpCommandQueue::setReadbackRegs:dre_readback\n");
        pReadDRERegs = &(pReadRegs[counter]);
        for (int i = 0; i < 768; i++)
        {
            pReadRegs[counter++] = MDP_AAL0_BASE + 0x0D8;
        }
    }
#endif
    if (((engineFlag >> tVENC) & 0x1) && m_VEncFlag)
    {
        DPLOGI("DpVEncCommander counter: %d addr[%#010x]", numReadRegs, pReadRegs[0]);
        return DP_STATUS_RETURN_SUCCESS;
    }

    m_recorder.setNumReadbackRegs(counter);
    DPLOGI("DpCommandQueue::setReadbackReg: pRet[0] :%X , counter:%d\n", pReadRegs[0], counter);
    return DP_STATUS_RETURN_SUCCESS;
}

uint32_t* DpCommandQueue::getReadbackValues(uint32_t& numValues)
{
    return m_recorder.getReadbackValues(numValues);
}

void DpCommandQueue::setPath(DpPathBase* pPath)
{
    m_recorder.setPath(pPath);
}

void DpCommandQueue::setTDShpPar(uint32_t TDShpPar)
{
    m_parTDSHP = TDShpPar;
}

uint32_t DpCommandQueue::getTDShpPar()
{
    return m_parTDSHP;
}

DP_STATUS_ENUM DpCommandQueue::frameSrcInfo(DpColorFormat format,
                                            int32_t       width,
                                            int32_t       height,
                                            int32_t       YPitch,
                                            int32_t       UVPitch,
                                            uint32_t      memAddr[3],
                                            uint32_t      memSize[3],
                                            DpSecure      secMode)
{
    m_recorder.setFrameSrcInfo(format,
                               width,
                               height,
                               YPitch,
                               UVPitch,
                               memAddr,
                               memSize,
                               secMode);
    return DP_STATUS_RETURN_SUCCESS;
}

DP_STATUS_ENUM DpCommandQueue::frameDstInfo(DpColorFormat format,
                                            int32_t       width,
                                            int32_t       height,
                                            int32_t       YPitch,
                                            int32_t       UVPitch,
                                            uint32_t      memAddr[3],
                                            uint32_t      memSize[3],
                                            DpSecure      secMode)
{
    int32_t regNum = m_recorder.getRegDstNum();

    if (regNum >= ISP_MAX_OUTPUT_PORT_NUM)
    {
        DPLOGE("invalid Dst Info regNum %d\n", regNum);
        return DP_STATUS_INVALID_PORT;
    }

    m_recorder.setFrameDstInfo(regNum,
                               format,
                               width,
                               height,
                               YPitch,
                               UVPitch,
                               memAddr,
                               memSize,
                               secMode);

    m_recorder.setRegDstNum(regNum + 1);

    return DP_STATUS_RETURN_SUCCESS;
}

void DpCommandQueue::initRegDstNum()
{
    m_recorder.setRegDstNum(0);
}

void DpCommandQueue::setPQSessionID(uint64_t PqSessionID)
{
    m_PqSessionID = PqSessionID;
}

uint64_t DpCommandQueue::getPQSessionID()
{
    return m_PqSessionID;

}

void DpCommandQueue::getReadbackStatus(bool& pq_readback, bool& hdr_readback, int32_t& dre_readback, uint32_t& engineFlag, uint32_t& VEncFlag)
{
    pq_readback  = m_recorder.getPQReadback();
    hdr_readback = m_recorder.getHDRReadback();
    dre_readback = m_recorder.getDREReadback();
    engineFlag   = m_recorder.getEngineFlag();
    VEncFlag = m_VEncFlag;
}

bool DpCommandQueue::getSyncMode()
{
    return m_recorder.getSyncMode();
}

uint32_t* DpCommandQueue::getReadbackPABuffer(uint32_t& readbackPABufferIndex)
{
    return m_recorder.getReadbackPABuffer(readbackPABufferIndex);
}

DP_STATUS_ENUM DpCommandQueue::setNumReadbackPABuffer(uint32_t numReadbackPABuffer)
{
    return m_recorder.setNumReadbackPABuffer(numReadbackPABuffer);
}

void DpCommandQueue::setSubtaskId(uint32_t id){
    m_recorder.setSubtaskId(id);
}

void DpCommandQueue::addMetLog(const char *name, uint32_t value)
{
    m_recorder.addMetLog(name, value);
}

void DpCommandQueue::addIspMetLog(char *log, uint32_t size)
{
    m_recorder.addIspMetLog(log, size);
}

#ifdef CMDQ_V3
void DpCommandQueue::addIspSecureMeta(cmdqSecIspMeta ispMeta)
{
    m_recorder.addIspSecureMeta(ispMeta);
}
#endif

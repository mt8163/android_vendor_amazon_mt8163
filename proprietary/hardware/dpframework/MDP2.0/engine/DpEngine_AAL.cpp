#include "DpTileEngine.h"
#include "DpEngineType.h"
#include "mdp_reg_aal.h"
#include "mmsys_config.h"
#include "tile_driver.h"
#if CONFIG_FOR_VERIFY_FPGA
#include "ConfigInfo.h"
#endif

#if CONFIG_FOR_OS_ANDROID
#include "PQSessionManager.h"
#include "PQAlgorithmFactory.h"
#endif
#define  AAL_CONFIG_FRAME_THRES (1)

//------------------------------------------------------------
// Dummy AAL0 driver engine
//-------------------------------------------------------------
class DpEngine_AAL: public DpTileEngine
{
public:
    DpEngine_AAL(uint32_t identifier)
        : DpTileEngine(identifier),
          m_pqSessionId(0),
          m_cropOffsetX(0),
          m_cropWidth(0),
          m_cropOffsetY(0),
          m_cropHeight(0),
          m_scenario(MEDIA_UNKNOWN),
          m_outHistXLeft(0),
          m_lastOutHorizontal(0),
          m_prevPABufferIndex(0)
    {
        memset(m_regLabel, -1, sizeof(m_regLabel));
        m_pData = &m_data;
    }

    ~DpEngine_AAL()
    {
    }

private:
    MDP_AAL_DATA m_data;
    uint64_t     m_pqSessionId;
    int32_t      m_cropOffsetX;
    int32_t      m_cropWidth;
    int32_t      m_cropOffsetY;
    int32_t      m_cropHeight;
    int32_t      m_scenario;
    int32_t      m_outHistXLeft;
    int32_t      m_lastOutHorizontal;
    int32_t      m_regLabel[MAX_NUM_READBACK_PA_BUFFER];
    uint32_t     m_prevPABufferIndex;

    DP_STATUS_ENUM onInitEngine(DpCommand&);

    DP_STATUS_ENUM onDeInitEngine(DpCommand&);

    DP_STATUS_ENUM onConfigFrame(DpCommand&,
                                 DpConfig&);

    DP_STATUS_ENUM onRetrieveFrameParam(struct TILE_PARAM_STRUCT*, DpCommand &command);

    DP_STATUS_ENUM onReconfigFrame(DpCommand&,
                                   DpConfig&);

    DP_STATUS_ENUM onConfigTile(DpCommand&);

    DP_STATUS_ENUM onPostProc(DpCommand &command);

    DP_STATUS_ENUM onAdvanceTile(DpCommand&);

    int64_t onQueryFeature()
    {
        return eAAL;
    }

    DP_STATUS_ENUM onReconfigTiles(DpCommand &command);
};

// register factory function
static DpEngineBase* AAL0Factory(DpEngineType type)
{
    if (tAAL0 == type)
    {
        return new DpEngine_AAL(0);
    }
    return NULL;
};

// register factory function
EngineReg AAL0Reg(AAL0Factory);

DP_STATUS_ENUM DpEngine_AAL::onInitEngine(DpCommand &command)
{
#ifndef SUPPORT_DRE
    //AAL enable
    MM_REG_WRITE(command, MDP_AAL_EN, 0x1, 0x1);

    //Relay mode
    MM_REG_WRITE(command, MDP_AAL_CFG, 0x1, 0x1);
#endif
    return DP_STATUS_RETURN_SUCCESS;
}


DP_STATUS_ENUM DpEngine_AAL::onDeInitEngine(DpCommand &command)
{
    // Disable AAL

    return DP_STATUS_RETURN_SUCCESS;
}

DP_STATUS_ENUM DpEngine_AAL::onConfigFrame(DpCommand &command, DpConfig &config)
{
#ifdef HW_SUPPORT_10BIT_PATH
    // 10 bit format
    if ((DP_COLOR_GET_10BIT_PACKED(config.inFormat) || DP_COLOR_GET_10BIT_LOOSE(config.inFormat)) &&
        (DP_COLOR_GET_10BIT_PACKED(config.outFormat) || DP_COLOR_GET_10BIT_LOOSE(config.outFormat)))
    {
        MM_REG_WRITE(command, MDP_AAL_CFG_MAIN, 0 << 7, 0x80);
    }
    else
    {
        MM_REG_WRITE(command, MDP_AAL_CFG_MAIN, 1 << 7, 0x80);
    }
#endif // HW_SUPPORT_10BIT_PATH

#ifdef SUPPORT_DRE
    DpTimeValue    begin;
    DpTimeValue    end;
    int32_t        diff;

    DPLOGI("DpEngine_AAL: onConfigFrame");

    DP_TIMER_GET_CURRENT_TIME(begin);

    PQSession* pPQSession = PQSessionManager::getInstance()->getPQSession(config.pqSessionId);
    if (pPQSession != NULL)
    {
        PQDREAdaptor* pPQDREAdaptor = PQAlgorithmFactory::getInstance()->getDRE(m_identifier);

        m_scenario = pPQDREAdaptor->getPQScenario(pPQSession);
        if (m_scenario == MEDIA_ISP_CAPTURE)
        {
            m_data.m_YCropFromFrameTop = true;
        }
    }

    DP_TIMER_GET_CURRENT_TIME(end);
    DP_TIMER_GET_DURATION_IN_MS(begin, end, diff);

    if (diff > AAL_CONFIG_FRAME_THRES)
    {
        DPLOGD("DpEngine_AAL: configFrame() time %d ms\n", diff);
    }

    m_pqSessionId       = config.pqSessionId;
    m_outHistXLeft      = 0;
    m_lastOutHorizontal = 0;
#endif
    return DP_STATUS_RETURN_SUCCESS;
}

DP_STATUS_ENUM DpEngine_AAL::onRetrieveFrameParam(struct TILE_PARAM_STRUCT*, DpCommand &command)
{
#ifdef SUPPORT_DRE
    assert(m_pFunc != NULL);
    assert(TILE_FUNC_MDP_BASE + getEngineType() == m_pFunc->func_num);

    m_outputDisable = m_pFunc->output_disable_flag;
    if (true == m_outputDisable)
    {
        DPLOGI("DpEngine_AAL::onRetrieveFrameParam: %s output disabled\n", getEngineName());
        return DP_STATUS_RETURN_SUCCESS;
    }

    /* retrieve tile core TILE_FUNC_BLOCK_STRUCT */
    m_cropOffsetX = m_pFunc->in_pos_xs;
    m_cropWidth = m_pFunc->in_pos_xe - m_pFunc->in_pos_xs + 1;
    m_cropOffsetY = m_pFunc->in_pos_ys;
    m_cropHeight = m_pFunc->in_pos_ye - m_pFunc->in_pos_ys + 1;

    DPLOGI("DpEngine_AAL::onRetrieveFrameParam m_cropOffsetX = %d, m_cropWidth = %d, m_cropOffsetY = %d, m_cropHeight = %d, m_inFrameWidth = %d, m_inFrameHeight = %d\n",
        m_cropOffsetX, m_cropWidth, m_cropOffsetY, m_cropHeight, m_inFrameWidth, m_inFrameHeight);

    DpTimeValue    begin;
    DpTimeValue    end;
    int32_t        diff;

    DP_TIMER_GET_CURRENT_TIME(begin);

    PQSession* pPQSession = PQSessionManager::getInstance()->getPQSession(m_pqSessionId);
    if (pPQSession != NULL)
    {
        PQDREAdaptor* pPQDREAdaptor = PQAlgorithmFactory::getInstance()->getDRE(m_identifier);
        int32_t imWidth, imHeight;

        if (m_scenario == MEDIA_ISP_CAPTURE)
        {
            imWidth = m_inFrameWidth;
            imHeight = m_inFrameHeight;
        }
        else
        {
            imWidth = m_cropWidth;
            imHeight = m_cropHeight;
        }
        pPQDREAdaptor->calRegs(pPQSession, command, imWidth, imHeight, m_frameConfigLabel, false);
        pPQSession->setDrePreviousSize(imWidth, imHeight);
        if (m_scenario == MEDIA_ISP_CAPTURE)
        {
            m_data.m_YCropFromFrameTop = true;
        }
    }

    DP_TIMER_GET_CURRENT_TIME(end);
    DP_TIMER_GET_DURATION_IN_MS(begin, end, diff);

    if (diff > AAL_CONFIG_FRAME_THRES)
    {
        DPLOGD("DpEngine_AAL: onRetrieveFrameParam() time %d ms\n", diff);
    }
#endif
    return DP_STATUS_RETURN_SUCCESS;
}

DP_STATUS_ENUM DpEngine_AAL::onReconfigFrame(DpCommand &command, DpConfig &config)
{
#ifdef SUPPORT_DRE
    DpTimeValue    begin;
    DpTimeValue    end;
    int32_t        diff;

    DPLOGI("DpEngine_AAL: onReconfigFrame");

    DP_TIMER_GET_CURRENT_TIME(begin);

    PQSession* pPQSession = PQSessionManager::getInstance()->getPQSession(config.pqSessionId);
    if (pPQSession != NULL)
    {
        PQDREAdaptor* pPQDREAdaptor = PQAlgorithmFactory::getInstance()->getDRE(m_identifier);
        int32_t imWidth, imHeight;

        if (m_scenario == MEDIA_ISP_CAPTURE)
        {
            imWidth = m_inFrameWidth;
            imHeight = m_inFrameHeight;
        }
        else
        {
            imWidth = m_cropWidth;
            imHeight = m_cropHeight;
        }
        m_scenario = pPQDREAdaptor->getPQScenario(pPQSession);
        pPQDREAdaptor->calRegs(pPQSession, command, imWidth, imHeight, m_frameConfigLabel, true);
        pPQSession->setDrePreviousSize(imWidth, imHeight);
        if (m_scenario == MEDIA_ISP_CAPTURE)
        {
            m_data.m_YCropFromFrameTop = true;
        }
    }

    DP_TIMER_GET_CURRENT_TIME(end);
    DP_TIMER_GET_DURATION_IN_MS(begin, end, diff);

    if (diff > AAL_CONFIG_FRAME_THRES)
    {
        DPLOGD("DpEngine_AAL: onReconfigFrame() time %d ms\n", diff);
    }

    m_pqSessionId       = config.pqSessionId;
    m_outHistXLeft      = 0;
    m_lastOutHorizontal = 0;
#endif
    return DP_STATUS_RETURN_SUCCESS;
}


DP_STATUS_ENUM DpEngine_AAL::onConfigTile(DpCommand &command)
{
    uint32_t AAL_in_hsize;
    uint32_t AAL_in_vsize;
    uint32_t AAL_out_hoffset;
    uint32_t AAL_out_voffset;
    uint32_t AAL_out_hsize;
    uint32_t AAL_out_vsize;
    uint32_t AAL_hist_left;

    // Set source size
    AAL_in_hsize    = m_inTileXRight  - m_inTileXLeft + 1;
    AAL_in_vsize    = m_inTileYBottom - m_inTileYTop + 1;
    MM_REG_WRITE(command, MDP_AAL_SIZE , (AAL_in_hsize << 16) +
                                         (AAL_in_vsize <<  0), 0x1FFF1FFF);

    // Set crop offset
    AAL_out_hoffset = m_outTileXLeft - m_inTileXLeft;
    AAL_out_voffset = m_outTileYTop  - m_inTileYTop;
    MM_REG_WRITE(command, MDP_AAL_OUTPUT_OFFSET, (AAL_out_hoffset << 16) +
                                                 (AAL_out_voffset <<  0), 0x00FF00FF);

    // Set target size
    AAL_out_hsize   = m_outTileXRight - m_outTileXLeft + 1;
    AAL_out_vsize   = m_outTileYBottom - m_outTileYTop + 1;
    MM_REG_WRITE(command, MDP_AAL_OUTPUT_SIZE, (AAL_out_hsize << 16) +
                                               (AAL_out_vsize <<  0), 0x1FFF1FFF);

    AAL_hist_left = (m_outTileXLeft > m_outHistXLeft) ? m_outTileXLeft : m_outHistXLeft;

    uint32_t winXStart = AAL_hist_left - m_inTileXLeft;
    uint32_t winXEnd = m_outTileXRight - m_inTileXLeft;
#ifdef SUPPORT_DRE
    PQDREAdaptor* pPQDREAdaptor = PQAlgorithmFactory::getInstance()->getDRE(m_identifier);

    if (m_scenario == MEDIA_ISP_CAPTURE)
    {
        DPLOGI("DpEngine_AAL::onConfigTile MEDIA_ISP_CAPTURE m_inFrameWidth = %d, m_inFrameHeight = %d, \n", m_inFrameWidth, m_inFrameHeight);
        pPQDREAdaptor->calTileRegs(command, m_inFrameWidth, m_inFrameHeight, m_inTileXLeft, m_inTileXRight, m_outTileXLeft, m_outTileXRight, winXStart, winXEnd);
    }
    else
    {
        DPLOGI("DpEngine_AAL::onConfigTile not MEDIA_ISP_CAPTURE m_cropOffsetX = %d, m_cropWidth = %d, m_cropOffsetY = %d, m_cropHeight = %d, \n", m_cropOffsetX, m_cropWidth, m_cropOffsetY, m_cropHeight);
        pPQDREAdaptor->calTileRegs(command, m_cropWidth, m_cropHeight, m_inTileXLeft - m_cropOffsetX, m_inTileXRight - m_cropOffsetX, m_outTileXLeft - m_cropOffsetY, m_outTileXRight - m_cropOffsetY, winXStart - m_cropOffsetY, winXEnd - m_cropOffsetY);
    }
#endif
    return DP_STATUS_RETURN_SUCCESS;
}

DP_STATUS_ENUM DpEngine_AAL::onPostProc(DpCommand &command)
{
#ifdef SUPPORT_DRE
    bool pq_readback;
    bool hdr_readback;
    int32_t dre_readback;
    uint32_t engineFlag;
    uint32_t VEncFlag;
    uint32_t counter = 0;
    uint32_t* readbackPABuffer = NULL;
    uint32_t readbackPABufferIndex = 0;
    uint32_t start_address = DRE30_HIST_START;

    command.getReadbackStatus(pq_readback, hdr_readback, dre_readback, engineFlag, VEncFlag);
    readbackPABuffer = command.getReadbackPABuffer(readbackPABufferIndex);
    if (readbackPABuffer == NULL)
    {
        DPLOGW("DpEngine_AAL::onPostProc : readbackPABuffer has been destroyed readbackPABuffer = 0x%08x, readbackPABufferIndex = %d\n", readbackPABuffer, readbackPABufferIndex);
        return DP_STATUS_RETURN_SUCCESS;
    }

    if (dre_readback != DpDREParam::DRESRAM::SRAMDefault)
    {
        // set SRAM ID for read DRE information
        MM_REG_WRITE(command, MDP_AAL_SRAM_CFG, (dre_readback << 6)|(dre_readback << 5)|(1 << 4), (0x7 << 4));

        MM_REG_READ_BEGIN(command);
        DPLOGI("DpEngine_AAL::onPostProc:dre_readback = %d\n", dre_readback);
        for (int i = 0; i < 768; i++)
        {
            MM_REG_WRITE(command, MDP_AAL_SRAM_RW_IF_2, start_address, MDP_AAL_SRAM_RW_IF_2_MASK);
            MM_REG_POLL(command, MDP_AAL_SRAM_STATUS, (0x1 << 17), (0x1 << 17));
            MM_REG_READ(command, MDP_AAL_SRAM_RW_IF_3, readbackPABuffer[((readbackPABufferIndex + counter) & (MAX_NUM_READBACK_PA_BUFFER - 1))], &m_regLabel[counter]);
            counter++;
            start_address += 4;
        }
        MM_REG_READ_END(command);
    }

    command.setNumReadbackPABuffer(counter);

    m_prevPABufferIndex = readbackPABufferIndex;
#endif
    return DP_STATUS_RETURN_SUCCESS;
}

DP_STATUS_ENUM DpEngine_AAL::onReconfigTiles(DpCommand &command)
{
#ifdef SUPPORT_DRE
    bool pq_readback;
    bool hdr_readback;
    int32_t dre_readback;
    uint32_t engineFlag;
    uint32_t VEncFlag;
    uint32_t counter = 0;
    uint32_t* readbackPABuffer = NULL;
    uint32_t readbackPABufferIndex = 0;

    command.getReadbackStatus(pq_readback, hdr_readback, dre_readback, engineFlag, VEncFlag);
    readbackPABuffer = command.getReadbackPABuffer(readbackPABufferIndex);
    if (readbackPABufferIndex == m_prevPABufferIndex)
    {
        DPLOGI("DpEngine_AAL::onReconfigTiles : PABufferIndex no change do nothing\n");
        return DP_STATUS_RETURN_SUCCESS;
    }

    if (readbackPABuffer == NULL)
    {
        DPLOGW("DpEngine_AAL::onReconfigTiles : readbackPABuffer has been destroyed readbackPABuffer = 0x%08x, readbackPABufferIndex = %d\n", readbackPABuffer, readbackPABufferIndex);
        return DP_STATUS_RETURN_SUCCESS;
    }

    if (dre_readback != DpDREParam::DRESRAM::SRAMDefault)
    {
        for (int i = 0; i < 768; i++)
        {
            MM_REG_READ(command, MDP_AAL_SRAM_RW_IF_3, readbackPABuffer[((readbackPABufferIndex + counter) & (MAX_NUM_READBACK_PA_BUFFER - 1))], NULL, m_regLabel[counter]);
            counter++;
        }
    }

    command.setNumReadbackPABuffer(counter);
    m_prevPABufferIndex = readbackPABufferIndex;
#endif
    return DP_STATUS_RETURN_SUCCESS;
}

DP_STATUS_ENUM DpEngine_AAL::onAdvanceTile(DpCommand &command)
{
#ifdef SUPPORT_DRE
    // FIXME: check tile order
    if (m_lastOutHorizontal != m_outHorizontal)
    {
        m_outHistXLeft      = m_outHorizontal ? (m_outTileXRight + 1) : 0;
        m_lastOutHorizontal = m_outHorizontal;
    }
#endif
    return DP_STATUS_RETURN_SUCCESS;
}

#include "DpTileEngine.h"
#include "DpEngineType.h"
#include "mdp_reg_hdr.h"
#include "mmsys_config.h"
#include "DpDataType.h"
#if CONFIG_FOR_VERIFY_FPGA
#include "ConfigInfo.h"
#endif

//------------------------------------------------------------
// Dummy HDR0 driver engine
//-------------------------------------------------------------
class DpEngine_HDR: public DpTileEngine
{
public:
    DpEngine_HDR(uint32_t identifier)
        : DpTileEngine(identifier)
    {
    }

    ~DpEngine_HDR()
    {
    }

private:
    DP_STATUS_ENUM onInitEngine(DpCommand&);

    DP_STATUS_ENUM onDeInitEngine(DpCommand&);

    DP_STATUS_ENUM onConfigFrame(DpCommand&,
                                 DpConfig&);

    DP_STATUS_ENUM onConfigTile(DpCommand&);

    int64_t onQueryFeature()
    {
        return eHDR;
    }

};

// register factory function
static DpEngineBase* HDR0Factory(DpEngineType type)
{
    if (tHDR0 == type)
    {
        return new DpEngine_HDR(0);
    }
    return NULL;
};

// register factory function
EngineReg HDR0Reg(HDR0Factory);

DP_STATUS_ENUM DpEngine_HDR::onInitEngine(DpCommand &command)
{
    //HDR enable
    MM_REG_WRITE(command, MDP_HDR_TOP, 0x1, 0x1);

    //Relay mode
    //MM_REG_WRITE(command, MDP_HDR_RELAY, 0x1, 0x1);

    return DP_STATUS_RETURN_SUCCESS;
}


DP_STATUS_ENUM DpEngine_HDR::onDeInitEngine(DpCommand &command)
{
    // Disable HDR
    return DP_STATUS_RETURN_SUCCESS;
}

DP_STATUS_ENUM DpEngine_HDR::onConfigFrame(DpCommand &command, DpConfig &config)
{
#ifdef HW_SUPPORT_10BIT_PATH
    // 10 bit format
    if ((DP_COLOR_GET_10BIT_PACKED(config.inFormat) || DP_COLOR_GET_10BIT_LOOSE(config.inFormat)) &&
        (DP_COLOR_GET_10BIT_PACKED(config.outFormat) || DP_COLOR_GET_10BIT_LOOSE(config.outFormat)))
    {
        MM_REG_WRITE(command, MDP_HDR_TOP, 3 << 28, 0x30000000);
    }
    else
    {
        MM_REG_WRITE(command, MDP_HDR_TOP, 1 << 28, 0x30000000);
    }
#endif // HW_SUPPORT_10BIT_PATH
    return DP_STATUS_RETURN_SUCCESS;
}


DP_STATUS_ENUM DpEngine_HDR::onConfigTile(DpCommand &command)
{
    uint32_t HDR_hsize;
    uint32_t HDR_vsize;
    uint32_t HDR_out_hoffset;
    uint32_t HDR_out_voffset;
    uint32_t HDR_out_hsize;
    uint32_t HDR_out_vsize;


    HDR_hsize      = m_inTileXRight   - m_inTileXLeft + 1;
    HDR_vsize      = m_inTileYBottom  - m_inTileYTop  + 1;
    // Set source offset
    MM_REG_WRITE(command, MDP_HDR_SIZE_0, (HDR_vsize << 16) +
                                          (HDR_hsize <<  0), 0x1FFF1FFF);
    // Set crop offset
    HDR_out_hoffset = m_outTileXLeft - m_inTileXLeft;
    HDR_out_voffset = m_outTileYTop  - m_inTileYTop;

    // Set target size
    HDR_out_hsize   = m_outTileXRight - m_outTileXLeft + 1;
    HDR_out_vsize   = m_outTileYBottom - m_outTileYTop + 1;

    MM_REG_WRITE(command, MDP_HDR_SIZE_0, (HDR_out_hsize << 16) +
                                                 (HDR_out_hoffset <<  0), 0x1FFF1FFF);
    MM_REG_WRITE(command, MDP_HDR_SIZE_1, (HDR_out_vsize << 16) +
                                               (HDR_out_voffset <<  0), 0x1FFF1FFF);

    DPLOGI("DpEngine_HDR: source h: %d, v: %d\n", HDR_hsize, HDR_vsize);
    DPLOGI("DpEngine_HDR: crop offset h: %d, v: %d\n", HDR_out_hoffset, HDR_out_voffset);
    DPLOGI("DpEngine_HDR: target h: %d, v: %d\n", HDR_out_hsize, HDR_out_vsize);

    return DP_STATUS_RETURN_SUCCESS;
}

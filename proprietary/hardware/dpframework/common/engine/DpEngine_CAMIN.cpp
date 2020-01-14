#include "DpTileEngine.h"
#include "DpEngineType.h"
#include "mmsys_config.h"

//------------------------------------------------------------
// Dummy CAMIN driver engine to merge SCL0, SCL1 or TDSHP info
//-------------------------------------------------------------
class DpEngine_CAMIN: public DpTileEngine
{
public:
    DpEngine_CAMIN(uint32_t identifier)
        : DpTileEngine(identifier)
    {
    }

    ~DpEngine_CAMIN()
    {
    }
    DP_STATUS_ENUM onConfigTile(DpCommand&);
};

// Register factory function
static DpEngineBase* CAMINFactory(DpEngineType type)
{
    if (tCAMIN == type)
    {
        return new DpEngine_CAMIN(0);
    }
    return NULL;
};

// Register factory function
EngineReg CAMINReg(CAMINFactory);

#ifdef tCAMIN2
// Register factory function
static DpEngineBase* CAMIN2Factory(DpEngineType type)
{
    if (tCAMIN2 == type)
    {
        return new DpEngine_CAMIN(1);
    }
    return NULL;
};

// Register factory function
EngineReg CAMIN2Reg(CAMIN2Factory);
#endif

DP_STATUS_ENUM DpEngine_CAMIN::onConfigTile(DpCommand &command)
{
    uint32_t isp_dl_w;
    uint32_t isp_dl_h;
#ifndef MDP_ASYNC_CFG_WD
    DP_UNUSED(command);
#endif

    isp_dl_w = m_inTileXRight - m_inTileXLeft + 1;
    isp_dl_h = m_inTileYBottom - m_inTileYTop + 1;

    if (m_identifier==0)
    {
        // CONFIG FOR ISP+MDP DL
#ifdef MDP_ASYNC_CFG_WD
        MM_REG_WRITE_MASK(command, MDP_ASYNC_CFG_WD, (isp_dl_h << 16) + isp_dl_w, 0x3FFF3FFF);
#endif
#ifdef ISP_RELAY_CFG_WD
        MM_REG_WRITE_MASK(command, ISP_RELAY_CFG_WD, (isp_dl_h << 16) + isp_dl_w, 0x3FFF3FFF);
#endif
        DPLOGI("CAMIN DL width=%d height=%d\n",isp_dl_w,isp_dl_h);
    }
    else
    {
        // CONFIG FOR ISP+MDP DL
#ifdef MDP_ASYNC_IPU_CFG_WD
        MM_REG_WRITE_MASK(command, MDP_ASYNC_IPU_CFG_WD, (isp_dl_h << 16) + isp_dl_w, 0x3FFF3FFF);
#endif
#ifdef IPU_RELAY_CFG_WD
        MM_REG_WRITE_MASK(command, IPU_RELAY_CFG_WD, (isp_dl_h << 16) + isp_dl_w, 0x3FFF3FFF);
#endif
        DPLOGI("CAMIN2 DL width=%d height=%d\n",isp_dl_w,isp_dl_h);
    }

    return DP_STATUS_RETURN_SUCCESS;
}


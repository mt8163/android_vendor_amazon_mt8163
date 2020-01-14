#ifndef __PQDREADAPTOR_H__
#define __PQDREADAPTOR_H__

#include "PQMutex.h"
#include "DpTileEngine.h"
#include "DpEngineType.h"
#include "PQSession.h"
#include "PQAlgorithmAdaptor.h"

#include "AdaptiveCALTM.h"

#define DRE_ENGINE_MAX_NUM (1)

#define DRE30_HIST_START         (1024)
#define DRE30_HIST_END           (4092)
#define DRE30_GAIN_START         (4096)
#define DRE30_GAIN_POINT16_START (6144)
#define DRE30_GAIN_END           (6268)

class PQDREAdaptor: public PQAlgorithmAdaptor
{
public:
    PQDREAdaptor(uint32_t identifier);
    ~PQDREAdaptor();

    static PQDREAdaptor* getInstance(uint32_t identifier);
    static void destroyInstance();
    int32_t getPQScenario(PQSession *pPQSession);
    void calRegs(PQSession* pPQSession, DpCommand &command, int32_t imWidth, int32_t imHeight,
        int32_t *pFrameConfigLabel, const bool curveOnly);
    void calTileRegs(DpCommand &command,
        const uint32_t hSize, const uint32_t vSize,
        const uint32_t inTileStart, const uint32_t inTileEnd,
        const uint32_t outTileStart, const uint32_t outTileEnd,
        const uint32_t winXStart, const uint32_t winXEnd);

private:
    void tuningDREInput(void *input, int32_t scenario);
    bool tuningDREOutput(void *output, int32_t scenario);
    void tuningDRESWReg(CDRETopFW *pDREFW, int32_t scenario);

    void initDREFWparam(const int32_t scenario, int32_t imWidth, int32_t imHeight);
    void onCalculateIspImpl(const bool isBypass, DpPqParam &PQParam, DRETopOutput *outParam, CDRETopFW *pDREFW);
    void onCalculate(PQSession *pPQSession, DpPqParam &PQParam, const bool isBypass, const uint32_t modifyDREBlock,
        DRETopOutput *outParam, bool *isApplyFw);
    void initDREOutput(DRETopOutput *outParam);
    void getDefaultDREOutput(uint32_t modifyDREBlk, DRETopOutput *outParam);

    void sramWrite(DpCommand &command, unsigned int addr, unsigned int value,
        int32_t *pFrameConfigLabel, int32_t &index, const bool curveOnly);
    void writeBlock(DpCommand &command, const DRETopOutput *output,
        const uint32_t block_x, const uint32_t block_y,
        const uint32_t dre_blk_x_num, const uint32_t dre_blk_y_num,
        int32_t *pFrameConfigLabel, int32_t &index, const bool curveOnly);
    void writeCurve16(DpCommand &command, const DRETopOutput *output,
        const uint32_t dre_blk_x_num, const uint32_t dre_blk_y_num,
        int32_t *pFrameConfigLabel, int32_t &index, const bool curveOnly);
    void writeDRE30Curve(DpCommand &command, const DRETopOutput *output,
        const uint32_t dre_blk_x_num, const uint32_t dre_blk_y_num,
        int32_t *pFrameConfigLabel, int32_t &index, const bool curveOnly);
    void debugDumpFWInput(const DRETopInput *inParam);
    void debugDumpFrameReg(const DRETopOutput *outParam, const DREInitParam *initDREParam, const DREInitReg *initDREReg);
    void debugDumpTileReg(const DREInitParam *initDREParam, const DREInitReg *initDREReg,
        const uint32_t outTileStart, const uint32_t outTileEnd);
    template <class regType>
    void writeStringWithTag(const char *pTag, const regType *pSrc, char *pDst, int &cnt);
    void dumpFwReg(const bool isApplyFw, const DpPqParam *PQParam,
        const DRETopInput *inReg, const DRETopOutput *outReg, const DREInitReg *initReg, const DREReg *swReg,
        const TAdaptiveCALTMReg *adaptiveCALTMReg, const TAdaptiveCALTMFace_Exif *adaptiveCALTMFace_exif, const TAdaptiveCALTMFace_Dump *adaptiveCALTMFace_dump);

private:
    static PQDREAdaptor *s_pInstance[DRE_ENGINE_MAX_NUM];
    static PQMutex  s_ALMutex;

    uint32_t       m_identifier;
    DRE_CONFIG_T   m_pDREConfig;
    CDRETopFW      *m_pDREFW;
    TAdaptiveCALTM *m_pAdaptiveCALTMFW;
    DRETopInput    m_pDREInput;
    DREInitParam   m_initDREParam;
    DRETopOutput   m_defaultDREOutput;
    uint32_t       m_enableDumpRegister;
};

#endif //__PQDREADAPTOR_H__

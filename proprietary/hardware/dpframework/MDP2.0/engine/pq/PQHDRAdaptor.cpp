#define LOG_TAG "PQ"
#define MTK_LOG_ENABLE 1
#include <cutils/log.h>
#include <fcntl.h>

#include <PQCommon.h>
#include <PQHDRAdaptor.h>
#include "PQTuningBuffer.h"

#include "mdp_reg_hdr.h"

#include "cust_color.h"

#if defined(HDR_IN_RDMA)
#define HDR_GAIN_TABLE_EVEN_WRITE (12)
#define HDR_GAIN_TABLE_ODD_WRITE  (13)
#define HDR_GAIN_TABLE_UPDATE     (8)
#else
#define HDR_GAIN_TABLE_EVEN_WRITE (14)
#define HDR_GAIN_TABLE_ODD_WRITE  (15)
#define HDR_GAIN_TABLE_UPDATE     (11)
#endif

#define HDR_DRIVER_DEBUG_MODE (1 << 0)

#define ARR_LEN_4BYTE(arr) (sizeof(arr) / 4)

PQHDRAdaptor* PQHDRAdaptor::s_pInstance[] = {};
PQMutex   PQHDRAdaptor::s_ALMutex;

PQHDRAdaptor* PQHDRAdaptor::getInstance(uint32_t identifier)
{
    AutoMutex lock(s_ALMutex);

    if(NULL == s_pInstance[identifier])
    {
        s_pInstance[identifier] = new PQHDRAdaptor(identifier);
        atexit(PQHDRAdaptor::destroyInstance);
    }

    return s_pInstance[identifier];
}

void PQHDRAdaptor::destroyInstance()
{
    AutoMutex lock(s_ALMutex);

    for (int identifier = 0; identifier < HDR_ENGINE_MAX_NUM; identifier++){
        if (NULL != s_pInstance[identifier])
        {
            delete s_pInstance[identifier];
            s_pInstance[identifier] = NULL;
        }
    }
}

PQHDRAdaptor::PQHDRAdaptor(uint32_t identifier)
        : m_identifier(identifier),
          m_lastWidth(0),
          m_lastHeight(0),
          PQAlgorithmAdaptor(PROXY_HDR_SWREG,
                             PROXY_HDR_INPUT,
                             PROXY_HDR_OUTPUT)
{
    PQ_LOGD("[PQHDRAdaptor] PQHDRAdaptor()... ");

    m_pHDRFW = new CPQHDRFW;
    getHDRTable();

    memset(&m_pHDRConfig, 0x0, sizeof(HDR_CONFIG_T));
    m_pHDRFW->setDebugFlag(m_pHDRConfig.debugFlag);

    memcpy(&m_initHDRFWReg, m_pHDRFW->pHDRFWReg, sizeof(HDRFWReg));
    memset(&m_lastHDRInfo, 0, sizeof(m_lastHDRInfo));
    memset(&m_lastOutput, 0, sizeof(m_lastOutput));
};

PQHDRAdaptor::~PQHDRAdaptor()
{
    PQ_LOGD("[PQHDRAdaptor] ~PQHDRAdaptor()... ");

    delete m_pHDRFW;
};

void PQHDRAdaptor::tuningHDRInput(DHDRINPUT *input, int32_t scenario)
{
    PQTuningBuffer *p_buffer = m_inputBuffer;
    unsigned int *overwritten_buffer = p_buffer->getOverWrittenBuffer();
    unsigned int *reading_buffer = p_buffer->getReadingBuffer();
    size_t copy_size = sizeof(DHDRINPUT);

    if (p_buffer->isValid() == false) {
        return;
    }

    if (copy_size > p_buffer->getModuleSize()) {
        copy_size = p_buffer->getModuleSize();
    }

    if (p_buffer->isOverwritten()) {
        if (p_buffer->isSync()) {
            p_buffer->pull();
        }
        memcpy(input, overwritten_buffer, copy_size);
    } else if (scenario == MEDIA_PICTURE) {
        p_buffer->resetReady();
        memcpy(reading_buffer, input, copy_size);
        p_buffer->push();
    } else if (p_buffer->toBePrepared()) {
        memcpy(reading_buffer, input, copy_size);
        p_buffer->push();
    }
}

bool PQHDRAdaptor::tuningHDROutput(DHDROUTPUT *output, int32_t scenario)
{
    PQTuningBuffer *p_buffer = m_outputBuffer;
    unsigned int *overwritten_buffer = p_buffer->getOverWrittenBuffer();
    unsigned int *reading_buffer = p_buffer->getReadingBuffer();
    size_t copy_size = sizeof(DHDROUTPUT);

    if (p_buffer->isValid() == false) {
        return false;
    }

    if (copy_size > p_buffer->getModuleSize()) {
        copy_size = p_buffer->getModuleSize();
    }

    if (p_buffer->isOverwritten()) {
        if (p_buffer->isSync()) {
            p_buffer->pull();
        }
        memcpy(output, overwritten_buffer, copy_size);
    } else if (scenario == MEDIA_PICTURE) {
        p_buffer->resetReady();
        memcpy(reading_buffer, output, copy_size);
        p_buffer->push();
    } else if (p_buffer->toBePrepared()) {
        memcpy(reading_buffer, output, copy_size);
        p_buffer->push();
    }

    if (p_buffer->isBypassHWAccess()) {
        return true;
    }

    return false;
}

void PQHDRAdaptor::tuningHDRSWReg(HDRFWReg *SWReg, int32_t scenario)
{
    PQTuningBuffer *p_buffer = m_swRegBuffer;
    unsigned int *overwritten_buffer = p_buffer->getOverWrittenBuffer();
    unsigned int *reading_buffer = p_buffer->getReadingBuffer();
    size_t copy_size = sizeof(HDRFWReg);

    if (p_buffer->isValid() == false) {
        return;
    }

    if (copy_size > p_buffer->getModuleSize()) {
        copy_size = p_buffer->getModuleSize();
    }

    if (p_buffer->isOverwritten()) {
        if (p_buffer->isSync()) {
            p_buffer->pull();
        }
        memcpy(SWReg, overwritten_buffer, copy_size);
    } else if (scenario == MEDIA_PICTURE) {
        p_buffer->resetReady();
        memcpy(reading_buffer, SWReg, copy_size);
        p_buffer->push();
    } else if (p_buffer->toBePrepared()) {
        memcpy(reading_buffer, SWReg, copy_size);
        p_buffer->push();
    }
}

bool PQHDRAdaptor::getHDRTable()
{
    PQConfig* pPQConfig = PQConfig::getInstance();
    int32_t offset = 0;
    int32_t size = 0;
    int32_t isNoError = 0;

    // Panel nits & gamut default settings
    m_internalDispPanelSpec.panel_nits = PQ_HDR_DEFAULT_PANEL_NITS;
    m_internalDispPanelSpec.gamut = REC709;

    /* get register value from ashmem */
    for (int index = 0; index < PROXY_HDR_CUST_MAX; index++)
    {
        offset += size;
        if (index == PROXY_HDR_CUST_REG)
        {
            size = ARR_LEN_4BYTE(HDRFWReg);
            isNoError = pPQConfig->getAshmemArray(PROXY_HDR_CUST, offset, m_pHDRFW->pHDRFWReg, size);
        }
        else if (index == PROXY_HDR_CUST_PANELSPEC)
        {
            size = ARR_LEN_4BYTE(PANEL_SPEC);
            isNoError = pPQConfig->getAshmemArray(PROXY_HDR_CUST, offset, &m_internalDispPanelSpec, size);
        }

        if (isNoError < 0)
        {
            break;
        }
    }

    return (isNoError == 1) ? 1 : 0;
}

bool PQHDRAdaptor::calRegs(PQSession* pPQSession, DpCommand &command, DpConfig &config,
    int32_t *pFrameConfigLabel, const bool curveOnly)
{
    DP_VDEC_DRV_COLORDESC_T HDRInfo;
    memset(&HDRInfo, 0, sizeof(DP_VDEC_DRV_COLORDESC_T));

    DpPqConfig* PQConfig;
    pPQSession->getDpPqConfig(&PQConfig);

    DpPqParam  PqParam;
    pPQSession->getPQParam(&PqParam);

    DHDRINPUT inParam;
    memset(&inParam, 0, sizeof(DHDRINPUT));

    DHDROUTPUT outParam;
    memset(&outParam, 0, sizeof(DHDROUTPUT));

    int32_t scenario = PqParam.scenario;

    bool hdrEnable = pPQSession->getHDRInfo(&PqParam, &HDRInfo);

    bool isDebugMode = false;

    PQ_LOGI("[PQHDRAdaptor] hdrEnable[%d], scenario[%d]\n", hdrEnable, scenario);

    {
        AutoMutex lock(s_ALMutex);

        command.setPQSessionID(config.pqSessionId);

        pPQSession->getHDRConfig(&m_pHDRConfig);

        if (hdrEnable == true)
        {
            hdrEnable = PQConfig->enHDR;
            if (m_pHDRConfig.ENABLE == 0 || m_pHDRConfig.ENABLE == 1)
                hdrEnable = m_pHDRConfig.ENABLE;
        }

        m_pHDRFW->setDebugFlag(m_pHDRConfig.debugFlag);

        if ((m_pHDRConfig.driverDebugFlag & HDR_DRIVER_DEBUG_MODE) != 0)
        {
            isDebugMode = true;
        }

        PQ_LOGI("[PQHDRAdaptor] hdrEnable[%d], debugFlag[%d], driverdebugFlag[%d]", hdrEnable, m_pHDRConfig.debugFlag, m_pHDRConfig.driverDebugFlag);

        if (hdrEnable == true &&
            (scenario == MEDIA_VIDEO || scenario == MEDIA_VIDEO_CODEC) &&
            isDebugMode == false)
        {
            initHDRInitParamIn(&HDRInfo, &inParam, PqParam.u.video.isHDR2SDR,
                m_pHDRConfig.externalPanelNits, config);

            onCalculate(pPQSession, &inParam, &outParam, config, &HDRInfo);
            if (isSEIInfoExist(&HDRInfo) || memcmp(&HDRInfo, &m_lastHDRInfo, sizeof(uint32_t) * 3) != 0)
            {
                memcpy(&m_lastHDRInfo,&HDRInfo,sizeof(m_lastHDRInfo));
            }
        }
    }

    uint32_t hdr_hw_relay = 0;
    uint32_t hdr_hw_en = 1;

    if (isDebugMode == false && (hdrEnable == false || outParam.hdr_en == 0))
    {
        hdr_hw_relay = 1;
#if defined(HDR_IN_RDMA)
        hdr_hw_en = 0;
#endif
    }

    if (curveOnly == false)
    {
        // Set MDP_HDR relay mode
        MM_REG_WRITE(command, MDP_HDR_RELAY, hdr_hw_relay, 0x1);

        // Set MDP_HDR enable
        MM_REG_WRITE(command, MDP_HDR_TOP, hdr_hw_en, 0x1);
    }

    bool bypassHWAccess = tuningHDROutput(&outParam, scenario);

    PQ_LOGI("[PQHDRAdaptor] hdr_hw_relay[%d], hdrEnable[%d], hdr_en[%d], m_pHDRConfig.debugFlag[%d], bypassHWAccess[%d]",
        hdr_hw_relay, hdrEnable, outParam.hdr_en, isDebugMode, bypassHWAccess);

    if (hdr_hw_relay == 0 && isDebugMode == false && bypassHWAccess == false)
    {
        if (curveOnly == false)
        {
            onConfigHDR(command, &outParam, pFrameConfigLabel);
        }
        else
        {
            onUpdateHDR(command, &outParam, pFrameConfigLabel);
        }
    }

    return (hdrEnable && outParam.hdr_en);
}

void PQHDRAdaptor::onConfigHDR(DpCommand &command, DHDROUTPUT *outParam, int32_t *pFrameConfigLabel)
{
    int32_t index = 0;

    MM_REG_WRITE(command, MDP_HDR_HIST_ADDR, 1 << 13 | 0 << 0, 0x0000203F, &pFrameConfigLabel[index++]); //set hist_addr to 0 at frame start
#if defined(HDR_IN_RDMA)
    MM_REG_WRITE(command, MDP_HDR_TOP, outParam->bt2020_in << 1
                                 | outParam->sdr_gamma << 12
                                 | outParam->BBC_gamma << 16
                                 | outParam->bt2020_const_luma << 17, 0x00031002, &pFrameConfigLabel[index++]);
#endif
    // y is config by frame, x is config by tile
    MM_REG_WRITE(command, MDP_HDR_HIST_CTRL_0,  outParam->hist_begin_y << 16, 0x1FFF0000, &pFrameConfigLabel[index++]);
    MM_REG_WRITE(command, MDP_HDR_HIST_CTRL_1,  outParam->hist_end_y << 16, 0x1FFF0000, &pFrameConfigLabel[index++]);

    MM_REG_WRITE(command, MDP_HDR_3x3_COEF_0, outParam->gamut_matrix_en << 0, 0x00000001, &pFrameConfigLabel[index++]);
    MM_REG_WRITE(command, MDP_HDR_3x3_COEF_1, outParam->c01 << 16
                                        | outParam->c00 << 0, 0xFFFFFFFF, &pFrameConfigLabel[index++]);
    MM_REG_WRITE(command, MDP_HDR_3x3_COEF_2, outParam->c10 << 16
                                        | outParam->c02 << 0, 0xFFFFFFFF, &pFrameConfigLabel[index++]);
    MM_REG_WRITE(command, MDP_HDR_3x3_COEF_3, outParam->c12 << 16
                                        | outParam->c11 << 0, 0xFFFFFFFF, &pFrameConfigLabel[index++]);
    MM_REG_WRITE(command, MDP_HDR_3x3_COEF_4, outParam->c21 << 16
                                        | outParam->c20 << 0, 0xFFFFFFFF, &pFrameConfigLabel[index++]);
    MM_REG_WRITE(command, MDP_HDR_3x3_COEF_5, outParam->c22 << 0, 0x0000FFFF, &pFrameConfigLabel[index++]);
#if defined(HDR_IN_RDMA)
    MM_REG_WRITE(command, MDP_HDR_TONE_MAP_P12, outParam->reg_p1 << 0
                                          | outParam->reg_p2 << 16, 0xFFFFFFFF, &pFrameConfigLabel[index++]);
    MM_REG_WRITE(command, MDP_HDR_TONE_MAP_P34, outParam->reg_p3 << 0
                                          | outParam->reg_p4 << 16, 0xFFFFFFFF, &pFrameConfigLabel[index++]);
    MM_REG_WRITE(command, MDP_HDR_TONE_MAP_P5,  outParam->reg_p5 << 0, 0x0000FFFF, &pFrameConfigLabel[index++]);

    MM_REG_WRITE(command, MDP_HDR_TONE_MAP_S0, outParam->reg_slope0 << 0, 0x0007FFFF, &pFrameConfigLabel[index++]);
    MM_REG_WRITE(command, MDP_HDR_TONE_MAP_S1, outParam->reg_slope1 << 0, 0x0007FFFF, &pFrameConfigLabel[index++]);
    MM_REG_WRITE(command, MDP_HDR_TONE_MAP_S2, outParam->reg_slope2 << 0, 0x0007FFFF, &pFrameConfigLabel[index++]);
    MM_REG_WRITE(command, MDP_HDR_TONE_MAP_S3, outParam->reg_slope3 << 0, 0x0007FFFF, &pFrameConfigLabel[index++]);
    MM_REG_WRITE(command, MDP_HDR_TONE_MAP_S4, outParam->reg_slope4 << 0, 0x0007FFFF, &pFrameConfigLabel[index++]);
#endif
    MM_REG_WRITE(command, MDP_HDR_B_CHANNEL_NR, outParam->reg_filter_no << 1
                                          | outParam->reg_nr_strength << 4, 0x000000F2, &pFrameConfigLabel[index++]);
    MM_REG_WRITE(command, MDP_HDR_A_LUMINANCE,  outParam->reg_luma_gain_en << 0
                                          | outParam->reg_maxRGB_weight << 4, 0x000000F1, &pFrameConfigLabel[index++]);

    MM_REG_WRITE(command, MDP_HDR_GAIN_TABLE_0, 0 << HDR_GAIN_TABLE_ODD_WRITE | 1 << HDR_GAIN_TABLE_EVEN_WRITE | 0 << 0,
                                          1 << HDR_GAIN_TABLE_ODD_WRITE | 1 << HDR_GAIN_TABLE_EVEN_WRITE | 0x7F << 0, &pFrameConfigLabel[index++]); //write even, set gain_table_addr = 0 at frame start

    for (int gainTableIndex = 0; gainTableIndex < GAIN_TABLE_DATA_MAX; gainTableIndex += 2)
    {
        MM_REG_WRITE(command, MDP_HDR_GAIN_TABLE_1, outParam->GainCurve[gainTableIndex + 1] << 16
                                              | outParam->GainCurve[gainTableIndex] << 0, 0xFFFFFFFF, &pFrameConfigLabel[index++]);
    }

    MM_REG_WRITE(command, MDP_HDR_GAIN_TABLE_0, 1 << HDR_GAIN_TABLE_ODD_WRITE | 0 << HDR_GAIN_TABLE_EVEN_WRITE | 0 << 0,
                                         1 << HDR_GAIN_TABLE_ODD_WRITE | 1 << HDR_GAIN_TABLE_EVEN_WRITE | 0x7F << 0, &pFrameConfigLabel[index++]); //write odd, set gain_table_addr = 0 at frame start

    for (int gainTableIndex = 0; gainTableIndex < GAIN_TABLE_DATA_MAX; gainTableIndex += 2)
    {
        MM_REG_WRITE(command, MDP_HDR_GAIN_TABLE_1, outParam->GainCurve[gainTableIndex + 1] << 16
                                              | outParam->GainCurve[gainTableIndex] << 0, 0xFFFFFFFF, &pFrameConfigLabel[index++]);
    }

    MM_REG_WRITE(command, MDP_HDR_GAIN_TABLE_0, 1 << HDR_GAIN_TABLE_UPDATE, 1 << HDR_GAIN_TABLE_UPDATE, &pFrameConfigLabel[index++]); //set gain_table_update
}

void PQHDRAdaptor::onUpdateHDR(DpCommand &command, DHDROUTPUT *outParam, int32_t *pFrameConfigLabel)
{
    int32_t index = 0;

    MM_REG_WRITE(command, MDP_HDR_HIST_ADDR, 1 << 13 | 0 << 0, 0x0000203F, NULL, pFrameConfigLabel[index++]); //set hist_addr to 0 at frame start
#if defined(HDR_IN_RDMA)
    MM_REG_WRITE(command, MDP_HDR_TOP, outParam->bt2020_in << 1
                                 | outParam->sdr_gamma << 12
                                 | outParam->BBC_gamma << 16
                                 | outParam->bt2020_const_luma << 17, 0x00031002, NULL, pFrameConfigLabel[index++]);
#endif
    // y is config by frame, x is config by tile
    MM_REG_WRITE(command, MDP_HDR_HIST_CTRL_0,  outParam->hist_begin_y << 16, 0x1FFF0000, NULL, pFrameConfigLabel[index++]);
    MM_REG_WRITE(command, MDP_HDR_HIST_CTRL_1,  outParam->hist_end_y << 16, 0x1FFF0000, NULL, pFrameConfigLabel[index++]);

    MM_REG_WRITE(command, MDP_HDR_3x3_COEF_0, outParam->gamut_matrix_en << 0, 0x00000001, NULL, pFrameConfigLabel[index++]);
    MM_REG_WRITE(command, MDP_HDR_3x3_COEF_1, outParam->c01 << 16
                                        | outParam->c00 << 0, 0xFFFFFFFF, NULL, pFrameConfigLabel[index++]);
    MM_REG_WRITE(command, MDP_HDR_3x3_COEF_2, outParam->c10 << 16
                                        | outParam->c02 << 0, 0xFFFFFFFF, NULL, pFrameConfigLabel[index++]);
    MM_REG_WRITE(command, MDP_HDR_3x3_COEF_3, outParam->c12 << 16
                                        | outParam->c11 << 0, 0xFFFFFFFF, NULL, pFrameConfigLabel[index++]);
    MM_REG_WRITE(command, MDP_HDR_3x3_COEF_4, outParam->c21 << 16
                                        | outParam->c20 << 0, 0xFFFFFFFF, NULL, pFrameConfigLabel[index++]);
    MM_REG_WRITE(command, MDP_HDR_3x3_COEF_5, outParam->c22 << 0, 0x0000FFFF, NULL, pFrameConfigLabel[index++]);
#if defined(HDR_IN_RDMA)
    MM_REG_WRITE(command, MDP_HDR_TONE_MAP_P12, outParam->reg_p1 << 0
                                          | outParam->reg_p2 << 16, 0xFFFFFFFF, NULL, pFrameConfigLabel[index++]);
    MM_REG_WRITE(command, MDP_HDR_TONE_MAP_P34, outParam->reg_p3 << 0
                                          | outParam->reg_p4 << 16, 0xFFFFFFFF, NULL, pFrameConfigLabel[index++]);
    MM_REG_WRITE(command, MDP_HDR_TONE_MAP_P5,  outParam->reg_p5 << 0, 0x0000FFFF, NULL, pFrameConfigLabel[index++]);

    MM_REG_WRITE(command, MDP_HDR_TONE_MAP_S0, outParam->reg_slope0 << 0, 0x0007FFFF, NULL, pFrameConfigLabel[index++]);
    MM_REG_WRITE(command, MDP_HDR_TONE_MAP_S1, outParam->reg_slope1 << 0, 0x0007FFFF, NULL, pFrameConfigLabel[index++]);
    MM_REG_WRITE(command, MDP_HDR_TONE_MAP_S2, outParam->reg_slope2 << 0, 0x0007FFFF, NULL, pFrameConfigLabel[index++]);
    MM_REG_WRITE(command, MDP_HDR_TONE_MAP_S3, outParam->reg_slope3 << 0, 0x0007FFFF, NULL, pFrameConfigLabel[index++]);
    MM_REG_WRITE(command, MDP_HDR_TONE_MAP_S4, outParam->reg_slope4 << 0, 0x0007FFFF, NULL, pFrameConfigLabel[index++]);
#endif
    MM_REG_WRITE(command, MDP_HDR_B_CHANNEL_NR, outParam->reg_filter_no << 1
                                          | outParam->reg_nr_strength << 4, 0x000000F2, NULL, pFrameConfigLabel[index++]);
    MM_REG_WRITE(command, MDP_HDR_A_LUMINANCE,  outParam->reg_luma_gain_en << 0
                                          | outParam->reg_maxRGB_weight << 4, 0x000000F1, NULL, pFrameConfigLabel[index++]);

    MM_REG_WRITE(command, MDP_HDR_GAIN_TABLE_0, 0 << HDR_GAIN_TABLE_ODD_WRITE | 1 << HDR_GAIN_TABLE_EVEN_WRITE | 0 << 0,
                                          1 << HDR_GAIN_TABLE_ODD_WRITE | 1 << HDR_GAIN_TABLE_EVEN_WRITE | 0x7F << 0, NULL, pFrameConfigLabel[index++]); //write even, set gain_table_addr = 0 at frame start

    for (int gainTableIndex = 0; gainTableIndex < GAIN_TABLE_DATA_MAX; gainTableIndex += 2)
    {
        MM_REG_WRITE(command, MDP_HDR_GAIN_TABLE_1, outParam->GainCurve[gainTableIndex + 1] << 16
                                              | outParam->GainCurve[gainTableIndex] << 0, 0xFFFFFFFF, NULL, pFrameConfigLabel[index++]);
    }

    MM_REG_WRITE(command, MDP_HDR_GAIN_TABLE_0, 1 << HDR_GAIN_TABLE_ODD_WRITE | 0 << HDR_GAIN_TABLE_EVEN_WRITE | 0 << 0,
                                         1 << HDR_GAIN_TABLE_ODD_WRITE | 1 << HDR_GAIN_TABLE_EVEN_WRITE | 0x7F << 0, NULL, pFrameConfigLabel[index++]); //write odd, set gain_table_addr = 0 at frame start

    for (int gainTableIndex = 0; gainTableIndex < GAIN_TABLE_DATA_MAX; gainTableIndex += 2)
    {
        MM_REG_WRITE(command, MDP_HDR_GAIN_TABLE_1, outParam->GainCurve[gainTableIndex + 1] << 16
                                              | outParam->GainCurve[gainTableIndex] << 0, 0xFFFFFFFF, NULL, pFrameConfigLabel[index++]);
    }

    MM_REG_WRITE(command, MDP_HDR_GAIN_TABLE_0, 1 << HDR_GAIN_TABLE_UPDATE, 1 << HDR_GAIN_TABLE_UPDATE, NULL, pFrameConfigLabel[index++]); //set gain_table_update
}

bool PQHDRAdaptor::isHDRinfoChanged(DP_VDEC_DRV_COLORDESC_T *HDRInfo)
{

    PQ_LOGI("[PQHDRAdaptor] m_lastHDRInfo.u4ColorPrimaries = %d", m_lastHDRInfo.u4ColorPrimaries);
    PQ_LOGI("[PQHDRAdaptor] m_lastHDRInfo.u4TransformCharacter = %d", m_lastHDRInfo.u4TransformCharacter);
    PQ_LOGI("[PQHDRAdaptor] m_lastHDRInfo.u4MatrixCoeffs = %d", m_lastHDRInfo.u4MatrixCoeffs);

    for (int index = 0; index < 3; index++)
    {
        PQ_LOGI("[PQHDRAdaptor] m_lastHDRInfo.u4DisplayPrimariesX[%d] = %d", index, m_lastHDRInfo.u4DisplayPrimariesX[index]);
        PQ_LOGI("[PQHDRAdaptor] m_lastHDRInfo.u4DisplayPrimariesY[%d] = %d", index, m_lastHDRInfo.u4DisplayPrimariesY[index]);
    }

    PQ_LOGI("[PQHDRAdaptor] m_lastHDRInfo.u4WhitePointX = %d", m_lastHDRInfo.u4WhitePointX);
    PQ_LOGI("[PQHDRAdaptor] m_lastHDRInfo.u4WhitePointY = %d", m_lastHDRInfo.u4WhitePointY);
    PQ_LOGI("[PQHDRAdaptor] m_lastHDRInfo.u4MaxDisplayMasteringLuminance = %d", m_lastHDRInfo.u4MaxDisplayMasteringLuminance);
    PQ_LOGI("[PQHDRAdaptor] m_lastHDRInfo.u4MinDisplayMasteringLuminance = %d", m_lastHDRInfo.u4MinDisplayMasteringLuminance);
    PQ_LOGI("[PQHDRAdaptor] m_lastHDRInfo.u4MaxContentLightLevel = %d", m_lastHDRInfo.u4MaxContentLightLevel);
    PQ_LOGI("[PQHDRAdaptor] m_lastHDRInfo.u4MaxPicAverageLightLevel = %d", m_lastHDRInfo.u4MaxPicAverageLightLevel);


    PQ_LOGI("[PQHDRAdaptor] HDRInfo->u4ColorPrimaries = %d", HDRInfo->u4ColorPrimaries);
    PQ_LOGI("[PQHDRAdaptor] HDRInfo->u4TransformCharacter = %d", HDRInfo->u4TransformCharacter);
    PQ_LOGI("[PQHDRAdaptor] HDRInfo->u4MatrixCoeffs = %d", HDRInfo->u4MatrixCoeffs);

    for (int index = 0; index < 3; index++)
    {
        PQ_LOGI("[PQHDRAdaptor] HDRInfo->u4DisplayPrimariesX[%d] = %d", index, HDRInfo->u4DisplayPrimariesX[index]);
        PQ_LOGI("[PQHDRAdaptor] HDRInfo->u4DisplayPrimariesY[%d] = %d", index, HDRInfo->u4DisplayPrimariesY[index]);
    }

    PQ_LOGI("[PQHDRAdaptor] HDRInfo->u4WhitePointX = %d", HDRInfo->u4WhitePointX);
    PQ_LOGI("[PQHDRAdaptor] HDRInfo->u4WhitePointY = %d", HDRInfo->u4WhitePointY);
    PQ_LOGI("[PQHDRAdaptor] HDRInfo->u4MaxDisplayMasteringLuminance = %d", HDRInfo->u4MaxDisplayMasteringLuminance);
    PQ_LOGI("[PQHDRAdaptor] HDRInfo->u4MinDisplayMasteringLuminance = %d", HDRInfo->u4MinDisplayMasteringLuminance);
    PQ_LOGI("[PQHDRAdaptor] HDRInfo->u4MaxContentLightLevel = %d", HDRInfo->u4MaxContentLightLevel);
    PQ_LOGI("[PQHDRAdaptor] HDRInfo->u4MaxPicAverageLightLevel = %d", HDRInfo->u4MaxPicAverageLightLevel);

    if (memcmp(HDRInfo, &m_lastHDRInfo, sizeof(uint32_t) * 3) != 0)
    {
        return true;
    }
    else if (memcmp(&(HDRInfo->u4DisplayPrimariesX[0]), &(m_lastHDRInfo.u4DisplayPrimariesX[0]), sizeof(uint32_t) * 12) != 0)
    {
        return isSEIInfoExist(HDRInfo);
    }

    return false;
}

bool PQHDRAdaptor::isSEIInfoExist(DP_VDEC_DRV_COLORDESC_T *HDRInfo)
{
        if (HDRInfo->u4DisplayPrimariesX[0] == 0 &&
            HDRInfo->u4DisplayPrimariesX[1] == 0 &&
            HDRInfo->u4DisplayPrimariesX[2] == 0 &&
            HDRInfo->u4DisplayPrimariesY[0] == 0 &&
            HDRInfo->u4DisplayPrimariesY[1] == 0 &&
            HDRInfo->u4DisplayPrimariesY[2] == 0 &&
            HDRInfo->u4WhitePointX == 0 &&
            HDRInfo->u4WhitePointY == 0 &&
            HDRInfo->u4MaxDisplayMasteringLuminance == 0 &&
            HDRInfo->u4MinDisplayMasteringLuminance == 0 &&
            HDRInfo->u4MaxContentLightLevel == 0 &&
            HDRInfo->u4MaxPicAverageLightLevel == 0)
            return false;
        else
            return true;
}

int32_t PQHDRAdaptor::onCalculate(PQSession* pPQSession, DHDRINPUT *input, DHDROUTPUT *output, DpConfig &config
                                ,DP_VDEC_DRV_COLORDESC_T *HDRInfo)
{
    HDRHandle* currHDR_p;
    uint64_t id = pPQSession->getID();

    currHDR_p = pPQSession->getHDRHandle();
    checkAndResetUnusedHDRFW(currHDR_p, id);

    PQHDRRegInfoList* pWaitingHistList = currHDR_p->pWaitingHistList;
    PQHDRRegInfoList* pDoneHistList    = currHDR_p->pDoneHistList;
    DpCondition* pHistListCond = currHDR_p->pHistListCond;

    //waiting for refer frame done
    while (pWaitingHistList->size() >= PQ_REFER_STEP)
    {
        PQ_LOGI("[PQHDRAdaptor] onCalculate(), id[%llx]  waiting[%d] done[%d] waiting...\n", id, pWaitingHistList->size(), pDoneHistList->size());
        pHistListCond->wait(s_ALMutex);

        //re-get handle after waiting
        currHDR_p = pPQSession->getHDRHandle();
        pWaitingHistList = currHDR_p->pWaitingHistList;
        pDoneHistList    = currHDR_p->pDoneHistList;
        pHistListCond    = currHDR_p->pHistListCond;
    }

    PQ_LOGI("[PQHDRAdaptor] onCalculate(), id[%llx]  waiting[%d] done[%d]....\n", id, pWaitingHistList->size(), pDoneHistList->size());

    DpPqParam param;
    pPQSession->getPQParam(&param);
    uint32_t step = PQ_REFER_STEP - pWaitingHistList->size();

    //the first PQ_REFER_STEP frames or video thumbnail, just return default hdr setting
    if ((pDoneHistList->size() < step) || (param.scenario == MEDIA_VIDEO_CODEC && param.enable == false))
    {
        if (!isHDRinfoChanged(HDRInfo)) //different video but same hdr info
        {
            memcpy(output, &(m_lastOutput), sizeof(DHDROUTPUT));
        }
        else
        {
            delete m_pHDRFW;

            m_pHDRFW = new CPQHDRFW;
            m_pHDRFW->setDebugFlag(m_pHDRConfig.debugFlag);
            memcpy(m_pHDRFW->pHDRFWReg, &m_initHDRFWReg, sizeof(HDRFWReg));

            tuningHDRInput(input, param.scenario);
            m_pHDRFW->onInitPlatform(input, output);
            memcpy(&m_lastOutput, output, sizeof(DHDROUTPUT));
            PQ_LOGI("[PQHDRAdaptor] first frame!  owner[%llx]", id);
        }

    }
    else
    {
        PQHDRRegInfoList::iterator iter = pDoneHistList->begin();
        std::advance(iter, pDoneHistList->size() - step);

        initHDRFWinput(input, currHDR_p, config);

        memcpy(&(input->RGBmaxHistogram_1), &(iter->hist), HDR_TOTAL_HISTOGRAM_NUM * sizeof(uint32_t));
        input->iHWReg.UPpos = ((iter->LetterBoxPos & UPposMask) >> UPposShift);
        input->iHWReg.DNpos = ((iter->LetterBoxPos & DNposMask) >> DNposShift);

        iter->ref--;

        if(iter->ref == 0)
        {
            pDoneHistList->erase(iter);
        }

        tuningHDRInput(input, param.scenario);

        tuningHDRSWReg(m_pHDRFW->pHDRFWReg , param.scenario);

        memcpy(output, &m_lastOutput, sizeof(DHDROUTPUT));
        m_pHDRFW->onCalculate(input, output);
        memcpy(&m_lastOutput, output, sizeof(DHDROUTPUT));
    }
    //push one node into waiting list
    PQHDRRegInfo HDRRegInfo;
    HDRRegInfo.ref = 1;//reference 1 by 1
    pWaitingHistList->push_back(HDRRegInfo);

    // update time of curr instance
    PQ_TIMER_GET_CURRENT_TIME(currHDR_p->workTime);

    return 0;
}

void PQHDRAdaptor::checkAndResetUnusedHDRFW(HDRHandle* currHDR_p, uint64_t id)
{
    PQTimeValue currTime;
    PQ_TIMER_GET_CURRENT_TIME(currTime);
    int32_t diff;

    PQ_TIMER_GET_DURATION_IN_MS(currHDR_p->workTime, currTime, diff);

    /*if HDR object is unused for a while, treat it as different video content and reset
      HDR related members, or the continuity of histogram may be wrong. */
    if (diff >= HDR_NEW_CLIP_TIME_INTERVAL)
    {
        /*reset HDR FW object, it will be newed later in first frame condition*/
        //delete currHDR_p->pHDRFW;
        //currHDR_p->pHDRFW = NULL;

        currHDR_p->pHistListCond->signal();

        /*reset DC lists, it wll trigger first frame condition later*/
        delete currHDR_p->pWaitingHistList;
        delete currHDR_p->pDoneHistList;
        delete currHDR_p->pHistListCond;

        currHDR_p->pWaitingHistList = new PQHDRRegInfoList;
        currHDR_p->pDoneHistList = new PQHDRRegInfoList;
        currHDR_p->pHistListCond = new DpCondition();

        PQ_LOGI("[PQHDRAdaptor] checkAndResetUnusedHDRFW(), id[%llx], time diff[%d]", id, diff);
    }
}

void PQHDRAdaptor::initHDRFWinput(DHDRINPUT *input, HDRHandle *currHDR_p, DpConfig &config)
{
    input->iHWReg.sdr_gamma = m_lastOutput.sdr_gamma;
    input->iHWReg.BBC_gamma = m_lastOutput.BBC_gamma;
    input->iHWReg.reg_hist_en = m_lastOutput.reg_hist_en;
    input->iHWReg.lbox_det_en = 1;

    input->cwidth = config.inCropWidth;
    input->cheight = config.inCropHeight;
    input->resolution_change = 0;

    PQ_LOGI("[PQHDRAdaptor] input->cwidth = %d, input->cheight = %d", input->cwidth, input->cheight);
    PQ_LOGI("[PQHDRAdaptor] m_lastWidth = %d, m_lastHeight = %d", m_lastWidth, m_lastHeight);

    if (m_lastWidth != config.inCropWidth || m_lastHeight != config.inCropHeight)
    {
        input->resolution_change = 1;
        m_lastWidth = config.inCropWidth;
        m_lastHeight = config.inCropHeight;
    }
}

void PQHDRAdaptor::initHDRInitParamIn(DP_VDEC_DRV_COLORDESC_T *HDRInfo, DHDRINPUT *input,
                            bool isHDR2SDR, uint32_t externalPanelNits, DpConfig &config)
{
    memcpy(&(input->HDR2SDR_STMDInfo), HDRInfo, sizeof(DP_VDEC_DRV_COLORDESC_T));

    if (isHDR2SDR)
    {
        /*external display*/
        input->panel_spec.panel_nits = externalPanelNits;
        input->panel_spec.gamut = REC709; /*always set REC709 for external display*/
    }
    else
    {
        /*internal display*/
        input->panel_spec.panel_nits = m_internalDispPanelSpec.panel_nits;
        input->panel_spec.gamut = m_internalDispPanelSpec.gamut;
    }

    m_pHDRFW->pHDRFWReg->panel_spec.panel_nits = input->panel_spec.panel_nits;

    PQ_LOGI("[PQHDRAdaptor] isHDR2SDR = %d, panel_nits = %d, panel_spec.gamut = %d", isHDR2SDR, input->panel_spec.panel_nits, input->panel_spec.gamut);
    PQ_LOGI("[PQHDRAdaptor] externalPanelNits = %d, m_internalDispPanelSpec.panel_nits = %d", externalPanelNits, m_internalDispPanelSpec.panel_nits);
}

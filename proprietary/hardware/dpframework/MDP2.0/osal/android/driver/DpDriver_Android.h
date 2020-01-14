#ifndef __DP_DRIVER_ANDROID_H__
#define __DP_DRIVER_ANDROID_H__

#include "DpDataType.h"
#include "DpMutex.h"
#include "ddp_drv.h"
#include "cmdq_mdp_pmqos.h"
#ifdef CMDQ_V3
#include "cmdq_v3_def.h"
#else
#include "cmdq_def.h"
#endif
#include <cutils/properties.h>
#ifndef ISP_MAX_OUTPUT_PORT_NUM
#define ISP_MAX_OUTPUT_PORT_NUM (4)
#endif

class DpPathBase;

class DpCommandBase
{
public:
    enum EVENT_TYPE_ENUM
    {
        RDMA0_FRAME_DONE = CMDQ_EVENT_MDP_RDMA0_EOF,
        RDMA1_FRAME_DONE = CMDQ_EVENT_MDP_RDMA1_EOF,
        WDMA_FRAME_START = CMDQ_EVENT_MDP_WDMA_SOF,
        WDMA_FRAME_DONE  = CMDQ_EVENT_MDP_WDMA_EOF,
        WROT1_FRAME_START = CMDQ_EVENT_MDP_WROT1_SOF,
        WROT1_FRAME_DONE = CMDQ_EVENT_MDP_WROT1_WRITE_EOF,
        SYNC_WROT0_SRAM_READY = CMDQ_SYNC_RESOURCE_WROT0,
        SYNC_WROT1_SRAM_READY = CMDQ_SYNC_RESOURCE_WROT1,
#ifdef CMDQ_6765_EVENT
        IMG_DL_RELAY_FRAME_START = CMDQ_EVENT_IMG_DL_RELAY_SOF,
#endif
        RDMA0_FRAME_START = CMDQ_EVENT_MDP_RDMA0_SOF,
        RSZ0_FRAME_START = CMDQ_EVENT_MDP_RSZ0_SOF,
        RSZ1_FRAME_START = CMDQ_EVENT_MDP_RSZ1_SOF,
#ifdef CMDQ_6797_EVENT
        WROT0_FRAME_START = CMDQ_EVENT_MDP_WROT0_SOF,
        WROT0_FRAME_DONE = CMDQ_EVENT_MDP_WROT0_WRITE_EOF,
        ISP_P2_0_DONE = CMDQ_EVENT_DIP_CQ_THREAD0_EOF,
        ISP_P2_1_DONE = CMDQ_EVENT_DIP_CQ_THREAD1_EOF,
        ISP_P2_2_DONE = CMDQ_EVENT_DIP_CQ_THREAD2_EOF,
        ISP_P2_3_DONE = CMDQ_EVENT_DIP_CQ_THREAD3_EOF,
        ISP_P2_4_DONE = CMDQ_EVENT_DIP_CQ_THREAD4_EOF,
        ISP_P2_5_DONE = CMDQ_EVENT_DIP_CQ_THREAD5_EOF,
        ISP_P2_6_DONE = CMDQ_EVENT_DIP_CQ_THREAD6_EOF,
        ISP_P2_7_DONE = CMDQ_EVENT_DIP_CQ_THREAD7_EOF,
        ISP_P2_8_DONE = CMDQ_EVENT_DIP_CQ_THREAD8_EOF,
        ISP_P2_9_DONE = CMDQ_EVENT_DIP_CQ_THREAD9_EOF,
        ISP_P2_10_DONE = CMDQ_EVENT_DIP_CQ_THREAD10_EOF,
        ISP_P2_11_DONE = CMDQ_EVENT_DIP_CQ_THREAD11_EOF,
        ISP_P2_12_DONE = CMDQ_EVENT_DIP_CQ_THREAD12_EOF,
        ISP_P2_13_DONE = CMDQ_EVENT_DIP_CQ_THREAD13_EOF,
        ISP_P2_14_DONE = CMDQ_EVENT_DIP_CQ_THREAD14_EOF,
#elif defined(CMDQ_6739_EVENT)
        WROT0_FRAME_START = CMDQ_EVENT_MDP_WROT0_SOF,
        WROT0_FRAME_DONE = CMDQ_EVENT_MDP_WROT0_WRITE_EOF,
        ISP_P2_0_DONE = CMDQ_EVENT_ISP_PASS2_0_EOF,
        ISP_P2_1_DONE = CMDQ_EVENT_ISP_PASS2_1_EOF,
        ISP_P2_2_DONE = CMDQ_EVENT_ISP_PASS2_2_EOF,
#else
        WROT0_FRAME_START  = CMDQ_EVENT_MDP_WROT_SOF,
        WROT0_FRAME_DONE = CMDQ_EVENT_MDP_WROT_WRITE_EOF,
        ISP_P2_0_DONE = CMDQ_EVENT_ISP_PASS2_0_EOF,
        ISP_P2_1_DONE = CMDQ_EVENT_ISP_PASS2_1_EOF,
        ISP_P2_2_DONE = CMDQ_EVENT_ISP_PASS2_2_EOF,
#endif
#ifdef CMDQ_V3
        WPE_FRAME_DONE = CMDQ_EVENT_WPE_A_EOF,
        WPE_B_FRAME_DONE = CMDQ_EVENT_WPE_B_FRAME_DONE,
        TDSHP0_FRAME_START = CMDQ_EVENT_MDP_TDSHP_SOF,
#endif
        JPEGENC_FRAME_DONE  = CMDQ_EVENT_JPEG_ENC_EOF,
        JPEGDEC_FRAME_DONE  = CMDQ_EVENT_JPEG_DEC_EOF,
        SYNC_TOKEN_VENC_EOF = CMDQ_SYNC_TOKEN_VENC_EOF,
        SYNC_TOKEN_VENC_INPUT_READY = CMDQ_SYNC_TOKEN_VENC_INPUT_READY,
        // Display driver
        DISP_RDMA0_FRAME_START = CMDQ_EVENT_DISP_RDMA0_SOF,
        DISP_RDMA0_FRAME_DONE = CMDQ_EVENT_DISP_RDMA0_EOF,
        DISP_OVL0_FRAME_DONE = CMDQ_EVENT_DISP_OVL0_EOF,
        DISP_WDMA0_FRAME_DONE = CMDQ_EVENT_DISP_WDMA0_EOF,
        // GPR tokens
        SYNC_TOKEN_GPR_READ = CMDQ_SYNC_TOKEN_GPR_SET_0,
        SYNC_TOKEN_GPR_WRITE = CMDQ_SYNC_TOKEN_GPR_SET_1,
        SYNC_TOKEN_GPR_POLL = CMDQ_SYNC_TOKEN_GPR_SET_1,
        SYNC_TOKEN_GPR_WRITE_FROM_MEM = CMDQ_SYNC_TOKEN_GPR_SET_3,
        SYNC_TOKEN_GPR_WRITE_FROM_REG = CMDQ_SYNC_TOKEN_GPR_SET_1,
    };
};

typedef struct DpReadbackRegs_t
{
    uint32_t m_regs[MAX_NUM_READBACK_REGS];
    uint32_t m_values[MAX_NUM_READBACK_REGS];
    uint32_t m_num;
    uint32_t m_engineFlag;
    uint32_t m_jpegEnc_filesize;
} DpReadbackRegs;

typedef struct DpFrameInfo_t
{
    DpColorFormat m_srcFormat;
    int32_t       m_srcWidth;
    int32_t       m_srcHeight;
    int32_t       m_srcYPitch;
    int32_t       m_srcUVPitch;
    uint32_t      m_srcMemAddr[3];
    uint32_t      m_srcMemSize[3];
    DpSecure      m_srcSecMode;

    DpColorFormat m_dstFormat[ISP_MAX_OUTPUT_PORT_NUM];
    int32_t       m_dstWidth[ISP_MAX_OUTPUT_PORT_NUM];
    int32_t       m_dstHeight[ISP_MAX_OUTPUT_PORT_NUM];
    int32_t       m_dstYPitch[ISP_MAX_OUTPUT_PORT_NUM];
    int32_t       m_dstUVPitch[ISP_MAX_OUTPUT_PORT_NUM];
    uint32_t      m_dstMemAddr[ISP_MAX_OUTPUT_PORT_NUM][3];
    uint32_t      m_dstMemSize[ISP_MAX_OUTPUT_PORT_NUM][3];
    DpSecure      m_dstSecMode[ISP_MAX_OUTPUT_PORT_NUM];
} DpFrameInfo;

class DpCommandBlock
{
public:
    enum COMMAND_TYPE_ENUM
    {
        // HW op code
        CMDQ_OP_READ  = 0x01,
        CMDQ_OP_MOVE  = 0x02,
        CMDQ_OP_WRITE = 0x04,
        CMDQ_OP_POLL  = 0x08,
        CMDQ_OP_JUMP  = 0x10,
        CMDQ_OP_WFE   = 0x20,   /* wait for event and clear */
        CMDQ_OP_EOC   = 0x40,   /* end of command */

        // pseudo op code
        CMDQ_OP_WRITE_FROM_MEM = 0x05,
        CMDQ_OP_WRITE_FROM_REG = 0x07,
        CMDQ_OP_SET_TOKEN      = 0x21,  /* set event */
        CMDQ_OP_CLEAR_TOKEN    = 0x23,  /* clear event */
        CMDQ_OP_WAIT_NO_CLEAR  = 0x25,  /* wait but not clear event */

        CMDQ_OP_READ_S = 0x80,	/* read operation (v3 only) */
        CMDQ_OP_WRITE_S = 0x90,	/* write operation (v3 only) */
        /* write with mask operation (v3 only) */
        CMDQ_OP_WRITE_S_W_MASK = 0x91,
        CMDQ_OP_LOGIC = 0xa0,	/* logic operation */
        CMDQ_OP_JUMP_C_ABSOLUTE = 0xb0, /* conditional jump (absolute) */
        CMDQ_OP_JUMP_C_RELATIVE = 0xb1, /* conditional jump (related) */
    };

    enum CMDQ_LOGIC_ENUM {
        CMDQ_LOGIC_ASSIGN = 0,
        CMDQ_LOGIC_ADD = 1,
        CMDQ_LOGIC_SUBTRACT = 2,
        CMDQ_LOGIC_MULTIPLY = 3,
        CMDQ_LOGIC_XOR = 8,
        CMDQ_LOGIC_NOT = 9,
        CMDQ_LOGIC_OR = 10,
        CMDQ_LOGIC_AND = 11,
        CMDQ_LOGIC_LEFT_SHIFT = 12,
        CMDQ_LOGIC_RIGHT_SHIFT = 13
    };

    enum DATA_REG_TYPE_ENUM
    {
        GPR_READ = CMDQ_DATA_REG_JPEG,                              // R0
        GPR_READ_DST = CMDQ_DATA_REG_JPEG_DST,                      // P1

        GPR_WRITE = CMDQ_DATA_REG_2D_SHARPNESS_0,                   // R5
        //GPR_WRITE_DST = CMDQ_DATA_REG_2D_SHARPNESS_0_DST,           // P4

        GPR_POLL = CMDQ_DATA_REG_2D_SHARPNESS_0,                    // R5
        //GPR_POLL_DST = CMDQ_DATA_REG_2D_SHARPNESS_0_DST,            // P4

        GPR_WRITE_FROM_MEM = CMDQ_DATA_REG_PQ_COLOR,                // R4
        GPR_WRITE_FROM_MEM_DST = CMDQ_DATA_REG_PQ_COLOR_DST,        // P3

        GPR_WRITE_FROM_REG = CMDQ_DATA_REG_2D_SHARPNESS_0,          // R5
        //GPR_WRITE_FROM_REG_DST = CMDQ_DATA_REG_2D_SHARPNESS_0_DST,  // P4
    };

    DpCommandBlock()
    {
    }

    virtual ~DpCommandBlock()
    {
    }

    virtual uint32_t getScenario() = 0;

    virtual uint32_t getPriority() = 0;

    virtual uint32_t getEngineFlag() = 0;

    virtual uint32_t getSecurePortFlag() = 0;

    virtual void* getBlockBaseSW() = 0;

    virtual uint32_t getBlockSize() = 0;

    virtual void* getSecureAddrMD() = 0;

    virtual uint32_t getSecureAddrCount() = 0;

    virtual DpFrameInfo getFrameInfo() = 0;

    virtual void setRegDstNum(int32_t regNum) = 0;

    virtual int32_t getRegDstNum() = 0;

    virtual uint32_t* getReadbackRegs(uint32_t& numReadRegs) = 0;

    virtual uint32_t* getReadbackValues(uint32_t& numValues) = 0;

    virtual uint32_t getISPDebugDumpRegs() = 0;

    virtual char* getFrameInfoToCMDQ() = 0;

    virtual mdp_pmqos* getMdpPmqos();

#ifdef CMDQ_V3
    virtual bool hasIspSecMeta();

    virtual cmdqSecIspMeta getSecIspMeta();
#endif
};

class DpDriver
{
public:
    typedef uint32_t EngUsages[CMDQ_MAX_ENGINE_COUNT];

    static DpDriver* getInstance();

    static void destroyInstance();

    DP_STATUS_ENUM getTDSHPGain(DISPLAY_TDSHP_T *pSharpness,
                                uint32_t        *pCurLevel);

    DP_STATUS_ENUM requireMutex(int32_t *pMutex);

    DP_STATUS_ENUM releaseMutex(int32_t mutex);

    DP_STATUS_ENUM waitFramedone(DpJobID pFrame, DpReadbackRegs &readBackRegs);

    DP_STATUS_ENUM execCommand(DpCommandBlock &block);

    DP_STATUS_ENUM submitCommand(DpCommandBlock &block, DpJobID* pRet, uint32_t extRecorderFlag = 0, char** pFrameInfo = NULL);

    DP_STATUS_ENUM queryEngUsages(EngUsages &engUsages);

    void addRefCnt(DpEngineType &sourceEng);

    void removeRefCnt(uint64_t pathFlags);

    DP_STATUS_ENUM allocatePABuffer(uint32_t numPABuffer, uint32_t *pPABuffer);

    DP_STATUS_ENUM releasePABuffer(uint32_t numPABuffer, uint32_t *pPABuffer);

    int32_t getEnableLog();

    int32_t getEnableSystrace();

    int32_t getEnableDumpBuffer();

    int32_t getEnableMet();

    char* getdumpBufferFolder();

    int32_t getEnableDumpRegister();

    int32_t getDisableReduceConfig();

    int32_t getPQSupport();

    int32_t getGlobalPQSupport();

    int32_t getMdpColor();

    DP_STATUS_ENUM notifyEngineWROT();

    int32_t getEventValue(int32_t event);

    uint32_t getMMSysMutexBase();

    DP_STATUS_ENUM setFrameInfo(DpFrameInfo frameInfo, int32_t portNum, char *frameInfoToCMDQ);

private:
    DP_STATUS_ENUM checkHandle();
    DP_STATUS_ENUM queryDeviceTreeInfo();

    static DpDriver *s_pInstance;
    static DpMutex  s_instMutex;

    int32_t         m_driverID;
    DpMutex         m_instMutex;
    int32_t         m_enableLog;
    int32_t         m_enableSystrace;
    int32_t         m_enableDumpBuffer;
    char            m_dumpBufferFolder[PROPERTY_VALUE_MAX];
    int32_t         m_enableCheckDumpReg;
    int32_t         m_enableDumpRegister;
    int32_t         m_enableCheckMet;
    int32_t         m_enableMet;
    int32_t         m_reduceConfigDisable;
    int32_t         m_pq_support;
    int32_t         m_mdpColor;

    int32_t         m_refCntRDMA0;
    int32_t         m_refCntRDMA1;

    int32_t         m_supportGlobalPQ;

    cmdqDTSDataStruct m_cmdqDts;

    DpDriver();

    DpDriver(DpDriver &other);

    ~DpDriver();
};

#endif  // __DP_DRIVER_CLIENT_ANDROID_H__

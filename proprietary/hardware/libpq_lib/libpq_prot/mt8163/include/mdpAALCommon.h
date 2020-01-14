#ifndef MDPAALCOMMON_H
#define MDPAALCOMMON_H


typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
typedef signed int int32_t;

// need to rename ???
enum DREDebugFlags {
    eDebugDisabled          = 0x0,
    eDebugInput             = 0x1,
    eDebugOutput            = 0x2,
    eDebugTime              = 0x4,
    eDebugContent           = 0x8,
    eDebugDRE               = 0x40,
    eDebugFilter            = 0x80,
    eDebugBasic             = 0x200,
    eDebugAll               = 0x3FF
};

// AAL internal variables for log use
struct DREInternalReg {
    int isGlobal;
    int blk_y_idx;
    int blk_x_idx;
    int SpikeWgt[16][16];
    int SpikeValue[16][16]; //ww
    int HistSum[16][16]; //ww
    int HistSumFallBackWgt[16][16]; //ww
    int SkinWgt[16][16];
    int HECurveSet[16][16][17];
    int SpikeCurveSet[16][16][17];
    int SkinCurveSet[16][16][17];
    int LLPCurveSet[16][16][17];
    int APLCompCurveSet[16][16][17];
    int FinalCurveSet[16][16][17];
};

// AAL FW registers
struct DREReg {
    int dre_fw_en;
    int dre_curve_en;
    int dre_gain_flt_en;
    int bADLWeight1;
    int bADLWeight2;
    int bADLWeight3;
    int bBSDCGain;
    int bBSACGain;
    int bBSLevel;
    int bMIDDCGain;
    int bMIDACGain;
    int bWSDCGain;
    int bWSACGain;
    int bWSLevel;
    int dre_dync_spike_wgt_min;
    int dre_dync_spike_wgt_max;
    int dre_dync_spike_th;
    int dre_dync_spike_slope;
    int bSpikeBlendmethod;
    int bSkinWgtSlope;
    int bSkinBlendmethod;
    int SkinDummy1;
    int Layer0_Ratio;
    int Layer1_Ratio;
    int Layer2_Ratio;
    int Dark_Ratio;
    int Bright_Ratio;
    int dre_dync_flt_coef_min;
    int dre_dync_flt_coef_max;
    int dre_dync_flt_ovr_pxl_th;
    int dre_dync_flt_ovr_pxl_slope;
    int dre_iir_force_range;
    int LLPValue;
    int LLPRatio;
    int APLCompRatioLow;
    int APLCompRatioHigh;
    int FltConfSlope;
    int FltConfTh;
    int BlkHistCountRatio;
    int BinIdxDiffSlope;
    int BinIdxDiffTh;
    int BinIdxDiffWgtOft;
    int APLTh;
    int APLSlope;
    int APLWgtOft;
    int APL2Th;
    int APL2Slope;
    int APL2WgtOft;
    int APL2WgtMax;
    int BlkSpaFltEn;
    int BlkSpaFltType;
    int LoadBlkCurveEn;
    int SaveBlkCurveEn;
    int Flat_Length_Th_Base;
    int Flat_Length_Slope_Base;
    int DZ_Fallback_En;
    int DZ_Size_Th;
    int DZ_Size_Slope;
};

#endif

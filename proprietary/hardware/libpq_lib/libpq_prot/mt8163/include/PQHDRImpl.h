
#ifndef __HDR_FW__
#define __HDR_FW__

#define HDR_ANDROID_PLATFORM

#ifndef _PQ_SUPPORT_HCML_
  #ifndef HDR_ANDROID_PLATFORM
    #define __Local_Sim__
  #endif
#endif

#include <string.h>
#define hist_bins  57
#define fw_stat_bins 33

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
typedef unsigned int UINT32;

#ifndef max
  #define max(a,b)  (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
  #define min(a,b)  (((a) < (b)) ? (a) : (b))
#endif

#ifdef HDR_ANDROID_PLATFORM
  #include <android/log.h>
  #include <math.h>

  #define HDR_LOGD(fp, fmt, ...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "%s: " fmt, __FUNCTION__, ##__VA_ARGS__ )
#else
  #include <math.h>
  #include "common.h"
  #include "utilities.h"

  #define HDR_LOGD(fp, fmt, ...) fprintf(fp, fmt, ##__VA_ARGS__)

#endif


enum HDRDebugFlags {
    eHDRDebugDisabled          = 0x0,
    eHDRDebugInput             = 0x1,
    eHDRDebugOutput            = 0x2,
    eHDRDebugTime              = 0x4,
    eHDRDebugContent_Frame     = 0x8,
    eHDRDebugContent_Stream    = 0x10,
    eHDRDebugAll               = 0xFF
};


typedef struct __VDEC_DRV_COLORDESC_T
{
  // comes from sequence VUI
  UINT32 u4ColorPrimaries;                  // colour_primaries emum, 1 = REC.709, 9 = BT. 2020
  UINT32 u4TransformCharacter;              // transfer_characteristics emume, 1 = BT. 709, 14 = BT. 2020 10b, 16 = ST2084, 18 = HLG
  UINT32 u4MatrixCoeffs;                    // matrix_coeffs emum, 9 = BT. 2020 non-constant luminance, 10 = BT.2020 c

  // comes from HDR static metadata
  UINT32 u4DisplayPrimariesX[3];            // display_primaries_x
  UINT32 u4DisplayPrimariesY[3];            // display_primaries_y
  UINT32 u4WhitePointX;                     // white_point_x
  UINT32 u4WhitePointY;                     // white_point_y
  UINT32 u4MaxDisplayMasteringLuminance;    // max_display_mastering_luminance //1=1 nits
  UINT32 u4MinDisplayMasteringLuminance;    // min_display_mastering_luminance// 1= 0.0001 nits
  UINT32 u4MaxCLL;                          // max_content_light_level
  UINT32 u4MaxFALL;                         // max frame-average light level

} __VDEC_DRV_COLORDESC_T;

typedef struct iHDRHWReg
{
  UINT32 sdr_gamma; //for BT.2020 only, not for HDR
  UINT32 BBC_gamma; //for HDR, HLG
  UINT32 reg_hist_en;

  UINT32 lbox_det_en;
  UINT32 UPpos;
  UINT32 DNpos;
} iHDRHWReg;

typedef enum GAMUT
{
  REC709 = 0,
  Others = 1, //by customer's panel spec
} GAMUT;

typedef struct PANEL_SPEC
{
  int panel_nits;
  enum GAMUT gamut;
} PANEL_SPEC;

// =================== HDR SW registers ================== //
typedef struct HDRFWReg
{

  int dynamic_rendering; //SW register, 1b

  int static_mode_gain_curve_idx; // 4b, 0~8

  int min_histogram_window;

  int gain_curve_boost_speed; //8b, 0~255

  int hlg_gain_curve_idx; // 4b, 0~9

  int tgt_hist_idx_array[10]; //0~32, 6b.  {20, 22, 24, 24, 25, 26, 27, 28, 29, 29}; //SW register

  int sw_p1, sw_p2, sw_p3, sw_p4, sw_p5; // 16+3 = 19b
  int sw_slope0, sw_slope1, sw_slope2, sw_slope3, sw_slope4; //12+4 = 16b

  int high_bright; //32b, 0~400000, SW register

  int dark_scene_slope1, dark_scene_slope2, dark_scene_darken_rate; // 9b, 0~256
  int dark_scene_p1, dark_scene_p2; // 32b, 0~400000

  int normal_scene_slope1, normal_scene_slope2, normal_scene_darken_rate; // 9b, 0~256
  int normal_scene_p1, normal_scene_p2; // 32b, 0~400000

  int bright_scene_slope1, bright_scene_slope2; // 9b, 0~256
  int bright_scene_p1, bright_scene_p2; // 32b, 0~400000

  int non_bright_scene_slope, non_bright_scene_lighten_rate; // 9b, 0~256
  int non_bright_scene_p1, non_bright_scene_p2; // 32b, 0~400000


  int panel_nits_change_rate; //SW register, 9b, 0.03 = 15/512
  int tgt_nits_change_step; //SW register, 4b, 0~15
  int fade_hist_change_rate; //SW register, 9b, 0.06 = 31/512
  int fade_tgt_nits_change_rate; //SW register, 9b, 0.1 = 51/512
  int tgt_nits_assign_factor; //SW register, 5b, 0~16
  int fade_tgt_nits_assign_factor; //SW register, 5b, 0~16

  int nr_strength_b; //4b, NR strength
  int mode_weight; //4b, maxRGB and Y bleng

  struct __VDEC_DRV_COLORDESC_T HDR2SDR_STMDInfo;
  struct PANEL_SPEC panel_spec;

  int tone_mapping_truncate_mode;
  int min_p_end;

  //3x3 matrix (gamut) 2'complement
  int c00;
  int c01;
  int c02;
  int c10;
  int c11;
  int c12;
  int c20;
  int c21;
  int c22;
  int gamut_matrix_en;

  int dynamic_mode_fix_gain_curve_en;
  int dynamic_mode_fixed_gain_curve_idx;
  int low_flicker_mode_en;

  int BT709_c00; //for customer file
  int BT709_c01;
  int BT709_c02;
  int BT709_c10;
  int BT709_c11;
  int BT709_c12;
  int BT709_c20;
  int BT709_c21;
  int BT709_c22;
  int BT2020_c00;
  int BT2020_c01;
  int BT2020_c02;
  int BT2020_c10;
  int BT2020_c11;
  int BT2020_c12;
  int BT2020_c20;
  int BT2020_c21;
  int BT2020_c22;

  int low_flicker_mode_scene_change_nits_diff;
  int low_flicker_mode_different_scene_light_decrease;
  int low_flicker_mode_different_scene_light_increase;
  int low_flicker_mode_fade_decrease;
  int low_flicker_mode_fade_increase;
  int low_flicker_mode_same_scene_chase_gap;
  int low_flicker_mode_same_scene_chase_converge_period;
  int low_flicker_mode_same_scene_chase_max_speed;


  int xpercent_histogram_tuning_en;
  int max_hist_70_100_percent_nits;
  int approach_saturate_region;

  int target_nits; //read only

  int test_en;
  int source_VUI_debug_en;

}HDRFWReg;

typedef struct DHDRINPUT{
  struct iHDRHWReg iHWReg;
  int cwidth, cheight;
  UINT32 resolution_change; // for streaming resolution change
  UINT32 RGBmaxHistogram_1[57];
  struct __VDEC_DRV_COLORDESC_T HDR2SDR_STMDInfo;
  struct PANEL_SPEC panel_spec;
} DHDRINPUT;


typedef struct DHDROUTPUT {
  UINT32 GainCurve[256];

  UINT32 reg_p1;
  UINT32 reg_p2;
  UINT32 reg_p3;
  UINT32 reg_p4;
  UINT32 reg_p5;

  UINT32 reg_slope0;
  UINT32 reg_slope1;
  UINT32 reg_slope2;
  UINT32 reg_slope3;
  UINT32 reg_slope4;

  UINT32 hist_begin_y;
  UINT32 hist_end_y;
  UINT32 hist_begin_x;
  UINT32 hist_end_x;
  UINT32 in_hsize;
  UINT32 in_vsize;

  //YUV2RGB
  UINT32 bt2020_in;
  UINT32 bt2020_const_luma;

  //ST2084 to linear
  UINT32 sdr_gamma;
  UINT32 BBC_gamma;

  //3x3 matrix (gamut) 2'complement
  UINT32 c00;
  UINT32 c01;
  UINT32 c02;
  UINT32 c10;
  UINT32 c11;
  UINT32 c12;
  UINT32 c20;
  UINT32 c21;
  UINT32 c22;
  UINT32 gamut_matrix_en;

  //lbox detection
  UINT32 UPpos;
  UINT32 DNpos;

  //histogram
  UINT32 reg_hist_en;

  //GainCurve
  UINT32 reg_luma_gain_en;

  //NR-B
  UINT32 reg_nr_strength;
  UINT32 reg_filter_no;

  //Adaptive luminance control
  UINT32 reg_maxRGB_weight;

  UINT32 hdr_en;
  UINT32 hdr_bypass;

} DHDROUTPUT;



///////////////////////////////////////////////////////////////////////////////
// DS FW Processing class
///////////////////////////////////////////////////////////////////////////////
class CPQHDRFW
{

    /* ........Dynamic HDR Rendering, functions......... */
public:

    void onCalculate(DHDRINPUT *input, DHDROUTPUT *output);
    void onInitPlatform(DHDRINPUT *input, DHDROUTPUT *output);
    void onInitPlatform(void);  // for customer setting
    void onInitCommon(void);
    int _set2sCompNum(int val, int bits);

    //void DHDRInitialize(DHDRINPUT *input);
    void tone_mapping_fw(DHDRINPUT *input);
    void tone_mapping_hw_setting(DHDROUTPUT *output);
    void gain_curve_gen_fw(DHDRINPUT *input);
    void gain_curve_hw_setting(DHDROUTPUT *output);
    void histogram_window_fw(DHDRINPUT *input, DHDROUTPUT *output);
    void setting_by_SWreg(DHDROUTPUT *output);

    int gain_curve_idx_1;
    int tgt_nits, tgt_nits_1, tgt_nits_2;
    int fw_tgt_nits, fw_tgt_nits_1;
    int panel_sdr_nits, panel_sdr_nits_1;// sdr_panel_percentage;
    int *sdr_avg_70_100_percent_nits, *sdr_avg_70_100_percent_nits_1;
    int *hist_70_100_percent_nits, *hist_70_100_percent_nits_1;
    int Hist_nits[hist_bins];
    int Hist_pdf[hist_bins], Hist_cdf[hist_bins];
    int Hist_avg_nits[hist_bins], Hist_sdr_avg_nits[hist_bins];

    int width, height;
    int iCurrFrmNum;
    int panel_nits;
    int hw_src_nits;
    int scene_change_flag;

    int GainCurve[256]; //local buffer
    int reg_p1, reg_p2, reg_p3, reg_p4, reg_p5, reg_slope0, reg_slope1, reg_slope2, reg_slope3, reg_slope4; //local buffer

    char out_buffer[600];
    char tmp_buffer[256];
    char out_buffer2[600];


    CPQHDRFW();
    ~CPQHDRFW();

    int testF;


private:
public:
    HDRFWReg *pHDRFWReg;

    UINT32 DebugFlags;
    void setDebugFlag(unsigned int debugFlag)
    {
      DebugFlags = debugFlag;
    }
};

#endif


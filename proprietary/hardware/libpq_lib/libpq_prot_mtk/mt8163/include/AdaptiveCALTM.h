#ifndef __ADAPTIVECALTM_H__
#define __ADAPTIVECALTM_H__

#include "mdpAALImpl.h"
#include "feature_custom_caltm_nvram.h"
#include "stdlib.h"

#define Max_Face_Dump 15
typedef enum ADAPTIVE_CALTM_SCENARIO_TYPE_ENUM
{
    ADAPTIVE_CALTM_SCENARIO_VIDEO,
    ADAPTIVE_CALTM_SCENARIO_PICTURE,
} ADAPTIVE_CALTM_SCENARIO_TYPE_ENUM;


struct TAdaptiveCALTM_ExtraInfo {
    int ISO;
    int LV;
    ADAPTIVE_CALTM_SCENARIO_TYPE_ENUM Scenario;
    unsigned int *LCSO;
    unsigned int LCSO_Size;
    unsigned int *DCE;
    unsigned int DCE_Size;
    unsigned int *LCE;
    unsigned int LCE_Size;
    void* mtkCameraFaceMetadata;
};

struct TAdaptiveCALTMFace_Exif{
    int CALTM_ISO;
    int CALTM_LV;
    int CALTM_face_num;
    int face_rect[Max_Face_Dump][4];
};

struct TAdaptiveCALTMFace_Dump{
    TAdaptiveCALTMFace_Exif adaptiveCALTMFace_Exif;
    int AdaptiveFaceConf[mdpDRE_BLK_NUM_Y][mdpDRE_BLK_NUM_X]; // Output map "Skin + Face infor"
    int DRESkinConf[mdpDRE_BLK_NUM_Y][mdpDRE_BLK_NUM_X]; // Input map "Skin infor"
};

struct TAdaptiveCALTMReg {
    int Enabled;
    int Strength;   // AdSkinWgt [0:255]
    int AdaptiveMethod; //Not Define yet
    int AdaptiveType;   //AdFaceU: [0:100]
    int CustomParametersSearchMode; //0: Interpolation Mode, 1: The Closet Mode
    int DebugFlag;
    int DebugTrace;
    int AdFaceL; // Lower bound of Face adaptive weight [0:100]
    int AdFaceU; // Upper bound of Face adaptive weight [0:100]
    int AdSkinWgt; // [0:255]
    int AdFaceWgt; // [0:255]
};

class TAdaptiveCALTM
{
private:
    CDRETopFW *DRETopFW;
    int adaptiveFaceConf[mdpDRE_BLK_NUM_Y][mdpDRE_BLK_NUM_X];
    int ApplyNvramAllTuningRegisterToSWRegister(FEATURE_NVRAM_CA_LTM_ALLTUNINGREG_T *AllTuningReg);
    int ComposeNvramAllTuningRegister(int Index, const FEATURE_NVRAM_CA_LTM_T *CustomParameters, FEATURE_NVRAM_CA_LTM_ALLTUNINGREG_T *AllTuningReg);
    int GetCustomParameters(const TAdaptiveCALTM_ExtraInfo &adaptivecaltm_extrainfo, const void *CustomParameters);
    int AdaptiveSWRegister(const TAdaptiveCALTM_ExtraInfo &adaptive_info);
    int onCalculateFaceConf(DRETopInput &Input, const TAdaptiveCALTM_ExtraInfo &extrainfo);
public:
    TAdaptiveCALTM(CDRETopFW *InputDREFW);
    TAdaptiveCALTMReg *AdaptiveCALTMReg;
    TAdaptiveCALTMFace_Exif *adaptiveCALTMFace_exif;
    TAdaptiveCALTMFace_Dump *adaptiveCALTMFace_dump;
    ~TAdaptiveCALTM();
    int onCalculateHW(const DREInitParam &InitParam, const TAdaptiveCALTM_ExtraInfo &adaptivecaltm_extrainfo, const void *CustomParameters, const unsigned int ispTuningFlag);
    int onCalculateSW(const DREInitParam &InitParam, DRETopInput &input, DRETopOutput *output, const TAdaptiveCALTM_ExtraInfo &adaptivecaltm_extrainfo, const void *CustomParameters, const unsigned int ispTuningFlag);

    int ImportFDInfoFromEXIF(TAdaptiveCALTM_ExtraInfo* extrainfo);
    void onCalculateSimulation(DRETopInput &input);
};


#endif

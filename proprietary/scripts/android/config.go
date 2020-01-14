package mediatek

import (
	"bufio"
	"io"
	"os"
	"path/filepath"
	"reflect"
	"sort"
	"strings"
	"sync"

	"github.com/google/blueprint/bootstrap"
	"github.com/google/blueprint/proptools"
)

var (
	initFeatureOnce sync.Once

	featureValues map[string]string
	featureNames  []string

	MtkTargetProject    string
	MtkBaseProject      string
	TargetBoardPlatform string
	MtkPlatformDir      string

	MtkPathSource         string
	MtkPathCommon         string
	MtkPathCustom         string
	MtkPathCustomPlatform string
)

type CcProperties struct {
	Srcs   []string
	Cflags []string

	Include_dirs       []string
	Local_include_dirs []string

	Shared_libs       []string
	Static_libs       []string
	Header_libs       []string
	Whole_static_libs []string
}

type JavaProperties struct {
	Aaptflags []string
}

func GetFeature(name string) string {
	variablesFileName := filepath.Join(bootstrap.BuildDir, "mtk_soong.config")
	decodeProjectConfig(variablesFileName)
	if value, ok := featureValues[name]; ok {
		return value
	}
	return ""
}

func InitVariableProperties() reflect.Value {
	variablesFileName := filepath.Join(bootstrap.BuildDir, "mtk_soong.config")
	decodeProjectConfig(variablesFileName)
	var zeroValues CcProperties
	variableType := reflect.ValueOf(&zeroValues).Type()
	var variableFields []reflect.StructField
	for _, featureName := range featureNames {
		if (featureName == "AUTO_ADD_GLOBAL_DEFINE_BY_NAME") ||
			(featureName == "AUTO_ADD_GLOBAL_DEFINE_BY_NAME_VALUE") ||
			(featureName == "AUTO_ADD_GLOBAL_DEFINE_BY_VALUE") {
			continue
		}
		variableName := proptools.FieldNameForProperty(strings.ToLower(featureName))
		variableFields = append(variableFields,
			reflect.StructField{
				Name: variableName,
				Type: variableType,
			})
	}
	customStruct := reflect.StructOf([]reflect.StructField{
		reflect.StructField{
			Name: "Mediatek_variables",
			Type: reflect.StructOf(variableFields),
		},
	})
	return reflect.New(customStruct.(reflect.Type))
}

func InitJavaProperties() reflect.Value {
	variablesFileName := filepath.Join(bootstrap.BuildDir, "mtk_soong.config")
	decodeProjectConfig(variablesFileName)
	var zeroValues JavaProperties
	variableType := reflect.ValueOf(&zeroValues).Type()
	var variableFields []reflect.StructField
	for _, featureName := range featureNames {
		if (featureName == "AUTO_ADD_GLOBAL_DEFINE_BY_NAME") ||
			(featureName == "AUTO_ADD_GLOBAL_DEFINE_BY_NAME_VALUE") ||
			(featureName == "AUTO_ADD_GLOBAL_DEFINE_BY_VALUE") {
			continue
		}
		variableName := proptools.FieldNameForProperty(strings.ToLower(featureName))
		variableFields = append(variableFields,
			reflect.StructField{
				Name: variableName,
				Type: variableType,
			})
	}
	customStruct := reflect.StructOf([]reflect.StructField{
		reflect.StructField{
			Name: "Mediatek_variables",
			Type: reflect.StructOf(variableFields),
		},
	})
	return reflect.New(customStruct.(reflect.Type))
}

func decodeProjectConfig(filename string) error {
	mandatoryNames := []string{
		"BUILD_MTK_LDVT",
		"FPGA_EARLY_PORTING",
		"MTK_AAL_SUPPORT",
		"MTK_AUDIO",
		"MTK_AUDIO_ADPCM_SUPPORT",
		"MTK_AUDIO_ALAC_SUPPORT",
		"MTK_AUDIO_APE_SUPPORT",
		"MTK_AUDIO_BLOUD_CUSTOMPARAMETER_REV",
		"MTK_AUDIO_NUMBER_OF_SPEAKER",
		"MTK_AUDIO_RAW_SUPPORT",
		"MTK_AUDIO_TUNING_TOOL_VERSION",
		"MTK_AUDIO_TUNNELING_SUPPORT",
		"MTK_AVI_PLAYBACK_SUPPORT",
		"MTK_AFPSGO_FBT_GAME",
		"MTK_BASIC_PACKAGE",
		"MTK_BESLOUDNESS_SUPPORT",
		"MTK_BLULIGHT_DEFENDER_SUPPORT",
		"MTK_BSP_PACKAGE",
		"MTK_BT_AVRCP_TG_APP_SETTINGS_SUPPORT",
		"MTK_BWC_SUPPORT",
		"MTK_CAM_ADV_CAM_SUPPORT",
		"MTK_CAM_MMSDK_SUPPORT",
		"MTK_CCTIA_SUPPORT",
		"MTK_CHAMELEON_DISPLAY_SUPPORT",
		"MTK_CIP_SUPPORT",
		"MTK_CLEARMOTION_SUPPORT",
		"MTK_DISPLAY_120HZ_SUPPORT",
		"MTK_DP_FRAMEWORK",
		"MTK_DRM_APP",
		"MTK_DX_HDCP_SUPPORT",
		"MTK_DYNAMIC_FPS_FW_SUPPORT",
		"MTK_DYNAMIC_FPS_SUPPORT",
		"MTK_EMMC_SUPPORT",
		"MTK_FLV_PLAYBACK_SUPPORT",
		"MTK_FM_SUPPORT",
		"MTK_GAUGE_VERSION",
		"MTK_GLOBAL_PQ_SUPPORT",
		"MTK_GMO_RAM_OPTIMIZE",
		"MTK_GPS_SUPPORT",
		"MTK_HAC_SUPPORT",
		"MTK_HIFIAUDIO_SUPPORT",
		"MTK_HIGH_RESOLUTION_AUDIO_SUPPORT",
		"MTK_IN_HOUSE_TEE_SUPPORT",
		"MTK_LCM_PHYSICAL_ROTATION",
		"MTK_MDLOGGER_SUPPORT",
		"MTK_MIRAVISION_IMAGE_DC_SUPPORT",
		"MTK_MIRAVISION_SUPPORT",
		"MTK_MP2_PLAYBACK_SUPPORT",
		"MTK_MKV_PLAYBACK_ENHANCEMENT",
		"MTK_NFC_SUPPORT",
		"MTK_OD_SUPPORT",
		"MTK_OMADRM_SUPPORT",
		"MTK_PQ_SUPPORT",
		"MTK_TB_WIFI_3G_MODE",
		"MTK_TC1_FEATURE",
		"MTK_TTY_SUPPORT",
		"MTK_UFS_SUPPORT",
		"MTK_USB_PHONECALL",
		"MTK_VILTE_SUPPORT",
		"MTK_VIWIFI_SUPPORT",
		"MTK_WFD_HDCP_RX_SUPPORT",
		"MTK_WFD_HDCP_TX_SUPPORT",
		"MTK_WFD_SINK_SUPPORT",
		"MTK_WFD_SINK_UIBC_SUPPORT",
		"MTK_WLAN_SUPPORT",
		"MTK_WMA_PLAYBACK_SUPPORT",
		"MTK_WMV_PLAYBACK_SUPPORT",
		"MTK_HIGH_QUALITY_THUMBNAIL",
		"MTK_THUMBNAIL_OPTIMIZATION",
		"MTK_SWIP_WMAPRO",
		"MTK_SLOW_MOTION_VIDEO_SUPPORT",
		"NOT_MTK_BASIC_PACKAGE",
		}
	initFeatureOnce.Do(func() {
		featureValues = make(map[string]string)
		if _, err := os.Stat(filename); err == nil {
			mtkFeatureOptions, err := parseProjectConfig(filename)
			if err != nil {
				return
			}
			for name, value := range mtkFeatureOptions {
				if _, ok := featureValues[name]; !ok {
					featureValues[name] = value
					featureNames = append(featureNames, name)
				}
			}
		}
		for _, name := range mandatoryNames {
			if _, ok := featureValues[name]; !ok {
				featureNames = append(featureNames, name)
			}
		}
		sort.Strings(featureNames)
		if value, ok := featureValues["MTK_TARGET_PROJECT"]; ok {
			MtkTargetProject = value
		}
		if value, ok := featureValues["MTK_BASE_PROJECT"]; ok {
			MtkBaseProject = value
		} else {
			MtkBaseProject = MtkTargetProject
		}
		if value, ok := featureValues["TARGET_BOARD_PLATFORM"]; ok {
			TargetBoardPlatform = value
		}
		if value, ok := featureValues["MTK_PLATFORM"]; ok {
			MtkPlatformDir = strings.ToLower(value)
		}
		MtkPathSource = "vendor/amazon/mt8163/proprietary"
		MtkPathCommon = filepath.Join(MtkPathSource, "custom", "common")
		MtkPathCustom = filepath.Join(MtkPathSource, "custom", MtkBaseProject)
		if MtkPlatformDir != "" {
			MtkPathCustomPlatform = filepath.Join(MtkPathSource, "custom", MtkPlatformDir)
		}
	})
	return nil
}

func parseProjectConfig(configFile string) (map[string]string, error) {
	f, err := os.Open(configFile)
	if err != nil {
		return nil, err
	}
	defer f.Close()
	r := bufio.NewReader(f)
	options := make(map[string]string)
	var lineLast, lineCurr string
	for {
		buf, err := r.ReadString('\n')
		lineStrip := strings.TrimSpace(buf)
		if lineLast == "" {
			lineCurr = lineStrip
		} else {
			lineCurr = lineLast + " " + lineStrip
		}
		if strings.HasSuffix(lineStrip, "\\") {
			lineLast = strings.TrimRight(lineCurr, "\\")
		} else {
			lineLast = ""
			for i := 0; i < len(lineCurr); i++ {
				if lineCurr[i] == '=' {
					var j int
					if (lineCurr[i-1] == ':') ||
						(lineCurr[i-1] == '?') ||
						(lineCurr[i-1] == '+') {
						j = i - 1
					} else {
						j = i
					}
					key := strings.TrimSpace(string(lineCurr[:j]))
					value := strings.TrimSpace(string(lineCurr[i+1:]))
					options[key] = value
					break
				}
			}
		}
		if err == io.EOF {
			break
		}
	}
	return options, nil
}

/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2012. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek Software")
 * have been modified by MediaTek Inc. All revisions are subject to any receiver's
 * applicable license agreements with MediaTek Inc.
 */

#ifndef ANDROID_SENSORS_H
#define ANDROID_SENSORS_H

#include <stdint.h>
#include <errno.h>
#include <sys/cdefs.h>
#include <sys/types.h>

#include <linux/input.h>

#include <hardware/hardware.h>
#include <hardware/sensors.h>

__BEGIN_DECLS

/*****************************************************************************/

int init_nusensors(hw_module_t const* module, hw_device_t** device);
//static int g_sensor_user_count[32];

/*****************************************************************************/

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
/*
#define ID_A  (0)
#define ID_M  (1)
#define ID_O  (2)
#define ID_Gyro  (3)

#define ID_P  (7)
#define ID_L  (4)
*/

/*****************************************************************************/

/*
 * The SENSORS Module
 */

/* the CM3602 is a binary proximity sensor that triggers around 9 cm on
 * this hardware */

/*****************************************************************************/

#define CM_DEVICE_NAME      "/dev/hwmsensor"
#define LS_DEVICE_NAME      "/dev/hwmsensor"


#define EVENT_TYPE_ACCEL_X                      ABS_X
#define EVENT_TYPE_ACCEL_Y                      ABS_Y
#define EVENT_TYPE_ACCEL_Z                      ABS_Z
#define EVENT_TYPE_ACCEL_UPDATE                 REL_X
#define EVENT_TYPE_ACCEL_STATUS             ABS_WHEEL
#define EVENT_TYPE_ACCEL_TIMESTAMP_HI           REL_HWHEEL
#define EVENT_TYPE_ACCEL_TIMESTAMP_LO           REL_DIAL

#define EVENT_TYPE_GYRO_X                          ABS_X
#define EVENT_TYPE_GYRO_Y                          ABS_Y
#define EVENT_TYPE_GYRO_Z                          ABS_Z
#define EVENT_TYPE_GYRO_UPDATE                     REL_X
#define EVENT_TYPE_GYRO_STATUS                 ABS_WHEEL
#define EVENT_TYPE_GYRO_TIMESTAMP_HI            REL_HWHEEL
#define EVENT_TYPE_GYRO_TIMESTAMP_LO            REL_DIAL

#define EVENT_TYPE_ORIENT_X                      ABS_RX
#define EVENT_TYPE_ORIENT_Y                      ABS_RY
#define EVENT_TYPE_ORIENT_Z                         ABS_RZ
#define EVENT_TYPE_ORIENT_UPDATE                 REL_RX
#define EVENT_TYPE_ORIENT_STATUS            ABS_THROTTLE
#define EVENT_TYPE_ORIENT_TIMESTAMP_HI           REL_WHEEL
#define EVENT_TYPE_ORIENT_TIMESTAMP_LO           REL_MISC

#define EVENT_TYPE_MAG_X                           ABS_X
#define EVENT_TYPE_MAG_Y                           ABS_Y
#define EVENT_TYPE_MAG_Z                           ABS_Z
#define EVENT_TYPE_MAG_UPDATE                      REL_X
#define EVENT_TYPE_MAG_STATUS                ABS_WHEEL
#define EVENT_TYPE_MAG_TIMESTAMP_HI              REL_HWHEEL
#define EVENT_TYPE_MAG_TIMESTAMP_LO              REL_DIAL

#define EVENT_TYPE_TEMPERATURE_VALUE     ABS_THROTTLE
#define EVENT_TYPE_TEMPERATURE_STATUS      ABS_X

#define EVENT_TYPE_BARO_VALUE                      REL_X
#define EVENT_TYPE_BARO_STATUS                     ABS_WHEEL
#define EVENT_TYPE_BARO_TIMESTAMP_HI             REL_HWHEEL
#define EVENT_TYPE_BARO_TIMESTAMP_LO             REL_DIAL

#define EVENT_TYPE_HMDY_VALUE                      REL_X
#define EVENT_TYPE_HMDY_STATUS                     ABS_WHEEL


#define EVENT_TYPE_STEP_COUNTER_VALUE              ABS_X
#define EVENT_TYPE_STEP_DETECTOR_VALUE          REL_Y
#define EVENT_TYPE_SIGNIFICANT_VALUE            REL_Z

#define EVENT_TYPE_INPK_VALUE            0x1
#define EVENT_TYPE_STATIONARY_VALUE      0x2

#define EVENT_TYPE_SHK_VALUE             REL_X
#define EVENT_TYPE_FDN_VALUE             REL_X
#define EVENT_TYPE_PKUP_VALUE            REL_X
#define EVENT_TYPE_BTS_VALUE             REL_X

#define EVENT_TYPE_PEDO_LENGTH          REL_X
#define EVENT_TYPE_PEDO_FREQUENCY       REL_Y
#define EVENT_TYPE_PEDO_COUNT           REL_Z
#define EVENT_TYPE_PEDO_DISTANCE        REL_RX
#define EVENT_TYPE_PEDO_TIMESTAMP_HI    REL_HWHEEL#define EVENT_TYPE_PEDO_TIMESTAMP_LO    REL_DIAL

#define EVENT_TYPE_PDR_X                ABS_RY
#define EVENT_TYPE_PDR_Y                ABS_RZ
#define EVENT_TYPE_PDR_Z                ABS_THROTTLE
#define EVENT_TYPE_PDR_SCALAR           ABS_RUDDER
#define EVENT_TYPE_PDR_STATUS           REL_X
#define EVENT_TYPE_ACT_IN_VEHICLE       ABS_X
#define EVENT_TYPE_ACT_ON_BICYCLE       ABS_Y
#define EVENT_TYPE_ACT_ON_FOOT          ABS_Z
#define EVENT_TYPE_ACT_STILL            ABS_RX
#define EVENT_TYPE_ACT_UNKNOWN          ABS_RY
#define EVENT_TYPE_ACT_TILTING          ABS_RZ
#define EVENT_TYPE_ACT_WALKING          ABS_HAT0X
#define EVENT_TYPE_ACT_STANDING         ABS_HAT0Y
#define EVENT_TYPE_ACT_LYING            ABS_HAT1X
#define EVENT_TYPE_ACT_RUNNING          ABS_HAT1Y
#define EVENT_TYPE_ACT_CLIMBING         ABS_HAT2X
#define EVENT_TYPE_ACT_SITTING          ABS_HAT2Y
#define EVENT_TYPE_ACT_STATUS           ABS_WHEEL
#define EVENT_TYPE_ACT_TIMESTAMP_HI     REL_HWHEEL
#define EVENT_TYPE_ACT_TIMESTAMP_LO     REL_DIAL

#define EVENT_TYPE_HRM_BPM       		ABS_X
#define EVENT_TYPE_HRM_STATUS    		ABS_Y


#define EVENT_TYPE_TILT_VALUE       		ABS_X
#define EVENT_TYPE_WAG_VALUE       		ABS_X
#define EVENT_TYPE_GLG_VALUE       		ABS_X
#define EVENT_TYPE_ANSWER_CALL_VALUE            REL_X

#define EVENT_TYPE_ALS_VALUE                 ABS_X
#define EVENT_TYPE_PS_VALUE                 REL_Z
#define EVENT_TYPE_ALS_STATUS             ABS_WHEEL
#define EVENT_TYPE_PS_STATUS                REL_Y

#define EVENT_TYPE_BATCH_X                      ABS_X
#define EVENT_TYPE_BATCH_Y                      ABS_Y
#define EVENT_TYPE_BATCH_Z                      ABS_Z
#define EVENT_TYPE_BATCH_STATUS             ABS_WHEEL
#define EVENT_TYPE_SENSORTYPE                REL_RZ
#define EVENT_TYPE_BATCH_VALUE                  ABS_RX
#define EVENT_TYPE_END_FLAG                     REL_RY
#define EVENT_TYPE_TIMESTAMP_HI    			REL_HWHEEL
#define EVENT_TYPE_TIMESTAMP_LO    			REL_DIAL
#define EVENT_TYPE_BATCH_READY                    REL_X

#define EVENT_TYPE_GRV_X               REL_RX
#define EVENT_TYPE_GRV_Y               REL_RY
#define EVENT_TYPE_GRV_Z               REL_RZ
#define EVENT_TYPE_GRV_SCALAR          REL_WHEEL
#define EVENT_TYPE_GRV_TIMESTAMP_HI    REL_HWHEEL
#define EVENT_TYPE_GRV_TIMESTAMP_LO    REL_DIAL
#define EVENT_TYPE_GRV_STATUS          REL_X

#define EVENT_TYPE_GRAV_X              REL_RX
#define EVENT_TYPE_GRAV_Y              REL_RY
#define EVENT_TYPE_GRAV_Z              REL_RZ
#define EVENT_TYPE_GRAV_TIMESTAMP_HI   REL_HWHEEL
#define EVENT_TYPE_GRAV_TIMESTAMP_LO   REL_DIAL
#define EVENT_TYPE_GRAV_STATUS         REL_X

#define EVENT_TYPE_LA_X              REL_RX
#define EVENT_TYPE_LA_Y              REL_RY
#define EVENT_TYPE_LA_Z              REL_RZ
#define EVENT_TYPE_LA_TIMESTAMP_HI   REL_HWHEEL
#define EVENT_TYPE_LA_TIMESTAMP_LO   REL_DIAL
#define EVENT_TYPE_LA_STATUS         REL_X

#define EVENT_TYPE_GMRV_X              REL_RX
#define EVENT_TYPE_GMRV_Y              REL_RY
#define EVENT_TYPE_GMRV_Z              REL_RZ
#define EVENT_TYPE_GMRV_SCALAR         REL_WHEEL
#define EVENT_TYPE_GMRV_TIMESTAMP_HI   REL_HWHEEL
#define EVENT_TYPE_GMRV_TIMESTAMP_LO   REL_DIAL
#define EVENT_TYPE_GMRV_STATUS         REL_X

#define EVENT_TYPE_RV_X              REL_RX
#define EVENT_TYPE_RV_Y              REL_RY
#define EVENT_TYPE_RV_Z              REL_RZ
#define EVENT_TYPE_RV_SCALAR         REL_WHEEL
#define EVENT_TYPE_RV_TIMESTAMP_HI   REL_HWHEEL
#define EVENT_TYPE_RV_TIMESTAMP_LO   REL_DIAL
#define EVENT_TYPE_RV_STATUS         REL_X

#define EVENT_TYPE_UNCALI_GYRO_X       ABS_X
#define EVENT_TYPE_UNCALI_GYRO_Y       ABS_Y
#define EVENT_TYPE_UNCALI_GYRO_Z       ABS_Z
#define EVENT_TYPE_UNCALI_GYRO_X_BIAS  ABS_RX
#define EVENT_TYPE_UNCALI_GYRO_Y_BIAS  ABS_RY
#define EVENT_TYPE_UNCALI_GYRO_Z_BIAS  ABS_RZ
#define EVENT_TYPE_UNCALI_GYRO_TIMESTAMP_HI   REL_HWHEEL
#define EVENT_TYPE_UNCALI_GYRO_TIMESTAMP_LO   REL_DIAL
#define EVENT_TYPE_UNCALI_GYRO_UPDATE  REL_X

#define EVENT_TYPE_UNCALI_MAG_X        ABS_X
#define EVENT_TYPE_UNCALI_MAG_Y        ABS_Y
#define EVENT_TYPE_UNCALI_MAG_Z        ABS_Z
#define EVENT_TYPE_UNCALI_MAG_X_BIAS   ABS_RX
#define EVENT_TYPE_UNCALI_MAG_Y_BIAS   ABS_RY
#define EVENT_TYPE_UNCALI_MAG_Z_BIAS   ABS_RZ
#define EVENT_TYPE_UNCALI_MAG_TIMESTAMP_HI   REL_HWHEEL
#define EVENT_TYPE_UNCALI_MAG_TIMESTAMP_LO   REL_DIAL
#define EVENT_TYPE_UNCALI_MAG_UPDATE   REL_X

#define EVENT_TYPE_STEP_COUNT                   ABS_GAS


// 720 LSG = 1G
#define LSG                         (720.0f)


// conversion of acceleration data to SI units (m/s^2)
#define CONVERT_A                   (GRAVITY_EARTH / LSG)
#define CONVERT_A_X                 (-CONVERT_A)
#define CONVERT_A_Y                 (CONVERT_A)
#define CONVERT_A_Z                 (-CONVERT_A)

// conversion of magnetic data to uT units
#define CONVERT_M                   (1.0f/16.0f)
#define CONVERT_M_X                 (-CONVERT_M)
#define CONVERT_M_Y                 (-CONVERT_M)
#define CONVERT_M_Z                 (CONVERT_M)

#define CONVERT_O                   (1.0f)
#define CONVERT_O_Y                 (CONVERT_O)
#define CONVERT_O_P                 (CONVERT_O)
#define CONVERT_O_R                 (-CONVERT_O)

#define SENSOR_STATE_MASK           (0x7FFF)

#ifdef SENSORHUB_MAX_INPUT_READER_NBEVENTS
    #define BATCH_SENSOR_MAX_READ_INPUT_NUMEVENTS 64
#else
    #define BATCH_SENSOR_MAX_READ_INPUT_NUMEVENTS 32
#endif

/*****************************************************************************/

__END_DECLS

#endif  // ANDROID_SENSORS_H

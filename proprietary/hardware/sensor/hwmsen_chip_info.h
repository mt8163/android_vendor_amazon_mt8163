/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __HWMSEN_CHIP_INFO_H__
#define __HWMSEN_CHIP_INFO_H__

#include "hwmsen_custom.h"

#ifndef ACCELEROMETER
    #define ACCELEROMETER             "ACCELEROMETER"
    #define ACCELEROMETER_VENDER         "MTK"
#endif
#ifndef ACCELEROMETER_RANGE
    #define ACCELEROMETER_RANGE        32.0f
#endif
#ifndef ACCELEROMETER_RESOLUTION
    #define ACCELEROMETER_RESOLUTION    4.0f/1024.0f
#endif
#ifndef ACCELEROMETER_POWER
    #define ACCELEROMETER_POWER        130.0f/1000.0f
#endif
#ifndef ACCELEROMETER_MINDELAY
    #define ACCELEROMETER_MINDELAY     10000
#endif

#ifndef PROXIMITY
    #define PROXIMITY             "PROXIMITY"
    #define PROXIMITY_VENDER         "MTK"
#endif
#ifndef PROXIMITY_RANGE
    #define PROXIMITY_RANGE         10.00f
#endif
#ifndef PROXIMITY_RESOLUTION
    #define PROXIMITY_RESOLUTION          1.0f
#endif
#ifndef PROXIMITY_POWER
    #define PROXIMITY_POWER            0.13f
#endif
#ifndef PROXIMITY_MINDELAY
    #define PROXIMITY_MINDELAY     0
#endif

#ifndef LIGHT
    #define LIGHT                 "LIGHT"
    #define LIGHT_VENDER             "MTK"
#endif
#ifndef LIGHT_RANGE
    #define LIGHT_RANGE            10240.0f
#endif
#ifndef LIGHT_RESOLUTION
    #define LIGHT_RESOLUTION         1.0f
#endif
#ifndef LIGHT_POWER
    #define LIGHT_POWER            0.13f
#endif
#ifndef LIGHT_MINDELAY
    #define LIGHT_MINDELAY     0
#endif

#ifndef MAGNETOMETER
    #define MAGNETOMETER             "MAGNETOMETER"
    #define MAGNETOMETER_VENDER         "MTK"
#endif
#ifndef MAGNETOMETER_RANGE
    #define MAGNETOMETER_RANGE         600.0f
#endif
#ifndef MAGNETOMETER_RESOLUTION
    #define MAGNETOMETER_RESOLUTION        0.0016667f
#endif
#ifndef MAGNETOMETER_POWER
    #define MAGNETOMETER_POWER        0.25f
#endif
#ifndef MAGNETOMETER_MINDELAY
    #define MAGNETOMETER_MINDELAY      100000
#endif

#ifndef ORIENTATION
    #define ORIENTATION             "ORIENTATION"
    #define ORIENTATION_VENDER         "MTK"
#endif
#ifndef ORIENTATION_RANGE
    #define ORIENTATION_RANGE        360.0f
#endif
#ifndef ORIENTATION_RESOLUTION
    #define ORIENTATION_RESOLUTION        1.0f
#endif
#ifndef ORIENTATION_POWER
    #define ORIENTATION_POWER        0.25f
#endif
#ifndef ORIENTATION_MINDELAY
    #define ORIENTATION_MINDELAY       100000
#endif

#ifndef UNCALI_MAG
    #define UNCALI_MAG             "UNCALI_MAG"
    #define UNCALI_MAG_VENDER         "MTK"
#endif
#ifndef UNCALI_MAG_RANGE
    #define UNCALI_MAG_RANGE         600.0f
#endif
#ifndef UNCALI_MAG_RESOLUTION
    #define UNCALI_MAG_RESOLUTION        0.0016667f
#endif
#ifndef UNCALI_MAG_POWER
    #define UNCALI_MAG_POWER        0.25f
#endif
#ifndef UNCALI_MAG_MINDELAY
    #define UNCALI_MAG_MINDELAY     20000
#endif

#ifndef GYROSCOPE
    #define GYROSCOPE             "GYROSCOPE"
    #define GYROSCOPE_VENDER         "MTK"
#endif
#ifndef GYROSCOPE_RANGE
    #define GYROSCOPE_RANGE            34.91f
#endif
#ifndef GYROSCOPE_RESOLUTION
    #define GYROSCOPE_RESOLUTION        0.0107f
#endif
#ifndef GYROSCOPE_POWER
    #define GYROSCOPE_POWER            6.1f
#endif
#ifndef GYROSCOPE_MINDELAY
    #define GYROSCOPE_MINDELAY        10000
#endif

#ifndef UNCALI_GYRO
    #define UNCALI_GYRO             "UNCALI_GYRO"
    #define UNCALI_GYRO_VENDER         "MTK"
#endif
#ifndef UNCALI_GYRO_RANGE
    #define UNCALI_GYRO_RANGE            34.91f
#endif
#ifndef UNCALI_GYRO_RESOLUTION
    #define UNCALI_GYRO_RESOLUTION        0.0107f
#endif
#ifndef UNCALI_GYRO_POWER
    #define UNCALI_GYRO_POWER            6.1f
#endif
#ifndef UNCALI_GYRO_MINDELAY
    #define UNCALI_GYRO_MINDELAY          10000
#endif

#ifndef PRESSURE
    #define PRESSURE             "PRESSURE"
    #define PRESSURE_VENDER            "MTK"
#endif
#ifndef PRESSURE_RANGE
    #define PRESSURE_RANGE             1100.0f
#endif
#ifndef PRESSURE_RESOLUTION
    #define PRESSURE_RESOLUTION         100.0f
#endif
#ifndef PRESSURE_POWER
    #define PRESSURE_POWER            0.5f
#endif
#ifndef PRESSURE_MINDELAY
    #define PRESSURE_MINDELAY         200000
#endif

#ifndef TEMPURATURE
    #define TEMPURATURE             "TEMPURATURE"
    #define TEMPURATURE_VENDER        "MTK"
#endif
#ifndef TEMPURATURE_RANGE
    #define TEMPURATURE_RANGE         85.0f
#endif
#ifndef TEMPURATURE_RESOLUTION
    #define TEMPURATURE_RESOLUTION         0.1f
#endif
#ifndef TEMPURATURE_POWER
    #define TEMPURATURE_POWER         0.5f
#endif

#ifndef HUMIDITY
    #define HUMIDITY             "HUMIDITY"
    #define HUMIDITY_VENDER        "MTK"
#endif
#ifndef HUMIDITY_RANGE
    #define HUMIDITY_RANGE         85.0f
#endif
#ifndef HUMIDITY_RESOLUTION
    #define HUMIDITY_RESOLUTION         0.1f
#endif
#ifndef HUMIDITY_POWER
    #define HUMIDITY_POWER         0.5f
#endif
#ifndef HUMIDITY_MINDELAY
    #define HUMIDITY_MINDELAY         0
#endif

#ifndef STEP_COUNTER
    #define STEP_COUNTER             "STEP_COUNTER"
    #define STEP_COUNTER_VENDER        "MTK"

    #define STEP_DETECTOR             "STEP_DETECTOR"
    #define STEP_DETECTOR_VENDER    "MTK"
#endif
#ifndef STEP_COUNTER_RANGE
    #define STEP_COUNTER_RANGE                 85.0f
    #define STEP_DETECTOR_RANGE             85.0f
#endif
#ifndef STEP_COUNTER_RESOLUTION
    #define STEP_COUNTER_RESOLUTION         0.1f
    #define STEP_DETECTOR_RESOLUTION         0.1f
#endif
#ifndef STEP_COUNTER_POWER
    #define STEP_COUNTER_POWER                 0.5f
    #define STEP_DETECTOR_POWER             0.5f
#endif
#ifndef STEP_COUNTER_MINDELAY
    #define STEP_COUNTER_MINDELAY         0
    #define STEP_DETECTOR_MINDELAY        0
#endif

#ifndef SIGNIFICANT_MOTION
    #define SIGNIFICANT_MOTION             "SIGNIFICANT_MOTION"
    #define SIGNIFICANT_MOTION_VENDER    "MTK"
#endif
#ifndef SIGNIFICANT_MOTION_RANGE
    #define SIGNIFICANT_MOTION_RANGE         85.0f
#endif
#ifndef SIGNIFICANT_MOTION_RESOLUTION
    #define SIGNIFICANT_MOTION_RESOLUTION     0.1f
#endif
#ifndef SIGNIFICANT_MOTION_POWER
    #define SIGNIFICANT_MOTION_POWER         0.5f
#endif

#ifndef IN_POCKET
    #define IN_POCKET                  "IN_POCKET"
    #define IN_POCKET_VENDER           "MTK"
#endif
#ifndef IN_POCKET_RANGE
    #define IN_POCKET_RANGE            85.0f
#endif
#ifndef IN_POCKET_RESOLUTION
    #define IN_POCKET_RESOLUTION       0.1f
#endif
#ifndef IN_POCKET_POWER
    #define IN_POCKET_POWER            0.5f
#endif

#ifndef PEDOMETER
    #define PEDOMETER                  "PEDOMETER"
    #define PEDOMETER_VENDER           "MTK"
#endif
#ifndef PEDOMETER_RANGE
    #define PEDOMETER_RANGE            85.0f
#endif
#ifndef PEDOMETER_RESOLUTION
    #define PEDOMETER_RESOLUTION       0.1f
#endif
#ifndef PEDOMETER_POWER
    #define PEDOMETER_POWER            0.5f
#endif
#ifndef PEDOMETER_MINDELAY
    #define PEDOMETER_MINDELAY         0
#endif

#ifndef ACTIVITY
    #define ACTIVITY                   "ACTIVITY"
    #define ACTIVITY_VENDER            "MTK"
#endif
#ifndef ACTIVITY_RANGE
    #define ACTIVITY_RANGE             85.0f
#endif
#ifndef ACTIVITY_RESOLUTION
    #define ACTIVITY_RESOLUTION        0.1f
#endif
#ifndef ACTIVITY_POWER
    #define ACTIVITY_POWER             0.5f
#endif
#ifndef ACTIVITY_MINDELAY
    #define ACTIVITY_MINDELAY         0
#endif

#ifndef SHAKE
    #define SHAKE                       "SHAKE"
    #define SHAKE_VENDER                "MTK"
#endif
#ifndef SHAKE_RANGE
    #define SHAKE_RANGE                 85.0f
#endif
#ifndef SHAKE_RESOLUTION
    #define SHAKE_RESOLUTION            0.1f
#endif
#ifndef SHAKE_POWER
    #define SHAKE_POWER                 0.5f
#endif

#ifndef PICK_UP
    #define PICK_UP                     "PICK_UP"
    #define PICK_UP_VENDER              "MTK"
#endif
#ifndef PICK_UP_RANGE
    #define PICK_UP_RANGE               85.0f
#endif
#ifndef PICK_UP_RESOLUTION
    #define PICK_UP_RESOLUTION          0.1f
#endif
#ifndef PICK_UP_POWER
    #define PICK_UP_POWER               0.5f
#endif

#ifndef FACE_DOWN
    #define FACE_DOWN                   "FACE_DOWN"
    #define FACE_DOWN_VENDER            "MTK"
#endif
#ifndef FACE_DOWN_RANGE
    #define FACE_DOWN_RANGE             85.0f
#endif
#ifndef FACE_DOWN_RESOLUTION
    #define FACE_DOWN_RESOLUTION        0.1f
#endif
#ifndef FACE_DOWN_POWER
    #define FACE_DOWN_POWER             0.5f
#endif

#ifndef HEART_RATE
    #define HEART_RATE                  "HEART_RATE"
    #define HEART_RATE_VENDER           "MTK"
#endif
#ifndef HEART_RATE_RANGE
    #define HEART_RATE_RANGE            500.0f
#endif
#ifndef HEART_RATE_RESOLUTION
    #define HEART_RATE_RESOLUTION       0.1f
#endif
#ifndef HEART_RATE_POWER
    #define HEART_RATE_POWER            0.5f
#endif

#ifndef TILT_DETECTOR
    #define TILT_DETECTOR               "TILT_DETECTOR"
    #define TILT_DETECTOR_VENDER        "MTK"
#endif
#ifndef TILT_DETECTOR_RANGE
    #define TILT_DETECTOR_RANGE         100.0f
#endif
#ifndef TILT_DETECTOR_RESOLUTION
    #define TILT_DETECTOR_RESOLUTION    0.1f
#endif
#ifndef TILT_DETECTOR_POWER
    #define TILT_DETECTOR_POWER         0.5f
#endif
#ifndef TILT_DETECTOR_MINDELAY
    #define TILT_DETECTOR_MINDELAY      0
#endif

#ifndef WAKE_GESTURE
    #define WAKE_GESTURE                "WAKE_GESTURE"
    #define WAKE_GESTURE_VENDER         "MTK"
#endif
#ifndef WAKE_GESTURE_RANGE
    #define WAKE_GESTURE_RANGE          85.0f
#endif
#ifndef WAKE_GESTURE_RESOLUTION
    #define WAKE_GESTURE_RESOLUTION     0.1f
#endif
#ifndef WAKE_GESTURE_POWER
    #define WAKE_GESTURE_POWER          0.5f
#endif

#ifndef GLANCE_GESTURE
    #define GLANCE_GESTURE              "GLANCE_GESTURE"
    #define GLANCE_GESTURE_VENDER       "MTK"
#endif
#ifndef GLANCE_GESTURE_RANGE
    #define GLANCE_GESTURE_RANGE        85.0f
#endif
#ifndef GLANCE_GESTURE_RESOLUTION
    #define GLANCE_GESTURE_RESOLUTION   0.1f
#endif
#ifndef GLANCE_GESTURE_POWER
    #define GLANCE_GESTURE_POWER        0.5f
#endif

#ifndef GAME_ROTATION_VECTOR
    #define GAME_ROTATION_VECTOR        "GAME ROTATION VECTOR"
    #define GAME_ROTATION_VECTOR_VENDER "MTK"
#endif
#ifndef GAME_ROTATION_VECTOR_RANGE
    #define GAME_ROTATION_VECTOR_RANGE  10240.0f
#endif
#ifndef GAME_ROTATION_VECTOR_RESOLUTION
    #define GAME_ROTATION_VECTOR_RESOLUTION 1.0f
#endif
#ifndef GAME_ROTATION_VECTOR_POWER
    #define GAME_ROTATION_VECTOR_POWER  0.5f
#endif
#ifndef GRV_MINDELAY
    #define GRV_MINDELAY          20000
#endif

#ifndef GEOMAGNETIC_ROTATION_VECTOR
    #define GEOMAGNETIC_ROTATION_VECTOR "GEOMAGNETIC ROTATION VECTOR"
    #define GEOMAGNETIC_ROTATION_VECTOR_VENDER  "MTK"
#endif
#ifndef GEOMAGNETIC_ROTATION_VECTOR_RANGE
    #define GEOMAGNETIC_ROTATION_VECTOR_RANGE   10240.0f
#endif
#ifndef GEOMAGNETIC_ROTATION_VECTOR_RESOLUTION
    #define GEOMAGNETIC_ROTATION_VECTOR_RESOLUTION  1.0f
#endif
#ifndef GEOMAGNETIC_ROTATION_VECTOR_POWER
    #define GEOMAGNETIC_ROTATION_VECTOR_POWER   0.5f
#endif
#ifndef GMRV_MINDELAY
    #define GMRV_MINDELAY         20000
#endif

#ifndef ROTATION_VECTOR
    #define ROTATION_VECTOR             "ROTATION VECTOR"
    #define ROTATION_VECTOR_VENDER      "MTK"
#endif
#ifndef ROTATION_VECTOR_RANGE
    #define ROTATION_VECTOR_RANGE       10240.0f
#endif
#ifndef ROTATION_VECTOR_RESOLUTION
    #define ROTATION_VECTOR_RESOLUTION  1.0f
#endif
#ifndef ROTATION_VECTOR_POWER
    #define ROTATION_VECTOR_POWER       0.5f
#endif
#ifndef RV_MINDELAY
    #define RV_MINDELAY       20000
#endif

#ifndef GRAVITY
    #define GRAVITY                     "GRAVITY"
    #define GRAVITY_VENDER              "MTK"
#endif
#ifndef GRAVITY_RANGE
    #define GRAVITY_RANGE               10240.0f
#endif
#ifndef GRAVITY_RESOLUTION
    #define GRAVITY_RESOLUTION          1.0f
#endif
#ifndef GRAVITY_POWER
    #define GRAVITY_POWER               0.5f
#endif
#ifndef GRAVITY_MINDELAY
    #define GRAVITY_MINDELAY            20000
#endif

#ifndef LINEARACCEL
    #define LINEARACCEL                 "LINEARACCEL"
    #define LINEARACCEL_VENDER          "MTK"
#endif
#ifndef LINEARACCEL_RANGE
    #define LINEARACCEL_RANGE           10240.0f
#endif
#ifndef LINEARACCEL_RESOLUTION
    #define LINEARACCEL_RESOLUTION      1.0f
#endif
#ifndef LINEARACCEL_POWER
    #define LINEARACCEL_POWER           0.5f
#endif
#ifndef LINEARACCEL_MINDELAY
    #define LINEARACCEL_MINDELAY        20000
#endif

#ifndef BRINGTOSEE
    #define BRINGTOSEE                  "BRING TO SEE DETECTOR"
    #define BRINGTOSEE_VENDER           "MTK"
#endif
#ifndef BRINGTOSEE_RANGE
    #define BRINGTOSEE_RANGE            10240.0f
#endif
#ifndef BRINGTOSEE_RESOLUTION
    #define BRINGTOSEE_RESOLUTION       1.0f
#endif
#ifndef BRINGTOSEE_POWER
    #define BRINGTOSEE_POWER            0.5f
#endif

#ifndef ANSWER_CALL
    #define ANSWER_CALL                 "Answer Call Detector"
    #define ANSWER_CALL_VENDER          "MTK"
#endif
#ifndef ANSWER_CALL_RANGE
    #define ANSWER_CALL_RANGE           10240.0f
#endif
#ifndef ANSWER_CALL_RESOLUTION
    #define ANSWER_CALL_RESOLUTION      1.0f
#endif
#ifndef ANSWER_CALL_POWER
    #define ANSWER_CALL_POWER           0.5f
#endif

#ifndef STATIONARY_SENSOR
    #define STATIONARY                  "STATIONARY"
    #define STATIONARY_VENDER           "MTK"
#endif
#ifndef STATIONARY_RANGE
    #define STATIONARY_RANGE            85.0f
#endif
#ifndef STATIONARY_RESOLUTION
    #define STATIONARY_RESOLUTION       0.1f
#endif
#ifndef STATIONARY_POWER
    #define STATIONARY_POWER            0.5f
#endif

#ifndef PDR
    #define PDR                         "PDR"
    #define PDR_VENDER                  "MTK"
#endif
#ifndef PDR_RANGE
    #define PDR_RANGE                   10240.0f
#endif
#ifndef PDR_RESOLUTION
    #define PDR_RESOLUTION              1.0f
#endif
#ifndef PDR_POWER
    #define PDR_POWER                   0.5f
#endif

#endif


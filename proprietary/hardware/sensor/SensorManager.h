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


#ifndef _SENSOR_MANAGER_H_

#define _SENSOR_MANAGER_H_

#include <hardware/sensors.h>
#include <utils/Log.h>
#include <utils/Mutex.h>
#include <utils/SortedVector.h>
#include <utils/KeyedVector.h>
#include <linux/hwmsensor.h>

#include "SensorContext.h"

class SensorConnection {
public:
    SensorConnection();
    bool addActiveHandle(uint32_t handle);
    bool removeActiveHandle(uint32_t handle);
    bool hasHandleIndexOf(uint32_t handle);
    size_t getNumActiveHandles() const { return mActiveHandles.size(); }
    void setMoudle(int moudle);
    int getMoudle(void);
private:
    android::SortedVector<uint32_t> mActiveHandles;
    int mMoudle;
};

struct SensorManager {
public:
    static SensorManager *getInstance();
    SensorConnection* createSensorConnection(int mSensorMoudle);
    void removeSensorConnection(SensorConnection* connection);
    void addSensorsList(sensor_t const *list, size_t count);
    int activate(SensorConnection *connection, int32_t sensor_handle, bool enabled);
    int batch(SensorConnection *connection, int32_t sensor_handle,
            int64_t sampling_period_ns,
            int64_t max_report_latency_ns);
    int flush(SensorConnection *connection, int32_t sensor_handle);
    int pollEvent(sensors_event_t* data, int count);
    int setEvent(sensors_event_t* data, int moudle);
    void setNativeConnection(SensorConnection *connection);
    void setSensorContext(sensors_poll_context_t *context);
    void setDirectConnection(SensorConnection *connection);

protected:
    SensorManager();
    SensorManager(const SensorManager& other);
    SensorManager& operator = (const SensorManager& other);
    bool isClientDisabled(SensorConnection *connection);
    bool isClientDisabledLocked(SensorConnection *connection);
    void cleanupConnection(SensorConnection *connection);
    int64_t getSensorMinDelayNs(int handle);
    size_t parsePollData(sensors_event_t *data,
        sensors_event_t *pollData, size_t pollCount);

private:
    static constexpr int32_t pollMaxBufferSize = 128;
    SensorConnection *mNativeConnection;
    SensorConnection *mDirectConnection;
    android::SortedVector<SensorConnection *> mActiveConnections;
    android::Mutex mActiveConnectionsLock;
    //android::Mutex mBatchParamsLock;
    struct BatchParams {
      int flags;
      nsecs_t batchDelay, batchTimeout;
      BatchParams() : flags(0), batchDelay(0), batchTimeout(0) {}
      BatchParams(int flag, nsecs_t delay, nsecs_t timeout): flags(flag), batchDelay(delay),
          batchTimeout(timeout) { }
      bool operator != (const BatchParams& other) {
          return other.batchDelay != batchDelay || other.batchTimeout != batchTimeout ||
                 other.flags != flags;
      }
    };
    struct Info {
        BatchParams bestBatchParams;
        android::KeyedVector<SensorConnection *, BatchParams> batchParams;

        Info() : bestBatchParams(0, -1, -1) {}
        int setBatchParamsForIdent(SensorConnection *connection, int flags, int64_t samplingPeriodNs,
                                        int64_t maxBatchReportLatencyNs);
        void selectBatchParams();
        ssize_t removeBatchParamsForIdent(SensorConnection *connection);

        int numActiveClients();
    };
    android::DefaultKeyedVector<int, Info> mActivationCount;
    android::SortedVector<SensorConnection *> mDisabledClients;
    android::Vector<sensor_t> mSensorList;
    sensors_poll_context_t *mSensorContext;
    static SensorManager *SensorManagerInstance;
};

#endif

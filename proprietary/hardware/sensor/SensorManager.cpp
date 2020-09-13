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

#include <memory>
#include <inttypes.h>
#include "SensorManager.h"
#include "DirectChannelManager.h"

#undef LOG_TAG
#define LOG_TAG "SensorManager"

#define DEBUG_CONNECTIONS false

SensorConnection::SensorConnection() {
    mMoudle = -1;
}

bool SensorConnection::addActiveHandle(uint32_t handle) {
    if (mActiveHandles.indexOf(handle) < 0) {
        mActiveHandles.add(handle);
        //ALOGE("SensorConnection::addActiveHandle add handle=%d.", handle);
        return true;
    }
    //ALOGE("SensorConnection::addActiveHandle already add handle=%d.", handle);
    return false;
}

bool SensorConnection::removeActiveHandle(uint32_t handle) {
    ssize_t index = mActiveHandles.indexOf(handle);
    if (index >= 0) {
        mActiveHandles.removeItemsAt(index, 1);
        //ALOGE("SensorConnection::addActiveHandle remove handle=%d.", handle);
    }
    return mActiveHandles.size() ? false : true;
}

bool SensorConnection::hasHandleIndexOf(uint32_t handle) {
    ssize_t index = mActiveHandles.indexOf(handle);
    if (index >= 0) {
        //ALOGE("SensorConnection::hasHandleIndexOf has.");
        return true;
    }
    //ALOGE("SensorConnection::hasHandleIndexOf has not.");
    return false;
}

void SensorConnection::setMoudle(int moudle) {
    mMoudle = moudle;
}

int SensorConnection::getMoudle(void) {
    return mMoudle;
}

SensorManager *SensorManager::SensorManagerInstance = nullptr;
SensorManager *SensorManager::getInstance() {
    if (SensorManagerInstance == nullptr) {
        SensorManager *sensors = new SensorManager;
        SensorManagerInstance = sensors;
    }
    return SensorManagerInstance;
}

SensorManager::SensorManager() {
    mSensorContext = nullptr;
    mNativeConnection = nullptr;
    mDirectConnection = nullptr;
}

bool SensorManager::isClientDisabled(SensorConnection *connection) {
    // android::Mutex::Autolock _l(mBatchParamsLock);
    return isClientDisabledLocked(connection);
}

bool SensorManager::isClientDisabledLocked(SensorConnection *connection) {
    return mDisabledClients.indexOf(connection) >= 0;
}

void SensorManager::cleanupConnection(SensorConnection *connection) {
    // android::Mutex::Autolock _l(mBatchParamsLock);
    mDisabledClients.remove(connection);
}

void SensorManager::setNativeConnection(SensorConnection *connection) {
    mNativeConnection = connection;
}

void SensorManager::setSensorContext(sensors_poll_context_t *context) {
    mSensorContext = context;
}

void SensorManager::setDirectConnection(SensorConnection *connection) {
    mDirectConnection = connection;
}

int SensorManager::Info::numActiveClients() {
    int num = 0;
    SensorManager *sensors = SensorManager::getInstance();
    for (size_t i = 0; i < batchParams.size(); ++i) {
        if (!sensors->isClientDisabledLocked(batchParams.keyAt(i))) {
            ++num;
        }
    }
    return num;
}

int SensorManager::Info::setBatchParamsForIdent(SensorConnection *connection, int flags,
                                                    int64_t samplingPeriodNs,
                                                    int64_t maxBatchReportLatencyNs) {
    ssize_t index = batchParams.indexOfKey(connection);
    if (index < 0) {
        ALOGE("setBatchParamsForIdent(connection=%p, period_ns=%" PRId64 " timeout=%" PRId64 ")",
              connection, samplingPeriodNs, maxBatchReportLatencyNs);
        return -1;
    }
    BatchParams& params = batchParams.editValueAt(index);
    params.flags = flags;
    params.batchDelay = samplingPeriodNs;
    params.batchTimeout = maxBatchReportLatencyNs;
    return 0;
}

void SensorManager::Info::selectBatchParams() {
    BatchParams bestParams(0, -1, -1);
    SensorManager *sensors = SensorManager::getInstance();

    for (size_t i = 0; i < batchParams.size(); ++i) {
        if (sensors->isClientDisabledLocked(batchParams.keyAt(i))) continue;
        BatchParams params = batchParams.valueAt(i);
        if (bestParams.batchDelay == -1 || params.batchDelay < bestParams.batchDelay) {
            bestParams.batchDelay = params.batchDelay;
        }
        if (bestParams.batchTimeout == -1 || params.batchTimeout < bestParams.batchTimeout) {
            bestParams.batchTimeout = params.batchTimeout;
        }
    }
    bestBatchParams = bestParams;
}

ssize_t SensorManager::Info::removeBatchParamsForIdent(SensorConnection *connection) {
    ssize_t idx = batchParams.removeItem(connection);
    if (idx >= 0) {
        selectBatchParams();
    }
    return idx;
}

void SensorManager::addSensorsList(sensor_t const *list, size_t count) {
    mActivationCount.setCapacity(count);
    Info model;

    for (size_t i = 0; i < count; ++i) {
        mActivationCount.add(list[i].handle - ID_OFFSET, model);
        mSensorContext->activate(list[i].handle - ID_OFFSET, 0);
        mSensorList.push_back(list[i]);
    }
}

int64_t SensorManager::getSensorMinDelayNs(int handle) {
    int64_t minDelay = -1;

    for (const auto & s : mSensorList) {
        if (s.handle == (handle + ID_OFFSET)) {
            minDelay = s.minDelay;
            return minDelay * 1000LL;
        }
    }
    return minDelay;
}

SensorConnection* SensorManager::createSensorConnection(int mSensorMoudle) {
    SensorConnection *mConnection = new SensorConnection();
    mActiveConnectionsLock.lock();
    if (mActiveConnections.indexOf(mConnection)) {
        mActiveConnections.add(mConnection);
        // ALOGE("SensorManager::privateCreateSensorConnection mActiveConnections add connection.");
    }
    mActiveConnectionsLock.unlock();
    mConnection->setMoudle(mSensorMoudle);
    ALOGI_IF(DEBUG_CONNECTIONS, "createSensorConnection connection=%p, moudle=%d.",
        mConnection, mConnection->getMoudle());
    return mConnection;
}

void SensorManager::removeSensorConnection(SensorConnection* connection) {
    ALOGI_IF(DEBUG_CONNECTIONS, "removeSensorConnection connection=%p.",
            connection);
    mActiveConnectionsLock.lock();
    ssize_t index = mActiveConnections.indexOf(connection);
    if (index >= 0) {
        mActiveConnections.removeItemsAt(index, 1);
        // ALOGE("SensorManager::privateRemoveSensorConnection mActiveConnections delete connection.");
    }
    mActiveConnectionsLock.unlock();
    cleanupConnection(connection);
    delete connection;
}

int SensorManager::activate(SensorConnection* connection, int32_t sensor_handle, bool enabled) {
    bool actuateHardware = false;

    mActiveConnectionsLock.lock();
    if (!!enabled)
        connection->addActiveHandle(sensor_handle);
    else
        connection->removeActiveHandle(sensor_handle);
    mActiveConnectionsLock.unlock();

    if (mActivationCount.indexOfKey(sensor_handle) < 0) {
        ALOGE("\tactivate >>>ERROR: handle: %d not found, sensor may load fail", sensor_handle);
        return -1;
    }

    // android::Mutex::Autolock _l(mBatchParamsLock);
    Info& info( mActivationCount.editValueFor(sensor_handle) );

    ALOGI_IF(DEBUG_CONNECTIONS, "activate: connection=%p, handle=0x%08x, enabled=%d, count=%zu",
             connection, sensor_handle, enabled, info.batchParams.size());

    if (enabled) {
        ALOGI_IF(DEBUG_CONNECTIONS, "enable index=%zd", info.batchParams.indexOfKey(connection));

        if (isClientDisabledLocked(connection)) {
            ALOGE("activate, isClientDisabledLocked(%p):true, handle:%d",
                    connection, sensor_handle);
            return -1;
        }

        if (info.batchParams.indexOfKey(connection) >= 0) {
          if (info.numActiveClients() == 1) {
              actuateHardware = true;
          }
        } else {
            ALOGE("\tactivate >>>ERROR: activate called without batch");
        }
    } else {
        ALOGI_IF(DEBUG_CONNECTIONS, "activate disable index=%zd", info.batchParams.indexOfKey(connection));
        if (info.removeBatchParamsForIdent(connection) >= 0) {
            if (info.numActiveClients() == 0) {
                actuateHardware = true;
            } else {
                ALOGI_IF(DEBUG_CONNECTIONS, "\tctivate>>> actuating h/w batch %d %d %" PRId64 " %" PRId64 "",
                        sensor_handle, info.bestBatchParams.flags, info.bestBatchParams.batchDelay,
                        info.bestBatchParams.batchTimeout);
                mSensorContext->batch(sensor_handle,
                        0,
                        info.bestBatchParams.batchDelay,
                        info.bestBatchParams.batchTimeout);
            }
        } else {
            // sensor wasn't enabled for this connection
        }

        if (isClientDisabledLocked(connection)) {
            ALOGI_IF(DEBUG_CONNECTIONS, "\tisClientDisabledLocked return");
            return 0;
        }
    }
    int err = 0;
    if (actuateHardware) {
        ALOGI_IF(DEBUG_CONNECTIONS, "\tactivate>>> actuating h/w activate handle=%d enabled=%d",
            sensor_handle, enabled);
        err = mSensorContext->activate(
                sensor_handle,
                enabled);
        ALOGE_IF(err, "activate Error %s sensor %d", enabled ? "activating" : "disabling", sensor_handle);

        if (err != 0 && enabled) {
            // Failure when enabling the sensor. Clean up on failure.
            info.removeBatchParamsForIdent(connection);
        }
    }
    return err;
}

int SensorManager::batch(SensorConnection *connection, int32_t sensor_handle,
            int64_t sampling_period_ns, int64_t max_report_latency_ns) {
    ALOGI_IF(DEBUG_CONNECTIONS, "batch handle(%d) sampling_period_ns(%" PRId64 ") max_report_latency_ns(%" PRId64 ").",
        sensor_handle, sampling_period_ns, max_report_latency_ns);

    int64_t mindelayNs = getSensorMinDelayNs(sensor_handle);
    if (mindelayNs > 0 && sampling_period_ns < mindelayNs)
        sampling_period_ns = mindelayNs;

    if (mActivationCount.indexOfKey(sensor_handle) < 0) {
        ALOGE("\tbatch >>>ERROR: handle: %d not found, sensor may load fail", sensor_handle);
        return -1;
    }

    // android::Mutex::Autolock _l(mBatchParamsLock);
    Info& info(mActivationCount.editValueFor(sensor_handle));

    if (info.batchParams.indexOfKey(connection) < 0) {
        BatchParams params(0, sampling_period_ns, max_report_latency_ns);
        info.batchParams.add(connection, params);
    } else {
        // A batch has already been called with this connection. Update the batch parameters.
        info.setBatchParamsForIdent(connection, 0, sampling_period_ns, max_report_latency_ns);
    }

    BatchParams prevBestBatchParams = info.bestBatchParams;
    // Find the minimum of all timeouts and batch_rates for this sensor.
    info.selectBatchParams();

    ALOGI_IF(DEBUG_CONNECTIONS,
            "\tbatch>>> curr_period=%" PRId64 " min_period=%" PRId64 " curr_timeout=%" PRId64 " min_timeout=%" PRId64 "",
            prevBestBatchParams.batchDelay, info.bestBatchParams.batchDelay,
            prevBestBatchParams.batchTimeout, info.bestBatchParams.batchTimeout);

    int err = 0;
    // If the min period or min timeout has changed since the last batch call, call batch.
    if (prevBestBatchParams != info.bestBatchParams) {
        ALOGI_IF(DEBUG_CONNECTIONS, "\tbatch>>> actuating h/w BATCH %d %d %" PRId64 " %" PRId64 "", sensor_handle,
                 info.bestBatchParams.flags, info.bestBatchParams.batchDelay,
                 info.bestBatchParams.batchTimeout);
        err = mSensorContext->batch(sensor_handle,
                    0, info.bestBatchParams.batchDelay, info.bestBatchParams.batchTimeout);
        if (err < 0) {
            ALOGE("batch failed %d %d %" PRId64 " %" PRId64 " err = %d",
                  sensor_handle,
                  info.bestBatchParams.flags, info.bestBatchParams.batchDelay,
                  info.bestBatchParams.batchTimeout, err);
            info.removeBatchParamsForIdent(connection);
        }
    }
    return err;
}

int SensorManager::flush(SensorConnection * /* connection */, int32_t sensor_handle) {
    ALOGI_IF(DEBUG_CONNECTIONS, "flush handle(%d).", sensor_handle);
    int err = mSensorContext->flush(sensor_handle);
    return err;
}

size_t SensorManager::parsePollData(sensors_event_t *data,
        sensors_event_t *pollData, size_t pollCount) {
    sensors_event_t event;
    size_t count = 0;

    if (pollCount == 0)
        return 0;
    mActiveConnectionsLock.lock();
    size_t nbConnections = mActiveConnections.size();
    // ALOGD_IF(DEBUG_CONNECTIONS, "poll %zu active connections", nbConnections);
    for (size_t position = 0; position < nbConnections; ++position) {
        SensorConnection *connection = mActiveConnections.itemAt(position);
        if (connection == mNativeConnection) {
            for (size_t i = 0; i < pollCount; ++i) {
                if (pollData[i].type == SENSOR_TYPE_META_DATA) {
                    ALOGD_IF(DEBUG_CONNECTIONS, "poll flush complete event sensor==%d",
                            pollData[i].meta_data.sensor);
                    data[count++] = pollData[i];
                } else {
                    int sensor_handle = pollData[i].sensor - ID_OFFSET;
                    bool has_handle = connection->hasHandleIndexOf(sensor_handle);
                    if (has_handle)
                        data[count++] = pollData[i];
                }
            }
        } else if (connection == mDirectConnection) {
            for (size_t i = 0; i < pollCount; ++i) {
                if (pollData[i].type != SENSOR_TYPE_META_DATA) {
                    int sensor_handle = pollData[i].sensor - ID_OFFSET;
                    bool has_handle = connection->hasHandleIndexOf(sensor_handle);
                    if (has_handle)
                        DirectChannelManager::getInstance()->sendDirectReportEvent(&pollData[i], 1);
                }
            }
        } else {
            for (size_t i = 0; i < pollCount; ++i) {
                if (pollData[i].type == SENSOR_TYPE_META_DATA)
                    continue;
                int sensor_handle = pollData[i].sensor - ID_OFFSET;
                bool has_handle = connection->hasHandleIndexOf(sensor_handle);
                if (has_handle) {
                    /* ALOGD_IF(DEBUG_CONNECTIONS,
                        "poll set event(handle=%d) for connection=%p, moudle=%d", sensor_handle,
                        connection, connection->getMoudle()); */
                    event = pollData[i];
                    event.sensor = sensor_handle;
                    setEvent(&event, connection->getMoudle());
                }
            }
        }
    }
    mActiveConnectionsLock.unlock();
    return count;
}

int SensorManager::pollEvent(sensors_event_t* data, int count) {
    size_t nbCount = 0;
    int pollBufferSize = count <= pollMaxBufferSize ? count : pollMaxBufferSize;
    std::unique_ptr<sensors_event_t[]> pollBuffer;
    do {
        pollBuffer.reset(new sensors_event_t[pollBufferSize]);
        int err = mSensorContext->pollEvent(pollBuffer.get(), pollBufferSize);
        if (err < 0)
            return 0; /* must return 0, otherwise sensorservice may abort */
        const size_t pollCount = (size_t)err;
        nbCount = parsePollData(data, pollBuffer.get(), pollCount);
        if (nbCount < 0)
            nbCount = 0;
    } while (!nbCount);
    return nbCount;
}

int SensorManager::setEvent(sensors_event_t *data, int moudle) {
    mSensorContext->setEvent(data, moudle);
    return 0;
}


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


#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>
#include <cutils/log.h>
#include <utils/SystemClock.h>
#include <utils/Timers.h>
#include <string.h>
#include "Gesture.h"

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "[Gesture Sensor]"
#endif

#define IGNORE_EVENT_TIME 350000000

GestureSensor::GestureSensor()
    : SensorBase(NULL, "GES_INPUTDEV_NAME"),
      mEnabled(0),
      mInputReader(32),
      mEnabledTime(0),
      input_sysfs_path_len(0),
      mPendingMask(0) {
    memset(mPendingEvents, 0, sizeof(mPendingEvents));
    mPendingEvents[inpocket].version = sizeof(sensors_event_t);
    mPendingEvents[inpocket].sensor = ID_IN_POCKET;
    mPendingEvents[inpocket].type = SENSOR_TYPE_IN_POCKET;

    mPendingEvents[stationary].version = sizeof(sensors_event_t);
    mPendingEvents[stationary].sensor = ID_STATIONARY;
    mPendingEvents[stationary].type = SENSOR_TYPE_STATIONARY;

    mdata_fd = FindDataFd();
    if (mdata_fd >= 0) {
        strcpy(input_sysfs_path, "/sys/class/misc/m_ges_misc/");
        input_sysfs_path_len = strlen(input_sysfs_path);
    }
    ALOGD("misc path =%s", input_sysfs_path);
}

GestureSensor::~GestureSensor() {
    if (mdata_fd >= 0)
        close(mdata_fd);
}

int GestureSensor::FindDataFd() {
    int fd = -1;
    int num = -1;
    char buf[64]={0};
    const char *devnum_dir = NULL;
    char buf_s[64] = {0};

    devnum_dir = "/sys/class/misc/m_ges_misc/gesdevnum";

    fd = open(devnum_dir, O_RDONLY);
    if (fd >= 0) {
        int ret = read(fd, buf, sizeof(buf));
        if (ret <= 0) {
            close(fd);
            return -1;
        }
        sscanf(buf, "%d\n", &num);
        close(fd);
    } else {
        return -1;
    }
    sprintf(buf_s, "/dev/input/event%d", num);
    fd = open(buf_s, O_RDONLY);
    ALOGE_IF(fd < 0, "couldn't find input device");
    return fd;
}

int GestureSensor::HandleToIndex(int handle) {
    switch (handle) {
    case ID_IN_POCKET:
        return inpocket;
    case ID_STATIONARY:
        return stationary;
    default:
        ALOGE("HandleToIndex(%d)\n", handle);
    }

    return -1;
}

int GestureSensor::IndexToHandle(int index) {
    switch (index) {
    case inpocket:
        return ID_IN_POCKET;
    case stationary:
        return ID_STATIONARY;
    default:
        ALOGE("IndexToHandle(%d)\n", index);
    }

    return -1;
}

int GestureSensor::enable(int32_t handle, int en) {
    int fd;
    int index;
    char buf[8];
    int flags = en ? 1 : 0;

    ALOGD("enable: handle:%d, en:%d \r\n", handle, en);
    strcpy(&input_sysfs_path[input_sysfs_path_len], "gesactive");
    ALOGD("path:%s \r\n", input_sysfs_path);
    fd = open(input_sysfs_path, O_RDWR);
    if (fd < 0) {
        ALOGD("no gesture enable control attr\r\n");
        return -1;
    }

    index = HandleToIndex(handle);
    if (index < 0) {
        close(fd);
        ALOGD("enable(%d, %d) fail", handle, en);
        return -1;
    }

    sprintf(buf, "%d : %d", handle, flags);
    if (flags) {
        mEnabled |= (1ULL << index);
        mEnabledTime = getTimestamp() + IGNORE_EVENT_TIME;
    } else {
        mEnabled &= ~(1ULL << index);
    }

    write(fd, buf, sizeof(buf));
    close(fd);
    ALOGD("enable(%d) done", mEnabled);
    return 0;
}

int GestureSensor::setDelay(int32_t handle, int64_t ns) {
    ALOGD("setDelay: regardless of the setDelay() value (handle=%d, ns=%lld)", handle, ns);
    return 0;
}

int GestureSensor::batch(int handle, int flags, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs) {
    int flag;
    int fd;

    ALOGE("batch: handle:%d, en:%d, samplingPeriodNs:%lld, maxBatchReportLatencyNs:%lld \r\n",
           handle, flags, samplingPeriodNs, maxBatchReportLatencyNs);
    if (maxBatchReportLatencyNs == 0) {
        flag = 0;
    } else {
        flag = 1;
    }

    strcpy(&input_sysfs_path[input_sysfs_path_len], "gesbatch");
    ALOGD("path:%s \r\n", input_sysfs_path);
    fd = open(input_sysfs_path, O_RDWR);
    if (fd < 0) {
        ALOGD("no gesture batch control attr\r\n");
        return -1;
    }

    char buf[2];
    buf[1] = 0;
    if (flag) {
        buf[0] = '1';
    } else {
        buf[0] = '0';
    }
    write(fd, buf, sizeof(buf));
    close(fd);

    ALOGD("ges batch(%d) done", flag);
    return 0;
}

int GestureSensor::flush(int handle) {
    ALOGD("flush, handle:%d\r\n", handle);
    return -errno;
}

int GestureSensor::readEvents(sensors_event_t* data, int count) {
    int i;
    int handle;

    if (count < 1)
        return -EINVAL;

    ssize_t n = mInputReader.fill(mdata_fd);
    if (n < 0)
        return n;
    int numEventReceived = 0;
    input_event const* event;

    while (count && mInputReader.readEvent(&event)) {
        int type = event->type;
        if (type == EV_REL) {
            processEvent(event->code, event->value);
            SelectGestureEvent(event->code);
        } else if (type == EV_SYN) {
            int64_t time = android::elapsedRealtimeNano();

            for (i = 0; i < numSensors; i++) {
                if (((mEnabled & (1ULL << i)) != 0)  && ((mPendingMask & (1ULL << i)) != 0)) {
                    if (time >= mEnabledTime) {
                        mPendingEvents[i].timestamp = time;
                        *data++ = mPendingEvents[i];
                        numEventReceived++;
                        mPendingMask &= ~(1 << i);
                        handle = IndexToHandle(i);
                        enable(handle, false);
                    }
                    count--;
                }
            }
        } else {
            ALOGE("unknown event (type=%d, code=%d)",
                    type, event->code);
        }
        mInputReader.next();
    }

    return numEventReceived;
}

void GestureSensor::SelectGestureEvent(int code) {
    ALOGD("SelectGestureEvent code=%d\r\n", code);
    switch (code) {
    case EVENT_TYPE_INPK_VALUE:
        mPendingMask |= (1 << inpocket);
        break;
    case EVENT_TYPE_STATIONARY_VALUE:
        mPendingMask |= (1 << stationary);
        break;
    }
}
void GestureSensor::processEvent(int code, int value) {
    ALOGD("processEvent code=%d,value=%d\r\n", code, value);
    switch (code) {
    case EVENT_TYPE_INPK_VALUE:
        mPendingEvents[inpocket].data[0] = (float) value;
        break;
    case EVENT_TYPE_STATIONARY_VALUE:
        mPendingEvents[stationary].data[0] = (float) value;
        break;
    }
}

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

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>
#include <string.h>

#include <cutils/log.h>

#include "Pdr.h"
#include <utils/SystemClock.h>
#include <utils/Timers.h>

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "PDR"
#endif

#define IGNORE_EVENT_TIME 350000000
#define SYSFS_PATH           "/sys/class/input"


/*****************************************************************************/
PdrSensor::PdrSensor()
    : SensorBase(NULL, "m_pdr_input"),  // PDR_INPUTDEV_NAME
      mEnabled(0),
      mInputReader(32)
{
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = ID_PDR;
    mPendingEvent.type = SENSOR_TYPE_PDR;
    memset(mPendingEvent.data, 0x00, sizeof(mPendingEvent.data));
    mPendingEvent.flags = 0;
    mPendingEvent.reserved0 = 0;
    mEnabledTime =0;
    mDataDiv = 1;
    mPendingEvent.timestamp =0;
    input_sysfs_path_len = 0;
    memset(input_sysfs_path, 0, PATH_MAX);

    mdata_fd = FindDataFd();
    if (mdata_fd >= 0) {
        strcpy(input_sysfs_path, "/sys/class/misc/m_pdr_misc/");
        input_sysfs_path_len = strlen(input_sysfs_path);
    }
    else
    {
        ALOGE(">> pdr couldn't find input device ");
        return;
    }

    char datapath[64]={"/sys/class/misc/m_pdr_misc/pdractive"};
    int fd = open(datapath, O_RDWR);
    char buf[64];
    int len;
    if (fd >= 0)
    {
        len = read(fd,buf,sizeof(buf)-1);
        if(len<=0)
        {
            ALOGD("read div err buf(%s)",buf );
        }
        else
        {
            buf[len] = '\0';
            sscanf(buf, "%d", &mDataDiv);
        // ALOGD("read div buf(%s)", datapath);
       // ALOGD("mdiv %d",mDataDiv );
        }
        close(fd);
    }
    else
    {
        ALOGE("open pdr misc path %s fail ", datapath);
    }
}

PdrSensor::~PdrSensor() {
if (mdata_fd >= 0)
    close(mdata_fd);
}
int PdrSensor::FindDataFd() {
   int fd = -1;
   int num = -1;
   char buf[64]={0};
   const char *devnum_dir = NULL;
   char buf_s[64] = {0};
   int len;

    devnum_dir = "/sys/class/misc/m_pdr_misc/pdrdevnum";

    fd = open(devnum_dir, O_RDONLY);
    if (fd >= 0){
        len = read(fd, buf, sizeof(buf)-1);
        close(fd);
        if (len <= 0){
            ALOGD("read devnum err buf(%s)", buf);
            return -1;
        }
        else{
            buf[len] = '\0';
            sscanf(buf, "%d\n", &num);
            ALOGD("len = %d, buf = %s",len, buf);
        }
     }
    else{
        return -1;
     }
    sprintf(buf_s, "/dev/input/event%d", num);
    fd = open(buf_s, O_RDONLY);
    ALOGE_IF(fd<0, "couldn't find input device");
    return fd;
}

int PdrSensor::enable(int32_t handle, int en)
{
    int fd;
    int flags = en ? 1 : 0;

    strcpy(&input_sysfs_path[input_sysfs_path_len], "pdractive");
    //ALOGD("path:%s \r\n",input_sysfs_path);
    fd = open(input_sysfs_path, O_RDWR);
    if(fd<0){
          ALOGD("no Pdr enable control attr\r\n" );
          return -1;
      }

     mEnabled = flags;
     char buf[2];
     buf[1] = 0;
     if (flags){
         buf[0] = '1';
         mEnabledTime = getTimestamp() + IGNORE_EVENT_TIME;
      }
     else{
         buf[0] = '0';
     }
     write(fd, buf, sizeof(buf));
     close(fd);

    ALOGD("Pdr enable(%d) done", mEnabled );
    return 0;
}

int PdrSensor::setDelay(int32_t handle, int64_t ns)
{
    int fd;
    strcpy(&input_sysfs_path[input_sysfs_path_len], "pdrdelay");
    fd = open(input_sysfs_path, O_RDWR);
    if(fd<0){
         ALOGD("no PDR setDelay control attr \r\n" );
         return -1;
     }
    char buf[80];
    sprintf(buf, "%lld", ns);
    write(fd, buf, strlen(buf)+1);
    close(fd);
    return 0;
}

int PdrSensor::batch(int handle, int flags, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
    int fd;
    int flag;

     if(maxBatchReportLatencyNs == 0){
                  flag = 0;
      }
      else{
         flag = 1;
      }

   strcpy(&input_sysfs_path[input_sysfs_path_len], "pdrbatch");
   ALOGD("path:%s \r\n",input_sysfs_path);
   fd = open(input_sysfs_path, O_RDWR);
   if(fd < 0){
       ALOGD("no pdr batch control attr\r\n");
       return -1;
    }

    char buf[2];
    buf[1] = 0;
    if (flag) {
         buf[0] = '1';
     }
    else{
        buf[0] = '0';
     }
    write(fd, buf, sizeof(buf));
    close(fd);

   ALOGD("PDR batch(%d) done", flag);
   return 0;
}

int PdrSensor::flush(int handle)
{
    return -errno;
}

int PdrSensor::readEvents(sensors_event_t* data, int count)
{
   if (count < 1)
        return -EINVAL;

    ssize_t n = mInputReader.fill(mdata_fd);
    if (n < 0)
        return n;
    int numEventReceived = 0;
    input_event const* event;

    while (count && mInputReader.readEvent(&event)) {
        int type = event->type;
        if ((type == EV_ABS) || (type == EV_REL)){
          processEvent(event->code, event->value);
        }
        else if (type == EV_SYN){
            int64_t time = android::elapsedRealtimeNano();//systemTime(SYSTEM_TIME_MONOTONIC);//timevalToNano(event->time);
            mPendingEvent.timestamp = time;
            if (mEnabled){
                      if (mPendingEvent.timestamp >= mEnabledTime){
                                    *data++ = mPendingEvent;
                                    numEventReceived++;
                       }
                count--;
            }
        }
        else if (type != EV_REL){
                    ALOGD("PdrSensor: unknown event (type=%d, code=%d)",
                    type, event->code);
        }
        mInputReader.next();
    }
    return numEventReceived;
}

void PdrSensor::processEvent(int code, int value)
{
    switch (code) {
    case EVENT_TYPE_PDR_X:
         mPendingEvent.data[0] = (float) value;//axis x
         break;
    case EVENT_TYPE_PDR_Y:
         mPendingEvent.data[1] = (float) value;//axis y
         break;
    case EVENT_TYPE_PDR_Z:
         mPendingEvent.data[2] = (float) value;//axis z
         break;
    case EVENT_TYPE_PDR_SCALAR:
         mPendingEvent.data[3] = (float) value;//scalar
         break;
    case EVENT_TYPE_PDR_STATUS:
         mPendingEvent.data[4] = (float) value;//status
         break;
    default:
         break;
    }
}

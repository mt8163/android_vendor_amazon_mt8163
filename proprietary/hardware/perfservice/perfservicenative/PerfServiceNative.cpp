/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2010. All rights reserved.
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


#define LOG_TAG "PerfService"
#define ATRACE_TAG ATRACE_TAG_PERF

#include "utils/Log.h"
#include "PerfServiceNative.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include <unistd.h>
#include <utils/String16.h>

#include <cutils/log.h>
#include <cutils/properties.h>
#include <binder/IServiceManager.h>
#include <binder/IPCThreadState.h>
#include <dlfcn.h>
#include <utils/Trace.h>

#if defined(HAVE_AEE_FEATURE)
#include <aee.h>
#define AEE_WARN_PERF(String) \
          do { \
              aee_system_warning( \
                  LOG_TAG, \
                  NULL, \
                  DB_OPT_DEFAULT, \
                  "%s PerfServiceNative Non-Support", \
                  String); \
          } while(0)
#else
#define AEE_WARN_PERF(String)
#endif

//#include <binder/IInterface.h>

namespace android
{

/* It should be sync with IPerfService.aidl */
enum {
    TRANSACTION_boostEnable = IBinder::FIRST_CALL_TRANSACTION,
    TRANSACTION_boostDisable,
    TRANSACTION_boostEnableTimeout,
    TRANSACTION_boostEnableTimeoutMs,
    TRANSACTION_notifyAppState,
    TRANSACTION_userReg,
    TRANSACTION_userRegBigLittle,
    TRANSACTION_userUnreg,
    TRANSACTION_userGetCapability,
    TRANSACTION_userRegScn,
    TRANSACTION_userRegScnConfig,
    TRANSACTION_userUnregScn,
    TRANSACTION_userEnable,
    TRANSACTION_userEnableTimeout,
    TRANSACTION_userEnableTimeoutMs,
    TRANSACTION_userEnableAsync,
    TRANSACTION_userEnableTimeoutAsync,
    TRANSACTION_userEnableTimeoutMsAsync,
    TRANSACTION_userDisable,
    TRANSACTION_userResetAll,
    TRANSACTION_userDisableAll,
    TRANSACTION_userRestoreAll,
    TRANSACTION_dumpAll,
    TRANSACTION_setFavorPid,
    TRANSACTION_restorePolicy,
    TRANSACTION_getLastBoostPid,
    TRANSACTION_notifyFrameUpdate,
    TRANSACTION_notifyDisplayType,
    TRANSACTION_notifyUserStatus,
    TRANSACTION_getClusterInfo,
    TRANSACTION_levelBoost,
    TRANSACTION_getPackAttr,
    TRANSACTION_NA_1,
    TRANSACTION_getGiftAttr,
    TRANSACTION_reloadWhiteList,
    TRANSACTION_setExclusiveCore,
};

static char packName[128] = "";
#if 0
static sp<IServiceManager> sm ;
static sp<IBinder> binder = NULL;
static Mutex sMutex;

#define MTK_LEVEL_BOOST_SUPPORT

#define BOOT_INFO_FILE "/sys/class/BOOT/BOOT/boot/boot_mode"
#define GIFTATTR_DEBUGPROP "debug.perf.giftEnable"
#define GIFT_ATTR_DEBUGSTR "Enable DEBUG_GIFTATTR"
#define RENDER_THREAD_UPDATE_DURATION   250000000
#define RENDER_THREAD_CHECK_DURATION    200000000

#define RENDER_BIT    0x800000
#define RENDER_MASK   0x7FFFFF

#ifdef DEBUG_GIFTATTR
static int gGIFT_ATTR_DEBUG = 1;
#else
static int gGIFT_ATTR_DEBUG = 0;
#endif

int (*perfCalcBoostLevel)(float) = NULL;
typedef int (*calc_boost_level)(float);

#define LEVEL_BOOST_NOP 0xffff

static int check_meta_mode(void)
{
    char bootMode[4];
    int fd;
    //check if in Meta mode
    fd = open(BOOT_INFO_FILE, O_RDONLY);
    if(fd < 0) {
        return 0; // not meta mode
    }

    if(read(fd, bootMode, 4) < 1) {
        close(fd);
        return 0;
    }

    if (bootMode[0] == 0x31 || bootMode[0] == 0x34) {
        close(fd);
        return 1; // meta mode, factory mode
    }

    close(fd);
    return 0;
}

static void init(void)
{
    Mutex::Autolock lock(sMutex);
    if(binder == NULL) {
        if(check_meta_mode())
            return;

        sm = defaultServiceManager();
        //binder = sm->getService(String16("mtk-perfservice"));
        binder = sm->checkService(String16("mtk-perfservice")); // use check to avoid null binder
    }
}
#endif

extern "C"
void PerfServiceNative_boostEnable(int scenario)
{
#if 0
    Parcel data, reply;
    init();

    ALOGI("PerfServiceNative_boostEnable:%d", scenario);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(scenario);
        binder->transact(TRANSACTION_boostEnable ,data,&reply); // should sync with IPerfService
    }
#else
    ALOGI("PerfServiceNative_boostEnable:%d", scenario);
    AEE_WARN_PERF(__FUNCTION__);
#endif
}

extern "C"
void PerfServiceNative_boostDisable(int scenario)
{
#if 0
    Parcel data, reply;
    init();

    ALOGI("PerfServiceNative_boostDisable:%d", scenario);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(scenario);
        binder->transact(TRANSACTION_boostDisable ,data,&reply);
    }
#else
    ALOGI("PerfServiceNative_boostDisable:%d", scenario);
    /*AEE_WARN_PERF(__FUNCTION__);*/
#endif
}

extern "C"
void PerfServiceNative_boostEnableTimeout(int scenario, int timeout)
{
#if 0
    Parcel data, reply;
    init();

    ALOGI("PerfServiceNative_boostEnableTimeout:%d, %d", scenario, timeout);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(scenario);
        data.writeInt32(timeout);
        binder->transact(TRANSACTION_boostEnableTimeout ,data,&reply);
    }
#else
    ALOGI("PerfServiceNative_boostEnableTimeout:%d, %d", scenario, timeout);
    AEE_WARN_PERF(__FUNCTION__);
#endif
}

extern "C"
void PerfServiceNative_boostEnableTimeoutMs(int scenario, int timeout_ms)
{
#if 0
    Parcel data, reply;
    init();

    ALOGI("PerfServiceNative_boostEnableTimeoutMs:%d, %d", scenario, timeout_ms);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(scenario);
        data.writeInt32(timeout_ms);
        binder->transact(TRANSACTION_boostEnableTimeoutMs ,data,&reply);
    }
#else
    ALOGI("PerfServiceNative_boostEnableTimeoutMs:%d, %d", scenario, timeout_ms);
    AEE_WARN_PERF(__FUNCTION__);
#endif
}

extern "C"
void PerfServiceNative_boostEnableAsync(int scenario)
{
#if 0
    Parcel data, reply;
    init();

    ALOGI("PerfServiceNative_boostEnableAsync:%d", scenario);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(scenario);
        binder->transact(TRANSACTION_boostEnable ,data,&reply,IBinder::FLAG_ONEWAY); // should sync with IPerfService
    }
#else
    ALOGI("PerfServiceNative_boostEnableAsync:%d", scenario);
    AEE_WARN_PERF(__FUNCTION__);
#endif
}

extern "C"
void PerfServiceNative_boostDisableAsync(int scenario)
{
#if 0
    Parcel data, reply;
    init();

    ALOGI("PerfServiceNative_boostDisableAsync:%d", scenario);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(scenario);
        binder->transact(TRANSACTION_boostDisable ,data,&reply,IBinder::FLAG_ONEWAY);
    }
#else
    ALOGI("PerfServiceNative_boostDisableAsync:%d", scenario);
    /*AEE_WARN_PERF(__FUNCTION__);*/
#endif
}

extern "C"
void PerfServiceNative_boostEnableTimeoutAsync(int scenario, int timeout)
{
#if 0
    Parcel data, reply;
    init();

    ALOGI("PerfServiceNative_boostEnableTimeoutAsync:%d, %d", scenario, timeout);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(scenario);
        data.writeInt32(timeout);
        binder->transact(TRANSACTION_boostEnableTimeout ,data,&reply,IBinder::FLAG_ONEWAY);
    }
#else
    ALOGI("PerfServiceNative_boostEnableTimeoutAsync:%d, %d", scenario, timeout);
    AEE_WARN_PERF(__FUNCTION__);
#endif
}

extern "C"
void PerfServiceNative_boostEnableTimeoutMsAsync(int scenario, int timeout_ms)
{
#if 0
    Parcel data, reply;
    init();

    ALOGI("PerfServiceNative_boostEnableTimeoutMsAsync:%d, %d", scenario, timeout_ms);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(scenario);
        data.writeInt32(timeout_ms);
        binder->transact(TRANSACTION_boostEnableTimeoutMs ,data,&reply,IBinder::FLAG_ONEWAY);
    }
#else
    ALOGI("PerfServiceNative_boostEnableTimeoutMsAsync:%d, %d", scenario, timeout_ms);
    AEE_WARN_PERF(__FUNCTION__);
#endif
}

extern "C"
int PerfServiceNative_userReg(int scn_core, int scn_freq)
{
#if 0
    Parcel data, reply;
    int    err, handle = -1, pid=-1, tid=-1;
    init();

    pid = (int)getpid();
    tid = (int)gettid();

    ALOGI("PerfServiceNative_userReg: %d, %d (pid:%d, tid:%d)", scn_core, scn_freq, pid, tid);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(scn_core);
        data.writeInt32(scn_freq);
        data.writeInt32(pid);
        data.writeInt32(tid);
        binder->transact(TRANSACTION_userReg ,data,&reply); // should sync with IPerfService
        err = reply.readExceptionCode();
        if(err < 0) {
            ALOGI("PerfServiceNative_userReg err:%d", err);
            return -1;
        }
        handle = reply.readInt32();
    }
    return handle;
#else
    ALOGI("PerfServiceNative_userReg: %d, %d", scn_core, scn_freq);
    /*AEE_WARN_PERF(__FUNCTION__);*/
    return 0;
#endif
}

extern "C"
int PerfServiceNative_userRegBigLittle(int scn_core_big, int scn_freq_big, int scn_core_little, int scn_freq_little)
{
#if 0
    Parcel data, reply;
    int    err, handle = -1, pid=-1, tid=-1;
    init();

    pid = (int)getpid();
    tid = (int)gettid();

    ALOGI("PerfServiceNative_userRegBigLittle: %d, %d, %d, %d (pid:%d, tid:%d)", scn_core_little, scn_freq_little, scn_core_big, scn_freq_big, pid, tid);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(scn_core_big);
        data.writeInt32(scn_freq_big);
        data.writeInt32(scn_core_little);
        data.writeInt32(scn_freq_little);
        data.writeInt32(pid);
        data.writeInt32(tid);
        binder->transact(TRANSACTION_userRegBigLittle ,data,&reply); // should sync with IPerfService
        err = reply.readExceptionCode();
        if(err < 0) {
            ALOGI("PerfServiceNative_userRegBigLittle err:%d", err);
            return -1;
        }
        handle = reply.readInt32();
    }
    return handle;
#else
    ALOGI("PerfServiceNative_userRegBigLittle: %d, %d, %d, %d", scn_core_little, scn_freq_little, scn_core_big, scn_freq_big);
    /*AEE_WARN_PERF(__FUNCTION__);*/
    return 0;
#endif
}

extern "C"
void PerfServiceNative_userUnreg(int handle)
{
#if 0
    Parcel data, reply;
    init();

    ALOGI("PerfServiceNative_userUnreg:%d", handle);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(handle);
        binder->transact(TRANSACTION_userUnreg ,data,&reply); // should sync with IPerfService
    }
#else
    ALOGI("PerfServiceNative_userUnreg:%d", handle);
    /*AEE_WARN_PERF(__FUNCTION__);*/
#endif
}

extern "C"
int PerfServiceNative_userGetCapability(int cmd)
{
#if 0
    Parcel data, reply;
    int err, value = -1;
    init();

    ALOGI("PerfServiceNative_userGetCapability: %d", cmd);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(cmd);
        binder->transact(TRANSACTION_userGetCapability ,data,&reply); // should sync with IPerfService
        err = reply.readExceptionCode();
        if(err < 0) {
            ALOGI("PerfServiceNative_userGetCapability err:%d", err);
            return -1;
        }
        value = reply.readInt32();
    }
    return value;
#else
    ALOGI("PerfServiceNative_userGetCapability: %d", cmd);
    /*AEE_WARN_PERF(__FUNCTION__);*/
    return 0;
#endif
}

extern "C"
int PerfServiceNative_userRegScn(void)
{
#if 0
    Parcel data, reply;
    int    err, handle = -1, pid=-1, tid=-1;
    init();

    pid = (int)getpid();
    tid = (int)gettid();

    ALOGI("PerfServiceNative_userRegScn: (pid:%d, tid:%d)", pid, tid);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(pid);
        data.writeInt32(tid);
        binder->transact(TRANSACTION_userRegScn ,data,&reply); // should sync with IPerfService
        err = reply.readExceptionCode();
        if(err < 0) {
            ALOGI("PerfServiceNative_userRegScn err:%d", err);
            return -1;
        }
        handle = reply.readInt32();
    }
    return handle;
#else
    /*AEE_WARN_PERF(__FUNCTION__);*/
    return 0;
#endif
}

extern "C"
void PerfServiceNative_userRegScnConfig(int handle, int cmd, int param_1, int param_2, int param_3, int param_4)
{
#if 0
    Parcel data, reply;
    init();

    ALOGI("PerfServiceNative_userRegScnConfig: handle:%d, cmd:%d, p1:%d, p2:%d, p3:%d, p4:%d", handle, cmd, param_1, param_2, param_3, param_4);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(handle);
        data.writeInt32(cmd);
        data.writeInt32(param_1);
        data.writeInt32(param_2);
        data.writeInt32(param_3);
        data.writeInt32(param_4);
        binder->transact(TRANSACTION_userRegScnConfig,data,&reply); // should sync with IPerfService
    }
#else
    ALOGI("PerfServiceNative_userRegScnConfig: handle:%d, cmd:%d, p1:%d, p2:%d, p3:%d, p4:%d", handle, cmd, param_1, param_2, param_3, param_4);
    /*AEE_WARN_PERF(__FUNCTION__);*/
#endif
}

extern "C"
void PerfServiceNative_userRegScnConfigAsync(int handle, int cmd, int param_1, int param_2, int param_3, int param_4)
{
#if 0
    Parcel data, reply;
    init();

    ALOGI("PerfServiceNative_userRegScnConfigAsync: handle:%d, cmd:%d, p1:%d, p2:%d, p3:%d, p4:%d", handle, cmd, param_1, param_2, param_3, param_4);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(handle);
        data.writeInt32(cmd);
        data.writeInt32(param_1);
        data.writeInt32(param_2);
        data.writeInt32(param_3);
        data.writeInt32(param_4);
        binder->transact(TRANSACTION_userRegScnConfig,data,&reply,IBinder::FLAG_ONEWAY); // should sync with IPerfService
    }
#else
    ALOGI("PerfServiceNative_userRegScnConfigAsync: handle:%d, cmd:%d, p1:%d, p2:%d, p3:%d, p4:%d", handle, cmd, param_1, param_2, param_3, param_4);
    /*AEE_WARN_PERF(__FUNCTION__);*/
#endif
}

extern "C"
void PerfServiceNative_userUnregScn(int handle)
{
#if 0
    Parcel data, reply;
    init();

    ALOGI("PerfServiceNative_userUnregScn: handle:%d", handle);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(handle);
        binder->transact(TRANSACTION_userUnregScn,data,&reply); // should sync with IPerfService
    }
#else
    ALOGI("PerfServiceNative_userUnregScn: handle:%d", handle);
    /*AEE_WARN_PERF(__FUNCTION__);*/
#endif
}

extern "C"
void PerfServiceNative_userEnable(int handle)
{
#if 0
    Parcel data, reply;
    init();

    ALOGI("PerfServiceNative_userEnable:%d", handle);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(handle);
        binder->transact(TRANSACTION_userEnable ,data,&reply); // should sync with IPerfService
    }
#else
    ALOGI("PerfServiceNative_userEnable:%d", handle);
    AEE_WARN_PERF(__FUNCTION__);
#endif
}

extern "C"
void PerfServiceNative_userEnableTimeout(int handle, int timeout)
{
#if 0
    Parcel data, reply;
    init();

    ALOGI("PerfServiceNative_userEnableTimeout:%d, %d", handle, timeout);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(handle);
        data.writeInt32(timeout);
        binder->transact(TRANSACTION_userEnableTimeout ,data,&reply);
    }
#else
    ALOGI("PerfServiceNative_userEnableTimeout:%d, %d", handle, timeout);
    AEE_WARN_PERF(__FUNCTION__);
#endif
}

extern "C"
void PerfServiceNative_userEnableTimeoutMs(int handle, int timeout_ms)
{
#if 0
    Parcel data, reply;
    init();

    ALOGI("PerfServiceNative_userEnableTimeoutMs:%d, %d", handle, timeout_ms);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(handle);
        data.writeInt32(timeout_ms);
        binder->transact(TRANSACTION_userEnableTimeoutMs ,data,&reply);
    }
#else
    ALOGI("PerfServiceNative_userEnableTimeoutMs:%d, %d", handle, timeout_ms);
    AEE_WARN_PERF(__FUNCTION__);
#endif
}

extern "C"
void PerfServiceNative_userEnableAsync(int handle)
{
#if 0
    Parcel data, reply;
    init();

    ALOGI("PerfServiceNative_userEnableAsync:%d", handle);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(handle);
        binder->transact(TRANSACTION_userEnableAsync ,data,&reply); // should sync with IPerfService
    }
#else
    ALOGI("PerfServiceNative_userEnableAsync:%d", handle);
    AEE_WARN_PERF(__FUNCTION__);
#endif
}

extern "C"
void PerfServiceNative_userEnableTimeoutAsync(int handle, int timeout)
{
#if 0
    Parcel data, reply;
    init();

    ALOGI("PerfServiceNative_userEnableTimeoutAsync:%d, %d", handle, timeout);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(handle);
        data.writeInt32(timeout);
        binder->transact(TRANSACTION_userEnableTimeoutAsync ,data,&reply);
    }
#else
    ALOGI("PerfServiceNative_userEnableTimeoutAsync:%d, %d", handle, timeout);
    AEE_WARN_PERF(__FUNCTION__);
#endif
}

extern "C"
void PerfServiceNative_userEnableTimeoutMsAsync(int handle, int timeout_ms)
{
#if 0
    Parcel data, reply;
    init();

    ALOGI("PerfServiceNative_userEnableTimeoutMsAsync:%d, %d", handle, timeout_ms);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(handle);
        data.writeInt32(timeout_ms);
        binder->transact(TRANSACTION_userEnableTimeoutMsAsync ,data,&reply);
    }
#else
    ALOGI("PerfServiceNative_userEnableTimeoutMsAsync:%d, %d", handle, timeout_ms);
    AEE_WARN_PERF(__FUNCTION__);
#endif
}

extern "C"
void PerfServiceNative_userDisable(int handle)
{
#if 0
    Parcel data, reply;
    init();

    ALOGI("PerfServiceNative_userDisable:%d", handle);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(handle);
        binder->transact(TRANSACTION_userDisable ,data,&reply);
    }
#else
    ALOGI("PerfServiceNative_userDisable:%d", handle);
    /*AEE_WARN_PERF(__FUNCTION__);*/
#endif
}

extern "C"
void PerfServiceNative_userDisableAsync(int handle)
{
#if 0
    Parcel data, reply;
    init();

    ALOGI("PerfServiceNative_userDisableAsync:%d", handle);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(handle);
        binder->transact(TRANSACTION_userDisable ,data,&reply,IBinder::FLAG_ONEWAY);
    }
#else
    ALOGI("PerfServiceNative_userDisableAsync:%d", handle);
    /*AEE_WARN_PERF(__FUNCTION__);*/
#endif
}

extern "C"
void PerfServiceNative_userResetAll(void)
{
#if 0
    Parcel data, reply;
    init();

    ALOGI("PerfServiceNative_userResetAll");

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        binder->transact(TRANSACTION_userResetAll ,data,&reply);
    }
#endif
    /*AEE_WARN_PERF(__FUNCTION__);*/
}

extern "C"
void PerfServiceNative_userDisableAll(void)
{
#if 0
    Parcel data, reply;
    init();

    ALOGI("PerfServiceNative_userDisableAll");

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        binder->transact(TRANSACTION_userDisableAll ,data,&reply);
    }
#endif
    /*AEE_WARN_PERF(__FUNCTION__);*/
}

extern "C"
void PerfServiceNative_dumpAll(void)
{
#if 0
    Parcel data, reply;
    init();

    ALOGI("PerfServiceNative_dumpAll");

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        binder->transact(TRANSACTION_dumpAll ,data,&reply);
    }
#endif
}

extern "C"
void PerfServiceNative_setFavorPid(int pid)
{
#if 0
    Parcel data, reply;
    init();

    ALOGI("PerfServiceNative_setFavorPid: pid:%d", pid);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(pid);
        binder->transact(TRANSACTION_setFavorPid,data,&reply,IBinder::FLAG_ONEWAY);
    }
#else
    ALOGI("PerfServiceNative_setFavorPid: pid:%d", pid);
    /*AEE_WARN_PERF(__FUNCTION__);*/
#endif
}

extern "C"
void PerfServiceNative_setBoostThread(void)
{
#if 0
    Parcel data, reply;
    int tid;
    init();

    tid = (int)gettid();
    ALOGI("PerfServiceNative_setBoostThread: pid:%d, tid:%d", (int)getpid(), tid);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(tid | RENDER_BIT);
        binder->transact(TRANSACTION_setFavorPid,data,&reply,IBinder::FLAG_ONEWAY);
    }
#endif
    /*AEE_WARN_PERF(__FUNCTION__);*/
}

extern "C"
void PerfServiceNative_restoreBoostThread(void)
{
#if 0
    Parcel data, reply;
    int tid;
    init();

    tid = (int)gettid();
    ALOGI("PerfServiceNative_restoreBoostThread: pid:%d, tid:%d", (int)getpid(), tid);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(tid | RENDER_BIT);
        binder->transact(TRANSACTION_restorePolicy,data,&reply,IBinder::FLAG_ONEWAY);
    }
#endif
    /*AEE_WARN_PERF(__FUNCTION__);*/
}

extern "C"
void PerfServiceNative_notifyFrameUpdate(int level)
{
#if 0
    Parcel data, reply;
    static nsecs_t mPreviousTime = 0;
    nsecs_t now = systemTime(CLOCK_MONOTONIC);
    //static int set_tid = 0;
    init();

    #if 0 // L MR1: foreground app is already in root group
    // get tid
    if(!set_tid) {
        level = (int)gettid();
        set_tid = 1;
    }
    #endif

    if(mPreviousTime == 0 || (now - mPreviousTime) > RENDER_THREAD_UPDATE_DURATION) { // 400ms
        //ALOGI("PerfServiceNative_notifyFrameUpdate:%d", (now - mPreviousTime)/1000000);
        if(binder!=NULL) {
            data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
            data.writeInt32(level);
            binder->transact(TRANSACTION_notifyFrameUpdate ,data,&reply,IBinder::FLAG_ONEWAY);
        }
        mPreviousTime = now;
    }
#else
    ALOGI("PerfServiceNative_notifyFrameUpdate: level:%d", level);
    /*AEE_WARN_PERF(__FUNCTION__);*/
#endif
}

extern "C"
void PerfServiceNative_notifyRenderTime(float time)
{
#if 0
    static void *handle;
    void  *func;
    int    boost_level = LEVEL_BOOST_NOP, first_frame = 0;
    static nsecs_t mPreviousTime = 0;
    char buff[64];
    nsecs_t now = systemTime(CLOCK_MONOTONIC);
    //init();


    PerfServiceNative_notifyFrameUpdate(0);

    if(handle == NULL) {
        handle = dlopen("libperfservice.so", RTLD_NOW);
        func = dlsym(handle, "perfCalcBoostLevel");
        perfCalcBoostLevel = reinterpret_cast<calc_boost_level>(func);
        if (perfCalcBoostLevel == NULL) {
            ALOGE("perfCalcBoostLevel init fail!");
        }
    }

    //ALOGI("PerfServiceNative_notifyRenderTime: time:%f", time);

    if(mPreviousTime == 0 || (now - mPreviousTime) > RENDER_THREAD_CHECK_DURATION) { // exceed RENDER_THREAD_CHECK_DURATION => first frame
        first_frame = 1;
    }
    mPreviousTime = now;

    if(first_frame) {
        //ALOGI("PerfServiceNative_notifyRenderTime: first_frame");
        if(perfCalcBoostLevel)
            perfCalcBoostLevel(0);
        return;
    }

    if(perfCalcBoostLevel) {
        boost_level = perfCalcBoostLevel(time);
    }

    // init value
    //sprintf(buff, "notifyRenderTime:%.2f", time);

    if(boost_level == LEVEL_BOOST_NOP)
        return;

    sprintf(buff, "levelBoost:%d", boost_level);
    ATRACE_BEGIN(buff);
#if defined(MTK_LEVEL_BOOST_SUPPORT)
    PerfServiceNative_levelBoost(boost_level);
#endif
    ATRACE_END();
#else
    ALOGI("PerfServiceNative_notifyRenderTime: time:%f", time);
    /*AEE_WARN_PERF(__FUNCTION__);*/
#endif
}

extern "C"
void PerfServiceNative_notifyDisplayType(int type)
{
#if 0
    Parcel data, reply;
    init();

    ALOGI("PerfServiceNative_notifyDisplayType:%d", type);
    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(type);
        binder->transact(TRANSACTION_notifyDisplayType ,data,&reply,IBinder::FLAG_ONEWAY);
    }
#else
    ALOGI("PerfServiceNative_notifyDisplayType:%d", type);
    /*AEE_WARN_PERF(__FUNCTION__);*/
#endif
}

extern "C"
void PerfServiceNative_notifyUserStatus(int type, int status)
{
#if 0
    Parcel data, reply;
    init();

    ALOGI("PerfServiceNative_notifyUserStatus:%d, status:%d", type, status);
    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(type);
        data.writeInt32(status);
        binder->transact(TRANSACTION_notifyUserStatus ,data,&reply,IBinder::FLAG_ONEWAY);
    }
#else
    ALOGI("PerfServiceNative_notifyUserStatus:%d, status:%d", type, status);
    /*AEE_WARN_PERF(__FUNCTION__);*/
#endif
}

extern "C"
int PerfServiceNative_getLastBoostPid()
{
#if 0
    Parcel data, reply;
    int err;
    //const int handle = 1;
    init();

    //ALOGI("PerfServiceNative_getLastBoostPid");
    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        binder->transact(TRANSACTION_getLastBoostPid,data,&reply);
        err = reply.readExceptionCode();
        if(err < 0) {
            ALOGI("PerfServiceNative_getLastBoostPid err:%d", err);
            return -1;
        }
        return reply.readInt32();
    }
    return -1;
#else
    /*AEE_WARN_PERF(__FUNCTION__);*/
    return -1;
#endif
}

extern "C"
const char* PerfServiceNative_getPackName()
{
#if 0
    int pid, ret;
    char spid[8];
    char path[64] = "/proc/";
    FILE *ifp;

    pid = PerfServiceNative_getLastBoostPid();
    //itoa(pid, spid, 10);
    sprintf(spid, "%d", pid);
    strcat(path, spid);
    strcat(path, "/cmdline");
    if ((ifp = fopen(path,"r")) == NULL)
        return "";
    ret = fscanf(ifp, "%s", packName);
    fclose(ifp);
    if (ret == 1)
        return packName;
    else
        return "";
#else
    /*AEE_WARN_PERF(__FUNCTION__);*/
    return packName;
#endif
}

extern "C"
int PerfServiceNative_getClusterInfo(int cmd, int id)
{
#if 0
    Parcel data, reply;
    int err, value = -1;
    init();

    ALOGI("PerfServiceNative_getClusterInfo: %d, %d", cmd, id);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(cmd);
        data.writeInt32(id);
        binder->transact(TRANSACTION_getClusterInfo,data,&reply); // should sync with IPerfService
        err = reply.readExceptionCode();
        if(err < 0) {
            ALOGI("PerfServiceNative_getClusterInfo err:%d", err);
            return -1;
        }
        value = reply.readInt32();
    }
    return value;
#else
    ALOGI("PerfServiceNative_getClusterInfo: %d, %d", cmd, id);
    /*AEE_WARN_PERF(__FUNCTION__);*/
    return 0;
#endif
}

extern "C"
void PerfServiceNative_levelBoost(int level)
{
#if 0
    Parcel data, reply;
    init();

    //ALOGI("PerfServiceNative_levelBoost:%d", level);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(level);
        binder->transact(TRANSACTION_levelBoost ,data,&reply,IBinder::FLAG_ONEWAY);
    }
#else
    ALOGI("PerfServiceNative_levelBoost:%d", level);
    /*AEE_WARN_PERF(__FUNCTION__);*/
#endif
}

extern "C"
int PerfServiceNative_getPackAttr(const char* packName, int cmd)
{
#if 0
    Parcel data, reply;
    int err, value = -1;
    init();

    ALOGI("PerfServiceNative_getPackAttr: %s, %d", packName, cmd);

    if(packName == NULL)
        return -1;

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeString16(String16(packName));
        data.writeInt32(cmd);
        binder->transact(TRANSACTION_getPackAttr,data,&reply); // should sync with IPerfService
        err = reply.readExceptionCode();
        if(err < 0) {
            ALOGI("PerfServiceNative_getPackAttr err:%d", err);
            return -1;
        }
        value = reply.readInt32();
    }
    return value;
#else
    ALOGI("PerfServiceNative_getPackAttr: %s, %d", packName, cmd);
    /*AEE_WARN_PERF(__FUNCTION__);*/
    return 0;
#endif
}


extern "C"
int PerfServiceNative_getGiftAttr(const char* packName,char* attrName,char* attrValue, int attrLen)
{
#if 0
    Parcel data, reply;
    int err, value = -1;
    String16 str16;
    char buf[100];

    init();

    if(packName == NULL || attrName == NULL || attrLen == 0)
        return -1;

    if((property_get(GIFTATTR_DEBUGPROP, buf, NULL)!=0) || (gGIFT_ATTR_DEBUG == 1)){
        memset(attrValue, 0, attrLen);
        memcpy(attrValue, GIFT_ATTR_DEBUGSTR, strlen(GIFT_ATTR_DEBUGSTR));
        value = strlen(attrValue);
        ALOGE("Debug Native PerfServiceNative_getGiftAttr");
    }
    else{
        if(binder!=NULL) {
            data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
            data.writeString16(String16(packName));
            data.writeString16(String16(attrName));
            binder->transact(TRANSACTION_getGiftAttr,data,&reply); // should sync with IPerfService
            err = reply.readExceptionCode();
            if(err < 0) {
                ALOGI("PerfServiceNative_getPackAttr err:%d", err);
                value = -1;
            }
            str16 = reply.readString16();
            if(strncmp(String8(str16).string(), "GiftEmpty", 9) != 0){
                memset(attrValue, 0, attrLen);
                value = String8(str16).length();
                if(value >= attrLen)
                    value = attrLen-1;
                memcpy(attrValue, (char *)String8(str16).string(), value);
            }
            else{
                value = 0;
            }
        }
    }

    return value;
#else
    ALOGI("PerfServiceNative_getPackAttr: %s, %s, %s, %d", packName, attrName, attrValue, attrLen);
    /*AEE_WARN_PERF(__FUNCTION__);*/
    return 0;
#endif
}

extern "C"
int PerfServiceNative_reloadWhiteList(void)
{
#if 0
    Parcel data, reply;
    int err;
    //const int handle = 1;
    init();

    //ALOGI("PerfServiceNative_getLastBoostPid");
    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        binder->transact(TRANSACTION_reloadWhiteList,data,&reply);
        err = reply.readExceptionCode();
        if(err < 0) {
            ALOGI("PerfServiceNative_reloadWhiteList err:%d", err);
            return -1;
        }
        return reply.readInt32();
    }
    return -1;
#else
    /*AEE_WARN_PERF(__FUNCTION__);*/
    return -1;
#endif
}

extern "C"
void PerfServiceNative_setExclusiveCore(int pid, int cpu_mask)
{
#if 0
    Parcel data, reply;
    int err, value = -1;
    init();

    ALOGI("PerfServiceNative_setExclusiveCore: %d, %x", pid, cpu_mask);

    if(binder!=NULL) {
        data.writeInterfaceToken(String16("com.mediatek.perfservice.IPerfService"));
        data.writeInt32(pid);
        data.writeInt32(cpu_mask);
        binder->transact(TRANSACTION_setExclusiveCore,data,&reply); // should sync with IPerfService
    }
#else
    ALOGI("PerfServiceNative_setExclusiveCore: %d, %x", pid, cpu_mask);
    /*AEE_WARN_PERF(__FUNCTION__);*/
#endif
}

}

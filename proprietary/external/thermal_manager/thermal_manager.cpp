/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein is
 * confidential and proprietary to MediaTek Inc. and/or its licensors. Without
 * the prior written permission of MediaTek inc. and/or its licensors, any
 * reproduction, modification, use or disclosure of MediaTek Software, and
 * information contained herein, in whole or in part, shall be strictly
 * prohibited.
 *
 * MediaTek Inc. (C) 2012. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER
 * ON AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL
 * WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
 * NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH
 * RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY,
 * INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES
 * TO LOOK ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO.
 * RECEIVER EXPRESSLY ACKNOWLEDGES THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO
 * OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES CONTAINED IN MEDIATEK
 * SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE
 * RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S
 * ENTIRE AND CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE
 * RELEASED HEREUNDER WILL BE, AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE
 * MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE
 * CHARGE PAID BY RECEIVER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek
 * Software") have been modified by MediaTek Inc. All revisions are subject to
 * any receiver's applicable license agreements with MediaTek Inc.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *      thermal_manager.cpp
 *
 * Project:
 * --------
 *      YuSu
 *
 * Description:
 * ------------
 *      Set default values to all thermal zone device drivers and cooling device drivers when
 *      system init.
 *
 * Author:
 * -------
 *      CT Fang (mtk02403)
 *
 ****************************************************************************/

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <dlfcn.h>
#include <cstdlib>
#include <android/log.h>

#define TM_LOG_TAG "thermal_mgr_cpp"
#define LIB_FULL_NAME "/vendor/lib/libmtcloader.so"
#define DEFAULT_THERMAL_CONF "/vendor/etc/.tp/thermal.conf"

#define TM_LOG(priority, fmt, ...) \
    __android_log_print(priority, TM_LOG_TAG, fmt, ##__VA_ARGS__)

namespace thermal {

// Type aliases for dynamically loaded function pointers
using LoadMtcFunc = int(*)(const char *);
using ChangePolicyFunc = int(*)(const char *, int);

class ThermalManager {
public:
    ThermalManager() = default;
    ~ThermalManager() {
        if (lib_handle_) {
            dlclose(lib_handle_);
        }
    }

    int run(const std::vector<std::string>& args);

private:
    void* lib_handle_ = nullptr;
    LoadMtcFunc load_mtc_ = nullptr;
    ChangePolicyFunc change_policy_ = nullptr;

    bool loadLibrary();
    bool loadFunctions();
    int executePolicy(const std::vector<std::string>& args);
};

bool ThermalManager::loadLibrary() {
    lib_handle_ = dlopen(LIB_FULL_NAME, RTLD_NOW);
    if (!lib_handle_) {
        TM_LOG(ANDROID_LOG_ERROR, "Failed to load %s: %s", LIB_FULL_NAME, dlerror());
        return false;
    }
    return true;
}

bool ThermalManager::loadFunctions() {
    load_mtc_ = reinterpret_cast<LoadMtcFunc>(dlsym(lib_handle_, "loadmtc"));
    if (!load_mtc_) {
        TM_LOG(ANDROID_LOG_ERROR, "loadmtc not found: %s", dlerror());
        return false;
    }

    change_policy_ = reinterpret_cast<ChangePolicyFunc>(dlsym(lib_handle_, "change_policy"));
    return true;
}

int ThermalManager::executePolicy(const std::vector<std::string>& args) {
    if (args.size() == 2) {
        TM_LOG(ANDROID_LOG_INFO, "Invoking loadmtc with: %s", args[1].c_str());
        return load_mtc_(args[1].c_str());
    } else if (args.size() == 3 && change_policy_) {
        int policy = std::stoi(args[2]);
        TM_LOG(ANDROID_LOG_INFO, "Invoking change_policy: %s %d", args[1].c_str(), policy);
        return change_policy_(args[1].c_str(), policy);
    } else {
        TM_LOG(ANDROID_LOG_INFO, "Using default config: %s", DEFAULT_THERMAL_CONF);
        return load_mtc_(DEFAULT_THERMAL_CONF);
    }
}

int ThermalManager::run(const std::vector<std::string>& args) {
    for (size_t i = 0; i < args.size(); ++i) {
        TM_LOG(ANDROID_LOG_INFO, "argv[%zu]: %s", i, args[i].c_str());
    }

    if (!loadLibrary() || !loadFunctions()) {
        return EXIT_FAILURE;
    }

    return executePolicy(args);
}

}  // namespace thermal

int main(int argc, char** argv) {
    std::vector<std::string> args(argv, argv + argc);
    thermal::ThermalManager manager;
    return manager.run(args);
}


/*
 * Copyright (C) 2016 The Android Open Source Project
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

#define LOG_TAG "android.hardware.thermal@1.0-impl-mtk"

#include <android-base/logging.h>
#include <utils/Errors.h>
#include "Thermal.h"

#include <fstream>
#include <string>
#include <sstream> 
#include <cctype>

namespace android {
namespace hardware {
namespace thermal {
namespace V1_0 {
namespace implementation {

const static std::string cpuPath = "/proc/mtktz/mtktscpu";
const static std::string batPath = "/proc/mtktz/mtktsbattery";
const static std::string skinPath = "/proc/mtktz/mtktsAP";
//TODO GPU temp

const static std::string coreNum = "/sys/devices/system/cpu/kernel_max";
const static std::string cpuUsage = "/proc/stat";

// Methods from ::android::hardware::thermal::V1_0::IThermal follow.
Return<void> Thermal::getTemperatures(getTemperatures_cb _hidl_cb) {
  ThermalStatus status;
  status.code = ThermalStatusCode::SUCCESS;
  hidl_vec<Temperature> temperatures;
  temperatures.resize(3);

  temperatures[0].type = TemperatureType::CPU;
  temperatures[1].type = TemperatureType::BATTERY;
  temperatures[2].type = TemperatureType::SKIN;

  temperatures[0].name = "CPU";
  temperatures[1].name = "BATTERY";
  temperatures[2].name = "AP";

  temperatures[0].throttlingThreshold = 85;
  temperatures[1].throttlingThreshold = 50;
  temperatures[2].throttlingThreshold = 50;

  temperatures[0].shutdownThreshold = 117;
  temperatures[1].shutdownThreshold = 60;
  temperatures[2].shutdownThreshold = 90;

  temperatures[0].vrThrottlingThreshold = 85;
  temperatures[1].vrThrottlingThreshold = 50;
  temperatures[2].vrThrottlingThreshold = 50;

  std::ifstream cpu(cpuPath);
  if (!cpu.is_open()) {
      status.code = ThermalStatusCode::FAILURE;
      status.debugMessage = "Failed to open " + cpuPath + ", error=" + std::to_string (errno)
                   + " (" + strerror(errno) + ")";
      _hidl_cb(status, temperatures);
      return Void();
  } else {
      float temp = 0.0f;
      cpu >> temp;
      temperatures[0].currentValue = temp * 0.001f;
  }

  std::ifstream bat(batPath);
  if (!bat.is_open()) {
      status.code = ThermalStatusCode::FAILURE;
      status.debugMessage = "Failed to open " + batPath + ", error=" + std::to_string (errno)
                   + " (" + strerror(errno) + ")";
      _hidl_cb(status, temperatures);
      return Void();
  } else {
      float temp = 0.0f;
      bat >> temp;
      temperatures[1].currentValue = temp * 0.001f;
  }

  std::ifstream skin(skinPath);
  if (!skin.is_open()) {
      status.code = ThermalStatusCode::FAILURE;
      status.debugMessage = "Failed to open " + skinPath + ", error=" + std::to_string (errno)
                   + " (" + strerror(errno) + ")";
      _hidl_cb(status, temperatures);
      return Void();
  } else {
      float temp = 0.0f;
      skin >> temp;
      temperatures[2].currentValue = temp * 0.001f;
  }

  _hidl_cb(status, temperatures);
  return Void();
}

Return<void> Thermal::getCpuUsages(getCpuUsages_cb _hidl_cb) {
  unsigned int core_num;
  ThermalStatus status;
  hidl_vec<CpuUsage> cpuUsages;
  status.code = ThermalStatusCode::SUCCESS;

  std::ifstream coreN(coreNum);
  if (!coreN.is_open()) {
      status.code = ThermalStatusCode::FAILURE;
      status.debugMessage = "Failed to open " + coreNum + ", error=" + std::to_string (errno)
                   + " (" + strerror(errno) + ")";
      _hidl_cb(status, cpuUsages);
      return Void();
  }

  coreN >> core_num;
  cpuUsages.resize(core_num);

  // Init default values
  for (unsigned int i = 0; i < core_num; i++) {
    cpuUsages[i].name = "CPU" + std::to_string (i);
    cpuUsages[i].active = 0;
    cpuUsages[i].total = 0;
    cpuUsages[i].isOnline = false;
  }

  std::ifstream cpu(cpuUsage);
  if (!cpu.is_open()) {
      status.code = ThermalStatusCode::FAILURE;
      status.debugMessage = "Failed to open " + cpuUsage + ", error=" + std::to_string (errno)
                   + " (" + strerror(errno) + ")";
      _hidl_cb(status, cpuUsages);
      return Void();
  }
  
  uint64_t user, nice, system, idle;
  unsigned int cpu_id;

  for(std::string line; getline(cpu, line);)
  {
    if(line.length() < 4 || line.compare("cpu") != 0 || !std::isdigit(line.at(3))) continue;
    std::stringstream ss;
    ss << line;
    ss >> cpu_id >> user >> nice >> system >> idle;
    if (cpu_id >= 0 && cpu_id <= core_num) {
	cpuUsages[cpu_id].isOnline = true;
	cpuUsages[cpu_id].active = user + nice + system;
	cpuUsages[cpu_id].total = user + nice + system + idle;
    }
  }

  _hidl_cb(status, cpuUsages);
  return Void();
}

Return<void> Thermal::getCoolingDevices(getCoolingDevices_cb _hidl_cb) {
  ThermalStatus status;
  status.code = ThermalStatusCode::SUCCESS;
  hidl_vec<CoolingDevice> coolingDevices;
  coolingDevices.resize(0);
  _hidl_cb(status, coolingDevices);
  return Void();
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace thermal
}  // namespace hardware
}  // namespace android

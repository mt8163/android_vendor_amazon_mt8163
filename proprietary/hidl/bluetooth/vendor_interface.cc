//
// Copyright 2016 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "vendor_interface.h"

#define LOG_TAG "android.hardware.bluetooth@1.0-impl"
#include <cutils/properties.h>
#include <utils/Log.h>

#include <dlfcn.h>
#include <fcntl.h>

#include "h4_protocol.h"
#include "mct_protocol.h"
#include "bt_mtk.h"

static const int INVALID_FD = -1;
int fd_list[CH_MAX] = {0};

namespace {

using android::hardware::bluetooth::V1_0::implementation::VendorInterface;
using android::hardware::hidl_vec;

struct {
  tINT_CMD_CBACK cb;
  uint16_t opcode;
} internal_command;

// True when LPM is not enabled yet or wake is not asserted.
bool lpm_wake_deasserted;
uint32_t lpm_timeout_ms;
bool recent_activity_flag;

VendorInterface* g_vendor_interface = nullptr;

HC_BT_HDR* WrapPacketAndCopy(uint16_t event, const hidl_vec<uint8_t>& data) {
  size_t packet_size = data.size() + sizeof(HC_BT_HDR);
  HC_BT_HDR* packet = reinterpret_cast<HC_BT_HDR*>(new uint8_t[packet_size]);
  packet->offset = 0;
  packet->len = data.size();
  packet->layer_specific = 0;
  packet->event = event;
  // TODO(eisenbach): Avoid copy here; if BT_HDR->data can be ensured to
  // be the only way the data is accessed, a pointer could be passed here...
  memcpy(packet->data, data.data(), data.size());
  return packet;
}

bool internal_command_event_match(const hidl_vec<uint8_t>& packet) {
  uint8_t event_code = packet[0];
  if (event_code != HCI_COMMAND_COMPLETE_EVENT) {
    ALOGE("%s: Unhandled event type %02X", __func__, event_code);
    return false;
  }

  size_t opcode_offset = HCI_EVENT_PREAMBLE_SIZE + 1;  // Skip num packets.

  uint16_t opcode = packet[opcode_offset] | (packet[opcode_offset + 1] << 8);

  ALOGV("%s internal_command.opcode = %04X opcode = %04x", __func__,
        internal_command.opcode, opcode);
  return opcode == internal_command.opcode;
}

extern "C" void* alloc_buffer(int size) {
  void* p = new uint8_t[size];
  ALOGV("%s pts: %p, size: %d", __func__, p, size);
  return p;
}

extern "C" void free_buffer(void* buffer) {
  ALOGV("%s ptr: %p", __func__, buffer);
  delete[] reinterpret_cast<uint8_t*>(buffer);
}

extern "C" uint8_t transmit_old(uint16_t opcode, void* buffer, tINT_CMD_CBACK callback) {
  ALOGV("%s opcode: 0x%04x, ptr: %p, cb: %p", __func__, opcode, buffer,
        callback);
  internal_command.cb = callback;
  internal_command.opcode = opcode;
  uint8_t type = HCI_PACKET_TYPE_COMMAND;
  HC_BT_HDR* bt_hdr = reinterpret_cast<HC_BT_HDR*>(buffer);
  VendorInterface::get()->Send(type, bt_hdr->data, bt_hdr->len);
  delete[] reinterpret_cast<uint8_t*>(buffer);
  return true;
}

extern "C" void callFirmwareConfigured(uint8_t result) {
  VendorInterface::get()->OnFirmwareConfigured(result);
}

} //namespace

namespace android {
namespace hardware {
namespace bluetooth {
namespace V1_0 {
namespace implementation {

class FirmwareStartupTimer {
 public:
  FirmwareStartupTimer() : start_time_(std::chrono::steady_clock::now()) {}

  ~FirmwareStartupTimer() {
    std::chrono::duration<double> duration =
        std::chrono::steady_clock::now() - start_time_;
    double s = duration.count();
    if (s == 0) return;
    ALOGI("Firmware configured in %.3fs", s);
  }

 private:
  std::chrono::steady_clock::time_point start_time_;
};

bool VendorInterface::Initialize(
    InitializeCompleteCallback initialize_complete_cb,
    PacketReadCallback event_cb, PacketReadCallback acl_cb,
    PacketReadCallback sco_cb) {
  if (g_vendor_interface) {
    ALOGE("%s: No previous Shutdown()?", __func__);
    return false;
  }
  g_vendor_interface = new VendorInterface();
  return g_vendor_interface->Open(initialize_complete_cb, event_cb, acl_cb,
                                  sco_cb);
}

void VendorInterface::Shutdown() {
  LOG_ALWAYS_FATAL_IF(!g_vendor_interface, "%s: No Vendor interface!",
                      __func__);
  g_vendor_interface->Close();
  delete g_vendor_interface;
  g_vendor_interface = nullptr;
}

VendorInterface* VendorInterface::get() { return g_vendor_interface; }

bool VendorInterface::Open(InitializeCompleteCallback initialize_complete_cb,
                           PacketReadCallback event_cb,
                           PacketReadCallback acl_cb,
                           PacketReadCallback sco_cb) {
  initialize_complete_cb_ = initialize_complete_cb;

  // Get the UART socket(s)
  int fd_count = 1;
  int retry = 0;
  
  fd_list[0] = open("/dev/stpbt", O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);

  while ((fd_list[0] == INVALID_FD) && (retry < 7)) {
    ALOGE("Can`t open /dev/stpbt. Retry after 2 seconds");
    usleep(2000000);
    fd_list[0] = open("/dev/stpbt", O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    retry++;
  }

  // The hal will crash either way so do it earlier.
  LOG_ALWAYS_FATAL_IF(fd_list[0] < 0, "Failed to open /dev/stpbt");

  /*if (fd_list[0] < 0) {
   ALOGE("Can't open /dev/stpbt\n");
   return -1;
  }*/

  if (fd_count < 1 || fd_count > CH_MAX - 1) {
    ALOGE("%s: fd_count %d is invalid!", __func__, fd_count);
    return false;
  }

  for (int i = 0; i < fd_count; i++) {
    if (fd_list[i] == INVALID_FD) {
      ALOGE("%s: fd %d is invalid!", __func__, fd_list[i]);
      return false;
    }
  }

  event_cb_ = event_cb;
  PacketReadCallback intercept_events = [this](const hidl_vec<uint8_t>& event) {
    HandleIncomingEvent(event);
  };

  if (fd_count == 1) {
    hci::H4Protocol* h4_hci =
        new hci::H4Protocol(fd_list[0], intercept_events, acl_cb, sco_cb);
    fd_watcher_.WatchFdForNonBlockingReads(
        fd_list[0], [h4_hci](int fd) { h4_hci->OnDataReady(fd); });
    hci_ = h4_hci;
  } else {
    hci::MctProtocol* mct_hci =
        new hci::MctProtocol(fd_list, intercept_events, acl_cb);
    fd_watcher_.WatchFdForNonBlockingReads(
        fd_list[CH_EVT], [mct_hci](int fd) { mct_hci->OnEventDataReady(fd); });
    fd_watcher_.WatchFdForNonBlockingReads(
        fd_list[CH_ACL_IN],
        [mct_hci](int fd) { mct_hci->OnAclDataReady(fd); });
    hci_ = mct_hci;
  }

  // Initially, the power management is off.
  lpm_wake_deasserted = true;

  // Start configuring the firmware
  firmware_startup_timer_ = new FirmwareStartupTimer();
  mtk_fw_cfg();

  return true;
}

void VendorInterface::Close() {
  // These callbacks may send HCI events (vendor-dependent), so make sure to
  // StopWatching the file descriptor after this.
  fd_watcher_.StopWatchingFileDescriptors();

  if (hci_ != nullptr) {
    delete hci_;
    hci_ = nullptr;
  }

  if (fd_list[0] != 0 && fd_list[0] != -1) {
    close(fd_list[0]);
  }

  //RadioMod cleanup
  BT_Cleanup();

  if (firmware_startup_timer_ != nullptr) {
    delete firmware_startup_timer_;
    firmware_startup_timer_ = nullptr;
  }
}

size_t VendorInterface::Send(uint8_t type, const uint8_t* data, size_t length) {
  recent_activity_flag = true;

  if (lpm_wake_deasserted == true) {
    // Restart the timer.
    fd_watcher_.ConfigureTimeout(std::chrono::milliseconds(lpm_timeout_ms),
                                 [this]() { OnTimeout(); });
    // Assert wake.
    lpm_wake_deasserted = false;
    ALOGV("%s: Sent wake before (%02x)", __func__, data[0] | (data[1] << 8));
  }

  return hci_->Send(type, data, length);
}

void VendorInterface::OnFirmwareConfigured(uint8_t result) {
  ALOGD("%s result: %d", __func__, result);

  if (firmware_startup_timer_ != nullptr) {
    delete firmware_startup_timer_;
    firmware_startup_timer_ = nullptr;
  }

  if (initialize_complete_cb_ != nullptr) {
    initialize_complete_cb_(result == 0);
    initialize_complete_cb_ = nullptr;
  }

  lpm_timeout_ms = 5000;

  ALOGD("%s Calling StartLowPowerWatchdog()", __func__);
  fd_watcher_.ConfigureTimeout(std::chrono::milliseconds(lpm_timeout_ms),
                               [this]() { OnTimeout(); });
}

void VendorInterface::OnTimeout() {
  ALOGV("%s", __func__);
  if (recent_activity_flag == false) {
    lpm_wake_deasserted = true;
    fd_watcher_.ConfigureTimeout(std::chrono::seconds(0), []() {
      ALOGE("Zero timeout! Should never happen.");
    });
  }
  recent_activity_flag = false;
}

void VendorInterface::HandleIncomingEvent(const hidl_vec<uint8_t>& hci_packet) {
  if (internal_command.cb != nullptr &&
      internal_command_event_match(hci_packet)) {
    HC_BT_HDR* bt_hdr = WrapPacketAndCopy(HCI_PACKET_TYPE_EVENT, hci_packet);

    // The callbacks can send new commands, so don't zero after calling.
    tINT_CMD_CBACK saved_cb = internal_command.cb;
    internal_command.cb = nullptr;
    saved_cb(bt_hdr);
  } else {
    event_cb_(hci_packet);
  }
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace bluetooth
}  // namespace hardware
}  // namespace android

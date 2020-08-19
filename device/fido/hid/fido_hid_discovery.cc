// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/hid/fido_hid_discovery.h"

#include <utility>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/hid/fido_hid_device.h"

namespace device {

namespace {

// Checks that the supported report sizes of |device| are sufficient for at
// least one byte of non-header data per report and not larger than our maximum
// size.
bool ReportSizesSufficient(const device::mojom::HidDeviceInfo& device) {
  return device.max_input_report_size > kHidInitPacketHeaderSize &&
         device.max_input_report_size <= kHidMaxPacketSize &&
         device.max_output_report_size > kHidInitPacketHeaderSize &&
         device.max_output_report_size <= kHidMaxPacketSize;
}

FidoHidDiscovery::HidManagerBinder& GetHidManagerBinder() {
  static base::NoDestructor<FidoHidDiscovery::HidManagerBinder> binder;
  return *binder;
}

}  // namespace

bool operator==(const VidPid& lhs, const VidPid& rhs) {
  return lhs.vid == rhs.vid && lhs.pid == rhs.pid;
}

bool operator<(const VidPid& lhs, const VidPid& rhs) {
  return lhs.vid < rhs.vid || (lhs.vid == rhs.vid && lhs.pid < rhs.pid);
}

FidoHidDiscovery::FidoHidDiscovery(base::flat_set<VidPid> ignore_list)
    : FidoDeviceDiscovery(FidoTransportProtocol::kUsbHumanInterfaceDevice),
      ignore_list_(std::move(ignore_list)) {
  constexpr uint16_t kFidoUsagePage = 0xf1d0;
  filter_.SetUsagePage(kFidoUsagePage);
}

FidoHidDiscovery::~FidoHidDiscovery() = default;

// static
void FidoHidDiscovery::SetHidManagerBinder(HidManagerBinder binder) {
  GetHidManagerBinder() = std::move(binder);
}

void FidoHidDiscovery::StartInternal() {
  const auto& binder = GetHidManagerBinder();
  if (!binder)
    return;

  binder.Run(hid_manager_.BindNewPipeAndPassReceiver());
  hid_manager_->GetDevicesAndSetClient(
      receiver_.BindNewEndpointAndPassRemote(),
      base::BindOnce(&FidoHidDiscovery::OnGetDevices,
                     weak_factory_.GetWeakPtr()));
}

void FidoHidDiscovery::DeviceAdded(
    device::mojom::HidDeviceInfoPtr device_info) {
  // The init packet header is the larger of the headers so we only compare
  // against it below.
  static_assert(
      kHidInitPacketHeaderSize >= kHidContinuationPacketHeaderSize,
      "init header is expected to be larger than continuation header");

  if (!filter_.Matches(*device_info) || !ReportSizesSufficient(*device_info)) {
    return;
  }

  const VidPid vid_pid{device_info->vendor_id, device_info->product_id};
  if (base::Contains(ignore_list_, vid_pid)) {
    FIDO_LOG(EVENT) << "Ignoring HID device " << vid_pid.vid << ":"
                    << vid_pid.pid;
    return;
  }

  AddDevice(std::make_unique<FidoHidDevice>(std::move(device_info),
                                            hid_manager_.get()));
}

void FidoHidDiscovery::DeviceRemoved(
    device::mojom::HidDeviceInfoPtr device_info) {
  // Ignore non-U2F devices.
  if (filter_.Matches(*device_info)) {
    RemoveDevice(FidoHidDevice::GetIdForDevice(*device_info));
  }
}

void FidoHidDiscovery::OnGetDevices(
    std::vector<device::mojom::HidDeviceInfoPtr> device_infos) {
  for (auto& device_info : device_infos)
    DeviceAdded(std::move(device_info));

  NotifyDiscoveryStarted(true);
}

}  // namespace device

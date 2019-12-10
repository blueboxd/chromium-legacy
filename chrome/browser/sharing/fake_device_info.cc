// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/sharing/fake_device_info.h"

std::unique_ptr<syncer::DeviceInfo> CreateFakeDeviceInfo(
    const std::string& id,
    const std::string& name,
    const base::Optional<syncer::DeviceInfo::SharingInfo>& sharing_info,
    sync_pb::SyncEnums_DeviceType device_type,
    base::SysInfo::HardwareInfo hardware_info,
    base::Time last_updated_timestamp) {
  return std::make_unique<syncer::DeviceInfo>(
      id, name, "chrome_version", "user_agent", device_type, "device_id",
      hardware_info, last_updated_timestamp,
      /*send_tab_to_self_receiving_enabled=*/false, sharing_info);
}

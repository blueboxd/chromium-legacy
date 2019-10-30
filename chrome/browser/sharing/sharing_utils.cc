// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_utils.h"

#include "base/strings/strcat.h"
#include "chrome/grit/generated_resources.h"
#include "components/sync_device_info/device_info.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
// Util function to return a string denoting the type of device.
std::string GetDeviceType(sync_pb::SyncEnums::DeviceType type) {
  int device_type_message_id = -1;

  switch (type) {
    case sync_pb::SyncEnums::TYPE_LINUX:
    case sync_pb::SyncEnums::TYPE_WIN:
    case sync_pb::SyncEnums::TYPE_CROS:
    case sync_pb::SyncEnums::TYPE_MAC:
      device_type_message_id = IDS_BROWSER_SHARING_DEVICE_TYPE_COMPUTER;
      break;

    case sync_pb::SyncEnums::TYPE_UNSET:
    case sync_pb::SyncEnums::TYPE_OTHER:
      device_type_message_id = IDS_BROWSER_SHARING_DEVICE_TYPE_DEVICE;
      break;

    case sync_pb::SyncEnums::TYPE_PHONE:
      device_type_message_id = IDS_BROWSER_SHARING_DEVICE_TYPE_PHONE;
      break;

    case sync_pb::SyncEnums::TYPE_TABLET:
      device_type_message_id = IDS_BROWSER_SHARING_DEVICE_TYPE_TABLET;
      break;
  }

  return l10n_util::GetStringUTF8(device_type_message_id);
}

}  // namespace

SharingDeviceNames GetSharingDeviceNames(const syncer::DeviceInfo* device) {
  DCHECK(device);
  SharingDeviceNames device_names;

  base::SysInfo::HardwareInfo hardware_info = device->hardware_info();

  // The model might be empty if other device is still on M78 or lower with sync
  // turned on.
  if (hardware_info.model.empty()) {
    device_names.full_name = device_names.short_name = device->client_name();
    return device_names;
  }

  sync_pb::SyncEnums::DeviceType type = device->device_type();

  // For chromeOS, return manufacturer + model.
  if (type == sync_pb::SyncEnums::TYPE_CROS) {
    device_names.short_name = device_names.full_name =
        base::StrCat({hardware_info.manufacturer, " ", hardware_info.model});
    return device_names;
  }

  if (hardware_info.manufacturer == "Apple Inc.") {
    // Internal names of Apple devices are formatted as MacbookPro2,3 or
    // iPhone2,1 or Ipad4,1.
    device_names.short_name = hardware_info.model.substr(
        0, hardware_info.model.find_first_of("0123456789,"));
    device_names.full_name = hardware_info.model;
    return device_names;
  }

  device_names.short_name =
      base::StrCat({hardware_info.manufacturer, " ", GetDeviceType(type)});
  device_names.full_name =
      base::StrCat({device_names.short_name, " ", hardware_info.model});
  return device_names;
}

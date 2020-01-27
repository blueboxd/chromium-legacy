// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/device_info_fetcher_linux.h"

#include "base/system/sys_info.h"
#include "net/base/network_interfaces.h"

namespace enterprise_reporting_private =
    ::extensions::api::enterprise_reporting_private;

namespace extensions {
namespace enterprise_reporting {

namespace {

std::string GetDeviceModel() {
  return base::SysInfo::HardwareModelName();
}

std::string GetOsVersion() {
  return base::SysInfo::OperatingSystemVersion();
}

std::string GetDeviceHostName() {
  return net::GetHostName();
}

std::string GetSerialNumber() {
  return std::string();
}

enterprise_reporting_private::SettingValue GetScreenlockSecured() {
  return enterprise_reporting_private::SETTING_VALUE_UNKNOWN;
}

enterprise_reporting_private::SettingValue GetDiskEncrypted() {
  return enterprise_reporting_private::SETTING_VALUE_UNKNOWN;
}

}  // namespace

DeviceInfoFetcherLinux::DeviceInfoFetcherLinux() = default;

DeviceInfoFetcherLinux::~DeviceInfoFetcherLinux() = default;

DeviceInfo DeviceInfoFetcherLinux::Fetch() {
  DeviceInfo device_info;
  device_info.os_name = "linux";
  device_info.os_version = GetOsVersion();
  device_info.device_host_name = GetDeviceHostName();
  device_info.device_model = GetDeviceModel();
  device_info.serial_number = GetSerialNumber();
  device_info.screen_lock_secured = GetScreenlockSecured();
  device_info.disk_encrypted = GetDiskEncrypted();
  return device_info;
}

}  // namespace enterprise_reporting
}  // namespace extensions

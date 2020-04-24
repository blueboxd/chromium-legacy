// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/device_info_fetcher.h"

#include "build/build_config.h"

#if defined(OS_MACOSX)
#include "chrome/browser/extensions/api/enterprise_reporting_private/device_info_fetcher_mac.h"
#elif defined(OS_WIN)
#include "chrome/browser/extensions/api/enterprise_reporting_private/device_info_fetcher_win.h"
#elif defined(OS_LINUX)
#include "chrome/browser/extensions/api/enterprise_reporting_private/device_info_fetcher_linux.h"
#endif

namespace extensions {
namespace enterprise_reporting {

namespace {

// Stub implementation of DeviceInfoFetcher.
class StubDeviceFetcher : public DeviceInfoFetcher {
 public:
  StubDeviceFetcher() {}

  ~StubDeviceFetcher() override = default;

  DeviceInfo Fetch() override {
    DeviceInfo device_info;
    device_info.os_name = "stubOS";
    device_info.os_version = "0.0.0.0";
    device_info.device_host_name = "midnightshift";
    device_info.device_model = "topshot";
    device_info.serial_number = "twirlchange";
    device_info.screen_lock_secured =
        ::extensions::api::enterprise_reporting_private::SETTING_VALUE_ENABLED;
    device_info.disk_encrypted =
        ::extensions::api::enterprise_reporting_private::SETTING_VALUE_DISABLED;
    return device_info;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StubDeviceFetcher);
};

}  // namespace

DeviceInfoFetcher::DeviceInfoFetcher() {}

DeviceInfoFetcher::~DeviceInfoFetcher() = default;

std::unique_ptr<DeviceInfoFetcher> DeviceInfoFetcher::CreateInstance() {
// TODO(pastarmovj): Instead of the if-defs implement the CreateInstance
// function in the platform specific classes.
#if defined(OS_MACOSX)
  return std::make_unique<DeviceInfoFetcherMac>();
#elif defined(OS_WIN)
  return std::make_unique<DeviceInfoFetcherWin>();
#elif defined(OS_LINUX)
  return std::make_unique<DeviceInfoFetcherLinux>();
#else
  return std::make_unique<StubDeviceFetcher>();
#endif
}

}  // namespace enterprise_reporting
}  // namespace extensions

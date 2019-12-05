// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/device_info_fetcher.h"

#include "base/logging.h"

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
  // TODO(996079): Return a stub implementation for now. We will add
  // platform-specific implementations in follow-up CLs.
  return std::make_unique<StubDeviceFetcher>();
}

}  // namespace enterprise_reporting
}  // namespace extensions

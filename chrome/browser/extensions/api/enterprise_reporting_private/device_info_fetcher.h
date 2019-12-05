// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_DEVICE_INFO_FETCHER_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_DEVICE_INFO_FETCHER_H_

#include "base/macros.h"
#include "base/values.h"
#include "chrome/common/extensions/api/enterprise_reporting_private.h"

namespace extensions {
namespace enterprise_reporting {

using DeviceInfo = ::extensions::api::enterprise_reporting_private::DeviceInfo;

// Interface used by the chrome.enterprise.reportingPrivate.getDeviceInfo()
// function that fetches info of the device. Each supported platform has its own
// subclass implementation.
class DeviceInfoFetcher {
 public:
  DeviceInfoFetcher();
  virtual ~DeviceInfoFetcher();

  // Returns a platform specific instance of DeviceInfoFetcher.
  static std::unique_ptr<DeviceInfoFetcher> CreateInstance();

  // Fetches the device information for the current platform.
  virtual DeviceInfo Fetch() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceInfoFetcher);
};

}  // namespace enterprise_reporting
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_DEVICE_INFO_FETCHER_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_DEVICE_INFO_FETCHER_MAC_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_DEVICE_INFO_FETCHER_MAC_H_

#include "chrome/browser/extensions/api/enterprise_reporting_private/device_info_fetcher.h"

namespace extensions {
namespace enterprise_reporting {

using DeviceInfo = ::extensions::api::enterprise_reporting_private::DeviceInfo;

// MacOS implementation of DeviceInfoFetcher.
class DeviceInfoFetcherMac : public DeviceInfoFetcher {
 public:
  DeviceInfoFetcherMac();
  ~DeviceInfoFetcherMac() override;

  // Fetches the device info for the current platform.
  DeviceInfo Fetch() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceInfoFetcherMac);
};

}  // namespace enterprise_reporting
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_DEVICE_INFO_FETCHER_MAC_H_

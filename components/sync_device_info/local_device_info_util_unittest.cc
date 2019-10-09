// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/local_device_info_util.h"

#include <utility>

#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "base/command_line.h"
#include "chromeos/constants/chromeos_switches.h"
#endif  // OS_CHROMEOS

namespace syncer {
namespace {

// Call GetPersonalizableDeviceNameBlocking and make sure its return
// value looks sane.
TEST(GetSessionNameTest, GetPersonalizableDeviceNameBlocking) {
  const std::string& session_name = GetPersonalizableDeviceNameBlocking();
  EXPECT_FALSE(session_name.empty());
}

#if defined(OS_CHROMEOS)

// Call GetPersonalizableDeviceNameBlocking on ChromeOS where the
// board type is CHROMEBOOK and make sure the return value is "Chromebook".
TEST(GetSessionNameTest, GetPersonalizableDeviceNameBlockingChromebook) {
  const char* kLsbRelease = "DEVICETYPE=CHROMEBOOK\n";
  base::SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease, base::Time());
  const std::string& session_name = GetPersonalizableDeviceNameBlocking();
  EXPECT_EQ("Chromebook", session_name);
}

// Call GetPersonalizableDeviceNameBlocking on ChromeOS where the
// board type is a CHROMEBOX and make sure the return value is "Chromebox".
TEST(GetSessionNameTest, GetPersonalizableDeviceNameBlockingChromebox) {
  const char* kLsbRelease = "DEVICETYPE=CHROMEBOX\n";
  base::SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease, base::Time());
  const std::string& session_name = GetPersonalizableDeviceNameBlocking();
  EXPECT_EQ("Chromebox", session_name);
}

#endif  // OS_CHROMEOS

}  // namespace
}  // namespace syncer

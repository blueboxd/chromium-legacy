// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/common/user_agent.h"

#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

using UserAgentTest = PlatformTest;

// Tests conversions between UserAgentType values and their descriptions
TEST_F(UserAgentTest, UserAgentTypeDescription) {
  const std::string kMobileDescription("MOBILE");
  const std::string kDesktopDescription("DESKTOP");
  const std::string kNoneDescription("NONE");
  const std::string kInvalidDescription(
      "not returned by GetUserAgentTypeDescription()");
  EXPECT_EQ(kMobileDescription,
            GetUserAgentTypeDescription(UserAgentType::MOBILE));
  EXPECT_EQ(kDesktopDescription,
            GetUserAgentTypeDescription(UserAgentType::DESKTOP));
  EXPECT_EQ(kNoneDescription, GetUserAgentTypeDescription(UserAgentType::NONE));
  EXPECT_EQ(UserAgentType::MOBILE,
            GetUserAgentTypeWithDescription(kMobileDescription));
  EXPECT_EQ(UserAgentType::DESKTOP,
            GetUserAgentTypeWithDescription(kDesktopDescription));
  EXPECT_EQ(UserAgentType::NONE,
            GetUserAgentTypeWithDescription(kNoneDescription));
  EXPECT_EQ(UserAgentType::NONE,
            GetUserAgentTypeWithDescription(kInvalidDescription));
}

// Tests the user agent returned for a specific product.
TEST_F(UserAgentTest, UserAgentForProduct) {
  std::string product = "my_product_name";

  std::string platform;
  std::string cpu;
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    platform = "iPad";
    cpu = "OS";
  } else {
    platform = "iPhone";
    cpu = "iPhone OS";
  }

  std::string os_version;
  int32_t os_major_version = 0;
  int32_t os_minor_version = 0;
  int32_t os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(
      &os_major_version, &os_minor_version, &os_bugfix_version);
  base::StringAppendF(&os_version, "%d_%d", os_major_version, os_minor_version);

  std::string safari_version;
  if (@available(iOS 13, *)) {
    safari_version = "604.1";
  } else {
    safari_version = "605.1";
  }

  std::string expected_user_agent;
  base::StringAppendF(
      &expected_user_agent,
      "Mozilla/5.0 (%s; CPU %s %s like Mac OS X) AppleWebKit/605.1.15 (KHTML, "
      "like Gecko) %s Mobile/15E148 Safari/%s",
      platform.c_str(), cpu.c_str(), os_version.c_str(), product.c_str(),
      safari_version.c_str());

  std::string result = BuildUserAgentFromProduct(product);

  EXPECT_EQ(expected_user_agent, result);
}

}  // namespace web

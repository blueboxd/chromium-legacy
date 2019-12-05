// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/common/user_agent.h"

#import <UIKit/UIKit.h>

#include <stddef.h>
#include <stdint.h>
#include <sys/sysctl.h>
#include <string>

#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/system/sys_info.h"
#include "ios/web/common/features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kDesktopUserAgent[] =
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_13_5) "
    "AppleWebKit/605.1.15 (KHTML, like Gecko) "
    "Version/11.1.1 "
    "Safari/605.1.15";

// UserAgentType description strings.
const char kUserAgentTypeAutomaticDescription[] = "AUTOMATIC";
const char kUserAgentTypeNoneDescription[] = "NONE";
const char kUserAgentTypeMobileDescription[] = "MOBILE";
const char kUserAgentTypeDesktopDescription[] = "DESKTOP";

std::string OSVersion(web::UserAgentType type) {
  int32_t os_major_version = 0;
  int32_t os_minor_version = 0;
  int32_t os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(
      &os_major_version, &os_minor_version, &os_bugfix_version);

  DCHECK_EQ(web::UserAgentType::MOBILE, type);
  std::string os_version;
  base::StringAppendF(&os_version, "%d_%d", os_major_version, os_minor_version);
  return os_version;
}

}  // namespace

namespace web {

std::string GetUserAgentTypeDescription(UserAgentType type) {
  switch (type) {
    case UserAgentType::AUTOMATIC:
      DCHECK(base::FeatureList::IsEnabled(features::kDefaultToDesktopOnIPad));
      return std::string(kUserAgentTypeAutomaticDescription);
    case UserAgentType::NONE:
      return std::string(kUserAgentTypeNoneDescription);
    case UserAgentType::MOBILE:
      return std::string(kUserAgentTypeMobileDescription);
    case UserAgentType::DESKTOP:
      return std::string(kUserAgentTypeDesktopDescription);
  }
}

UserAgentType GetUserAgentTypeWithDescription(const std::string& description) {
  if (description == std::string(kUserAgentTypeMobileDescription))
    return UserAgentType::MOBILE;
  if (description == std::string(kUserAgentTypeDesktopDescription))
    return UserAgentType::DESKTOP;
  return UserAgentType::NONE;
}

UserAgentType GetDefaultUserAgent(UIView* web_view) {
  DCHECK(base::FeatureList::IsEnabled(features::kDefaultToDesktopOnIPad));
  BOOL isRegularRegular = web_view.traitCollection.horizontalSizeClass ==
                              UIUserInterfaceSizeClassRegular &&
                          web_view.traitCollection.verticalSizeClass ==
                              UIUserInterfaceSizeClassRegular;
  return isRegularRegular ? UserAgentType::DESKTOP : UserAgentType::MOBILE;
}

std::string BuildOSCpuInfo(web::UserAgentType type) {
  std::string os_cpu;
  DCHECK_EQ(web::UserAgentType::MOBILE, type);
  // Remove the end of the platform name. For example "iPod touch" becomes
  // "iPod".
  std::string platform =
      base::SysNSStringToUTF8([[UIDevice currentDevice] model]);
  size_t position = platform.find_first_of(" ");
  if (position != std::string::npos)
    platform = platform.substr(0, position);

  base::StringAppendF(&os_cpu, "%s; CPU %s %s like Mac OS X", platform.c_str(),
                      (platform == "iPad") ? "OS" : "iPhone OS",
                      OSVersion(type).c_str());

  return os_cpu;
}

std::string BuildUserAgentFromProduct(UserAgentType type,
                                      const std::string& product) {
  if (type == web::UserAgentType::DESKTOP)
    return kDesktopUserAgent;

  DCHECK_EQ(web::UserAgentType::MOBILE, type);
  std::string user_agent;
  base::StringAppendF(&user_agent,
                      "Mozilla/5.0 (%s) AppleWebKit/605.1.15"
                      " (KHTML, like Gecko) %s Mobile/15E148 Safari/604.1",
                      BuildOSCpuInfo(type).c_str(), product.c_str());

  return user_agent;
}

}  // namespace web

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_util.h"

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "net/third_party/uri_template/uri_template.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/enterprise_util.h"
#include "base/win/windows_version.h"
#include "chrome/browser/win/parental_controls.h"
#endif

namespace chrome_browser_net {

namespace {

#if defined(OS_WIN)
bool ShouldDisableDohForWindowsParentalControls() {
  const WinParentalControls& parental_controls = GetWinParentalControls();
  if (parental_controls.web_filter)
    return true;

  // Some versions before Windows 8 may not fully support |web_filter|, so
  // conservatively disable doh for any recognized parental controls.
  if (parental_controls.any_restrictions &&
      base::win::GetVersion() < base::win::Version::WIN8) {
    return true;
  }

  return false;
}
#endif  // defined(OS_WIN)

}  // namespace

bool ShouldDisableDohForManaged() {
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  if (g_browser_process->browser_policy_connector()->HasMachineLevelPolicies())
    return true;
#endif
#if defined(OS_WIN)
  if (base::IsMachineExternallyManaged())
    return true;
#endif
  return false;
}

bool ShouldDisableDohForParentalControls() {
#if defined(OS_WIN)
  return ShouldDisableDohForWindowsParentalControls();
#endif

  return false;
}

}  // namespace chrome_browser_net

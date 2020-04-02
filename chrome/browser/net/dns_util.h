// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_DNS_UTIL_H_
#define CHROME_BROWSER_NET_DNS_UTIL_H_

#include <vector>

#include "base/strings/string_piece.h"

class PrefRegistrySimple;
class PrefService;

namespace chrome_browser_net {

// Implements the whitespace-delimited group syntax for DoH templates.
std::vector<base::StringPiece> SplitDohTemplateGroup(base::StringPiece group);

// Returns true if a group of templates are all valid per
// net::dns_util::IsValidDohTemplate().  This should be checked before updating
// stored preferences.
bool IsValidDohTemplateGroup(base::StringPiece group);

const char kDnsOverHttpsModeOff[] = "off";
const char kDnsOverHttpsModeAutomatic[] = "automatic";
const char kDnsOverHttpsModeSecure[] = "secure";

// Forced management description types. We will check for the override cases in
// the order they are listed in the enum.
enum class SecureDnsUiManagementMode {
  // Chrome did not override the secure DNS settings.
  kNoOverride,
  // Secure DNS was disabled due to detection of a managed environment.
  kDisabledManaged,
  // Secure DNS was disabled due to detection of OS-level parental controls.
  kDisabledParentalControls,
};

// Registers the backup preference required for the DNS probes setting reset.
// TODO(crbug.com/1062698): Remove this once the privacy settings redesign
// is fully launched.
void RegisterDNSProbesSettingBackupPref(PrefRegistrySimple* registry);

// Backs up the unneeded preference controlling DNS and captive portal probes
// once the privacy settings redesign is enabled, or restores the backup
// in case the feature is rolled back.
// TODO(crbug.com/1062698): Remove this once the privacy settings redesign
// is fully launched.
void MigrateDNSProbesSettingToOrFromBackup(PrefService* prefs);

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_DNS_UTIL_H_

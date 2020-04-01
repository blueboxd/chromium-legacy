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

// Returns true if there are any active machine level policies or if the machine
// is domain joined. This special logic is used to disable DoH by default for
// Desktop platforms (the enterprise policy field default_for_enterprise_users
// only applies to ChromeOS). We don't attempt enterprise detection on Android
// at this time.
bool ShouldDisableDohForManaged();

// Returns true if there are parental controls detected on the device.
bool ShouldDisableDohForParentalControls();

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

// Detailed descriptions of the secure DNS mode. These values are logged to UMA.
// Entries should not be renumbered and numeric values should never be reused.
// Please keep in sync with "SecureDnsModeDetails" in
// src/tools/metrics/histograms/enums.xml.
enum class SecureDnsModeDetailsForHistogram {
  // The mode is controlled by the user and is set to 'off'.
  kOffByUser = 0,
  // The mode is controlled via enterprise policy and is set to 'off'.
  kOffByEnterprisePolicy = 1,
  // Chrome detected a managed environment and forced the mode to 'off'.
  kOffByDetectedManagedEnvironment = 2,
  // Chrome detected parental controls and forced the mode to 'off'.
  kOffByDetectedParentalControls = 3,
  // The mode is controlled by the user and is set to 'automatic' (the default
  // mode).
  kAutomaticByUser = 4,
  // The mode is controlled via enterprise policy and is set to 'automatic'.
  kAutomaticByEnterprisePolicy = 5,
  // The mode is controlled by the user and is set to 'secure'.
  kSecureByUser = 6,
  // The mode is controlled via enterprise policy and is set to 'secure'.
  kSecureByEnterprisePolicy = 7,
  kMaxValue = kSecureByEnterprisePolicy,
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

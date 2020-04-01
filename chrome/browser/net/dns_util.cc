// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_util.h"

#include <algorithm>
#include <string>

#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/common/chrome_features.h"
#include "components/embedder_support/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "net/dns/public/util.h"
#include "net/third_party/uri_template/uri_template.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/enterprise_util.h"
#include "base/win/windows_version.h"
#include "chrome/browser/win/parental_controls.h"
#endif

namespace chrome_browser_net {

namespace {

const char kAlternateErrorPagesBackup[] = "alternate_error_pages.backup";

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

void RegisterDNSProbesSettingBackupPref(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kAlternateErrorPagesBackup, true);
}

void MigrateDNSProbesSettingToOrFromBackup(PrefService* prefs) {
  // If the privacy settings redesign is enabled and the user value of the
  // preference hasn't been backed up yet, back it up, and clear it. That way,
  // the preference will revert to using the hardcoded default value (unless
  // it's managed by a policy or an extension). This is necessary, as the
  // privacy settings redesign removed the user-facing toggle, and so the
  // user value of the preference is no longer modifiable.
  if (base::FeatureList::IsEnabled(features::kPrivacySettingsRedesign) &&
      !prefs->HasPrefPath(kAlternateErrorPagesBackup)) {
    // If the user never changed the value of the preference and still uses the
    // hardcoded default value, we'll consider it to be the user value for
    // the purposes of this migration.
    const base::Value* user_value =
        prefs->FindPreference(embedder_support::kAlternateErrorPagesEnabled)
                ->HasUserSetting()
            ? prefs->GetUserPrefValue(
                  embedder_support::kAlternateErrorPagesEnabled)
            : prefs->GetDefaultPrefValue(
                  embedder_support::kAlternateErrorPagesEnabled);

    DCHECK(user_value->is_bool());
    prefs->SetBoolean(kAlternateErrorPagesBackup, user_value->GetBool());
    prefs->ClearPref(embedder_support::kAlternateErrorPagesEnabled);
  }

  // If the privacy settings redesign is rolled back and there is a backed up
  // value of the preference, restore it to the original preference, and clear
  // the backup.
  if (!base::FeatureList::IsEnabled(features::kPrivacySettingsRedesign) &&
      prefs->HasPrefPath(kAlternateErrorPagesBackup)) {
    prefs->SetBoolean(embedder_support::kAlternateErrorPagesEnabled,
                      prefs->GetBoolean(kAlternateErrorPagesBackup));
    prefs->ClearPref(kAlternateErrorPagesBackup);
  }
}

std::vector<base::StringPiece> SplitDohTemplateGroup(base::StringPiece group) {
  // Templates in a group are whitespace-separated.
  return SplitStringPiece(group, " ", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
}

bool IsValidDohTemplateGroup(base::StringPiece group) {
  // All templates must be valid for the group to be considered valid.
  std::vector<base::StringPiece> templates = SplitDohTemplateGroup(group);
  return std::all_of(templates.begin(), templates.end(), [](auto t) {
    std::string method;
    return net::dns_util::IsValidDohTemplate(t, &method);
  });
}

}  // namespace chrome_browser_net

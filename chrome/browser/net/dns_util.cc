// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_util.h"

#include <algorithm>
#include <string>

#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "chrome/common/chrome_features.h"
#include "components/embedder_support/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "net/dns/public/util.h"
#include "net/third_party/uri_template/uri_template.h"
#include "url/gurl.h"

namespace chrome_browser_net {

namespace {

const char kAlternateErrorPagesBackup[] = "alternate_error_pages.backup";

}  // namespace

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

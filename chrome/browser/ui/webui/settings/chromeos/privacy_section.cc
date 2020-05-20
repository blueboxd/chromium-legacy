// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/privacy_section.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/settings/shared_settings_localized_strings_provider.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {
namespace settings {
namespace {

const std::vector<SearchConcept>& GetPrivacySearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_PRIVACY_VERIFIED_ACCESS,
       mojom::kPrivacyAndSecuritySectionPath,
       mojom::SearchResultIcon::kShield,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kVerifiedAccess}},
      {IDS_OS_SETTINGS_TAG_PRIVACY,
       mojom::kPrivacyAndSecuritySectionPath,
       mojom::SearchResultIcon::kShield,
       mojom::SearchResultDefaultRank::kHigh,
       mojom::SearchResultType::kSection,
       {.section = mojom::Section::kPrivacyAndSecurity}},
      {IDS_OS_SETTINGS_TAG_PRIVACY_WIFI_SLEEP,
       mojom::kPrivacyAndSecuritySectionPath,
       mojom::SearchResultIcon::kShield,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kKeepWifiOnDuringSleep},
       {IDS_OS_SETTINGS_TAG_PRIVACY_WIFI_SLEEP_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const std::vector<SearchConcept>& GetPrivacyGoogleChromeSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_PRIVACY_CRASH_REPORTS,
       mojom::kPrivacyAndSecuritySectionPath,
       mojom::SearchResultIcon::kShield,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kUsageStatsAndCrashReports},
       {IDS_OS_SETTINGS_TAG_PRIVACY_CRASH_REPORTS_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace

PrivacySection::PrivacySection(Profile* profile,
                               SearchTagRegistry* search_tag_registry)
    : OsSettingsSection(profile, search_tag_registry) {
  registry()->AddSearchTags(GetPrivacySearchConcepts());
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  registry()->AddSearchTags(GetPrivacyGoogleChromeSearchConcepts());
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

PrivacySection::~PrivacySection() = default;

void PrivacySection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"privacyPageTitle", IDS_SETTINGS_PRIVACY},
      {"enableLogging", IDS_SETTINGS_ENABLE_LOGGING_TOGGLE_TITLE},
      {"enableLoggingDesc", IDS_SETTINGS_ENABLE_LOGGING_TOGGLE_DESC},
      {"wakeOnWifi", IDS_SETTINGS_WAKE_ON_WIFI_DESCRIPTION},
      {"enableContentProtectionAttestation",
       IDS_SETTINGS_ENABLE_CONTENT_PROTECTION_ATTESTATION},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddBoolean(
      "privacySettingsRedesignEnabled",
      base::FeatureList::IsEnabled(::features::kPrivacySettingsRedesign));

  html_source->AddString("syncAndGoogleServicesLearnMoreURL",
                         chrome::kSyncAndGoogleServicesLearnMoreURL);
  ::settings::AddPersonalizationOptionsStrings(html_source);
}

int PrivacySection::GetSectionNameMessageId() const {
  return IDS_SETTINGS_PRIVACY;
}

void PrivacySection::RegisterHierarchy(HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSetting(mojom::Setting::kVerifiedAccess);
  generator->RegisterTopLevelSetting(mojom::Setting::kKeepWifiOnDuringSleep);
  generator->RegisterTopLevelSetting(
      mojom::Setting::kUsageStatsAndCrashReports);
}

}  // namespace settings
}  // namespace chromeos

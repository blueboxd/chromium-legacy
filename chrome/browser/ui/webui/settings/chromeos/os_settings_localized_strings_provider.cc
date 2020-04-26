// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_localized_strings_provider.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/mojom/assistant_state_controller.mojom.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/chromeos/local_search_service/local_search_service.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/network_element_localized_strings_provider.h"
#include "chrome/browser/ui/webui/management_ui.h"
#include "chrome/browser/ui/webui/policy_indicator_localized_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/about_page_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/accessibility_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/apps_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/bluetooth_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/crostini_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/date_time_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/files_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/internet_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/languages_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/multidevice_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_features_util.h"
#include "chrome/browser/ui/webui/settings/chromeos/people_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/personalization_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/plugin_vm_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/printing_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/privacy_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/reset_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_concept.h"
#include "chrome/browser/ui/webui/settings/chromeos/search_strings_provider.h"
#include "chrome/browser/ui/webui/settings/shared_settings_localized_strings_provider.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_ui/version_ui_constants.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/display/types/display_constants.h"

namespace chromeos {
namespace settings {
namespace {

std::vector<local_search_service::Data> ConceptVectorToDataVector(
    const std::vector<SearchConcept>& tags_group) {
  std::vector<local_search_service::Data> data_list;

  for (const auto& concept : tags_group) {
    std::vector<base::string16> search_tags;

    // Add the canonical tag.
    search_tags.push_back(
        l10n_util::GetStringUTF16(concept.canonical_message_id));

    // Add all alternate tags.
    for (size_t i = 0; i < SearchConcept::kMaxAltTagsPerConcept; ++i) {
      int curr_alt_tag = concept.alt_tag_ids[i];
      if (curr_alt_tag == SearchConcept::kAltTagEnd)
        break;
      search_tags.push_back(l10n_util::GetStringUTF16(curr_alt_tag));
    }

    // Note: A stringified version of the canonical tag message ID is used as
    // the identifier for this search data.
    data_list.emplace_back(base::NumberToString(concept.canonical_message_id),
                           search_tags);
  }

  return data_list;
}

void AddCommonStrings(content::WebUIDataSource* html_source, Profile* profile) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"add", IDS_ADD},
      {"advancedPageTitle", IDS_SETTINGS_ADVANCED},
      {"back", IDS_ACCNAME_BACK},
      {"basicPageTitle", IDS_SETTINGS_BASIC},
      {"cancel", IDS_CANCEL},
      {"clear", IDS_SETTINGS_CLEAR},
      {"close", IDS_CLOSE},
      {"confirm", IDS_CONFIRM},
      {"continue", IDS_SETTINGS_CONTINUE},
      {"controlledByExtension", IDS_SETTINGS_CONTROLLED_BY_EXTENSION},
      {"custom", IDS_SETTINGS_CUSTOM},
      {"delete", IDS_SETTINGS_DELETE},
      {"deviceOff", IDS_SETTINGS_DEVICE_OFF},
      {"deviceOn", IDS_SETTINGS_DEVICE_ON},
      {"disable", IDS_DISABLE},
      {"done", IDS_DONE},
      {"edit", IDS_SETTINGS_EDIT},
      {"extensionsLinkTooltip", IDS_SETTINGS_MENU_EXTENSIONS_LINK_TOOLTIP},
      {"learnMore", IDS_LEARN_MORE},
      {"menu", IDS_MENU},
      {"menuButtonLabel", IDS_SETTINGS_MENU_BUTTON_LABEL},
      {"moreActions", IDS_SETTINGS_MORE_ACTIONS},
      {"ok", IDS_OK},
      {"restart", IDS_SETTINGS_RESTART},
      {"save", IDS_SAVE},
      {"searchResultBubbleText", IDS_SEARCH_RESULT_BUBBLE_TEXT},
      {"searchResultsBubbleText", IDS_SEARCH_RESULTS_BUBBLE_TEXT},
      {"settings", IDS_SETTINGS_SETTINGS},
      {"settingsAltPageTitle", IDS_SETTINGS_ALT_PAGE_TITLE},
      {"subpageArrowRoleDescription", IDS_SETTINGS_SUBPAGE_BUTTON},
      {"notValidWebAddress", IDS_SETTINGS_NOT_VALID_WEB_ADDRESS},
      {"notValidWebAddressForContentType",
       IDS_SETTINGS_NOT_VALID_WEB_ADDRESS_FOR_CONTENT_TYPE},

      // Common font related strings shown in a11y and appearance sections.
      {"quickBrownFox", IDS_SETTINGS_QUICK_BROWN_FOX},
      {"verySmall", IDS_SETTINGS_VERY_SMALL_FONT},
      {"small", IDS_SETTINGS_SMALL_FONT},
      {"medium", IDS_SETTINGS_MEDIUM_FONT},
      {"large", IDS_SETTINGS_LARGE_FONT},
      {"veryLarge", IDS_SETTINGS_VERY_LARGE_FONT},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddBoolean("isGuest", features::IsGuestModeActive());

  html_source->AddBoolean("isSupervised", profile->IsSupervised());
}

void AddChromeOSUserStrings(content::WebUIDataSource* html_source,
                            Profile* profile) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  const user_manager::User* primary_user = user_manager->GetPrimaryUser();
  std::string primary_user_email = primary_user->GetAccountId().GetUserEmail();
  html_source->AddString("primaryUserEmail", primary_user_email);
  html_source->AddString("browserSettingsBannerText",
                         l10n_util::GetStringFUTF16(
                             IDS_SETTINGS_BROWSER_SETTINGS_BANNER,
                             base::ASCIIToUTF16(chrome::kChromeUISettingsURL)));
  html_source->AddBoolean(
      "isSecondaryUser",
      user && user->GetAccountId() != primary_user->GetAccountId());
  html_source->AddString(
      "secondaryUserBannerText",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_SECONDARY_USER_BANNER,
                                 base::ASCIIToUTF16(primary_user_email)));
}

void AddSearchInSettingsStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"searchPrompt", IDS_SETTINGS_SEARCH_PROMPT},
      {"searchNoResults", IDS_SEARCH_NO_RESULTS},
      {"searchResults", IDS_SEARCH_RESULTS},
      {"searchResultSelected", IDS_OS_SEARCH_RESULT_ROW_A11Y_RESULT_SELECTED},
      {"searchResultsOne", IDS_OS_SEARCH_BOX_A11Y_ONE_RESULT},
      {"searchResultsNumber", IDS_OS_SEARCH_BOX_A11Y_RESULT_COUNT},
      // TODO(dpapad): IDS_DOWNLOAD_CLEAR_SEARCH and IDS_HISTORY_CLEAR_SEARCH
      // are identical, merge them to one and re-use here.
      {"clearSearch", IDS_DOWNLOAD_CLEAR_SEARCH},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddString(
      "searchNoOsResultsHelp",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_SEARCH_NO_RESULTS_HELP,
          base::ASCIIToUTF16(chrome::kOsSettingsSearchHelpURL)));

  html_source->AddBoolean(
      "newOsSettingsSearch",
      base::FeatureList::IsEnabled(chromeos::features::kNewOsSettingsSearch));
}

}  // namespace

OsSettingsLocalizedStringsProvider::OsSettingsLocalizedStringsProvider(
    Profile* profile,
    local_search_service::LocalSearchService* local_search_service,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    syncer::SyncService* sync_service,
    SupervisedUserService* supervised_user_service,
    KerberosCredentialsManager* kerberos_credentials_manager,
    ArcAppListPrefs* arc_app_list_prefs)
    : index_(local_search_service->GetIndex(
          local_search_service::IndexId::kCrosSettings)) {
  // Add per-page string providers.
  // TODO(khorimoto): Add providers for the remaining pages.
  per_page_providers_.push_back(
      std::make_unique<InternetStringsProvider>(profile, /*delegate=*/this));
  per_page_providers_.push_back(
      std::make_unique<BluetoothStringsProvider>(profile, /*delegate=*/this));
  per_page_providers_.push_back(std::make_unique<MultiDeviceStringsProvider>(
      profile, /*delegate=*/this, multidevice_setup_client));
  per_page_providers_.push_back(std::make_unique<PeopleStringsProvider>(
      profile, /*delegate=*/this, sync_service, supervised_user_service,
      kerberos_credentials_manager));
  per_page_providers_.push_back(
      std::make_unique<DeviceStringsProvider>(profile, /*delegate=*/this));
  per_page_providers_.push_back(
      std::make_unique<PersonalizationStringsProvider>(
          profile, /*delegate=*/this, profile->GetPrefs()));
  per_page_providers_.push_back(
      std::make_unique<SearchStringsProvider>(profile, /*delegate=*/this));
  per_page_providers_.push_back(std::make_unique<AppsStringsProvider>(
      profile, /*delegate=*/this, profile->GetPrefs(), arc_app_list_prefs));
  per_page_providers_.push_back(std::make_unique<CrostiniStringsProvider>(
      profile, /*delegate=*/this, profile->GetPrefs()));
  per_page_providers_.push_back(std::make_unique<PluginVmStringsProvider>(
      profile, /*delegate=*/this, profile->GetPrefs()));
  per_page_providers_.push_back(
      std::make_unique<DateTimeStringsProvider>(profile, /*delegate=*/this));
  per_page_providers_.push_back(
      std::make_unique<PrivacyStringsProvider>(profile, /*delegate=*/this));
  per_page_providers_.push_back(
      std::make_unique<LanguagesStringsProvider>(profile, /*delegate=*/this));
  per_page_providers_.push_back(
      std::make_unique<FilesStringsProvider>(profile, /*delegate=*/this));
  per_page_providers_.push_back(
      std::make_unique<PrintingStringsProvider>(profile, /*delegate=*/this));
  per_page_providers_.push_back(std::make_unique<AccessibilityStringsProvider>(
      profile, /*delegate=*/this, profile->GetPrefs()));
  per_page_providers_.push_back(
      std::make_unique<ResetStringsProvider>(profile, /*delegate=*/this));
  per_page_providers_.push_back(
      std::make_unique<AboutPageStringsProvider>(profile, /*delegate=*/this));
}

OsSettingsLocalizedStringsProvider::~OsSettingsLocalizedStringsProvider() =
    default;

void OsSettingsLocalizedStringsProvider::AddOsLocalizedStrings(
    content::WebUIDataSource* html_source,
    Profile* profile) {
  for (const auto& per_page_provider : per_page_providers_)
    per_page_provider->AddUiStrings(html_source);

  // TODO(khorimoto): Migrate these to OsSettingsPerPageStringsProvider
  // instances.
  AddChromeOSUserStrings(html_source, profile);
  AddCommonStrings(html_source, profile);
  AddSearchInSettingsStrings(html_source);

  policy_indicator::AddLocalizedStrings(html_source);

  html_source->UseStringsJs();
}

const SearchConcept*
OsSettingsLocalizedStringsProvider::GetCanonicalTagMetadata(
    int canonical_message_id) const {
  const auto it = canonical_id_to_metadata_map_.find(canonical_message_id);
  if (it == canonical_id_to_metadata_map_.end())
    return nullptr;
  return it->second;
}

void OsSettingsLocalizedStringsProvider::Shutdown() {
  index_ = nullptr;

  // Delete all per-page providers, since some of them depend on KeyedServices.
  per_page_providers_.clear();
}

void OsSettingsLocalizedStringsProvider::AddSearchTags(
    const std::vector<SearchConcept>& tags_group) {
  // Note: Can be null after Shutdown().
  if (!index_)
    return;

  index_->AddOrUpdate(ConceptVectorToDataVector(tags_group));

  // Add each concept to the map. Note that it is safe to take the address of
  // each concept because all concepts are allocated via static
  // base::NoDestructor objects in the Get*SearchConcepts() helper functions.
  for (const auto& concept : tags_group)
    canonical_id_to_metadata_map_[concept.canonical_message_id] = &concept;
}

void OsSettingsLocalizedStringsProvider::RemoveSearchTags(
    const std::vector<SearchConcept>& tags_group) {
  // Note: Can be null after Shutdown().
  if (!index_)
    return;

  std::vector<std::string> ids;
  for (const auto& concept : tags_group) {
    canonical_id_to_metadata_map_.erase(concept.canonical_message_id);
    ids.push_back(base::NumberToString(concept.canonical_message_id));
  }

  index_->Delete(ids);
}

}  // namespace settings
}  // namespace chromeos

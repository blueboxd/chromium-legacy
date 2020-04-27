// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_localized_strings_provider.h"

#include "chrome/browser/chromeos/local_search_service/local_search_service.h"
#include "chrome/browser/profiles/profile.h"
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
#include "chrome/browser/ui/webui/settings/chromeos/main_page_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/multidevice_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/people_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/personalization_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/plugin_vm_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/printing_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/privacy_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/reset_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_concept.h"
#include "chrome/browser/ui/webui/settings/chromeos/search_strings_provider.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"

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
  per_page_providers_.push_back(
      std::make_unique<MainPageStringsProvider>(profile, /*delegate=*/this));
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
    content::WebUIDataSource* html_source) {
  for (const auto& per_page_provider : per_page_providers_)
    per_page_provider->AddUiStrings(html_source);
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

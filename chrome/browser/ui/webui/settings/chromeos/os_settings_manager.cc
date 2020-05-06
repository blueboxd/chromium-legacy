// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_manager.h"

#include "base/feature_list.h"
#include "chrome/browser/chromeos/local_search_service/local_search_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/chromeos/about_section.h"
#include "chrome/browser/ui/webui/settings/chromeos/accessibility_section.h"
#include "chrome/browser/ui/webui/settings/chromeos/apps_section.h"
#include "chrome/browser/ui/webui/settings/chromeos/bluetooth_section.h"
#include "chrome/browser/ui/webui/settings/chromeos/crostini_section.h"
#include "chrome/browser/ui/webui/settings/chromeos/date_time_section.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_section.h"
#include "chrome/browser/ui/webui/settings/chromeos/files_section.h"
#include "chrome/browser/ui/webui/settings/chromeos/internet_section.h"
#include "chrome/browser/ui/webui/settings/chromeos/languages_section.h"
#include "chrome/browser/ui/webui/settings/chromeos/main_section.h"
#include "chrome/browser/ui/webui/settings/chromeos/multidevice_section.h"
#include "chrome/browser/ui/webui/settings/chromeos/people_section.h"
#include "chrome/browser/ui/webui/settings/chromeos/personalization_section.h"
#include "chrome/browser/ui/webui/settings/chromeos/plugin_vm_section.h"
#include "chrome/browser/ui/webui/settings/chromeos/printing_section.h"
#include "chrome/browser/ui/webui/settings/chromeos/privacy_section.h"
#include "chrome/browser/ui/webui/settings/chromeos/reset_section.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_concept.h"
#include "chrome/browser/ui/webui/settings/chromeos/search_section.h"
#include "chromeos/constants/chromeos_features.h"
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

OsSettingsManager::OsSettingsManager(
    Profile* profile,
    local_search_service::LocalSearchService* local_search_service,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    syncer::SyncService* sync_service,
    SupervisedUserService* supervised_user_service,
    KerberosCredentialsManager* kerberos_credentials_manager,
    ArcAppListPrefs* arc_app_list_prefs,
    signin::IdentityManager* identity_manager,
    android_sms::AndroidSmsService* android_sms_service,
    CupsPrintersManager* printers_manager)
    : index_(local_search_service->GetIndex(
          local_search_service::IndexId::kCrosSettings)) {
  // Add per-page string providers.
  sections_.push_back(
      std::make_unique<MainSection>(profile, /*delegate=*/this));
  sections_.push_back(
      std::make_unique<InternetSection>(profile, /*delegate=*/this));
  sections_.push_back(
      std::make_unique<BluetoothSection>(profile, /*delegate=*/this));
  sections_.push_back(std::make_unique<MultiDeviceSection>(
      profile, /*delegate=*/this, multidevice_setup_client, android_sms_service,
      profile->GetPrefs()));
  sections_.push_back(std::make_unique<PeopleSection>(
      profile, /*delegate=*/this, sync_service, supervised_user_service,
      kerberos_credentials_manager, identity_manager, profile->GetPrefs()));
  sections_.push_back(std::make_unique<DeviceSection>(
      profile, /*delegate=*/this, profile->GetPrefs()));
  sections_.push_back(std::make_unique<PersonalizationSection>(
      profile, /*delegate=*/this, profile->GetPrefs()));
  sections_.push_back(
      std::make_unique<SearchSection>(profile, /*delegate=*/this));
  sections_.push_back(std::make_unique<AppsSection>(
      profile, /*delegate=*/this, profile->GetPrefs(), arc_app_list_prefs));
  sections_.push_back(std::make_unique<CrostiniSection>(
      profile, /*delegate=*/this, profile->GetPrefs()));
  sections_.push_back(std::make_unique<PluginVmSection>(
      profile, /*delegate=*/this, profile->GetPrefs()));
  sections_.push_back(
      std::make_unique<DateTimeSection>(profile, /*delegate=*/this));
  sections_.push_back(
      std::make_unique<PrivacySection>(profile, /*delegate=*/this));
  sections_.push_back(
      std::make_unique<LanguagesSection>(profile, /*delegate=*/this));
  sections_.push_back(
      std::make_unique<FilesSection>(profile, /*delegate=*/this));
  sections_.push_back(std::make_unique<PrintingSection>(
      profile, /*delegate=*/this, printers_manager));
  sections_.push_back(std::make_unique<AccessibilitySection>(
      profile, /*delegate=*/this, profile->GetPrefs()));
  sections_.push_back(
      std::make_unique<ResetSection>(profile, /*delegate=*/this));
  sections_.push_back(
      std::make_unique<AboutSection>(profile, /*delegate=*/this));
}

OsSettingsManager::~OsSettingsManager() = default;

void OsSettingsManager::AddLoadTimeData(content::WebUIDataSource* html_source) {
  for (const auto& section : sections_)
    section->AddLoadTimeData(html_source);
  html_source->UseStringsJs();
}

void OsSettingsManager::AddHandlers(content::WebUI* web_ui) {
  for (const auto& section : sections_)
    section->AddHandlers(web_ui);
}

const SearchConcept* OsSettingsManager::GetCanonicalTagMetadata(
    int canonical_message_id) const {
  const auto it = canonical_id_to_metadata_map_.find(canonical_message_id);
  if (it == canonical_id_to_metadata_map_.end())
    return nullptr;
  return it->second;
}

void OsSettingsManager::Shutdown() {
  index_ = nullptr;

  // Delete all per-page providers, since some of them depend on KeyedServices.
  sections_.clear();
}

void OsSettingsManager::AddSearchTags(
    const std::vector<SearchConcept>& tags_group) {
  // Note: |index_| is null after Shutdown().
  if (!index_ || !base::FeatureList::IsEnabled(features::kNewOsSettingsSearch))
    return;

  index_->AddOrUpdate(ConceptVectorToDataVector(tags_group));

  // Add each concept to the map. Note that it is safe to take the address of
  // each concept because all concepts are allocated via static
  // base::NoDestructor objects in the Get*SearchConcepts() helper functions.
  for (const auto& concept : tags_group)
    canonical_id_to_metadata_map_[concept.canonical_message_id] = &concept;
}

void OsSettingsManager::RemoveSearchTags(
    const std::vector<SearchConcept>& tags_group) {
  // Note: |index_| is null after Shutdown().
  if (!index_ || !base::FeatureList::IsEnabled(features::kNewOsSettingsSearch))
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

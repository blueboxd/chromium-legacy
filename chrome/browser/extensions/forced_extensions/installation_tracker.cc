// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/forced_extensions/installation_tracker.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/forced_extensions/installation_reporter.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension_urls.h"

namespace extensions {

InstallationTracker::InstallationTracker(ExtensionRegistry* registry,
                                         Profile* profile)
    : registry_(registry),
      profile_(profile),
      pref_service_(profile->GetPrefs()) {
  registry_observer_.Add(registry);
  reporter_observer_.Add(InstallationReporter::Get(profile));
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      pref_names::kInstallForceList,
      base::BindRepeating(&InstallationTracker::OnForcedExtensionsPrefChanged,
                          base::Unretained(this)));

  // Try to load list now.
  OnForcedExtensionsPrefChanged();
}

InstallationTracker::~InstallationTracker() = default;

void InstallationTracker::AddExtensionInfo(const ExtensionId& extension_id,
                                           ExtensionStatus status,
                                           bool is_from_store) {
  auto result =
      extensions_.emplace(extension_id, ExtensionInfo{status, is_from_store});
  DCHECK(result.second);
  if (result.first->second.status == ExtensionStatus::PENDING)
    pending_extensions_counter_++;
}

void InstallationTracker::ChangeExtensionStatus(const ExtensionId& extension_id,
                                                ExtensionStatus status) {
  auto item = extensions_.find(extension_id);
  if (item == extensions_.end())
    return;
  if (item->second.status == ExtensionStatus::PENDING)
    pending_extensions_counter_--;
  item->second.status = status;
  if (item->second.status == ExtensionStatus::PENDING)
    pending_extensions_counter_++;
}

void InstallationTracker::RemoveExtensionInfo(const ExtensionId& extension_id) {
  auto item = extensions_.find(extension_id);
  DCHECK(item != extensions_.end());
  if (item->second.status == ExtensionStatus::PENDING)
    pending_extensions_counter_--;
  extensions_.erase(item);
}

void InstallationTracker::OnForcedExtensionsPrefChanged() {
  const base::DictionaryValue* value =
      pref_service_->GetDictionary(pref_names::kInstallForceList);
  if (!value)
    return;

  // Store extensions in a list instead of removing them because we don't want
  // to change a collection while iterating though it.
  std::vector<ExtensionId> extensions_to_remove;
  for (const auto& extension : extensions_) {
    const ExtensionId& extension_id = extension.first;
    if (value->FindKey(extension_id) == nullptr)
      extensions_to_remove.push_back(extension_id);
  }

  for (const auto& extension_id : extensions_to_remove)
    RemoveExtensionInfo(extension_id);

  // Report if all remaining extensions were removed from policy.
  if (loaded_ && pending_extensions_counter_ == 0)
    NotifyInstallationFinished();

  // Load forced extensions list only once.
  if (value->empty() || loaded_) {
    return;
  }

  loaded_ = true;

  for (const auto& entry : *value) {
    const ExtensionId& extension_id = entry.first;
    std::string* update_url = nullptr;
    if (entry.second->is_dict()) {
      update_url =
          entry.second->FindStringKey(ExternalProviderImpl::kExternalUpdateUrl);
    }
    bool is_from_store =
        update_url && *update_url == extension_urls::kChromeWebstoreUpdateURL;

    AddExtensionInfo(extension_id,
                     registry_->enabled_extensions().Contains(extension_id)
                         ? ExtensionStatus::LOADED
                         : ExtensionStatus::PENDING,
                     is_from_store);
  }
  if (pending_extensions_counter_ == 0)
    NotifyInstallationFinished();
}

void InstallationTracker::OnShutdown(ExtensionRegistry*) {
  registry_observer_.RemoveAll();
}

void InstallationTracker::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void InstallationTracker::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

void InstallationTracker::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  ChangeExtensionStatus(extension->id(), ExtensionStatus::LOADED);
  if (loaded_ && pending_extensions_counter_ == 0)
    NotifyInstallationFinished();
}

void InstallationTracker::OnExtensionInstallationFailed(
    const ExtensionId& extension_id,
    InstallationReporter::FailureReason reason) {
  ChangeExtensionStatus(extension_id, ExtensionStatus::FAILED);
  if (loaded_ && pending_extensions_counter_ == 0)
    NotifyInstallationFinished();
}

bool InstallationTracker::IsComplete() const {
  return complete_;
}

void InstallationTracker::NotifyInstallationFinished() {
  complete_ = true;
  registry_observer_.RemoveAll();
  reporter_observer_.RemoveAll();
  pref_change_registrar_.RemoveAll();
  for (auto& obs : observers_)
    obs.OnForceInstallationFinished();
  InstallationReporter::Get(profile_)->Clear();
}

}  //  namespace extensions

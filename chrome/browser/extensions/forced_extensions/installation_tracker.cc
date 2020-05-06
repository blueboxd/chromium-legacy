// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/forced_extensions/installation_tracker.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/forced_extensions/installation_reporter.h"
#include "chrome/browser/policy/profile_policy_connector.h"
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
  // Load immediately if PolicyService is ready, or wait for it to finish
  // initializing first.
  if (policy_service()->IsInitializationComplete(policy::POLICY_DOMAIN_CHROME))
    OnForcedExtensionsPrefReady();
  else
    policy_service()->AddObserver(policy::POLICY_DOMAIN_CHROME, this);
}

InstallationTracker::~InstallationTracker() {
  policy_service()->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
}

void InstallationTracker::UpdateCounters(ExtensionStatus status, int delta) {
  switch (status) {
    case ExtensionStatus::PENDING:
      load_pending_count_ += delta;
      FALLTHROUGH;
    case ExtensionStatus::LOADED:
      ready_pending_count_ += delta;
      break;
    case ExtensionStatus::READY:
    case ExtensionStatus::FAILED:
      break;
  }
}

void InstallationTracker::AddExtensionInfo(const ExtensionId& extension_id,
                                           ExtensionStatus status,
                                           bool is_from_store) {
  auto result =
      extensions_.emplace(extension_id, ExtensionInfo{status, is_from_store});
  DCHECK(result.second);
  UpdateCounters(result.first->second.status, +1);
}

void InstallationTracker::ChangeExtensionStatus(const ExtensionId& extension_id,
                                                ExtensionStatus status) {
  DCHECK_GE(status_, kWaitingForExtensionLoads);
  auto item = extensions_.find(extension_id);
  if (item == extensions_.end())
    return;
  UpdateCounters(item->second.status, -1);
  item->second.status = status;
  UpdateCounters(item->second.status, +1);
}

void InstallationTracker::OnPolicyUpdated(const policy::PolicyNamespace& ns,
                                          const policy::PolicyMap& previous,
                                          const policy::PolicyMap& current) {}

void InstallationTracker::OnPolicyServiceInitialized(
    policy::PolicyDomain domain) {
  DCHECK_EQ(domain, policy::POLICY_DOMAIN_CHROME);
  DCHECK_EQ(status_, kWaitingForPolicyService);
  policy_service()->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
  OnForcedExtensionsPrefReady();
}

void InstallationTracker::OnForcedExtensionsPrefReady() {
  DCHECK(
      policy_service()->IsInitializationComplete(policy::POLICY_DOMAIN_CHROME));
  DCHECK_EQ(status_, kWaitingForPolicyService);

  // Listen for extension loads and install failures.
  status_ = kWaitingForExtensionLoads;
  registry_observer_.Add(registry_);
  reporter_observer_.Add(InstallationReporter::Get(profile_));

  const base::DictionaryValue* value =
      pref_service_->GetDictionary(pref_names::kInstallForceList);
  if (value) {
    // Add each extension to |extensions_|.
    for (const auto& entry : *value) {
      const ExtensionId& extension_id = entry.first;
      std::string* update_url = nullptr;
      if (entry.second->is_dict()) {
        update_url = entry.second->FindStringKey(
            ExternalProviderImpl::kExternalUpdateUrl);
      }
      bool is_from_store =
          update_url && *update_url == extension_urls::kChromeWebstoreUpdateURL;

      ExtensionStatus status = ExtensionStatus::PENDING;
      if (registry_->enabled_extensions().Contains(extension_id)) {
        status = registry_->ready_extensions().Contains(extension_id)
                     ? ExtensionStatus::READY
                     : ExtensionStatus::LOADED;
      }
      AddExtensionInfo(extension_id, status, is_from_store);
    }
  }

  // Run observers if there are no pending installs.
  MaybeNotifyObservers();
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
  MaybeNotifyObservers();
}

void InstallationTracker::OnExtensionReady(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  ChangeExtensionStatus(extension->id(), ExtensionStatus::READY);
  MaybeNotifyObservers();
}

void InstallationTracker::OnExtensionInstallationFailed(
    const ExtensionId& extension_id,
    InstallationReporter::FailureReason reason) {
  auto item = extensions_.find(extension_id);
  // If the extension is loaded, ignore the failure.
  if (item == extensions_.end() ||
      item->second.status == ExtensionStatus::LOADED ||
      item->second.status == ExtensionStatus::READY)
    return;
  ChangeExtensionStatus(extension_id, ExtensionStatus::FAILED);
  MaybeNotifyObservers();
}

bool InstallationTracker::IsDoneLoading() const {
  return status_ == kWaitingForExtensionReady || status_ == kComplete;
}

bool InstallationTracker::IsReady() const {
  return status_ == kComplete;
}

policy::PolicyService* InstallationTracker::policy_service() {
  return profile_->GetProfilePolicyConnector()->policy_service();
}

void InstallationTracker::MaybeNotifyObservers() {
  DCHECK_GE(status_, kWaitingForExtensionLoads);
  if (status_ == kWaitingForExtensionLoads && load_pending_count_ == 0) {
    for (auto& obs : observers_)
      obs.OnForceInstalledExtensionsLoaded();
    status_ = kWaitingForExtensionReady;
  }
  if (status_ == kWaitingForExtensionReady && ready_pending_count_ == 0) {
    for (auto& obs : observers_)
      obs.OnForceInstalledExtensionsReady();
    status_ = kComplete;
    registry_observer_.RemoveAll();
    reporter_observer_.RemoveAll();
    InstallationReporter::Get(profile_)->Clear();
  }
}

}  //  namespace extensions

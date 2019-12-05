// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/forced_extensions/installation_tracker.h"

#include <set>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/forced_extensions/installation_reporter.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"
#include "extensions/common/extension_urls.h"

namespace {
// Timeout to report UMA if not all force-installed extension were loaded.
constexpr base::TimeDelta kInstallationTimeout =
    base::TimeDelta::FromMinutes(5);
}  // namespace

namespace extensions {

InstallationTracker::InstallationTracker(
    ExtensionRegistry* registry,
    Profile* profile,
    std::unique_ptr<base::OneShotTimer> timer)
    : registry_(registry),
      profile_(profile),
      pref_service_(profile->GetPrefs()),
      start_time_(base::Time::Now()),
      timer_(std::move(timer)) {
  observer_.Add(registry_);
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      pref_names::kInstallForceList,
      base::BindRepeating(&InstallationTracker::OnForcedExtensionsPrefChanged,
                          base::Unretained(this)));

  timer_->Start(FROM_HERE, kInstallationTimeout,
                base::BindRepeating(&InstallationTracker::ReportResults,
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
    ReportResults();

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
    ReportResults();
}

void InstallationTracker::OnShutdown(ExtensionRegistry*) {
  observer_.RemoveAll();
  pref_change_registrar_.RemoveAll();
  timer_->Stop();
}

void InstallationTracker::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  ChangeExtensionStatus(extension->id(), ExtensionStatus::LOADED);
  if (pending_extensions_counter_ == 0)
    ReportResults();
}

void InstallationTracker::OnExtensionInstallationFailed(
    const ExtensionId& extension_id,
    InstallationReporter::FailureReason reason) {
  ChangeExtensionStatus(extension_id, ExtensionStatus::FAILED);
  if (pending_extensions_counter_ == 0)
    ReportResults();
}

void InstallationTracker::ReportResults() {
  DCHECK(!reported_);
  // Report only if there was non-empty list of force-installed extensions.
  if (!extensions_.empty()) {
    UMA_HISTOGRAM_COUNTS_100("Extensions.ForceInstalledTotalCandidateCount",
                             extensions_.size());
    std::set<ExtensionId> missing_forced_extensions;
    for (const auto& extension : extensions_) {
      if (extension.second.status != ExtensionStatus::LOADED)
        missing_forced_extensions.insert(extension.first);
    }
    if (missing_forced_extensions.empty()) {
      UMA_HISTOGRAM_LONG_TIMES("Extensions.ForceInstalledLoadTime",
                               base::Time::Now() - start_time_);
      // TODO(burunduk): Remove VLOGs after resolving crbug/917700 and
      // crbug/904600.
      VLOG(2) << "All forced extensions seems to be installed";
    } else {
      InstallationReporter* installation_reporter =
          InstallationReporter::Get(profile_);
      size_t enabled_missing_count = missing_forced_extensions.size();
      auto installed_extensions = registry_->GenerateInstalledExtensionsSet();
      for (const auto& entry : *installed_extensions)
        missing_forced_extensions.erase(entry->id());
      size_t installed_missing_count = missing_forced_extensions.size();

      UMA_HISTOGRAM_COUNTS_100("Extensions.ForceInstalledTimedOutCount",
                               enabled_missing_count);
      UMA_HISTOGRAM_COUNTS_100(
          "Extensions.ForceInstalledTimedOutAndNotInstalledCount",
          installed_missing_count);
      VLOG(2) << "Failed to install " << installed_missing_count
              << " forced extensions.";
      for (const auto& extension_id : missing_forced_extensions) {
        InstallationReporter::InstallationData installation =
            installation_reporter->Get(extension_id);
        UMA_HISTOGRAM_ENUMERATION(
            "Extensions.ForceInstalledFailureCacheStatus",
            installation.downloading_cache_status.value_or(
                ExtensionDownloaderDelegate::CacheStatus::CACHE_UNKNOWN));
        if (!installation.failure_reason && installation.install_stage) {
          installation.failure_reason =
              InstallationReporter::FailureReason::IN_PROGRESS;
          InstallationReporter::Stage install_stage =
              installation.install_stage.value();
          UMA_HISTOGRAM_ENUMERATION("Extensions.ForceInstalledStage",
                                    install_stage);
          if (install_stage == InstallationReporter::Stage::DOWNLOADING) {
            DCHECK(installation.downloading_stage);
            ExtensionDownloaderDelegate::Stage downloading_stage =
                installation.downloading_stage.value();
            UMA_HISTOGRAM_ENUMERATION(
                "Extensions.ForceInstalledDownloadingStage", downloading_stage);
          }
        }
        InstallationReporter::FailureReason failure_reason =
            installation.failure_reason.value_or(
                InstallationReporter::FailureReason::UNKNOWN);
        UMA_HISTOGRAM_ENUMERATION("Extensions.ForceInstalledFailureReason",
                                  failure_reason);
        VLOG(2) << "Forced extension " << extension_id
                << " failed to install with data="
                << InstallationReporter::GetFormattedInstallationData(
                       installation);
        if (installation.install_error_detail) {
          CrxInstallErrorDetail detail =
              installation.install_error_detail.value();
          UMA_HISTOGRAM_ENUMERATION(
              "Extensions.ForceInstalledFailureCrxInstallError", detail);
        }
      }
    }
  }
  reported_ = true;
  InstallationReporter::Get(profile_)->Clear();
  observer_.RemoveAll();
  pref_change_registrar_.RemoveAll();
  timer_->Stop();
}

}  //  namespace extensions

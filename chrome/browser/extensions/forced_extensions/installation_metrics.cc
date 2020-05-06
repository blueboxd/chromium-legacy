// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/forced_extensions/installation_metrics.h"

#include <set>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_management_constants.h"
#include "chrome/browser/extensions/forced_extensions/installation_reporter.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/updater/extension_downloader.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"

#if defined(OS_CHROMEOS)
#include "components/arc/arc_prefs.h"
#include "components/user_manager/user_manager.h"
#endif  // defined(OS_CHROMEOS)

namespace extensions {

using ExtensionStatus = InstallationTracker::ExtensionStatus;

namespace {
// Timeout to report UMA if not all force-installed extension were loaded.
constexpr base::TimeDelta kInstallationTimeout =
    base::TimeDelta::FromMinutes(5);

#if defined(OS_CHROMEOS)

InstallationMetrics::SessionType GetSessionFromUserType(
    user_manager::UserType user_type) {
  switch (user_type) {
    case user_manager::USER_TYPE_REGULAR:
      return InstallationMetrics::SessionType::SESSION_TYPE_REGULAR_EXISTING;
    case user_manager::USER_TYPE_GUEST:
      return InstallationMetrics::SessionType::SESSION_TYPE_GUEST;
    case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
      return InstallationMetrics::SessionType::SESSION_TYPE_PUBLIC_ACCOUNT;
    case user_manager::USER_TYPE_SUPERVISED:
      return InstallationMetrics::SessionType::SESSION_TYPE_SUPERVISED;
    case user_manager::USER_TYPE_KIOSK_APP:
      return InstallationMetrics::SessionType::SESSION_TYPE_KIOSK_APP;
    case user_manager::USER_TYPE_CHILD:
      return InstallationMetrics::SessionType::SESSION_TYPE_CHILD;
    case user_manager::USER_TYPE_ARC_KIOSK_APP:
      return InstallationMetrics::SessionType::SESSION_TYPE_ARC_KIOSK_APP;
    case user_manager::USER_TYPE_ACTIVE_DIRECTORY:
      return InstallationMetrics::SessionType::SESSION_TYPE_ACTIVE_DIRECTORY;
    case user_manager::USER_TYPE_WEB_KIOSK_APP:
      return InstallationMetrics::SessionType::SESSION_TYPE_WEB_KIOSK_APP;
    default:
      NOTREACHED();
  }
  return InstallationMetrics::SessionType::kMaxValue;
}
#endif  // defined(OS_CHROMEOS)
}  // namespace

InstallationMetrics::InstallationMetrics(
    ExtensionRegistry* registry,
    Profile* profile,
    InstallationTracker* tracker,
    std::unique_ptr<base::OneShotTimer> timer)
    : registry_(registry),
      profile_(profile),
      tracker_(tracker),
      start_time_(base::Time::Now()),
      timer_(std::move(timer)) {
  timer_->Start(
      FROM_HERE, kInstallationTimeout,
      base::BindOnce(&InstallationMetrics::OnForceInstalledExtensionsLoaded,
                     base::Unretained(this)));
  if (tracker_->IsDoneLoading())
    OnForceInstalledExtensionsLoaded();
  else
    tracker_observer_.Add(tracker_);
}

InstallationMetrics::~InstallationMetrics() = default;

bool InstallationMetrics::IsMisconfiguration(
    const InstallationReporter::InstallationData& installation_data,
    const ExtensionId& id) {
  if (installation_data.install_error_detail) {
    ExtensionManagement* management =
        ExtensionManagementFactory::GetForBrowserContext(profile_);
    CrxInstallErrorDetail detail =
        installation_data.install_error_detail.value();
    if (detail == CrxInstallErrorDetail::KIOSK_MODE_ONLY)
      return true;

    if (detail == CrxInstallErrorDetail::DISALLOWED_BY_POLICY &&
        !management->IsAllowedManifestType(
            installation_data.extension_type.value(), id)) {
      return true;
    }
  }
#if defined(OS_CHROMEOS)
  // REPLACED_BY_ARC_APP error is a misconfiguration if it ARC++ is enabled for
  // the device.
  if (profile_->GetPrefs()->IsManagedPreference(arc::prefs::kArcEnabled) &&
      profile_->GetPrefs()->GetBoolean(arc::prefs::kArcEnabled) &&
      installation_data.failure_reason ==
          InstallationReporter::FailureReason::REPLACED_BY_ARC_APP)
    return true;
#endif  // defined(OS_CHROMEOS)

  if (installation_data.failure_reason ==
      InstallationReporter::FailureReason::NOT_PERFORMING_NEW_INSTALL)
    return true;

  return false;
}

#if defined(OS_CHROMEOS)
// Returns the type of session in case extension fails to install.
InstallationMetrics::SessionType InstallationMetrics::GetSessionType() {
  SessionType current_session = GetSessionFromUserType(
      user_manager::UserManager::Get()->GetActiveUser()->GetType());
  // Check if it is regular user and if the user is a new one.
  if (current_session == SessionType::SESSION_TYPE_REGULAR_EXISTING &&
      user_manager::UserManager::Get()->IsCurrentUserNew())
    return SessionType::SESSION_TYPE_REGULAR_NEW;

  return current_session;
}
#endif  // defined(OS_CHROMEOS)

void InstallationMetrics::ReportDisableReason(const ExtensionId& extension_id) {
  int disable_reasons =
      ExtensionPrefs::Get(profile_)->GetDisableReasons(extension_id);
  // Choose any disable reason among the disable reasons for this extension.
  disable_reasons = disable_reasons & ~(disable_reasons - 1);
  base::UmaHistogramSparse("Extensions.ForceInstalledNotLoadedDisableReason",
                           disable_reasons);
}

void InstallationMetrics::ReportMetrics() {
  base::UmaHistogramCounts100("Extensions.ForceInstalledTotalCandidateCount",
                              tracker_->extensions().size());
  std::set<ExtensionId> missing_forced_extensions;
  for (const auto& extension : tracker_->extensions()) {
    if (extension.second.status != ExtensionStatus::LOADED)
      missing_forced_extensions.insert(extension.first);
  }
  if (missing_forced_extensions.empty()) {
    base::UmaHistogramLongTimes("Extensions.ForceInstalledLoadTime",
                                base::Time::Now() - start_time_);
    // TODO(burunduk): Remove VLOGs after resolving crbug/917700 and
    // crbug/904600.
    VLOG(2) << "All forced extensions seems to be installed";
    return;
  }
  InstallationReporter* installation_reporter =
      InstallationReporter::Get(profile_);
  size_t enabled_missing_count = missing_forced_extensions.size();
  size_t blacklisted_count = 0;
  auto installed_extensions = registry_->GenerateInstalledExtensionsSet();
  auto blacklisted_extensions = registry_->GenerateInstalledExtensionsSet(
      ExtensionRegistry::IncludeFlag::BLACKLISTED);
  for (const auto& entry : *installed_extensions) {
    if (missing_forced_extensions.count(entry->id())) {
      missing_forced_extensions.erase(entry->id());
      ReportDisableReason(entry->id());
      if (blacklisted_extensions->Contains(entry->id()))
        blacklisted_count++;
    }
  }
  size_t misconfigured_extensions = 0;
  size_t installed_missing_count = missing_forced_extensions.size();

  base::UmaHistogramCounts100("Extensions.ForceInstalledTimedOutCount",
                              enabled_missing_count);
  base::UmaHistogramCounts100(
      "Extensions.ForceInstalledTimedOutAndNotInstalledCount",
      installed_missing_count);
  base::UmaHistogramCounts100("Extensions.ForceInstalledAndBlackListed",
                              blacklisted_count);
  VLOG(2) << "Failed to install " << installed_missing_count
          << " forced extensions.";
  for (const auto& extension_id : missing_forced_extensions) {
    InstallationReporter::InstallationData installation =
        installation_reporter->Get(extension_id);
    base::UmaHistogramEnumeration(
        "Extensions.ForceInstalledFailureCacheStatus",
        installation.downloading_cache_status.value_or(
            ExtensionDownloaderDelegate::CacheStatus::CACHE_UNKNOWN));
    if (!installation.failure_reason && installation.install_stage) {
      installation.failure_reason =
          InstallationReporter::FailureReason::IN_PROGRESS;
      InstallationReporter::Stage install_stage =
          installation.install_stage.value();
      base::UmaHistogramEnumeration("Extensions.ForceInstalledStage",
                                    install_stage);
      if (install_stage == InstallationReporter::Stage::DOWNLOADING) {
        DCHECK(installation.downloading_stage);
        ExtensionDownloaderDelegate::Stage downloading_stage =
            installation.downloading_stage.value();
        base::UmaHistogramEnumeration(
            "Extensions.ForceInstalledDownloadingStage", downloading_stage);
      }
    }
    if (IsMisconfiguration(installation, extension_id))
      misconfigured_extensions++;
    InstallationReporter::FailureReason failure_reason =
        installation.failure_reason.value_or(
            InstallationReporter::FailureReason::UNKNOWN);
    base::UmaHistogramEnumeration("Extensions.ForceInstalledFailureReason3",
                                  failure_reason);
    if (tracker_->extensions().at(extension_id).is_from_store) {
      base::UmaHistogramEnumeration(
          "Extensions.WebStore_ForceInstalledFailureReason3", failure_reason);
    } else {
      base::UmaHistogramEnumeration(
          "Extensions.OffStore_ForceInstalledFailureReason3", failure_reason);
    }

    // In case of CRX_FETCH_FAILURE, report the network error code, HTTP
    // error code and number of fetch tries made.
    if (failure_reason ==
        InstallationReporter::FailureReason::CRX_FETCH_FAILED) {
      base::UmaHistogramSparse("Extensions.ForceInstalledNetworkErrorCode",
                               installation.network_error_code.value());

      if (installation.response_code) {
        base::UmaHistogramSparse("Extensions.ForceInstalledHttpErrorCode",
                                 installation.response_code.value());
      }
      base::UmaHistogramExactLinear("Extensions.ForceInstalledFetchTries",
                                    installation.fetch_tries.value(),
                                    ExtensionDownloader::kMaxRetries);
    }

    // In case of MANIFEST_FETCH_FAILURE, report the network error code,
    // HTTP error code and number of fetch tries made.
    if (failure_reason ==
        InstallationReporter::FailureReason::MANIFEST_FETCH_FAILED) {
      base::UmaHistogramSparse(
          "Extensions.ForceInstalledManifestFetchFailedNetworkErrorCode",
          installation.network_error_code.value());

      if (installation.response_code) {
        base::UmaHistogramSparse(
            "Extensions.ForceInstalledManifestFetchFailedHttpErrorCode",
            installation.response_code.value());
      }
      base::UmaHistogramExactLinear(
          "Extensions.ForceInstalledManifestFetchFailedFetchTries",
          installation.fetch_tries.value(), ExtensionDownloader::kMaxRetries);
    }
#if defined(OS_CHROMEOS)
    // Report type of session in case Force Installed Extensions fail to
    // install only if there is an active user. There can be extensions on
    // the login screen. There is no active user on the login screen and
    // thus we would not report in that case.
    if (user_manager::UserManager::Get()->GetActiveUser()) {
      base::UmaHistogramEnumeration(
          "Extensions.ForceInstalledFailureSessionType", GetSessionType());
    }
#endif  // defined(OS_CHROMEOS)
    VLOG(2) << "Forced extension " << extension_id
            << " failed to install with data="
            << InstallationReporter::GetFormattedInstallationData(installation);
    if (installation.install_error_detail) {
      CrxInstallErrorDetail detail = installation.install_error_detail.value();
      base::UmaHistogramEnumeration(
          "Extensions.ForceInstalledFailureCrxInstallError", detail);
    }
    if (installation.unpacker_failure_reason) {
      base::UmaHistogramEnumeration(
          "Extensions.ForceInstalledFailureSandboxUnpackFailureReason",
          installation.unpacker_failure_reason.value(),
          SandboxedUnpackerFailureReason::NUM_FAILURE_REASONS);
    }
    if (failure_reason ==
            InstallationReporter::FailureReason::CRX_FETCH_URL_EMPTY &&
        installation.update_check_status) {
      base::UmaHistogramEnumeration(
          "Extensions.ForceInstalledFailureUpdateCheckStatus",
          installation.update_check_status.value());
    }
  }
  bool non_misconfigured_failure_occurred =
      misconfigured_extensions != missing_forced_extensions.size();
  base::UmaHistogramBoolean(
      "Extensions."
      "ForceInstalledSessionsWithNonMisconfigurationFailureOccured",
      non_misconfigured_failure_occurred);
}

void InstallationMetrics::OnForceInstalledExtensionsLoaded() {
  if (reported_)
    return;
  // Report only if there was non-empty list of force-installed extensions.
  if (!tracker_->extensions().empty())
    ReportMetrics();
  reported_ = true;
  timer_->Stop();
}

}  //  namespace extensions

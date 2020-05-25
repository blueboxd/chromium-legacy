// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/forced_extensions/install_stage_tracker.h"

#include "base/check_op.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker_factory.h"
#include "net/base/net_errors.h"

namespace extensions {

// InstallStageTracker::InstallationData implementation.

InstallStageTracker::InstallationData::InstallationData() = default;

InstallStageTracker::InstallationData::InstallationData(
    const InstallationData&) = default;

std::string InstallStageTracker::GetFormattedInstallationData(
    const InstallationData& data) {
  std::ostringstream str;
  str << "failure_reason: "
      << static_cast<int>(data.failure_reason.value_or(FailureReason::UNKNOWN));
  if (data.install_error_detail) {
    str << "; install_error_detail: "
        << static_cast<int>(data.install_error_detail.value());
    if (data.install_error_detail.value() ==
        CrxInstallErrorDetail::DISALLOWED_BY_POLICY) {
      str << "; extension_type: "
          << static_cast<int>(data.extension_type.value());
    }
  }
  if (data.install_stage) {
    str << "; install_stage: " << static_cast<int>(data.install_stage.value());
  }
  if (data.install_stage && data.install_stage.value() == Stage::DOWNLOADING &&
      data.downloading_stage) {
    str << "; downloading_stage: "
        << static_cast<int>(data.downloading_stage.value());
  }
  // No extra check for stage: we may be interested in cache status even in case
  // of successfull extension install.
  if (data.downloading_cache_status) {
    str << "; downloading_cache_status: "
        << static_cast<int>(data.downloading_cache_status.value());
  }
  if (data.failure_reason == FailureReason::MANIFEST_FETCH_FAILED ||
      data.failure_reason == FailureReason::CRX_FETCH_FAILED) {
    str << "; network_error_code: "
        << static_cast<int>(data.network_error_code.value());
    if (data.network_error_code == net::Error::ERR_HTTP_RESPONSE_CODE_FAILURE) {
      str << "; response_code: "
          << static_cast<int>(data.response_code.value());
    }
    str << "; fetch_tries: " << static_cast<int>(data.fetch_tries.value());
  }
  if (data.failure_reason ==
      FailureReason::CRX_INSTALL_ERROR_SANDBOXED_UNPACKER_FAILURE) {
    str << "; unpacker_failure_reason: "
        << static_cast<int>(data.unpacker_failure_reason.value());
  }
  if (data.update_check_status) {
    str << "; update_check_status: "
        << static_cast<int>(data.update_check_status.value());
  }
  if (data.manifest_invalid_error) {
    str << "; manifest_invalid_error: "
        << static_cast<int>(data.manifest_invalid_error.value());
  }

  return str.str();
}

// InstallStageTracker::Observer implementation.

InstallStageTracker::Observer::~Observer() = default;

// InstallStageTracker implementation.

InstallStageTracker::InstallStageTracker(const content::BrowserContext* context)
    : browser_context_(context) {}

InstallStageTracker::~InstallStageTracker() = default;

// static
InstallStageTracker* InstallStageTracker::Get(
    content::BrowserContext* context) {
  return InstallStageTrackerFactory::GetForBrowserContext(context);
}

void InstallStageTracker::ReportManifestInvalidFailure(
    const ExtensionId& id,
    ManifestInvalidError error) {
  InstallationData& data = installation_data_map_[id];
  data.failure_reason = FailureReason::MANIFEST_INVALID;
  data.manifest_invalid_error = error;
  NotifyObserversOfFailure(id, data.failure_reason.value(), data);
}

void InstallStageTracker::ReportInstallationStage(const ExtensionId& id,
                                                  Stage stage) {
  InstallationData& data = installation_data_map_[id];
  data.install_stage = stage;
  for (auto& observer : observers_) {
    observer.OnExtensionDataChangedForTesting(id, browser_context_, data);
  }
}

void InstallStageTracker::ReportDownloadingStage(
    const ExtensionId& id,
    ExtensionDownloaderDelegate::Stage stage) {
  InstallationData& data = installation_data_map_[id];
  data.downloading_stage = stage;
  for (auto& observer : observers_) {
    observer.OnExtensionDataChangedForTesting(id, browser_context_, data);
  }
}

void InstallStageTracker::ReportDownloadingCacheStatus(
    const ExtensionId& id,
    ExtensionDownloaderDelegate::CacheStatus cache_status) {
  DCHECK_NE(cache_status,
            ExtensionDownloaderDelegate::CacheStatus::CACHE_UNKNOWN);
  InstallationData& data = installation_data_map_[id];
  data.downloading_cache_status = cache_status;
  for (auto& observer : observers_) {
    observer.OnExtensionDataChangedForTesting(id, browser_context_, data);
  }
}

void InstallStageTracker::ReportManifestUpdateCheckStatus(
    const ExtensionId& id,
    const std::string& status) {
  InstallationData& data = installation_data_map_[id];
  // Map the current status to UpdateCheckStatus enum.
  if (status == "ok")
    data.update_check_status = UpdateCheckStatus::kOk;
  else if (status == "noupdate")
    data.update_check_status = UpdateCheckStatus::kNoUpdate;
  else if (status == "error-internal")
    data.update_check_status = UpdateCheckStatus::kErrorInternal;
  else if (status == "error-hash")
    data.update_check_status = UpdateCheckStatus::kErrorHash;
  else if (status == "error-osnotsupported")
    data.update_check_status = UpdateCheckStatus::kErrorOsNotSupported;
  else if (status == "error-hwnotsupported")
    data.update_check_status = UpdateCheckStatus::kErrorHardwareNotSupported;
  else if (status == "error-unsupportedprotocol")
    data.update_check_status = UpdateCheckStatus::kErrorUnsupportedProtocol;
  else
    data.update_check_status = UpdateCheckStatus::kUnknown;

  for (auto& observer : observers_) {
    observer.OnExtensionDataChangedForTesting(id, browser_context_, data);
  }
}

void InstallStageTracker::ReportFetchError(
    const ExtensionId& id,
    FailureReason reason,
    const ExtensionDownloaderDelegate::FailureData& failure_data) {
  DCHECK(reason == FailureReason::MANIFEST_FETCH_FAILED ||
         reason == FailureReason::CRX_FETCH_FAILED);
  InstallationData& data = installation_data_map_[id];
  data.failure_reason = reason;
  data.network_error_code = failure_data.network_error_code;
  data.response_code = failure_data.response_code;
  data.fetch_tries = failure_data.fetch_tries;
  NotifyObserversOfFailure(id, reason, data);
}

void InstallStageTracker::ReportFailure(const ExtensionId& id,
                                        FailureReason reason) {
  DCHECK_NE(reason, FailureReason::UNKNOWN);
  InstallationData& data = installation_data_map_[id];
  data.failure_reason = reason;
  NotifyObserversOfFailure(id, reason, data);
}

void InstallStageTracker::ReportExtensionTypeForPolicyDisallowedExtension(
    const ExtensionId& id,
    Manifest::Type extension_type) {
  InstallationData& data = installation_data_map_[id];
  data.extension_type = extension_type;
}

void InstallStageTracker::ReportCrxInstallError(
    const ExtensionId& id,
    FailureReason reason,
    CrxInstallErrorDetail crx_install_error) {
  DCHECK(reason == FailureReason::CRX_INSTALL_ERROR_DECLINED ||
         reason == FailureReason::CRX_INSTALL_ERROR_OTHER);
  InstallationData& data = installation_data_map_[id];
  data.failure_reason = reason;
  data.install_error_detail = crx_install_error;
  NotifyObserversOfFailure(id, reason, data);
}

void InstallStageTracker::ReportSandboxedUnpackerFailureReason(
    const ExtensionId& id,
    SandboxedUnpackerFailureReason unpacker_failure_reason) {
  InstallationData& data = installation_data_map_[id];
  data.failure_reason =
      FailureReason::CRX_INSTALL_ERROR_SANDBOXED_UNPACKER_FAILURE;
  data.unpacker_failure_reason = unpacker_failure_reason;
  NotifyObserversOfFailure(
      id, FailureReason::CRX_INSTALL_ERROR_SANDBOXED_UNPACKER_FAILURE, data);
}

InstallStageTracker::InstallationData InstallStageTracker::Get(
    const ExtensionId& id) {
  auto it = installation_data_map_.find(id);
  return it == installation_data_map_.end() ? InstallationData() : it->second;
}

void InstallStageTracker::Clear() {
  installation_data_map_.clear();
}

void InstallStageTracker::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void InstallStageTracker::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void InstallStageTracker::NotifyObserversOfFailure(
    const ExtensionId& id,
    FailureReason reason,
    const InstallationData& data) {
  for (auto& observer : observers_) {
    observer.OnExtensionInstallationFailed(id, reason);
    observer.OnExtensionDataChangedForTesting(id, browser_context_, data);
  }
}

}  //  namespace extensions

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/forced_extensions/installation_reporter.h"

#include <map>

#include "base/check_op.h"
#include "base/no_destructor.h"
#include "chrome/browser/extensions/forced_extensions/installation_reporter_factory.h"

namespace extensions {

InstallationReporter::InstallationData::InstallationData() = default;

InstallationReporter::InstallationData::InstallationData(
    const InstallationData&) = default;

std::string InstallationReporter::GetFormattedInstallationData(
    const InstallationData& data) {
  std::ostringstream str;
  str << "failure_reason: "
      << static_cast<int>(data.failure_reason.value_or(FailureReason::UNKNOWN));
  if (data.install_error_detail) {
    str << "; install_error_detail: "
        << static_cast<int>(data.install_error_detail.value());
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
  return str.str();
}

InstallationReporter::Observer::~Observer() = default;

InstallationReporter::InstallationReporter(
    const content::BrowserContext* context)
    : browser_context_(context) {}

InstallationReporter::~InstallationReporter() = default;

// static
InstallationReporter* InstallationReporter::Get(
    content::BrowserContext* context) {
  return InstallationReporterFactory::GetForBrowserContext(context);
}

void InstallationReporter::ReportInstallationStage(const ExtensionId& id,
                                                   Stage stage) {
  InstallationData& data = installation_data_map_[id];
  data.install_stage = stage;
  for (auto& observer : observers_) {
    observer.OnExtensionDataChangedForTesting(id, browser_context_, data);
  }
}

void InstallationReporter::ReportDownloadingStage(
    const ExtensionId& id,
    ExtensionDownloaderDelegate::Stage stage) {
  InstallationData& data = installation_data_map_[id];
  data.downloading_stage = stage;
  for (auto& observer : observers_) {
    observer.OnExtensionDataChangedForTesting(id, browser_context_, data);
  }
}

void InstallationReporter::ReportDownloadingCacheStatus(
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

void InstallationReporter::ReportManifestUpdateCheckStatus(
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

void InstallationReporter::ReportFetchError(
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

void InstallationReporter::ReportFailure(const ExtensionId& id,
                                         FailureReason reason) {
  DCHECK_NE(reason, FailureReason::UNKNOWN);
  InstallationData& data = installation_data_map_[id];
  data.failure_reason = reason;
  NotifyObserversOfFailure(id, reason, data);
}

void InstallationReporter::ReportExtensionTypeForPolicyDisallowedExtension(
    const ExtensionId& id,
    Manifest::Type extension_type) {
  InstallationData& data = installation_data_map_[id];
  data.extension_type = extension_type;
}

void InstallationReporter::ReportCrxInstallError(
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

void InstallationReporter::ReportSandboxedUnpackerFailureReason(
    const ExtensionId& id,
    SandboxedUnpackerFailureReason unpacker_failure_reason) {
  InstallationData& data = installation_data_map_[id];
  data.failure_reason =
      FailureReason::CRX_INSTALL_ERROR_SANDBOXED_UNPACKER_FAILURE;
  data.unpacker_failure_reason = unpacker_failure_reason;
  NotifyObserversOfFailure(
      id, FailureReason::CRX_INSTALL_ERROR_SANDBOXED_UNPACKER_FAILURE, data);
}

InstallationReporter::InstallationData InstallationReporter::Get(
    const ExtensionId& id) {
  auto it = installation_data_map_.find(id);
  return it == installation_data_map_.end() ? InstallationData() : it->second;
}

void InstallationReporter::Clear() {
  installation_data_map_.clear();
}

void InstallationReporter::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void InstallationReporter::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void InstallationReporter::NotifyObserversOfFailure(
    const ExtensionId& id,
    FailureReason reason,
    const InstallationData& data) {
  for (auto& observer : observers_) {
    observer.OnExtensionInstallationFailed(id, reason);
    observer.OnExtensionDataChangedForTesting(id, browser_context_, data);
  }
}

}  //  namespace extensions

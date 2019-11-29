// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/forced_extensions/installation_reporter.h"

#include <map>

#include "base/logging.h"
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

void InstallationReporter::ReportFailure(const ExtensionId& id,
                                         FailureReason reason) {
  DCHECK_NE(reason, FailureReason::UNKNOWN);
  InstallationData& data = installation_data_map_[id];
  data.failure_reason = reason;
  for (auto& observer : observers_) {
    observer.OnExtensionInstallationFailed(id, reason);
    observer.OnExtensionDataChangedForTesting(id, browser_context_, data);
  }
}

void InstallationReporter::ReportCrxInstallError(
    const ExtensionId& id,
    FailureReason reason,
    CrxInstallErrorDetail crx_install_error) {
  DCHECK(reason == FailureReason::CRX_INSTALL_ERROR_DECLINED ||
         reason ==
             FailureReason::CRX_INSTALL_ERROR_SANDBOXED_UNPACKER_FAILURE ||
         reason == FailureReason::CRX_INSTALL_ERROR_OTHER);
  InstallationData& data = installation_data_map_[id];
  data.failure_reason = reason;
  data.install_error_detail = crx_install_error;
  for (auto& observer : observers_) {
    observer.OnExtensionInstallationFailed(id, reason);
    observer.OnExtensionDataChangedForTesting(id, browser_context_, data);
  }
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

}  //  namespace extensions

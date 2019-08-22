// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/forced_extensions/installation_reporter.h"

#include <map>

#include "base/logging.h"
#include "base/no_destructor.h"

namespace {

using InstallationDataMap =
    std::map<extensions::ExtensionId,
             extensions::InstallationReporter::InstallationData>;

InstallationDataMap& GetInstallationDataMap(const Profile* profile) {
  static base::NoDestructor<std::map<const Profile*, InstallationDataMap>>
      failure_maps;
  return (*failure_maps)[profile];
}

extensions::InstallationReporter::TestObserver* g_test_observer = nullptr;

}  // namespace

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
  return str.str();
}

InstallationReporter::TestObserver::~TestObserver() = default;

// static
void InstallationReporter::ReportInstallationStage(const Profile* profile,
                                                   const ExtensionId& id,
                                                   Stage stage) {
  InstallationData& data = GetInstallationDataMap(profile)[id];
  data.install_stage = stage;
  if (g_test_observer) {
    g_test_observer->OnExtensionDataChanged(id, profile, data);
  }
}

// static
void InstallationReporter::ReportDownloadingStage(
    const Profile* profile,
    const ExtensionId& id,
    ExtensionDownloaderDelegate::Stage stage) {
  InstallationData& data = GetInstallationDataMap(profile)[id];
  data.downloading_stage = stage;
  if (g_test_observer) {
    g_test_observer->OnExtensionDataChanged(id, profile, data);
  }
}

// static
void InstallationReporter::ReportFailure(const Profile* profile,
                                         const ExtensionId& id,
                                         FailureReason reason) {
  DCHECK_NE(reason, FailureReason::UNKNOWN);
  InstallationData& data = GetInstallationDataMap(profile)[id];
  data.failure_reason = reason;
  if (g_test_observer) {
    g_test_observer->OnExtensionDataChanged(id, profile, data);
  }
}

// static
void InstallationReporter::ReportCrxInstallError(
    const Profile* profile,
    const ExtensionId& id,
    FailureReason reason,
    CrxInstallErrorDetail crx_install_error) {
  DCHECK(reason == FailureReason::CRX_INSTALL_ERROR_DECLINED ||
         reason ==
             FailureReason::CRX_INSTALL_ERROR_SANDBOXED_UNPACKER_FAILURE ||
         reason == FailureReason::CRX_INSTALL_ERROR_OTHER);
  InstallationData& data = GetInstallationDataMap(profile)[id];
  data.failure_reason = reason;
  data.install_error_detail = crx_install_error;
  if (g_test_observer) {
    g_test_observer->OnExtensionDataChanged(id, profile, data);
  }
}

// static
InstallationReporter::InstallationData InstallationReporter::Get(
    const Profile* profile,
    const ExtensionId& id) {
  InstallationDataMap& map = GetInstallationDataMap(profile);
  auto it = map.find(id);
  return it == map.end() ? InstallationData() : it->second;
}

// static
void InstallationReporter::Clear(const Profile* profile) {
  GetInstallationDataMap(profile).clear();
}

// static
void InstallationReporter::SetTestObserver(TestObserver* observer) {
  g_test_observer = observer;
}

}  //  namespace extensions

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/unittest_util.h"

#include "base/version.h"
#include "chrome/updater/tag.h"

namespace updater {

const char kChromeAppId[] = "{8A69D345-D564-463C-AFF1-A69D9E530F96}";

namespace tagging {

std::ostream& operator<<(std::ostream& os, const ErrorCode& error_code) {
  switch (error_code) {
    case ErrorCode::kSuccess:
      return os << "ErrorCode::kSuccess";
    case ErrorCode::kUnrecognizedName:
      return os << "ErrorCode::kUnrecognizedName";
    case ErrorCode::kTagIsInvalid:
      return os << "ErrorCode::kTagIsInvalid";
    case ErrorCode::kAttributeMustHaveValue:
      return os << "ErrorCode::kAttributeMustHaveValue";
    case ErrorCode::kApp_AppIdNotSpecified:
      return os << "ErrorCode::kApp_AppIdNotSpecified";
    case ErrorCode::kApp_ExperimentLabelsCannotBeWhitespace:
      return os << "ErrorCode::kApp_ExperimentLabelsCannotBeWhitespace";
    case ErrorCode::kApp_AppIdIsNotValid:
      return os << "ErrorCode::kApp_AppIdIsNotValid";
    case ErrorCode::kApp_AppNameCannotBeWhitespace:
      return os << "ErrorCode::kApp_AppNameCannotBeWhitespace";
    case ErrorCode::kApp_NeedsAdminValueIsInvalid:
      return os << "ErrorCode::kApp_NeedsAdminValueIsInvalid";
    case ErrorCode::kAppInstallerData_AppIdNotFound:
      return os << "ErrorCode::kAppInstallerData_AppIdNotFound";
    case ErrorCode::kAppInstallerData_InstallerDataCannotBeSpecifiedBeforeAppId:
      return os << "ErrorCode::kAppInstallerData_"
                   "InstallerDataCannotBeSpecifiedBeforeAppId";
    case ErrorCode::kGlobal_BundleNameCannotBeWhitespace:
      return os << "ErrorCode::kGlobal_BundleNameCannotBeWhitespace";
    case ErrorCode::kGlobal_ExperimentLabelsCannotBeWhitespace:
      return os << "ErrorCode::kGlobal_ExperimentLabelsCannotBeWhitespace";
    case ErrorCode::kGlobal_BrowserTypeIsInvalid:
      return os << "ErrorCode::kGlobal_BrowserTypeIsInvalid";
    case ErrorCode::kGlobal_FlightingValueIsNotABoolean:
      return os << "ErrorCode::kGlobal_FlightingValueIsNotABoolean";
    case ErrorCode::kGlobal_UsageStatsValueIsInvalid:
      return os << "ErrorCode::kGlobal_UsageStatsValueIsInvalid";
    default:
      return os << "ErrorCode(" << static_cast<int>(error_code) << ")";
  }
}

std::ostream& operator<<(std::ostream& os,
                         const AppArgs::NeedsAdmin& needs_admin) {
  switch (needs_admin) {
    case AppArgs::NeedsAdmin::kNo:
      return os << "AppArgs::NeedsAdmin::kNo";
    case AppArgs::NeedsAdmin::kYes:
      return os << "AppArgs::NeedsAdmin::kYes";
    case AppArgs::NeedsAdmin::kPrefers:
      return os << "AppArgs::NeedsAdmin::kPrefers";
    default:
      return os << "AppArgs::NeedsAdmin(" << static_cast<int>(needs_admin)
                << ")";
  }
}

std::ostream& operator<<(std::ostream& os,
                         const TagArgs::BrowserType& browser_type) {
  switch (browser_type) {
    case TagArgs::BrowserType::kUnknown:
      return os << "TagArgs::BrowserType::kUnknown";
    case TagArgs::BrowserType::kDefault:
      return os << "TagArgs::BrowserType::kDefault";
    case TagArgs::BrowserType::kInternetExplorer:
      return os << "TagArgs::BrowserType::kInternetExplorer";
    case TagArgs::BrowserType::kFirefox:
      return os << "TagArgs::BrowserType::kFirefox";
    case TagArgs::BrowserType::kChrome:
      return os << "TagArgs::BrowserType::kChrome";
    default:
      return os << "TagArgs::BrowserType(" << static_cast<int>(browser_type)
                << ")";
  }
}

}  // namespace tagging

bool operator==(const UpdateService::UpdateState& lhs,
                const UpdateService::UpdateState& rhs) {
  const bool versions_equal =
      (lhs.next_version.IsValid() && rhs.next_version.IsValid() &&
       lhs.next_version == rhs.next_version) ||
      (!lhs.next_version.IsValid() && !rhs.next_version.IsValid());
  return versions_equal && lhs.app_id == rhs.app_id && lhs.state == rhs.state &&
         lhs.downloaded_bytes == rhs.downloaded_bytes &&
         lhs.total_bytes == rhs.total_bytes &&
         lhs.install_progress == rhs.install_progress &&
         lhs.error_category == rhs.error_category &&
         lhs.error_code == rhs.error_code && lhs.extra_code1 == rhs.extra_code1;
}

std::ostream& operator<<(std::ostream& os,
                         const UpdateService::UpdateState& update_state) {
  auto state_formatter = [update_state]() {
    switch (update_state.state) {
      case UpdateService::UpdateState::State::kUnknown:
        return "unknown";
      case UpdateService::UpdateState::State::kNotStarted:
        return "not started";
      case UpdateService::UpdateState::State::kCheckingForUpdates:
        return "checking for updates";
      case UpdateService::UpdateState::State::kUpdateAvailable:
        return "update available";
      case UpdateService::UpdateState::State::kDownloading:
        return "downloading";
      case UpdateService::UpdateState::State::kInstalling:
        return "installing";
      case UpdateService::UpdateState::State::kUpdated:
        return "updated";
      case UpdateService::UpdateState::State::kNoUpdate:
        return "no update";
      case UpdateService::UpdateState::State::kUpdateError:
        return "update error";
    }
  };

  auto version_formatter = [update_state]() {
    return update_state.next_version.IsValid()
               ? update_state.next_version.GetString()
               : "";
  };

  auto error_category_formatter = [update_state]() {
    switch (update_state.error_category) {
      case UpdateService::ErrorCategory::kNone:
        return "none";
      case UpdateService::ErrorCategory::kDownload:
        return "download";
      case UpdateService::ErrorCategory::kUnpack:
        return "unpack";
      case UpdateService::ErrorCategory::kInstall:
        return "install";
      case UpdateService::ErrorCategory::kService:
        return "service";
      case UpdateService::ErrorCategory::kUpdateCheck:
        return "update check";
    }
  };

  return os << "UpdateState {app_id: " << update_state.app_id
            << ", state: " << state_formatter()
            << ", next_version: " << version_formatter()
            << ", downloaded_bytes: " << update_state.downloaded_bytes
            << ", total_bytes: " << update_state.total_bytes
            << ", install_progress: " << update_state.install_progress
            << ", error_category: " << error_category_formatter()
            << ", error_code: " << update_state.error_code
            << ", extra_code1: " << update_state.extra_code1 << "}";
}

}  // namespace updater

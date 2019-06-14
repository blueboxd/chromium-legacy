// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise_reporting/report_generator.h"

#include <utility>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/channel_info.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"

#if defined(OS_WIN)
#include "base/win/wmi.h"
#endif

namespace em = enterprise_management;

namespace enterprise_reporting {
namespace {

std::string GetChromePath() {
  base::FilePath path;
  base::PathService::Get(base::DIR_EXE, &path);
  return path.AsUTF8Unsafe();
}

}  // namespace

ReportGenerator::ReportGenerator() = default;
ReportGenerator::~ReportGenerator() = default;

std::vector<std::unique_ptr<em::ChromeDesktopReportRequest>>
ReportGenerator::Generate() {
  CreateBasicRequest();
  requests_.push_back(
      std::make_unique<em::ChromeDesktopReportRequest>(basic_request_));
  return std::move(requests_);
}

void ReportGenerator::CreateBasicRequest() {
  basic_request_.set_computer_name(this->GetMachineName());
  basic_request_.set_os_user_name(GetOSUserName());
  basic_request_.set_serial_number(GetSerialNumber());
  basic_request_.set_allocated_os_report(GetOSReport().release());
  basic_request_.set_allocated_browser_report(GetBrowserReport().release());
  for (auto& profile : GetProfiles()) {
    basic_request_.mutable_browser_report()->add_profile_info_list()->Swap(
        profile.get());
  }
}

std::unique_ptr<em::OSReport> ReportGenerator::GetOSReport() {
  auto report = std::make_unique<em::OSReport>();
  report->set_name(policy::GetOSPlatform());
  report->set_arch(policy::GetOSArchitecture());
  report->set_version(policy::GetOSVersion());
  return report;
}

std::string ReportGenerator::GetMachineName() {
  return policy::GetMachineName();
}

std::string ReportGenerator::GetOSUserName() {
  return policy::GetOSUsername();
}

std::string ReportGenerator::GetSerialNumber() {
#if defined(OS_WIN)
  return base::UTF16ToUTF8(
      base::win::WmiComputerSystemInfo::Get().serial_number());
#else
  return std::string();
#endif
}

std::unique_ptr<em::BrowserReport> ReportGenerator::GetBrowserReport() {
  auto report = std::make_unique<em::BrowserReport>();
  report->set_browser_version(version_info::GetVersionNumber());
  report->set_channel(policy::ConvertToProtoChannel(chrome::GetChannel()));
  report->set_executable_path(GetChromePath());
  return report;
}

std::vector<std::unique_ptr<em::ChromeUserProfileInfo>>
ReportGenerator::GetProfiles() {
  std::vector<std::unique_ptr<em::ChromeUserProfileInfo>> profiles;
  for (auto* entry : g_browser_process->profile_manager()
                         ->GetProfileAttributesStorage()
                         .GetAllProfilesAttributes()) {
    profiles.push_back(std::make_unique<em::ChromeUserProfileInfo>());
    profiles.back()->set_id(entry->GetPath().AsUTF8Unsafe());
    profiles.back()->set_name(base::UTF16ToUTF8(entry->GetName()));
    profiles.back()->set_is_full_report(false);
  }
  return profiles;
}

}  // namespace enterprise_reporting

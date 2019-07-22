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

const size_t kMaximumReportSize =
    5000000;  // The report size limitation is 5mb.

std::string GetChromePath() {
  base::FilePath path;
  base::PathService::Get(base::DIR_EXE, &path);
  return path.AsUTF8Unsafe();
}

}  // namespace

ReportGenerator::ReportGenerator()
    : maximum_report_size_(kMaximumReportSize), weak_ptr_factory_(this) {}

ReportGenerator::~ReportGenerator() = default;

void ReportGenerator::Generate(ReportCallback callback) {
  DCHECK(!callback_);
  callback_ = std::move(callback);
  CreateBasicRequest();

  if (basic_request_size_ > maximum_report_size_) {
    // Basic request is already too large so we can't upload any valid report.
    // Skip all Profiles and response an empty request list.
    GetNextProfileReport(
        basic_request_.browser_report().chrome_user_profile_infos_size());
    return;
  }

  requests_.push_back(
      std::make_unique<em::ChromeDesktopReportRequest>(basic_request_));
  GetNextProfileReport(0);
}

void ReportGenerator::SetMaximumReportSizeForTesting(size_t size) {
  maximum_report_size_ = size;
}

void ReportGenerator::CreateBasicRequest() {
  basic_request_.set_computer_name(this->GetMachineName());
  basic_request_.set_os_user_name(GetOSUserName());
  basic_request_.set_serial_number(GetSerialNumber());
  basic_request_.set_allocated_os_report(GetOSReport().release());
  basic_request_.set_allocated_browser_report(GetBrowserReport().release());
  for (auto& profile : GetProfiles()) {
    basic_request_.mutable_browser_report()
        ->add_chrome_user_profile_infos()
        ->Swap(profile.get());
  }
  basic_request_size_ = basic_request_.ByteSizeLong();
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

void ReportGenerator::GetNextProfileReport(int profile_index) {
  if (profile_index >=
      basic_request_.browser_report().chrome_user_profile_infos_size()) {
    std::move(callback_).Run(std::move(requests_));
    return;
  }

  auto basic_profile_report =
      basic_request_.browser_report().chrome_user_profile_infos(profile_index);
  profile_report_generator_.MaybeGenerate(
      base::FilePath::FromUTF8Unsafe(basic_profile_report.id()),
      basic_profile_report.name(),
      base::BindOnce(&ReportGenerator::OnProfileReportReady,
                     weak_ptr_factory_.GetWeakPtr(), profile_index));
}

void ReportGenerator::OnProfileReportReady(
    int profile_index,
    std::unique_ptr<em::ChromeUserProfileInfo> profile_report) {
  // Move to the next Profile if Profile is not loaded and there is no full
  // report.
  if (!profile_report) {
    GetNextProfileReport(profile_index + 1);
    return;
  }

  size_t profile_report_size = profile_report->ByteSizeLong();
  size_t current_request_size = requests_.back()->ByteSizeLong();

  if (current_request_size + profile_report_size <= maximum_report_size_) {
    // The new full Profile report can be appended into the current request.
    requests_.back()
        ->mutable_browser_report()
        ->mutable_chrome_user_profile_infos(profile_index)
        ->Swap(profile_report.get());
  } else if (basic_request_size_ + profile_report_size <=
             maximum_report_size_) {
    // The new full Profile report is too big to be appended into the current
    // request, move it to the next request if possible.
    requests_.push_back(
        std::make_unique<em::ChromeDesktopReportRequest>(basic_request_));
    requests_.back()
        ->mutable_browser_report()
        ->mutable_chrome_user_profile_infos(profile_index)
        ->Swap(profile_report.get());
  }
  // Else: The new full Profile report is too big to be uploaded, skip this
  // Profile report.
  // TODO(crbug.com/956237): Record this event with UMA metrics.

  GetNextProfileReport(profile_index + 1);
}

}  // namespace enterprise_reporting

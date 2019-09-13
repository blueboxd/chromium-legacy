// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise_reporting/report_generator.h"

#include <utility>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
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
#include "content/public/browser/plugin_service.h"
#include "content/public/common/webplugininfo.h"

#if defined(OS_WIN)
#include "base/win/wmi.h"
#endif

namespace em = enterprise_management;

namespace enterprise_reporting {
namespace {

const size_t kMaximumReportSize =
    5000000;  // The report size limitation is 5mb.

constexpr char kRequestCountMetricsName[] =
    "Enterprise.CloudReportingRequestCount";
constexpr char kRequestSizeMetricsName[] =
    "Enterprise.CloudReportingRequestSize";
constexpr char kBasicRequestSizeMetricsName[] =
    "Enterprise.CloudReportingBasicRequestSize";

// Because server only stores 20 profiles for each report and when report is
// separated into requests, there is at least one profile per request. It means
// server will truncate the report when there are more than 20 requests. Actions
// are needed if there are many reports exceed this limitation.
const int kRequestCountMetricMaxValue = 21;

std::string GetChromePath() {
  base::FilePath path;
  base::PathService::Get(base::DIR_EXE, &path);
  return path.AsUTF8Unsafe();
}

}  // namespace

ReportGenerator::ReportGenerator() : maximum_report_size_(kMaximumReportSize) {}

ReportGenerator::~ReportGenerator() = default;

void ReportGenerator::Generate(ReportCallback callback) {
  DCHECK(!callback_);
  callback_ = std::move(callback);
  CreateBasicRequest();
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
  content::PluginService::GetInstance()->GetPlugins(base::BindOnce(
      &ReportGenerator::OnPluginsReady, weak_ptr_factory_.GetWeakPtr()));
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

void ReportGenerator::GenerateProfileReportWithIndex(int profile_index) {
  DCHECK_LT(profile_index,
            basic_request_.browser_report().chrome_user_profile_infos_size());

  auto basic_profile_report =
      basic_request_.browser_report().chrome_user_profile_infos(profile_index);
  auto profile_report = profile_report_generator_.MaybeGenerate(
      base::FilePath::FromUTF8Unsafe(basic_profile_report.id()),
      basic_profile_report.name());

  // Return if Profile is not loaded and there is no full report.
  if (!profile_report) {
    return;
  }

  // Use size diff to calculate estimated request size after full profile report
  // is added. There are still few bytes difference but close enough.
  size_t profile_report_incremental_size =
      profile_report->ByteSizeLong() -
      basic_request_.browser_report()
          .chrome_user_profile_infos(profile_index)
          .ByteSizeLong();
  size_t current_request_size = requests_.back()->ByteSizeLong();

  if (current_request_size + profile_report_incremental_size <=
      maximum_report_size_) {
    // The new full Profile report can be appended into the current request.
    requests_.back()
        ->mutable_browser_report()
        ->mutable_chrome_user_profile_infos(profile_index)
        ->Swap(profile_report.get());
  } else if (basic_request_size_ + profile_report_incremental_size <=
             maximum_report_size_) {
    // The new full Profile report is too big to be appended into the current
    // request, move it to the next request if possible. Record metrics for the
    // current request's size.
    base::UmaHistogramMemoryKB(kRequestSizeMetricsName,
                               requests_.back()->ByteSizeLong() / 1024);
    requests_.push(
        std::make_unique<em::ChromeDesktopReportRequest>(basic_request_));
    requests_.back()
        ->mutable_browser_report()
        ->mutable_chrome_user_profile_infos(profile_index)
        ->Swap(profile_report.get());
  } else {
    // The new full Profile report is too big to be uploaded, skip this
    // Profile report. But we still add the report size into metrics so
    // that we could understand the situation better.
    base::UmaHistogramMemoryKB(
        kRequestSizeMetricsName,
        (basic_request_size_ + profile_report_incremental_size) / 1024);
  }
}

void ReportGenerator::OnPluginsReady(
    const std::vector<content::WebPluginInfo>& plugins) {
  auto* browser_report = basic_request_.mutable_browser_report();
  for (auto plugin : plugins) {
    auto* plugin_info = browser_report->add_plugins();
    plugin_info->set_name(base::UTF16ToUTF8(plugin.name));
    plugin_info->set_version(base::UTF16ToUTF8(plugin.version));
    plugin_info->set_filename(plugin.path.BaseName().AsUTF8Unsafe());
    plugin_info->set_description(base::UTF16ToUTF8(plugin.desc));
  }

  OnBasicRequestReady();
}

void ReportGenerator::OnBasicRequestReady() {
  basic_request_size_ = basic_request_.ByteSizeLong();
  base::UmaHistogramMemoryKB(kBasicRequestSizeMetricsName,
                             basic_request_size_ / 1024);
  if (basic_request_size_ > maximum_report_size_) {
    // Basic request is already too large so we can't upload any valid report.
    // Skip all Profiles and response an empty request list.
    Response();
    return;
  }

  requests_.push(
      std::make_unique<em::ChromeDesktopReportRequest>(basic_request_));
  for (int index = 0;
       index < basic_request_.browser_report().chrome_user_profile_infos_size();
       index++) {
    GenerateProfileReportWithIndex(index);
  }
  base::UmaHistogramMemoryKB(kRequestSizeMetricsName,
                             requests_.back()->ByteSizeLong() / 1024);
  Response();
}

void ReportGenerator::Response() {
  base::UmaHistogramExactLinear(kRequestCountMetricsName, requests_.size(),
                                kRequestCountMetricMaxValue);
  std::move(callback_).Run(std::move(requests_));
}

}  // namespace enterprise_reporting

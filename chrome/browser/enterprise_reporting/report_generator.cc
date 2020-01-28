// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise_reporting/report_generator.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"

#if defined(OS_WIN)
#include "base/win/wmi.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/enterprise_reporting/android_app_info_generator.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#endif

namespace em = enterprise_management;

namespace enterprise_reporting {

ReportGenerator::ReportGenerator() = default;

ReportGenerator::~ReportGenerator() = default;

void ReportGenerator::Generate(ReportCallback callback) {
  DCHECK(!callback_);
  callback_ = std::move(callback);
  CreateBasicRequest();
}

void ReportGenerator::SetMaximumReportSizeForTesting(size_t size) {
  report_request_queue_generator_.SetMaximumReportSizeForTesting(size);
}

void ReportGenerator::CreateBasicRequest() {
#if defined(OS_CHROMEOS)
  AppendAndroidAppInfos();
#else
  basic_request_.set_computer_name(this->GetMachineName());
  basic_request_.set_os_user_name(GetOSUserName());
  basic_request_.set_serial_number(GetSerialNumber());
  basic_request_.set_allocated_os_report(GetOSReport().release());
#endif

  browser_report_generator_.Generate(base::BindOnce(
      &ReportGenerator::OnBrowserReportReady, weak_ptr_factory_.GetWeakPtr()));
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

#if defined(OS_CHROMEOS)

void ReportGenerator::AppendAndroidAppInfos() {
  // Android application is only supported for primary profile.
  Profile* primary_profile =
      g_browser_process->profile_manager()->GetPrimaryUserProfile();

  if (!arc::IsArcPlayStoreEnabledForProfile(primary_profile))
    return;

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(primary_profile);

  if (!prefs) {
    LOG(ERROR) << base::StringPrintf(
        "Failed to generate ArcAppListPrefs instance for primary user profile "
        "(debug name: %s).",
        primary_profile->GetDebugName().c_str());
    return;
  }

  AndroidAppInfoGenerator generator;
  for (std::string app_id : prefs->GetAppIds()) {
    em::AndroidAppInfo* app_info = basic_request_.add_android_app_infos();
    generator.Generate(prefs, app_id)->Swap(app_info);
  }
}

#endif

void ReportGenerator::OnBrowserReportReady(
    std::unique_ptr<em::BrowserReport> browser_report) {
  basic_request_.set_allocated_browser_report(browser_report.release());
  ReportRequests requests =
      report_request_queue_generator_.Generate(basic_request_);
  std::move(callback_).Run(std::move(requests));
}

}  // namespace enterprise_reporting

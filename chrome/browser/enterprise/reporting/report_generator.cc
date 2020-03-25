// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/report_generator.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
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
#include "chrome/browser/enterprise/reporting/android_app_info_generator.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#endif

namespace em = enterprise_management;

namespace enterprise_reporting {

ReportGenerator::ReportGenerator() = default;

ReportGenerator::~ReportGenerator() = default;

void ReportGenerator::Generate(bool with_profiles, ReportCallback callback) {
  CreateBasicRequest(std::make_unique<ReportRequest>(), with_profiles,
                     std::move(callback));
}

void ReportGenerator::SetMaximumReportSizeForTesting(size_t size) {
  report_request_queue_generator_.SetMaximumReportSizeForTesting(size);
}

void ReportGenerator::CreateBasicRequest(
    std::unique_ptr<ReportRequest> basic_request,
    bool with_profiles,
    ReportCallback callback) {
#if defined(OS_CHROMEOS)
  SetAndroidAppInfos(basic_request.get());
#else
  basic_request->set_computer_name(this->GetMachineName());
  basic_request->set_os_user_name(GetOSUserName());
  basic_request->set_serial_number(GetSerialNumber());
  basic_request->set_allocated_os_report(GetOSReport().release());
#endif

  browser_report_generator_.Generate(base::BindOnce(
      &ReportGenerator::OnBrowserReportReady, weak_ptr_factory_.GetWeakPtr(),
      with_profiles, std::move(callback), std::move(basic_request)));
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

void ReportGenerator::SetAndroidAppInfos(ReportRequest* basic_request) {
  DCHECK(basic_request);
  basic_request->clear_android_app_infos();

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
    basic_request->mutable_android_app_infos()->AddAllocated(
        generator.Generate(prefs, app_id).release());
  }
}

#endif

void ReportGenerator::OnBrowserReportReady(
    bool with_profiles,
    ReportCallback callback,
    std::unique_ptr<ReportRequest> basic_request,
    std::unique_ptr<em::BrowserReport> browser_report) {
  basic_request->set_allocated_browser_report(browser_report.release());

  if (with_profiles) {
    // Generate a queue of requests containing detailed profile information.
    std::move(callback).Run(
        report_request_queue_generator_.Generate(*basic_request));
    return;
  }

  // Return a queue containing only the basic request and browser report without
  // detailed profile information.
  ReportRequests requests;
  requests.push(std::move(basic_request));
  std::move(callback).Run(std::move(requests));
}

}  // namespace enterprise_reporting

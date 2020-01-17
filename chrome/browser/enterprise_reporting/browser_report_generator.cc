// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise_reporting/browser_report_generator.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/channel_info.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"
#include "content/public/common/webplugininfo.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#endif

namespace {

std::string GetExecutablePath() {
  base::FilePath path;
  base::PathService::Get(base::DIR_EXE, &path);
  return path.AsUTF8Unsafe();
}

}  // namespace

namespace enterprise_reporting {

BrowserReportGenerator::BrowserReportGenerator() = default;

BrowserReportGenerator::~BrowserReportGenerator() = default;

void BrowserReportGenerator::Generate(ReportCallback callback) {
  DCHECK(!callback_);
  callback_ = std::move(callback);

  auto report = std::make_unique<em::BrowserReport>();
  GenerateBasicInfos(report.get());
  GenerateProfileInfos(report.get());

  // std::move is required here because the function completes the report
  // asynchronously.
  GeneratePluginsIfNeeded(std::move(report));
}

void BrowserReportGenerator::GenerateBasicInfos(em::BrowserReport* report) {
#if !defined(OS_CHROMEOS)
  report->set_browser_version(version_info::GetVersionNumber());
  report->set_channel(policy::ConvertToProtoChannel(chrome::GetChannel()));
#endif

  report->set_executable_path(GetExecutablePath());
}

void BrowserReportGenerator::GenerateProfileInfos(em::BrowserReport* report) {
  for (auto* entry : g_browser_process->profile_manager()
                         ->GetProfileAttributesStorage()
                         .GetAllProfilesAttributes()) {
#if defined(OS_CHROMEOS)
    // Skip sign-in and lock screen app profile on Chrome OS.
    if (!chromeos::ProfileHelper::IsRegularProfilePath(
            entry->GetPath().BaseName())) {
      continue;
    }
#endif  // defined(OS_CHROMEOS)
    em::ChromeUserProfileInfo* profile =
        report->add_chrome_user_profile_infos();
    profile->set_id(entry->GetPath().AsUTF8Unsafe());
    profile->set_name(base::UTF16ToUTF8(entry->GetName()));
    profile->set_is_full_report(false);
  }
}

void BrowserReportGenerator::GeneratePluginsIfNeeded(
    std::unique_ptr<em::BrowserReport> report) {
#if defined(OS_CHROMEOS) || !BUILDFLAG(ENABLE_PLUGINS)
  std::move(callback_).Run(std::move(report));
#else
  content::PluginService::GetInstance()->GetPlugins(
      base::BindOnce(&BrowserReportGenerator::OnPluginsReady,
                     weak_ptr_factory_.GetWeakPtr(), std::move(report)));
#endif
}

#if BUILDFLAG(ENABLE_PLUGINS)
void BrowserReportGenerator::OnPluginsReady(
    std::unique_ptr<em::BrowserReport> report,
    const std::vector<content::WebPluginInfo>& plugins) {
  for (content::WebPluginInfo plugin : plugins) {
    em::Plugin* plugin_info = report->add_plugins();
    plugin_info->set_name(base::UTF16ToUTF8(plugin.name));
    plugin_info->set_version(base::UTF16ToUTF8(plugin.version));
    plugin_info->set_filename(plugin.path.BaseName().AsUTF8Unsafe());
    plugin_info->set_description(base::UTF16ToUTF8(plugin.desc));
  }

  std::move(callback_).Run(std::move(report));
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)

}  // namespace enterprise_reporting

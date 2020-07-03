// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/browser_report_generator_desktop.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/upgrade_detector/build_state.h"
#include "chrome/common/channel_info.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/version_info/version_info.h"
#include "ppapi/buildflags/buildflags.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#endif

namespace em = ::enterprise_management;

// TODO(crbug.com/1102047): Move Chrome OS code to its own delegate
namespace enterprise_reporting {

BrowserReportGeneratorDesktop::BrowserReportGeneratorDesktop() = default;

BrowserReportGeneratorDesktop::~BrowserReportGeneratorDesktop() = default;

std::string BrowserReportGeneratorDesktop::GetExecutablePath() {
  base::FilePath path;
  return base::PathService::Get(base::DIR_EXE, &path) ? path.AsUTF8Unsafe()
                                                      : std::string();
}

version_info::Channel BrowserReportGeneratorDesktop::GetChannel() {
  return chrome::GetChannel();
}

void BrowserReportGeneratorDesktop::GenerateBuildStateInfo(
    em::BrowserReport* report) {
#if !defined(OS_CHROMEOS)
  const auto* const build_state = g_browser_process->GetBuildState();
  if (build_state->update_type() != BuildState::UpdateType::kNone) {
    const auto& installed_version = build_state->installed_version();
    if (installed_version)
      report->set_installed_browser_version(installed_version->GetString());
  }
#endif
}

// Generates user profiles info in the given report instance.
void BrowserReportGeneratorDesktop::GenerateProfileInfo(
    em::BrowserReport* report) {
  for (const auto* entry : g_browser_process->profile_manager()
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

void BrowserReportGeneratorDesktop::GeneratePluginsIfNeeded(
    ReportCallback callback,
    std::unique_ptr<em::BrowserReport> report) {
#if defined(OS_CHROMEOS) || !BUILDFLAG(ENABLE_PLUGINS)
  std::move(callback).Run(std::move(report));
#else
  content::PluginService::GetInstance()->GetPlugins(base::BindOnce(
      &BrowserReportGeneratorDesktop::OnPluginsReady,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback), std::move(report)));
#endif
}

void BrowserReportGeneratorDesktop::OnPluginsReady(
    ReportCallback callback,
    std::unique_ptr<em::BrowserReport> report,
    const std::vector<content::WebPluginInfo>& plugins) {
#if BUILDFLAG(ENABLE_PLUGINS)
  for (const content::WebPluginInfo& plugin : plugins) {
    em::Plugin* plugin_info = report->add_plugins();
    plugin_info->set_name(base::UTF16ToUTF8(plugin.name));
    plugin_info->set_version(base::UTF16ToUTF8(plugin.version));
    plugin_info->set_filename(plugin.path.BaseName().AsUTF8Unsafe());
    plugin_info->set_description(base::UTF16ToUTF8(plugin.desc));
  }
#endif  // BUILDFLAG(ENABLE_PLUGINS)

  std::move(callback).Run(std::move(report));
}

}  // namespace enterprise_reporting

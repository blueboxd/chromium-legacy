// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise_reporting/profile_report_generator.h"

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise_reporting/extension_info.h"
#include "chrome/browser/enterprise_reporting/policy_info.h"
#include "chrome/browser/policy/policy_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/webplugininfo.h"

namespace enterprise_reporting {

ProfileReportGenerator::ProfileReportGenerator() {}

ProfileReportGenerator::~ProfileReportGenerator() = default;

void ProfileReportGenerator::set_extensions_and_plugins_enabled(bool enabled) {
  extensions_and_plugins_enabled_ = enabled;
}

void ProfileReportGenerator::set_policies_enabled(bool enabled) {
  policies_enabled_ = enabled;
}

void ProfileReportGenerator::MaybeGenerate(const base::FilePath& path,
                                           const std::string& name,
                                           ReportCallback callback) {
  DCHECK(!callback_);
  callback_ = std::move(callback);

  profile_ = g_browser_process->profile_manager()->GetProfileByPath(path);

  if (!profile_) {
    CheckReportStatus();
    return;
  }

  report_ = std::make_unique<em::ChromeUserProfileInfo>();
  report_->set_id(path.AsUTF8Unsafe());
  report_->set_name(name);
  report_->set_is_full_report(true);

  is_plugin_info_ready_ = false;

  GetSigninUserInfo();
  GetExtensionInfo();
  GetPluginInfo();

  if (policies_enabled_) {
    // TODO(crbug.com/983151): Upload policy error as their IDs.
    policies_ = policy::GetAllPolicyValuesAsDictionary(
        profile_, /* with_user_policies */ true, /* convert_values */ false,
        /* with_device_data*/ false,
        /* is_pretty_print */ false, /* convert_types */ false);
    GetChromePolicyInfo();
    GetExtensionPolicyInfo();
    GetPolicyFetchTimestampInfo();
  }

  CheckReportStatus();
  return;
}

void ProfileReportGenerator::GetSigninUserInfo() {
  auto account_info =
      IdentityManagerFactory::GetForProfile(profile_)->GetPrimaryAccountInfo();
  if (account_info.IsEmpty())
    return;
  auto* signed_in_user_info = report_->mutable_chrome_signed_in_user();
  signed_in_user_info->set_email(account_info.email);
  signed_in_user_info->set_obfudscated_gaiaid(account_info.gaia);
}

void ProfileReportGenerator::GetExtensionInfo() {
  if (!extensions_and_plugins_enabled_)
    return;
  AppendExtensionInfoIntoProfileReport(profile_, report_.get());
}

void ProfileReportGenerator::GetPluginInfo() {
  if (!extensions_and_plugins_enabled_) {
    is_plugin_info_ready_ = true;
    return;
  }

  content::PluginService::GetInstance()->GetPlugins(
      base::BindOnce(&ProfileReportGenerator::OnPluginsLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ProfileReportGenerator::GetChromePolicyInfo() {
  AppendChromePolicyInfoIntoProfileReport(policies_, report_.get());
}

void ProfileReportGenerator::GetExtensionPolicyInfo() {
  AppendExtensionPolicyInfoIntoProfileReport(policies_, report_.get());
}

void ProfileReportGenerator::GetPolicyFetchTimestampInfo() {
  AppendMachineLevelUserCloudPolicyFetchTimestamp(report_.get());
}

void ProfileReportGenerator::OnPluginsLoaded(
    const std::vector<content::WebPluginInfo>& plugins) {
  for (auto plugin : plugins) {
    auto* plugin_info = report_->add_plugins();
    plugin_info->set_name(base::UTF16ToUTF8(plugin.name));
    plugin_info->set_version(base::UTF16ToUTF8(plugin.version));
    plugin_info->set_filename(plugin.path.BaseName().AsUTF8Unsafe());
    plugin_info->set_description(base::UTF16ToUTF8(plugin.desc));
  }

  is_plugin_info_ready_ = true;
  CheckReportStatus();
}

void ProfileReportGenerator::CheckReportStatus() {
  // Report is not generated, return nullptr.
  if (!report_) {
    std::move(callback_).Run(nullptr);
    return;
  }

  // Report is not ready, quit and wait.
  if (!is_plugin_info_ready_)
    return;

  std::move(callback_).Run(std::move(report_));
}

}  // namespace enterprise_reporting

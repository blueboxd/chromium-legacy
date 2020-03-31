// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/profile_report_generator.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/util/values/values_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/reporting/extension_info.h"
#include "chrome/browser/enterprise/reporting/policy_info.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/policy/chrome_policy_conversions_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "extensions/common/extension_urls.h"

namespace em = enterprise_management;

namespace enterprise_reporting {
namespace {

const int kMaxNumberOfExtensionRequest = 1000;

// Extension request are moved out of the pending list once user confirm the
// notification. However, there is no need to upload these requests anymore as
// long as admin made a decision.
bool ShouldUploadExtensionRequest(
    const std::string& extension_id,
    const std::string& webstore_update_url,
    extensions::ExtensionManagement* extension_management) {
  auto mode = extension_management->GetInstallationMode(extension_id,
                                                        webstore_update_url);
  return (mode == extensions::ExtensionManagement::INSTALLATION_BLOCKED ||
          mode == extensions::ExtensionManagement::INSTALLATION_REMOVED) &&
         !extension_management->IsInstallationExplicitlyBlocked(extension_id);
}

}  // namespace
ProfileReportGenerator::ProfileReportGenerator() = default;

ProfileReportGenerator::~ProfileReportGenerator() = default;

void ProfileReportGenerator::set_extensions_enabled(bool enabled) {
  extensions_enabled_ = enabled;
}

void ProfileReportGenerator::set_policies_enabled(bool enabled) {
  policies_enabled_ = enabled;
}

std::unique_ptr<em::ChromeUserProfileInfo>
ProfileReportGenerator::MaybeGenerate(const base::FilePath& path,
                                      const std::string& name) {
  profile_ = g_browser_process->profile_manager()->GetProfileByPath(path);

  if (!profile_) {
    return nullptr;
  }

  report_ = std::make_unique<em::ChromeUserProfileInfo>();
  report_->set_id(path.AsUTF8Unsafe());
  report_->set_name(name);
  report_->set_is_full_report(true);

  GetSigninUserInfo();
  GetExtensionInfo();
  GetExtensionRequest();

  if (policies_enabled_) {
    // TODO(crbug.com/983151): Upload policy error as their IDs.
    auto client =
        std::make_unique<policy::ChromePolicyConversionsClient>(profile_);
    policies_ = policy::DictionaryPolicyConversions(std::move(client))
                    .EnableConvertTypes(false)
                    .EnablePrettyPrint(false)
                    .ToValue();
    GetChromePolicyInfo();
    GetExtensionPolicyInfo();
    GetPolicyFetchTimestampInfo();
  }

  return std::move(report_);
}

void ProfileReportGenerator::GetSigninUserInfo() {
  auto account_info =
      IdentityManagerFactory::GetForProfile(profile_)->GetPrimaryAccountInfo();
  if (account_info.IsEmpty())
    return;
  auto* signed_in_user_info = report_->mutable_chrome_signed_in_user();
  signed_in_user_info->set_email(account_info.email);
  signed_in_user_info->set_obfudscated_gaia_id(account_info.gaia);
}

void ProfileReportGenerator::GetExtensionInfo() {
  if (!extensions_enabled_)
    return;
  AppendExtensionInfoIntoProfileReport(profile_, report_.get());
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

void ProfileReportGenerator::GetExtensionRequest() {
  if (!profile_->GetPrefs()->GetBoolean(prefs::kCloudExtensionRequestEnabled))
    return;
  const base::DictionaryValue* pending_requests =
      profile_->GetPrefs()->GetDictionary(prefs::kCloudExtensionRequestIds);

  // In case a corrupted profile prefs causing |pending_requests| to be null.
  if (!pending_requests)
    return;

  extensions::ExtensionManagement* extension_management =
      extensions::ExtensionManagementFactory::GetForBrowserContext(profile_);
  std::string webstore_update_url =
      extension_urls::GetDefaultWebstoreUpdateUrl().spec();

  int number_of_requests = 0;
  for (const auto& it : *pending_requests) {
    if (!ShouldUploadExtensionRequest(it.first, webstore_update_url,
                                      extension_management)) {
      continue;
    }

    // Use a hard limitation to prevent users adding too many requests. 1000
    // requests should use less than 50 kb report space.
    number_of_requests += 1;
    if (number_of_requests > kMaxNumberOfExtensionRequest)
      break;

    auto* request = report_->add_extension_requests();
    request->set_id(it.first);
    base::Optional<base::Time> timestamp = ::util::ValueToTime(
        it.second->FindKey(extension_misc::kExtensionRequestTimestamp));
    if (timestamp)
      request->set_request_timestamp(timestamp->ToJavaTime());
  }
}

}  // namespace enterprise_reporting

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise_reporting/profile_report_generator.h"

#include "base/files/file_path.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/core/browser/account_info.h"
#include "services/identity/public/cpp/identity_manager.h"

namespace enterprise_reporting {

ProfileReportGenerator::ProfileReportGenerator() = default;

ProfileReportGenerator::~ProfileReportGenerator() = default;

void ProfileReportGenerator::set_extensions_and_plugins_enabled(bool enabled) {
  extensions_and_plugins_enabled_ = enabled;
}

void ProfileReportGenerator::set_policies_enabled(bool enabled) {
  policies_enabled_ = enabled;
}

std::unique_ptr<em::ChromeUserProfileInfo>
ProfileReportGenerator::MaybeGenerate(const base::FilePath& path,
                                      const std::string& name) {
  profile_ = g_browser_process->profile_manager()->GetProfileByPath(path);
  if (!profile_)
    return nullptr;

  report_ = std::make_unique<em::ChromeUserProfileInfo>();
  report_->set_id(path.AsUTF8Unsafe());
  report_->set_name(name);
  report_->set_is_full_report(true);

  GetSigninUserInfo();

  return std::move(report_);
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

}  // namespace enterprise_reporting

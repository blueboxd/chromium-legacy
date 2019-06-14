// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_PROFILE_REPORT_GENERATOR_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_PROFILE_REPORT_GENERATOR_H_

#include "base/macros.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace base {
class FilePath;
}

class Profile;

namespace enterprise_reporting {

/**
 * A report generator that collects Profile related information that is selected
 * by policies.
 */
class ProfileReportGenerator {
 public:
  ProfileReportGenerator();
  ~ProfileReportGenerator();

  void set_extensions_and_plugins_enabled(bool enabled);
  void set_policies_enabled(bool enabled);

  // Generates report for Profile if it's activated. Otherwise, returns null.
  std::unique_ptr<em::ChromeUserProfileInfo> MaybeGenerate(
      const base::FilePath& path,
      const std::string& name);

 protected:
  // Get Signin information includes email and gaia id.
  virtual void GetSigninUserInfo();

  // TODO(zmin): void GetExtensionInfo();
  // TODO(zmin): void GetPluginInfo();
  // TODO(zmin): void GetChromePolicyInfo();
  // TODO(zmin): void GetExtensionPolicyInfo();
  // TODO(zmin): void GetPolicyFetchTimestampInfo();

 private:
  Profile* profile_;

  bool extensions_and_plugins_enabled_ = true;
  bool policies_enabled_ = true;

  std::unique_ptr<em::ChromeUserProfileInfo> report_;

  DISALLOW_COPY_AND_ASSIGN(ProfileReportGenerator);
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_PROFILE_REPORT_GENERATOR_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_PROFILE_REPORT_GENERATOR_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_PROFILE_REPORT_GENERATOR_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace base {
class FilePath;
}

namespace content {
struct WebPluginInfo;
}

class Profile;

namespace enterprise_reporting {

/**
 * A report generator that collects Profile related information that is selected
 * by policies.
 */
class ProfileReportGenerator {
 public:
  using ReportCallback =
      base::OnceCallback<void(std::unique_ptr<em::ChromeUserProfileInfo>)>;

  ProfileReportGenerator();
  ~ProfileReportGenerator();

  void set_extensions_and_plugins_enabled(bool enabled);
  void set_policies_enabled(bool enabled);

  // Generates report for Profile if it's activated. Returns the report with
  // |callback| once it's ready. The report is null if it can't be generated.
  void MaybeGenerate(const base::FilePath& path,
                     const std::string& name,
                     ReportCallback callback);

 protected:
  // Get Signin information includes email and gaia id.
  virtual void GetSigninUserInfo();

  void GetExtensionInfo();
  void GetPluginInfo();
  void GetChromePolicyInfo();
  void GetExtensionPolicyInfo();
  void GetPolicyFetchTimestampInfo();

 private:
  void OnPluginsLoaded(const std::vector<content::WebPluginInfo>& plugins);

  void CheckReportStatus();

  Profile* profile_;
  base::Value policies_;

  bool extensions_and_plugins_enabled_ = true;
  bool policies_enabled_ = true;
  bool is_plugin_info_ready_ = false;
  ReportCallback callback_;

  std::unique_ptr<em::ChromeUserProfileInfo> report_ = nullptr;

  base::WeakPtrFactory<ProfileReportGenerator> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ProfileReportGenerator);
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_PROFILE_REPORT_GENERATOR_H_

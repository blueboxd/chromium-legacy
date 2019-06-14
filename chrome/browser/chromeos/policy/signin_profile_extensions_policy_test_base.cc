// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/signin_profile_extensions_policy_test_base.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

SigninProfileExtensionsPolicyTestBase::SigninProfileExtensionsPolicyTestBase(
    version_info::Channel channel)
    : channel_(channel), scoped_current_channel_(channel) {}

void SigninProfileExtensionsPolicyTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(chromeos::switches::kLoginManager);
  command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
}

void SigninProfileExtensionsPolicyTestBase::SetUpOnMainThread() {
  DevicePolicyCrosBrowserTest::SetUpOnMainThread();

  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &SigninProfileExtensionsPolicyTestBase::InterceptMockHttp,
      base::Unretained(this)));
  ASSERT_TRUE(embedded_test_server()->Start());
}

void SigninProfileExtensionsPolicyTestBase::AddExtensionForForceInstallation(
    const std::string& extension_id,
    const std::string& update_manifest_relative_path) {
  const GURL update_manifest_url =
      embedded_test_server()->GetURL(update_manifest_relative_path);
  const std::string policy_item_value = base::ReplaceStringPlaceholders(
      "$1;$2", {extension_id, update_manifest_url.spec()}, nullptr);
  device_policy()
      ->payload()
      .mutable_device_login_screen_extensions()
      ->add_device_login_screen_extensions(policy_item_value);
  RefreshDevicePolicy();
}

std::unique_ptr<net::test_server::HttpResponse>
SigninProfileExtensionsPolicyTestBase::InterceptMockHttp(
    const net::test_server::HttpRequest& request) {
  const std::string kFileNameToIntercept = "update_manifest.xml";
  if (request.GetURL().ExtractFileName() != kFileNameToIntercept)
    return nullptr;

  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  // Remove the leading '/'.
  std::string relative_manifest_path = request.GetURL().path().substr(1);
  std::string manifest_response;
  CHECK(base::ReadFileToString(test_data_dir.Append(relative_manifest_path),
                               &manifest_response));

  base::ReplaceSubstringsAfterOffset(
      &manifest_response, 0, "mock.http",
      embedded_test_server()->host_port_pair().ToString());

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("text/xml");
  response->set_content(manifest_response);
  return response;
}

Profile* SigninProfileExtensionsPolicyTestBase::GetInitialProfile() {
  // Intentionally not using the |chromeos::ProfileHelper::GetSigninProfile|
  // method here, as it performs the lazy construction of the profile, while for
  // the testing purposes it's better to assert that it has been created before.
  Profile* const profile =
      g_browser_process->profile_manager()->GetProfileByPath(
          chromeos::ProfileHelper::GetSigninProfileDir());
  DCHECK(profile);

  return profile;
}

}  // namespace policy

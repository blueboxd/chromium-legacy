// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/certificate_provider/test_certificate_provider_extension_login_screen_mixin.h"

#include <string>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/certificate_provider/test_certificate_provider_extension.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "extensions/test/test_background_page_first_load_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

// Extension ID of the test certificate provider extension.
constexpr char kTestCertProviderExtensionId[] =
    "ecmhnokcdiianioonpgakiooenfnonid";
// Path to the update manifest XML file of the test certificate provider
// extension.
constexpr char kTestCertProviderExtensionUpdateManifestPath[] =
    "/extensions/test_certificate_provider/update_manifest.xml";

Profile* GetProfile() {
  return chromeos::ProfileHelper::GetSigninProfile()->GetOriginalProfile();
}

}  // namespace

// static
std::string TestCertificateProviderExtensionLoginScreenMixin::GetExtensionId() {
  return kTestCertProviderExtensionId;
}

TestCertificateProviderExtensionLoginScreenMixin::
    TestCertificateProviderExtensionLoginScreenMixin(
        InProcessBrowserTestMixinHost* host,
        chromeos::DeviceStateMixin* device_state_mixin,
        bool load_extension_immediately)
    : InProcessBrowserTestMixin(host),
      device_state_mixin_(device_state_mixin),
      load_extension_immediately_(load_extension_immediately) {
  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  embedded_test_server_.ServeFilesFromDirectory(test_data_dir);
  // Rewrite the "mock.http" domain with the test server's address in the
  // served "update_manifest.xml" files, to let the extensions subsystem fetch
  // the .crx file referred from the update manifest.
  extensions::policy_test_utils::SetUpEmbeddedTestServer(
      &embedded_test_server_);
}

TestCertificateProviderExtensionLoginScreenMixin::
    ~TestCertificateProviderExtensionLoginScreenMixin() = default;

void TestCertificateProviderExtensionLoginScreenMixin::SetUpOnMainThread() {
  test_certificate_provider_extension_ =
      std::make_unique<TestCertificateProviderExtension>(GetProfile(),
                                                         GetExtensionId());
  ASSERT_TRUE(embedded_test_server_.Start());
  if (load_extension_immediately_) {
    AddExtensionForForceInstallation();
    WaitUntilExtensionLoaded();
  }
}

void TestCertificateProviderExtensionLoginScreenMixin::TearDownOnMainThread() {
  test_certificate_provider_extension_.reset();
}

void TestCertificateProviderExtensionLoginScreenMixin::
    AddExtensionForForceInstallation() {
  const GURL update_manifest_url = embedded_test_server_.GetURL(
      kTestCertProviderExtensionUpdateManifestPath);
  const std::string policy_item_value = base::ReplaceStringPlaceholders(
      "$1;$2", {GetExtensionId(), update_manifest_url.spec()}, nullptr);
  device_state_mixin_->RequestDevicePolicyUpdate()
      ->policy_payload()
      ->mutable_device_login_screen_extensions()
      ->add_device_login_screen_extensions(policy_item_value);
}

void TestCertificateProviderExtensionLoginScreenMixin::
    WaitUntilExtensionLoaded() {
  extensions::TestBackgroundPageFirstLoadObserver bg_page_first_load_observer(
      GetProfile(), GetExtensionId());
  bg_page_first_load_observer.Wait();
}

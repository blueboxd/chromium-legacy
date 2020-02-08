// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/mature_extension_checker.h"

#include <memory>

#include "base/files/file_path.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"

namespace {

constexpr char kRegularId[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
constexpr char kMatureId[] = "dfhpodpjggiioolfhoimofdbfjibmedp";
constexpr char kRegularTheme[] = "idlfhncioikpdnlhnmcjogambnefbbfp";
constexpr char kMatureTheme[] = "ibcijncamhmjjdodjamgiipcgnnaeagd";

}  // namespace

namespace extensions {

class MatureExtensionCheckerTest : public ExtensionServiceTestWithInstall {
 public:
  MatureExtensionCheckerTest() = default;
  ~MatureExtensionCheckerTest() override = default;

  void SetUp() override {
    ExtensionServiceTestWithInstall::SetUp();

    ExtensionServiceInitParams params = CreateDefaultInitParams();
    params.profile_is_supervised = true;
    InitializeExtensionService(params);

    mature_extension_checker_ =
        std::make_unique<extensions::MatureExtensionChecker>(profile());
  }

 protected:
  std::unique_ptr<extensions::MatureExtensionChecker> mature_extension_checker_;
};

TEST_F(MatureExtensionCheckerTest, RequestFailure) {
  EXPECT_FALSE(mature_extension_checker_->HasRequestForExtension(kRegularId));
  EXPECT_FALSE(mature_extension_checker_->HasRequestForExtension(kMatureId));
  EXPECT_FALSE(mature_extension_checker_->HasDataForExtension(kRegularId));
  EXPECT_FALSE(mature_extension_checker_->HasDataForExtension(kMatureId));
  // Send out RPC to request maturity data for these two extensions.
  mature_extension_checker_->CheckMatureDataForExtension(kRegularId);
  mature_extension_checker_->CheckMatureDataForExtension(kMatureId);
  // We have already sent out requests.
  EXPECT_TRUE(mature_extension_checker_->HasRequestForExtension(kRegularId));
  EXPECT_TRUE(mature_extension_checker_->HasRequestForExtension(kMatureId));
  EXPECT_FALSE(mature_extension_checker_->HasDataForExtension(kRegularId));
  EXPECT_FALSE(mature_extension_checker_->HasDataForExtension(kMatureId));
  // Simulate webstore request failures.
  mature_extension_checker_->OnWebstoreRequestFailure(kRegularId);
  mature_extension_checker_->OnWebstoreRequestFailure(kMatureId);
  EXPECT_FALSE(mature_extension_checker_->HasRequestForExtension(kRegularId));
  EXPECT_FALSE(mature_extension_checker_->HasRequestForExtension(kMatureId));
  EXPECT_TRUE(mature_extension_checker_->HasDataForExtension(kRegularId));
  EXPECT_TRUE(mature_extension_checker_->HasDataForExtension(kMatureId));
  // Since the requests failed, return false by default.
  EXPECT_FALSE(mature_extension_checker_->IsExtensionMature(kRegularId));
  EXPECT_FALSE(mature_extension_checker_->IsExtensionMature(kMatureId));
}

TEST_F(MatureExtensionCheckerTest, ResponseParseSuccess) {
  // Install both extensions.
  base::FilePath regular_path = data_dir().AppendASCII("good.crx");
  base::FilePath mature_path = data_dir().AppendASCII("good2048.crx");
  EXPECT_TRUE(InstallCRX(regular_path, INSTALL_NEW));
  EXPECT_TRUE(InstallCRX(mature_path, INSTALL_NEW));
  EXPECT_FALSE(mature_extension_checker_->HasRequestForExtension(kRegularId));
  EXPECT_FALSE(mature_extension_checker_->HasRequestForExtension(kMatureId));
  EXPECT_FALSE(mature_extension_checker_->HasDataForExtension(kRegularId));
  EXPECT_FALSE(mature_extension_checker_->HasDataForExtension(kMatureId));
  // Send out RPC to request maturity data for these two extensions.
  mature_extension_checker_->CheckMatureDataForExtension(kRegularId);
  mature_extension_checker_->CheckMatureDataForExtension(kMatureId);
  // We have already sent out requests.
  EXPECT_TRUE(mature_extension_checker_->HasRequestForExtension(kRegularId));
  EXPECT_TRUE(mature_extension_checker_->HasRequestForExtension(kMatureId));
  EXPECT_FALSE(mature_extension_checker_->HasDataForExtension(kRegularId));
  EXPECT_FALSE(mature_extension_checker_->HasDataForExtension(kMatureId));
  // Simulate webstore response parse success.
  mature_extension_checker_->MarkExtensionMatureForTesting(kRegularId, false);
  mature_extension_checker_->MarkExtensionMatureForTesting(kMatureId, true);
  EXPECT_FALSE(mature_extension_checker_->HasRequestForExtension(kRegularId));
  EXPECT_FALSE(mature_extension_checker_->HasRequestForExtension(kMatureId));
  EXPECT_TRUE(mature_extension_checker_->HasDataForExtension(kRegularId));
  EXPECT_TRUE(mature_extension_checker_->HasDataForExtension(kMatureId));
  // The regular extension should be marked family safe.
  EXPECT_FALSE(mature_extension_checker_->IsExtensionMature(kRegularId));
  // The mature extension should be marked family unsafe.
  EXPECT_TRUE(mature_extension_checker_->IsExtensionMature(kMatureId));
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile());
  // The regular extension should not have any disable reasons.
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kRegularId));
  EXPECT_EQ(extensions::disable_reason::DISABLE_NONE,
            extension_prefs->GetDisableReasons(kRegularId));
  // The mature extension should be disabled.
  EXPECT_TRUE(registry()->disabled_extensions().Contains(kMatureId));
  EXPECT_EQ(extensions::disable_reason::DISABLE_BLOCKED_MATURE,
            extension_prefs->GetDisableReasons(kMatureId));
}

// Let's test that mature themes are blocked too.
TEST_F(MatureExtensionCheckerTest, ResponseParseSuccessTheme) {
  // Install both themes.
  base::FilePath regular_path = data_dir().AppendASCII("theme.crx");
  base::FilePath mature_path = data_dir().AppendASCII("theme2.crx");
  EXPECT_TRUE(InstallCRX(regular_path, INSTALL_NEW));
  EXPECT_TRUE(InstallCRX(mature_path, INSTALL_NEW));
  EXPECT_FALSE(
      mature_extension_checker_->HasRequestForExtension(kRegularTheme));
  EXPECT_FALSE(mature_extension_checker_->HasRequestForExtension(kMatureTheme));
  EXPECT_FALSE(mature_extension_checker_->HasDataForExtension(kRegularTheme));
  EXPECT_FALSE(mature_extension_checker_->HasDataForExtension(kMatureTheme));
  // Send out RPC to request maturity data for these two themes.
  mature_extension_checker_->CheckMatureDataForExtension(kRegularTheme);
  mature_extension_checker_->CheckMatureDataForExtension(kMatureTheme);
  // We have already sent out requests.
  EXPECT_TRUE(mature_extension_checker_->HasRequestForExtension(kRegularTheme));
  EXPECT_TRUE(mature_extension_checker_->HasRequestForExtension(kMatureTheme));
  EXPECT_FALSE(mature_extension_checker_->HasDataForExtension(kRegularTheme));
  EXPECT_FALSE(mature_extension_checker_->HasDataForExtension(kMatureTheme));
  // Simulate webstore response parse success.
  mature_extension_checker_->MarkExtensionMatureForTesting(kRegularTheme,
                                                           false);
  mature_extension_checker_->MarkExtensionMatureForTesting(kMatureTheme, true);
  EXPECT_FALSE(
      mature_extension_checker_->HasRequestForExtension(kRegularTheme));
  EXPECT_FALSE(mature_extension_checker_->HasRequestForExtension(kMatureTheme));
  EXPECT_TRUE(mature_extension_checker_->HasDataForExtension(kRegularTheme));
  EXPECT_TRUE(mature_extension_checker_->HasDataForExtension(kMatureTheme));
  // The regular theme should be marked family safe.
  EXPECT_FALSE(mature_extension_checker_->IsExtensionMature(kRegularTheme));
  // The mature theme should be marked family unsafe.
  EXPECT_TRUE(mature_extension_checker_->IsExtensionMature(kMatureTheme));
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile());
  // The regular theme should not have any disable reasons.
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kRegularTheme));
  EXPECT_EQ(extensions::disable_reason::DISABLE_NONE,
            extension_prefs->GetDisableReasons(kRegularTheme));
  // The mature theme should be disabled.
  EXPECT_TRUE(registry()->disabled_extensions().Contains(kMatureTheme));
  EXPECT_EQ(extensions::disable_reason::DISABLE_BLOCKED_MATURE,
            extension_prefs->GetDisableReasons(kMatureTheme));
}

TEST_F(MatureExtensionCheckerTest, ResponseParseFailure) {
  EXPECT_FALSE(mature_extension_checker_->HasRequestForExtension(kRegularId));
  EXPECT_FALSE(mature_extension_checker_->HasRequestForExtension(kMatureId));
  EXPECT_FALSE(mature_extension_checker_->HasDataForExtension(kRegularId));
  EXPECT_FALSE(mature_extension_checker_->HasDataForExtension(kMatureId));
  // Send out RPC to request maturity data for these two extensions.
  mature_extension_checker_->CheckMatureDataForExtension(kRegularId);
  mature_extension_checker_->CheckMatureDataForExtension(kMatureId);
  // We have already sent out requests.
  EXPECT_TRUE(mature_extension_checker_->HasRequestForExtension(kRegularId));
  EXPECT_TRUE(mature_extension_checker_->HasRequestForExtension(kMatureId));
  EXPECT_FALSE(mature_extension_checker_->HasDataForExtension(kRegularId));
  EXPECT_FALSE(mature_extension_checker_->HasDataForExtension(kMatureId));
  // Simulate webstore response parse failures.
  mature_extension_checker_->OnWebstoreResponseParseFailure(kRegularId,
                                                            "Test error");
  mature_extension_checker_->OnWebstoreResponseParseFailure(kMatureId,
                                                            "Test error");
  EXPECT_FALSE(mature_extension_checker_->HasRequestForExtension(kRegularId));
  EXPECT_FALSE(mature_extension_checker_->HasRequestForExtension(kMatureId));
  EXPECT_TRUE(mature_extension_checker_->HasDataForExtension(kRegularId));
  EXPECT_TRUE(mature_extension_checker_->HasDataForExtension(kMatureId));
  // Since the requests failed, return false by default.
  EXPECT_FALSE(mature_extension_checker_->IsExtensionMature(kRegularId));
  EXPECT_FALSE(mature_extension_checker_->IsExtensionMature(kMatureId));
}

TEST_F(MatureExtensionCheckerTest, RemoveDisableReasonBlockedMature) {
  // Child logs in and tries to install a mature extension.
  base::FilePath mature_path = data_dir().AppendASCII("good2048.crx");
  EXPECT_TRUE(InstallCRX(mature_path, INSTALL_NEW));
  // Send out RPC to request maturity data for this extension.
  mature_extension_checker_->CheckMatureDataForExtension(kMatureId);
  // Webstore Item Json Data API replies saying that this extension is mature.
  mature_extension_checker_->MarkExtensionMatureForTesting(kMatureId, true);
  // The mature extension should be disabled.
  EXPECT_TRUE(registry()->disabled_extensions().Contains(kMatureId));
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile());
  EXPECT_EQ(extensions::disable_reason::DISABLE_BLOCKED_MATURE,
            extension_prefs->GetDisableReasons(kMatureId));

  // Now suppose the extension become no longer mature and the child signs out
  // and logs back in.
  mature_extension_checker_->CheckMatureDataForExtension(kMatureId);
  // Webstore Item Json Data API replies saying that this extension is no longer
  // mature.
  mature_extension_checker_->MarkExtensionMatureForTesting(kMatureId, false);
  // The now appropriate extension should be enabled, assuming parent approval
  // has been granted.
  EXPECT_TRUE(registry()->enabled_extensions().Contains(kMatureId));
  EXPECT_EQ(extensions::disable_reason::DISABLE_NONE,
            extension_prefs->GetDisableReasons(kMatureId));
}

}  // namespace extensions

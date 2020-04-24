// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_test_helper.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plugin_vm {

const char kBaseDriveUrl[] = "https://drive.google.com/open?id=";
const char kDriveFileId[] = "Yxhi5BDTxsEl9onT8AunH4o_tkKviFGjY";
const char kDriveExtraParam[] = "&foobar=barfoo";

class PluginVmUtilTest : public testing::Test {
 public:
  PluginVmUtilTest() = default;

  MOCK_METHOD(void, OnPolicyChanged, (bool));

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<PluginVmTestHelper> test_helper_;

  void SetUp() override {
    testing_profile_ = std::make_unique<TestingProfile>();
    test_helper_ = std::make_unique<PluginVmTestHelper>(testing_profile_.get());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PluginVmUtilTest);
};

TEST_F(PluginVmUtilTest, PluginVmShouldBeAllowedOnceAllConditionsAreMet) {
  EXPECT_FALSE(IsPluginVmAllowedForProfile(testing_profile_.get()));

  test_helper_->AllowPluginVm();
  EXPECT_TRUE(IsPluginVmAllowedForProfile(testing_profile_.get()));
}

TEST_F(PluginVmUtilTest, PluginVmShouldNotBeAllowedUnlessAllConditionsAreMet) {
  EXPECT_FALSE(IsPluginVmAllowedForProfile(testing_profile_.get()));

  test_helper_->SetUserRequirementsToAllowPluginVm();
  EXPECT_FALSE(IsPluginVmAllowedForProfile(testing_profile_.get()));

  test_helper_->EnablePluginVmFeature();
  EXPECT_FALSE(IsPluginVmAllowedForProfile(testing_profile_.get()));

  test_helper_->EnterpriseEnrollDevice();
  EXPECT_FALSE(IsPluginVmAllowedForProfile(testing_profile_.get()));

  test_helper_->SetPolicyRequirementsToAllowPluginVm();
  EXPECT_TRUE(IsPluginVmAllowedForProfile(testing_profile_.get()));
}

TEST_F(PluginVmUtilTest, PluginVmShouldBeConfiguredOnceAllConditionsAreMet) {
  EXPECT_FALSE(IsPluginVmConfigured(testing_profile_.get()));

  testing_profile_->GetPrefs()->SetBoolean(
      plugin_vm::prefs::kPluginVmImageExists, true);
  EXPECT_TRUE(IsPluginVmConfigured(testing_profile_.get()));
}

TEST_F(PluginVmUtilTest, GetPluginVmLicenseKey) {
  // If no license key is set, the method should return the empty string.
  EXPECT_EQ(std::string(), GetPluginVmLicenseKey());

  const std::string kLicenseKey = "LICENSE_KEY";
  testing_profile_->ScopedCrosSettingsTestHelper()->SetString(
      chromeos::kPluginVmLicenseKey, kLicenseKey);
  EXPECT_EQ(kLicenseKey, GetPluginVmLicenseKey());
}

TEST_F(PluginVmUtilTest, AddPluginVmPolicyObserver) {
  const std::unique_ptr<PluginVmPolicySubscription> subscription =
      std::make_unique<plugin_vm::PluginVmPolicySubscription>(
          testing_profile_.get(),
          base::BindRepeating(&PluginVmUtilTest::OnPolicyChanged,
                              base::Unretained(this)));

  EXPECT_FALSE(IsPluginVmAllowedForProfile(testing_profile_.get()));

  EXPECT_CALL(*this, OnPolicyChanged(true));
  test_helper_->AllowPluginVm();
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnPolicyChanged(false));
  testing_profile_->ScopedCrosSettingsTestHelper()->SetString(
      chromeos::kPluginVmLicenseKey, "");
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnPolicyChanged(true));
  const std::string kLicenseKey = "LICENSE_KEY";
  testing_profile_->ScopedCrosSettingsTestHelper()->SetString(
      chromeos::kPluginVmLicenseKey, kLicenseKey);
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnPolicyChanged(false));
  testing_profile_->ScopedCrosSettingsTestHelper()->SetBoolean(
      chromeos::kPluginVmAllowed, false);
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnPolicyChanged(true));
  testing_profile_->ScopedCrosSettingsTestHelper()->SetBoolean(
      chromeos::kPluginVmAllowed, true);
}

TEST_F(PluginVmUtilTest, DriveLinkDetection) {
  std::string base_url(kBaseDriveUrl);
  std::string file_id(kDriveFileId);

  EXPECT_TRUE(IsDriveUrl(GURL(base_url + file_id)));
  EXPECT_TRUE(IsDriveUrl(
      GURL(base_url + file_id + kDriveExtraParam + kDriveExtraParam)));

  EXPECT_FALSE(IsDriveUrl(GURL("https://othersite.com?id=" + file_id)));
  EXPECT_FALSE(
      IsDriveUrl(GURL("https://drive.google.com.othersite.com?id=" + file_id)));
  EXPECT_FALSE(IsDriveUrl(GURL(base_url)));
}

TEST_F(PluginVmUtilTest, DriveLinkIdExtraction) {
  std::string base_url(kBaseDriveUrl);
  std::string file_id(kDriveFileId);

  EXPECT_EQ(GetIdFromDriveUrl(GURL(base_url + file_id)), file_id);
  EXPECT_EQ(GetIdFromDriveUrl(GURL(base_url + file_id + kDriveExtraParam)),
            file_id);
  EXPECT_EQ(GetIdFromDriveUrl(
                GURL(base_url + file_id + kDriveExtraParam + kDriveExtraParam)),
            file_id);
}

}  // namespace plugin_vm

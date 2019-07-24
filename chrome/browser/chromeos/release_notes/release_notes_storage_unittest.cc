// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/release_notes/release_notes_storage.h"

#include <memory>

#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/login/login_state/login_state.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class ReleaseNotesStorageTest : public testing::Test {
 protected:
  ReleaseNotesStorageTest()
      : user_manager_(new FakeChromeUserManager()),
        scoped_user_manager_(
            std::unique_ptr<chromeos::FakeChromeUserManager>(user_manager_)) {}
  ~ReleaseNotesStorageTest() override {}

  std::unique_ptr<Profile> CreateProfile(std::string email) {
    AccountId account_id_ = AccountId::FromUserEmailGaiaId(email, "12345");
    user_manager_->AddUser(account_id_);
    TestingProfile::Builder builder;
    builder.SetProfileName(email);
    return builder.Build();
  }

  FakeChromeUserManager* user_manager_;
  user_manager::ScopedUserManager scoped_user_manager_;
  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(ReleaseNotesStorageTest);
};

TEST_F(ReleaseNotesStorageTest, ModifyLastRelease) {
  std::unique_ptr<Profile> profile = CreateProfile("test@gmail.com");

  profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);
  std::unique_ptr<ReleaseNotesStorage> release_notes_storage =
      std::make_unique<ReleaseNotesStorage>(profile.get());
  profile.get()->GetPrefs()->SetInteger(prefs::kReleaseNotesLastShownMilestone,
                                        -1);

  EXPECT_EQ(true, release_notes_storage->ShouldNotify());
  release_notes_storage->MarkNotificationShown();
  EXPECT_NE(-1, profile.get()->GetPrefs()->GetInteger(
                    prefs::kReleaseNotesLastShownMilestone));
  EXPECT_EQ(false, release_notes_storage->ShouldNotify());
}

TEST_F(ReleaseNotesStorageTest, ShouldShowReleaseNotes) {
  std::unique_ptr<Profile> profile = CreateProfile("test@gmail.com");

  profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);
  std::unique_ptr<ReleaseNotesStorage> release_notes_storage =
      std::make_unique<ReleaseNotesStorage>(profile.get());
  profile.get()->GetPrefs()->SetInteger(prefs::kReleaseNotesLastShownMilestone,
                                        -1);

  EXPECT_EQ(true, release_notes_storage->ShouldNotify());
}

TEST_F(ReleaseNotesStorageTest, ShouldNotShowReleaseNotes) {
  std::unique_ptr<Profile> profile = CreateProfile("test@company.com");

  profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  std::unique_ptr<ReleaseNotesStorage> release_notes_storage =
      std::make_unique<ReleaseNotesStorage>(profile.get());
  profile.get()->GetPrefs()->SetInteger(prefs::kReleaseNotesLastShownMilestone,
                                        -1);

  EXPECT_EQ(false, release_notes_storage->ShouldNotify());
}

TEST_F(ReleaseNotesStorageTest, ShouldShowReleaseNotesGoogler) {
  std::unique_ptr<Profile> profile = CreateProfile("test@google.com");

  profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  std::unique_ptr<ReleaseNotesStorage> release_notes_storage =
      std::make_unique<ReleaseNotesStorage>(profile.get());
  profile.get()->GetPrefs()->SetInteger(prefs::kReleaseNotesLastShownMilestone,
                                        -1);

  EXPECT_EQ(true, release_notes_storage->ShouldNotify());
}

}  // namespace chromeos

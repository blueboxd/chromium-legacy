// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/voice_interaction/voice_interaction_controller_client.h"

#include "ash/public/cpp/voice_interaction_controller.h"
#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_util.h"
#include "components/arc/test/fake_arc_session.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class VoiceInteractionControllerClientTest : public ChromeAshTestBase {
 public:
  VoiceInteractionControllerClientTest()
      : fake_user_manager_(
            std::make_unique<chromeos::FakeChromeUserManager>()) {}
  ~VoiceInteractionControllerClientTest() override = default;

  void SetUp() override {
    ChromeAshTestBase::SetUp();

    // Setup test profile.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("user@gmail.com");
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII("TestArcProfile"));
    profile_ = profile_builder.Build();

    // Setup dependencies
    arc_session_manager_ =
        std::make_unique<ArcSessionManager>(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));
    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        profile()->GetProfileUserName(), "1234567890"));
    GetFakeUserManager()->AddUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);

    voice_interaction_controller_client_ =
        std::make_unique<VoiceInteractionControllerClient>();
    voice_interaction_controller_client_->SetProfile(profile_.get());
  }

  void TearDown() override {
    voice_interaction_controller_client_.reset();
    profile_.reset();
    arc_session_manager_->Shutdown();
    arc_session_manager_.reset();
    ChromeAshTestBase::TearDown();
  }

  VoiceInteractionControllerClient* voice_interaction_controller_client() {
    return voice_interaction_controller_client_.get();
  }

  Profile* profile() { return profile_.get(); }

  ArcSessionManager* arc_session_manager() {
    return arc_session_manager_.get();
  }

 private:
  chromeos::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<chromeos::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingProfile> profile_;
  user_manager::ScopedUserManager fake_user_manager_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  std::unique_ptr<VoiceInteractionControllerClient>
      voice_interaction_controller_client_;
};

TEST_F(VoiceInteractionControllerClientTest, PrefChangeSendsNotification) {
  PrefService* prefs = profile()->GetPrefs();

  ASSERT_EQ(false, prefs->GetBoolean(prefs::kVoiceInteractionEnabled));
  prefs->SetBoolean(prefs::kVoiceInteractionEnabled, true);
  ASSERT_EQ(true, prefs->GetBoolean(prefs::kVoiceInteractionEnabled));
  EXPECT_EQ(true, ash::VoiceInteractionController::Get()->settings_enabled());

  ASSERT_EQ(false, prefs->GetBoolean(prefs::kVoiceInteractionContextEnabled));
  prefs->SetBoolean(prefs::kVoiceInteractionContextEnabled, true);
  ASSERT_EQ(true, prefs->GetBoolean(prefs::kVoiceInteractionContextEnabled));
  EXPECT_EQ(true, ash::VoiceInteractionController::Get()->context_enabled());

  ASSERT_EQ(false, prefs->GetBoolean(prefs::kVoiceInteractionHotwordEnabled));
  prefs->SetBoolean(prefs::kVoiceInteractionHotwordEnabled, true);
  ASSERT_EQ(true, prefs->GetBoolean(prefs::kVoiceInteractionHotwordEnabled));
  EXPECT_EQ(true, ash::VoiceInteractionController::Get()->hotword_enabled());

  ASSERT_EQ("", prefs->GetString(language::prefs::kApplicationLocale));
  prefs->SetString(language::prefs::kApplicationLocale, "en-CA");
  ASSERT_EQ("en-CA", prefs->GetString(language::prefs::kApplicationLocale));
  EXPECT_EQ("en-CA", ash::VoiceInteractionController::Get()->locale());
}

}  // namespace arc

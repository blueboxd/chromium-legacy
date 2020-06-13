// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_settings/ios_cookie_blocker.h"

#include "base/test/scoped_feature_list.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/content_settings/cookie_settings_factory.h"
#import "ios/web/public/browsing_data/cookie_blocking_mode.h"
#include "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class IOSCookieBlockerTest : public PlatformTest {
 public:
  IOSCookieBlockerTest() {
    feature_list_.InitAndEnableFeature(
        content_settings::kImprovedCookieControls);

    TestChromeBrowserState::Builder builder;
    chrome_browser_state_ = builder.Build();
  }

  void SetUp() override {
    PlatformTest::SetUp();

    content_settings::ContentSettingsRegistry::GetInstance()->ResetForTest();
    content_settings::CookieSettings::RegisterProfilePrefs(prefs_.registry());
    HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());
    scoped_refptr<HostContentSettingsMap> settings_map =
        new HostContentSettingsMap(
            &prefs_, false /* is_off_the_record */,
            false /* store_last_modified */,
            false /* migrate_requesting_and_top_level_origin_settings */,
            false /* restore_session */);
    cookie_settings_ = new content_settings::CookieSettings(
        settings_map.get(), &prefs_, false, "chrome-extension");

    cookie_blocker_ =
        new IOSCookieBlocker(chrome_browser_state_.get(), cookie_settings_);
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;

  sync_preferences::TestingPrefServiceSyncable prefs_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  IOSCookieBlocker* cookie_blocker_;
};

// Test that CredentialProviderService can be created.
TEST_F(IOSCookieBlockerTest, CorrectBlockingMode) {
  EXPECT_EQ(web::CookieBlockingMode::kAllow,
            chrome_browser_state_->GetCookieBlockingMode());

  // Turn on third party cookie blocking preference.
  prefs_.SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));
  EXPECT_EQ(web::CookieBlockingMode::kBlockThirdParty,
            chrome_browser_state_->GetCookieBlockingMode());

  // Disable cookies altogether.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  EXPECT_EQ(web::CookieBlockingMode::kBlock,
            chrome_browser_state_->GetCookieBlockingMode());

  // Undo settings, to verify that state goes back to allow.
  prefs_.SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kOff));
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_ALLOW);
  EXPECT_EQ(web::CookieBlockingMode::kAllow,
            chrome_browser_state_->GetCookieBlockingMode());
}

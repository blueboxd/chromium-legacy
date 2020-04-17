// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cookie_controls/cookie_controls_controller.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/tab_specific_content_settings_delegate.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_service.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/browser/tab_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockCookieControlsView : public CookieControlsView {
 public:
  MOCK_METHOD3(OnStatusChanged,
               void(CookieControlsStatus, CookieControlsEnforcement, int));
  MOCK_METHOD1(OnBlockedCookiesCountChanged, void(int));
};

}  // namespace

// More readable output for test expectation.
std::ostream& operator<<(std::ostream& os, const CookieControlsStatus& status) {
  switch (status) {
    case CookieControlsStatus::kDisabled:
      return os << "kDisabled";
    case CookieControlsStatus::kEnabled:
      return os << "kEnabled";
    case CookieControlsStatus::kDisabledForSite:
      return os << "kDisabledForSite";
    case CookieControlsStatus::kUninitialized:
      return os << "kUninitialized";
  }
}

std::ostream& operator<<(std::ostream& os,
                         const CookieControlsEnforcement& enforcement) {
  switch (enforcement) {
    case CookieControlsEnforcement::kNoEnforcement:
      return os << "kNoEnforcement";
    case CookieControlsEnforcement::kEnforcedByCookieSetting:
      return os << "kEnforcedByCookieSetting";
    case CookieControlsEnforcement::kEnforcedByExtension:
      return os << "kEnforcedByExtension";
    case CookieControlsEnforcement::kEnforcedByPolicy:
      return os << "kEnforcedByPolicy";
  }
}

class CookieControlsTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    feature_list.InitAndEnableFeature(
        content_settings::kImprovedCookieControls);
    ChromeRenderViewHostTestHarness::SetUp();
    content_settings::TabSpecificContentSettings::CreateForWebContents(
        web_contents(),
        std::make_unique<chrome::TabSpecificContentSettingsDelegate>(
            web_contents()));
    profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(content_settings::CookieControlsMode::kOn));
    NavigateAndCommit(GURL("chrome://newtab"));

    cookie_controls_ =
        std::make_unique<CookieControlsController>(web_contents());
    cookie_controls_->AddObserver(mock());
    testing::Mock::VerifyAndClearExpectations(mock());
  }

  void TearDown() override {
    cookie_controls_->RemoveObserver(mock());
    cookie_controls_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  CookieControlsController* cookie_controls() { return cookie_controls_.get(); }

  MockCookieControlsView* mock() { return &mock_; }

  content_settings::TabSpecificContentSettings*
  tab_specific_content_settings() {
    return content_settings::TabSpecificContentSettings::FromWebContents(
        web_contents());
  }

 private:
  base::test::ScopedFeatureList feature_list;
  MockCookieControlsView mock_;
  std::unique_ptr<CookieControlsController> cookie_controls_;
};

TEST_F(CookieControlsTest, NewTabPage) {
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kDisabled,
                              CookieControlsEnforcement::kNoEnforcement, 0));
  cookie_controls()->Update(web_contents());
}

TEST_F(CookieControlsTest, SomeWebSite) {
  // Visiting a website should enable the UI.
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement, 0));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Accessing cookies should not change anything.
  EXPECT_CALL(*mock(), OnBlockedCookiesCountChanged(0));
  tab_specific_content_settings()->OnWebDatabaseAccessed(
      GURL("https://example.com"), /*blocked=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Blocking cookies should update the blocked cookie count.
  EXPECT_CALL(*mock(), OnBlockedCookiesCountChanged(1));
  tab_specific_content_settings()->OnWebDatabaseAccessed(
      GURL("https://thirdparty.com"), /*blocked=*/true);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Navigating somewhere else should reset the cookie count.
  NavigateAndCommit(GURL("https://somethingelse.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement, 0));
  cookie_controls()->Update(web_contents());
}

TEST_F(CookieControlsTest, PreferenceDisabled) {
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement, 0));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Disabling the feature should disable the UI.
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kDisabled,
                              CookieControlsEnforcement::kNoEnforcement, 0));
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kOff));
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsTest, DisableForSite) {
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement, 0));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Disabling cookie blocking for example.com should update the ui.
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kDisabledForSite,
                              CookieControlsEnforcement::kNoEnforcement, 0));
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Visiting some other site, should switch back to kEnabled.
  NavigateAndCommit(GURL("https://somethingelse.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement, 0));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Visiting example.com should set status to kDisabledForSite.
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kDisabledForSite,
                              CookieControlsEnforcement::kNoEnforcement, 0));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Enabling example.com again should change status to kEnabled.
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement, 0));
  cookie_controls()->OnCookieBlockingEnabledForSite(true);
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsTest, Incognito) {
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement, 0));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Create incognito web_contents and CookieControlsController.
  std::unique_ptr<content::WebContents> incognito_web_contents =
      content::WebContentsTester::CreateTestWebContents(
          profile()->GetOffTheRecordProfile(), nullptr);
  content_settings::TabSpecificContentSettings::CreateForWebContents(
      incognito_web_contents.get(),
      std::make_unique<chrome::TabSpecificContentSettingsDelegate>(
          incognito_web_contents.get()));
  auto* tester = content::WebContentsTester::For(incognito_web_contents.get());
  MockCookieControlsView incognito_mock_;
  CookieControlsController incognito_cookie_controls(
      incognito_web_contents.get());
  incognito_cookie_controls.AddObserver(&incognito_mock_);

  // Navigate incognito web_contents to the same URL.
  tester->NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(incognito_mock_,
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement, 0));
  incognito_cookie_controls.Update(incognito_web_contents.get());
  testing::Mock::VerifyAndClearExpectations(mock());
  testing::Mock::VerifyAndClearExpectations(&incognito_mock_);

  // Allow cookies in regular mode should also allow in incognito but enforced
  // through regular mode.
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kDisabledForSite,
                              CookieControlsEnforcement::kNoEnforcement, 0));
  EXPECT_CALL(
      incognito_mock_,
      OnStatusChanged(CookieControlsStatus::kDisabledForSite,
                      CookieControlsEnforcement::kEnforcedByCookieSetting, 0));
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  incognito_cookie_controls.Update(incognito_web_contents.get());
  testing::Mock::VerifyAndClearExpectations(mock());
  testing::Mock::VerifyAndClearExpectations(&incognito_mock_);
}

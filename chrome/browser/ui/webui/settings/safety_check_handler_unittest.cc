// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/safety_check_handler.h"

#include "base/bind.h"
#include "base/optional.h"
#include "chrome/browser/ui/webui/help/test_version_updater.h"
#include "chrome/browser/ui/webui/settings/safety_check_handler_observer.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestSafetyCheckObserver : public SafetyCheckHandlerObserver {
 public:
  void OnUpdateCheckStart() override { update_check_started_ = true; }

  void OnUpdateCheckResult(VersionUpdater::Status status,
                           int progress,
                           bool rollback,
                           const std::string& version,
                           int64_t update_size,
                           const base::string16& message) override {
    update_check_status_ = status;
  }

  void OnSafeBrowsingCheckStart() override {
    safe_browsing_check_started_ = true;
  }

  void OnSafeBrowsingCheckResult(
      SafetyCheckHandler::SafeBrowsingStatus status) override {
    safe_browsing_check_status_ = status;
  }

  bool update_check_started_ = false;
  base::Optional<VersionUpdater::Status> update_check_status_;
  bool safe_browsing_check_started_ = false;
  base::Optional<SafetyCheckHandler::SafeBrowsingStatus>
      safe_browsing_check_status_;
};

class TestingSafetyCheckHandler : public SafetyCheckHandler {
 public:
  using SafetyCheckHandler::set_web_ui;

  TestingSafetyCheckHandler(std::unique_ptr<VersionUpdater> version_updater,
                            SafetyCheckHandlerObserver* observer)
      : SafetyCheckHandler(std::move(version_updater), observer) {}
};

class SafetyCheckHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // The unique pointer to a TestVersionUpdater gets moved to
    // SafetyCheckHandler, but a raw pointer is retained here to change its
    // state.
    auto version_updater = std::make_unique<TestVersionUpdater>();
    version_updater_ = version_updater.get();
    test_web_ui_.set_web_contents(web_contents());
    safety_check_ = std::make_unique<TestingSafetyCheckHandler>(
        std::move(version_updater), &observer_);
    safety_check_->set_web_ui(&test_web_ui_);
  }

 protected:
  TestVersionUpdater* version_updater_;
  TestSafetyCheckObserver observer_;
  content::TestWebUI test_web_ui_;
  std::unique_ptr<TestingSafetyCheckHandler> safety_check_;
};

TEST_F(SafetyCheckHandlerTest, PerformSafetyCheck_AllChecksInvoked) {
  safety_check_->PerformSafetyCheck();
  EXPECT_TRUE(observer_.update_check_started_);
  EXPECT_TRUE(observer_.safe_browsing_check_started_);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_Updated) {
  version_updater_->SetReturnedStatus(VersionUpdater::Status::UPDATED);
  safety_check_->PerformSafetyCheck();
  ASSERT_TRUE(observer_.update_check_status_.has_value());
  EXPECT_EQ(VersionUpdater::Status::UPDATED, observer_.update_check_status_);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_NotUpdated) {
  version_updater_->SetReturnedStatus(VersionUpdater::Status::DISABLED);
  safety_check_->PerformSafetyCheck();
  ASSERT_TRUE(observer_.update_check_status_.has_value());
  EXPECT_EQ(VersionUpdater::Status::DISABLED, observer_.update_check_status_);
}

TEST_F(SafetyCheckHandlerTest, CheckSafeBrowsing_Enabled) {
  Profile::FromWebUI(&test_web_ui_)
      ->GetPrefs()
      ->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  safety_check_->PerformSafetyCheck();
  ASSERT_TRUE(observer_.safe_browsing_check_status_.has_value());
  EXPECT_EQ(SafetyCheckHandler::SafeBrowsingStatus::ENABLED,
            observer_.safe_browsing_check_status_);
}

TEST_F(SafetyCheckHandlerTest, CheckSafeBrowsing_Disabled) {
  Profile::FromWebUI(&test_web_ui_)
      ->GetPrefs()
      ->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  safety_check_->PerformSafetyCheck();
  ASSERT_TRUE(observer_.safe_browsing_check_status_.has_value());
  EXPECT_EQ(SafetyCheckHandler::SafeBrowsingStatus::DISABLED,
            observer_.safe_browsing_check_status_);
}

TEST_F(SafetyCheckHandlerTest, CheckSafeBrowsing_DisabledByAdmin) {
  TestingProfile::FromWebUI(&test_web_ui_)
      ->AsTestingProfile()
      ->GetTestingPrefService()
      ->SetManagedPref(prefs::kSafeBrowsingEnabled,
                       std::make_unique<base::Value>(false));
  safety_check_->PerformSafetyCheck();
  ASSERT_TRUE(observer_.safe_browsing_check_status_.has_value());
  EXPECT_EQ(SafetyCheckHandler::SafeBrowsingStatus::DISABLED_BY_ADMIN,
            observer_.safe_browsing_check_status_);
}

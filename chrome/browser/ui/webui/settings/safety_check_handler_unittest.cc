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

  void OnUpdateCheckResult(SafetyCheckHandler::UpdateStatus status) override {
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
  base::Optional<SafetyCheckHandler::UpdateStatus> update_check_status_;
  bool safe_browsing_check_started_ = false;
  base::Optional<SafetyCheckHandler::SafeBrowsingStatus>
      safe_browsing_check_status_;
};

class TestingSafetyCheckHandler : public SafetyCheckHandler {
 public:
  using SafetyCheckHandler::AllowJavascript;
  using SafetyCheckHandler::set_web_ui;

  TestingSafetyCheckHandler(std::unique_ptr<VersionUpdater> version_updater,
                            SafetyCheckHandlerObserver* observer)
      : SafetyCheckHandler(std::move(version_updater), observer) {}
};

class SafetyCheckHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override;

  // Returns whether one of the WebUI callbacks for safety check status changes
  // has the specified |safetyCheckComponent| and |newState|.
  bool HasSafetyCheckStatusChangedWithData(int component, int new_state);

 protected:
  TestVersionUpdater* version_updater_ = nullptr;
  TestSafetyCheckObserver observer_;
  content::TestWebUI test_web_ui_;
  std::unique_ptr<TestingSafetyCheckHandler> safety_check_;
};

void SafetyCheckHandlerTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  // The unique pointer to a TestVersionUpdater gets moved to
  // SafetyCheckHandler, but a raw pointer is retained here to change its
  // state.
  auto version_updater = std::make_unique<TestVersionUpdater>();
  version_updater_ = version_updater.get();
  test_web_ui_.set_web_contents(web_contents());
  safety_check_ = std::make_unique<TestingSafetyCheckHandler>(
      std::move(version_updater), &observer_);
  test_web_ui_.ClearTrackedCalls();
  safety_check_->set_web_ui(&test_web_ui_);
  safety_check_->AllowJavascript();
}

bool SafetyCheckHandlerTest::HasSafetyCheckStatusChangedWithData(
    int component,
    int new_state) {
  for (const auto& it : test_web_ui_.call_data()) {
    const content::TestWebUI::CallData& data = *it;
    if (data.function_name() != "cr.webUIListenerCallback") {
      continue;
    }
    std::string event;
    if ((!data.arg1()->GetAsString(&event)) ||
        event != "safety-check-status-changed") {
      continue;
    }
    const base::DictionaryValue* dictionary = nullptr;
    if (!data.arg2()->GetAsDictionary(&dictionary)) {
      continue;
    }
    int cur_component, cur_new_state;
    if (dictionary->GetInteger("safetyCheckComponent", &cur_component) &&
        cur_component == component &&
        dictionary->GetInteger("newState", &cur_new_state) &&
        cur_new_state == new_state) {
      return true;
    }
  }
  return false;
}

TEST_F(SafetyCheckHandlerTest, PerformSafetyCheck_AllChecksInvoked) {
  safety_check_->PerformSafetyCheck();
  EXPECT_TRUE(observer_.update_check_started_);
  EXPECT_TRUE(observer_.safe_browsing_check_started_);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_Updated) {
  version_updater_->SetReturnedStatus(VersionUpdater::Status::UPDATED);
  safety_check_->PerformSafetyCheck();
  ASSERT_TRUE(observer_.update_check_status_.has_value());
  EXPECT_EQ(SafetyCheckHandler::UpdateStatus::kUpdated,
            observer_.update_check_status_);
  EXPECT_TRUE(HasSafetyCheckStatusChangedWithData(
      static_cast<int>(SafetyCheckHandler::SafetyCheckComponent::kUpdates),
      static_cast<int>(SafetyCheckHandler::UpdateStatus::kUpdated)));
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_NotUpdated) {
  version_updater_->SetReturnedStatus(
      VersionUpdater::Status::DISABLED_BY_ADMIN);
  safety_check_->PerformSafetyCheck();
  ASSERT_TRUE(observer_.update_check_status_.has_value());
  EXPECT_EQ(SafetyCheckHandler::UpdateStatus::kDisabledByAdmin,
            observer_.update_check_status_);
  EXPECT_TRUE(HasSafetyCheckStatusChangedWithData(
      static_cast<int>(SafetyCheckHandler::SafetyCheckComponent::kUpdates),
      static_cast<int>(SafetyCheckHandler::UpdateStatus::kDisabledByAdmin)));
}

TEST_F(SafetyCheckHandlerTest, CheckSafeBrowsing_Enabled) {
  Profile::FromWebUI(&test_web_ui_)
      ->GetPrefs()
      ->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  safety_check_->PerformSafetyCheck();
  ASSERT_TRUE(observer_.safe_browsing_check_status_.has_value());
  EXPECT_EQ(SafetyCheckHandler::SafeBrowsingStatus::kEnabled,
            observer_.safe_browsing_check_status_);
  EXPECT_TRUE(HasSafetyCheckStatusChangedWithData(
      static_cast<int>(SafetyCheckHandler::SafetyCheckComponent::kSafeBrowsing),
      static_cast<int>(SafetyCheckHandler::SafeBrowsingStatus::kEnabled)));
}

TEST_F(SafetyCheckHandlerTest, CheckSafeBrowsing_Disabled) {
  Profile::FromWebUI(&test_web_ui_)
      ->GetPrefs()
      ->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  safety_check_->PerformSafetyCheck();
  ASSERT_TRUE(observer_.safe_browsing_check_status_.has_value());
  EXPECT_EQ(SafetyCheckHandler::SafeBrowsingStatus::kDisabled,
            observer_.safe_browsing_check_status_);
  EXPECT_TRUE(HasSafetyCheckStatusChangedWithData(
      static_cast<int>(SafetyCheckHandler::SafetyCheckComponent::kSafeBrowsing),
      static_cast<int>(SafetyCheckHandler::SafeBrowsingStatus::kDisabled)));
}

TEST_F(SafetyCheckHandlerTest, CheckSafeBrowsing_DisabledByAdmin) {
  TestingProfile::FromWebUI(&test_web_ui_)
      ->AsTestingProfile()
      ->GetTestingPrefService()
      ->SetManagedPref(prefs::kSafeBrowsingEnabled,
                       std::make_unique<base::Value>(false));
  safety_check_->PerformSafetyCheck();
  ASSERT_TRUE(observer_.safe_browsing_check_status_.has_value());
  EXPECT_EQ(SafetyCheckHandler::SafeBrowsingStatus::kDisabledByAdmin,
            observer_.safe_browsing_check_status_);
  EXPECT_TRUE(HasSafetyCheckStatusChangedWithData(
      static_cast<int>(SafetyCheckHandler::SafetyCheckComponent::kSafeBrowsing),
      static_cast<int>(
          SafetyCheckHandler::SafeBrowsingStatus::kDisabledByAdmin)));
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/safety_check_handler.h"

#include <string>

#include "base/bind.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/user_action_tester.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/help/test_version_updater.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/core/browser/bulk_leak_check_service.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_web_ui.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "ui/chromeos/devicetype_utils.h"
#endif

// Components for building event strings.
constexpr char kUpdates[] = "updates";
constexpr char kPasswords[] = "passwords";
constexpr char kSafeBrowsing[] = "safe-browsing";

class TestingSafetyCheckHandler : public SafetyCheckHandler {
 public:
  using SafetyCheckHandler::AllowJavascript;
  using SafetyCheckHandler::DisallowJavascript;
  using SafetyCheckHandler::set_web_ui;

  TestingSafetyCheckHandler(
      std::unique_ptr<VersionUpdater> version_updater,
      password_manager::BulkLeakCheckService* leak_service)
      : SafetyCheckHandler(std::move(version_updater), leak_service) {}
};

class SafetyCheckHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override;

  // Returns a |base::DictionaryValue| for safety check status update that
  // has the specified |component| and |new_state| if it exists; nullptr
  // otherwise.
  const base::DictionaryValue* GetSafetyCheckStatusChangedWithDataIfExists(
      const std::string& component,
      int new_state);

  void VerifyDisplayString(const base::DictionaryValue* event,
                           const base::string16& expected);
  void VerifyDisplayString(const base::DictionaryValue* event,
                           const std::string& expected);

 protected:
  TestVersionUpdater* version_updater_ = nullptr;
  std::unique_ptr<password_manager::BulkLeakCheckService> test_leak_service_;
  content::TestWebUI test_web_ui_;
  std::unique_ptr<TestingSafetyCheckHandler> safety_check_;

 private:
  // Replaces any instances of browser name (e.g. Google Chrome, Chromium, etc)
  // with "browser" to make sure tests work both on Chromium and Google Chrome.
  void ReplaceBrowserName(base::string16* s);
};

void SafetyCheckHandlerTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  // The unique pointer to a TestVersionUpdater gets moved to
  // SafetyCheckHandler, but a raw pointer is retained here to change its
  // state.
  auto version_updater = std::make_unique<TestVersionUpdater>();
  test_leak_service_ = std::make_unique<password_manager::BulkLeakCheckService>(
      nullptr, nullptr);
  version_updater_ = version_updater.get();
  test_web_ui_.set_web_contents(web_contents());
  safety_check_ = std::make_unique<TestingSafetyCheckHandler>(
      std::move(version_updater), test_leak_service_.get());
  test_web_ui_.ClearTrackedCalls();
  safety_check_->set_web_ui(&test_web_ui_);
  safety_check_->AllowJavascript();
}

const base::DictionaryValue*
SafetyCheckHandlerTest::GetSafetyCheckStatusChangedWithDataIfExists(
    const std::string& component,
    int new_state) {
  for (const auto& it : test_web_ui_.call_data()) {
    const content::TestWebUI::CallData& data = *it;
    if (data.function_name() != "cr.webUIListenerCallback") {
      continue;
    }
    std::string event;
    if ((!data.arg1()->GetAsString(&event)) ||
        event != "safety-check-" + component + "-status-changed") {
      continue;
    }
    const base::DictionaryValue* dictionary = nullptr;
    if (!data.arg2()->GetAsDictionary(&dictionary)) {
      continue;
    }
    int cur_new_state;
    if (dictionary->GetInteger("newState", &cur_new_state) &&
        cur_new_state == new_state) {
      return dictionary;
    }
  }
  return nullptr;
}

void SafetyCheckHandlerTest::VerifyDisplayString(
    const base::DictionaryValue* event,
    const base::string16& expected) {
  base::string16 display;
  ASSERT_TRUE(event->GetString("displayString", &display));
  ReplaceBrowserName(&display);
  // Need to also replace any instances of Chrome and Chromium in the expected
  // string due to an edge case on ChromeOS, where a device name is "Chrome",
  // which gets replaced in the display string.
  base::string16 expected_replaced = expected;
  ReplaceBrowserName(&expected_replaced);
  EXPECT_EQ(expected_replaced, display);
}

void SafetyCheckHandlerTest::VerifyDisplayString(
    const base::DictionaryValue* event,
    const std::string& expected) {
  VerifyDisplayString(event, base::ASCIIToUTF16(expected));
}

void SafetyCheckHandlerTest::ReplaceBrowserName(base::string16* s) {
  base::ReplaceSubstringsAfterOffset(s, 0, base::ASCIIToUTF16("Google Chrome"),
                                     base::ASCIIToUTF16("Browser"));
  base::ReplaceSubstringsAfterOffset(s, 0, base::ASCIIToUTF16("Chrome"),
                                     base::ASCIIToUTF16("Browser"));
  base::ReplaceSubstringsAfterOffset(s, 0, base::ASCIIToUTF16("Chromium"),
                                     base::ASCIIToUTF16("Browser"));
}

TEST_F(SafetyCheckHandlerTest, PerformSafetyCheck_MetricsRecorded) {
  base::UserActionTester user_action_tester;
  safety_check_->PerformSafetyCheck();
  EXPECT_EQ(1, user_action_tester.GetActionCount("SafetyCheck.Started"));
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_Checking) {
  version_updater_->SetReturnedStatus(VersionUpdater::Status::CHECKING);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kUpdates,
          static_cast<int>(SafetyCheckHandler::UpdateStatus::kChecking));
  ASSERT_TRUE(event);
  VerifyDisplayString(event, base::UTF8ToUTF16("Running…"));
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_Updated) {
  version_updater_->SetReturnedStatus(VersionUpdater::Status::UPDATED);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kUpdates,
          static_cast<int>(SafetyCheckHandler::UpdateStatus::kUpdated));
  ASSERT_TRUE(event);
#if defined(OS_CHROMEOS)
  base::string16 expected = base::ASCIIToUTF16("Your ") +
                            ui::GetChromeOSDeviceName() +
                            base::ASCIIToUTF16(" is up to date");
  VerifyDisplayString(event, expected);
#else
  VerifyDisplayString(event, "Browser is up to date");
#endif
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_Updating) {
  version_updater_->SetReturnedStatus(VersionUpdater::Status::UPDATING);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kUpdates,
          static_cast<int>(SafetyCheckHandler::UpdateStatus::kUpdating));
  ASSERT_TRUE(event);
#if defined(OS_CHROMEOS)
  VerifyDisplayString(event, "Updating your device");
#else
  VerifyDisplayString(event, "Updating Browser");
#endif
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_Relaunch) {
  version_updater_->SetReturnedStatus(VersionUpdater::Status::NEARLY_UPDATED);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kUpdates,
          static_cast<int>(SafetyCheckHandler::UpdateStatus::kRelaunch));
  ASSERT_TRUE(event);
#if defined(OS_CHROMEOS)
  VerifyDisplayString(
      event, "Nearly up to date! Restart your device to finish updating.");
#else
  VerifyDisplayString(event,
                      "Nearly up to date! Relaunch Browser to finish "
                      "updating. Incognito windows won't reopen.");
#endif
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_DisabledByAdmin) {
  version_updater_->SetReturnedStatus(
      VersionUpdater::Status::DISABLED_BY_ADMIN);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kUpdates,
          static_cast<int>(SafetyCheckHandler::UpdateStatus::kDisabledByAdmin));
  ASSERT_TRUE(event);
  VerifyDisplayString(
      event,
      "Updates are managed by <a target=\"_blank\" "
      "href=\"https://support.google.com/accounts/answer/6208960\">your "
      "administrator</a>");
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_FailedOffline) {
  version_updater_->SetReturnedStatus(VersionUpdater::Status::FAILED_OFFLINE);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kUpdates,
          static_cast<int>(SafetyCheckHandler::UpdateStatus::kFailedOffline));
  ASSERT_TRUE(event);
  VerifyDisplayString(event,
                      "Browser can't check for updates. Try checking your "
                      "internet connection.");
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_Failed) {
  version_updater_->SetReturnedStatus(VersionUpdater::Status::FAILED);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kUpdates,
          static_cast<int>(SafetyCheckHandler::UpdateStatus::kFailed));
  ASSERT_TRUE(event);
  VerifyDisplayString(
      event,
      "Browser didn't update, something went wrong. <a target=\"_blank\" "
      "href=\"https://support.google.com/chrome/answer/111996\">Fix Browser "
      "update problems and failed updates.</a>");
}

TEST_F(SafetyCheckHandlerTest, CheckSafeBrowsing_Enabled) {
  Profile::FromWebUI(&test_web_ui_)
      ->GetPrefs()
      ->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kSafeBrowsing,
          static_cast<int>(SafetyCheckHandler::SafeBrowsingStatus::kEnabled));
  ASSERT_TRUE(event);
  VerifyDisplayString(event,
                      "Safe Browsing is up to date and protecting you from "
                      "harmful sites and downloads");
}

TEST_F(SafetyCheckHandlerTest, CheckSafeBrowsing_Disabled) {
  Profile::FromWebUI(&test_web_ui_)
      ->GetPrefs()
      ->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kSafeBrowsing,
          static_cast<int>(SafetyCheckHandler::SafeBrowsingStatus::kDisabled));
  ASSERT_TRUE(event);
  VerifyDisplayString(
      event, "Safe Browsing is off. To stay safe on the web, turn it on.");
}

TEST_F(SafetyCheckHandlerTest, CheckSafeBrowsing_DisabledByAdmin) {
  TestingProfile::FromWebUI(&test_web_ui_)
      ->AsTestingProfile()
      ->GetTestingPrefService()
      ->SetManagedPref(prefs::kSafeBrowsingEnabled,
                       std::make_unique<base::Value>(false));
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kSafeBrowsing,
          static_cast<int>(
              SafetyCheckHandler::SafeBrowsingStatus::kDisabledByAdmin));
  ASSERT_TRUE(event);
  VerifyDisplayString(
      event,
      "<a target=\"_blank\" "
      "href=\"https://support.google.com/accounts/answer/6208960\">Your "
      "administrator</a> has turned off Safe Browsing");
}

TEST_F(SafetyCheckHandlerTest, CheckSafeBrowsing_DisabledByExtension) {
  TestingProfile::FromWebUI(&test_web_ui_)
      ->AsTestingProfile()
      ->GetTestingPrefService()
      ->SetExtensionPref(prefs::kSafeBrowsingEnabled,
                         std::make_unique<base::Value>(false));
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kSafeBrowsing,
          static_cast<int>(
              SafetyCheckHandler::SafeBrowsingStatus::kDisabledByExtension));
  ASSERT_TRUE(event);
  VerifyDisplayString(event, "An extension has turned off Safe Browsing");
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_ObserverRemovedAfterError) {
  safety_check_->PerformSafetyCheck();
  // First, a "running" change of state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking));
  ASSERT_TRUE(event);
  VerifyDisplayString(event, base::UTF8ToUTF16("Running…"));
  // Second, an "offline" state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kNetworkError);
  const base::DictionaryValue* event2 =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kOffline));
  ASSERT_TRUE(event2);
  VerifyDisplayString(event2,
                      "Browser can't check your passwords. Try checking your "
                      "internet connection.");
  // Another error, but since the previous state is terminal, the handler should
  // no longer be observing the BulkLeakCheckService state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kServiceError);
  const base::DictionaryValue* event3 =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kOffline));
  ASSERT_TRUE(event3);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_InterruptedAndRefreshed) {
  safety_check_->PerformSafetyCheck();
  // Password check running.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking));
  ASSERT_TRUE(event);
  VerifyDisplayString(event, base::UTF8ToUTF16("Running…"));
  // The check gets interrupted and the page is refreshed.
  safety_check_->DisallowJavascript();
  safety_check_->AllowJavascript();
  // Another run of the safety check.
  safety_check_->PerformSafetyCheck();
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  const base::DictionaryValue* event2 =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking));
  ASSERT_TRUE(event2);
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kSignedOut);
  const base::DictionaryValue* event3 =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kSignedOut));
  ASSERT_TRUE(event3);
  VerifyDisplayString(
      event3,
      "Browser can't check your passwords because you're not signed in");
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_StartedTwice) {
  safety_check_->PerformSafetyCheck();
  safety_check_->PerformSafetyCheck();
  // First, a "running" change of state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking));
  ASSERT_TRUE(event);
  // Second, an "offline" state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kServiceError);
  const base::DictionaryValue* event2 =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kError));
  ASSERT_TRUE(event2);
  VerifyDisplayString(event2,
                      "Browser can't check your passwords. Try again later.");
}

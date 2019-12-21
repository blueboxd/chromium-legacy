// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/assistant_test_mixin.h"

#include "ash/assistant/model/ui/assistant_card_element.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/main_stage/assistant_ui_element_view.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/test/assistant_test_api.h"
#include "ash/public/mojom/assistant_state_controller.mojom-shared.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/test/embedded_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/login/auth/user_context.h"
#include "components/account_id/account_id.h"
#include "components/language/core/browser/pref_names.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/events/test/event_generator.h"

namespace chromeos {
namespace assistant {

namespace {

constexpr const char kTestUser[] = "test_user@gmail.com";
constexpr const char kTestUserGaiaId[] = "test_user@gaia.id";

LoginManagerMixin::TestUserInfo GetTestUserInfo() {
  return LoginManagerMixin::TestUserInfo(
      AccountId::FromUserEmailGaiaId(kTestUser, kTestUserGaiaId));
}

bool Equals(const char* left, const char* right) {
  return strcmp(left, right) == 0;
}

// Run the loop until the timeout expires, or until it is quit through other
// mechanisms.
// Returns whether the loop finished successfully, i.e. |false| if the timeout
// expired.
bool RunWithTimeout(base::RunLoop* run_loop, base::TimeDelta timeout) {
  bool success = true;

  base::RunLoop::ScopedRunTimeoutForTest scoped_timeout(
      timeout, base::BindLambdaForTesting([&success, run_loop] {
        success = false;
        run_loop->QuitClosure();
      }));
  run_loop->Run();

  return success;
}

// Waiter that blocks in the |Wait| method until a given |mojom::AssistantState|
// is reached, or until a timeout is hit.
// On timeout this will abort the test with a useful error message.
class AssistantStatusWaiter : private ash::AssistantStateObserver {
 public:
  AssistantStatusWaiter(ash::AssistantState* state,
                        ash::mojom::AssistantState expected_status)
      : state_(state), expected_status_(expected_status) {
    state_->AddObserver(this);
  }

  ~AssistantStatusWaiter() override { state_->RemoveObserver(this); }

  void Wait(base::TimeDelta wait_timeout) {
    if (state_->assistant_state() == expected_status_)
      return;

    // Wait until we're ready or we hit the timeout.
    run_loop_ = std::make_unique<base::RunLoop>();
    bool success = RunWithTimeout(run_loop_.get(), wait_timeout);
    run_loop_.reset();

    EXPECT_TRUE(success)
        << "Timeout waiting for AssistantStatus |" << expected_status_ << "|. "
        << "Current status is |" << state_->assistant_state() << "|. "
        << "One possible cause is that you're using an expired access token.";
  }

 private:
  void OnAssistantStatusChanged(ash::mojom::AssistantState status) override {
    if (status == expected_status_ && run_loop_ != nullptr)
      run_loop_->Quit();
  }

  ash::AssistantState* const state_;
  ash::mojom::AssistantState const expected_status_;

  std::unique_ptr<base::RunLoop> run_loop_;
};

// Base class that observes all new responses being displayed under the
// |parent_view|, and searches for any of the given |expected_responses|, or
// until a timeout is hit. On timeout this will abort the test with a useful
// error message.
// The derived classes must implement the logic to extract the response from a
// given view.
class ResponseWaiter : private views::ViewObserver {
 public:
  ResponseWaiter(views::View* parent_view,
                 const std::vector<std::string>& expected_responses)
      : parent_view_(parent_view), expected_responses_(expected_responses) {
    parent_view_->AddObserver(this);
  }
  ~ResponseWaiter() override {
    if (parent_view_)
      parent_view_->RemoveObserver(this);
  }

  void Wait(base::TimeDelta wait_timeout) {
    if (HasExpectedResponse())
      return;

    // Wait until we're ready or we hit the timeout.
    run_loop_ = std::make_unique<base::RunLoop>();
    bool success = RunWithTimeout(run_loop_.get(), wait_timeout);
    run_loop_.reset();

    EXPECT_TRUE(success) << "Timeout waiting for Assistant response.\n"
                         << "Expected any of " << FormatExpectedResponses()
                         << ".\n"
                         << "Got \"" << GetResponseText() << "\"";
  }

 private:
  // views::ViewObserver overrides:
  void OnViewHierarchyChanged(
      views::View* observed_view,
      const views::ViewHierarchyChangedDetails& details) override {
    if (run_loop_ && HasExpectedResponse())
      run_loop_->Quit();
  }

  void OnViewIsDeleting(views::View* observed_view) override {
    DCHECK(observed_view == parent_view_);

    if (run_loop_) {
      FAIL() << parent_view_->GetClassName() << " is deleted "
             << "before receiving the Assistant response.\n"
             << "Expected any of " << FormatExpectedResponses() << ".\n"
             << "Got \"" << GetResponseText() << "\"";
      run_loop_->Quit();
    }

    parent_view_ = nullptr;
  }

  bool HasExpectedResponse() const {
    std::string response = GetResponseText();

    for (const std::string& expected : expected_responses_) {
      if (response.find(expected) != std::string::npos)
        return true;
    }
    return false;
  }

  std::string GetResponseText() const {
    return GetResponseTextRecursive(parent_view_);
  }

  std::string GetResponseTextRecursive(views::View* view) const {
    base::Optional<std::string> response_maybe = GetResponseTextOfView(view);
    if (response_maybe) {
      return response_maybe.value() + "\n";
    } else {
      std::string result;
      for (views::View* child : view->children())
        result += GetResponseTextRecursive(child);
      return result;
    }
  }

  virtual base::Optional<std::string> GetResponseTextOfView(
      views::View* view) const = 0;

  std::string FormatExpectedResponses() const {
    std::string result = "{\n";
    for (const std::string& expected : expected_responses_)
      result += "    \"" + expected + "\",\n";
    result += "}";
    return result;
  }

  views::View* parent_view_;
  std::vector<std::string> expected_responses_;

  std::unique_ptr<base::RunLoop> run_loop_;
};

class TextResponseWaiter : public ResponseWaiter {
 public:
  using ResponseWaiter::ResponseWaiter;

 private:
  base::Optional<std::string> GetResponseTextOfView(
      views::View* view) const override {
    if (Equals(view->GetClassName(), "AssistantTextElementView")) {
      auto* text_view = static_cast<ash::AssistantUiElementView*>(view);
      return text_view->ToStringForTesting();
    }

    return base::nullopt;
  }
};

class CardResponseWaiter : public ResponseWaiter {
 public:
  using ResponseWaiter::ResponseWaiter;

  base::Optional<std::string> GetResponseTextOfView(
      views::View* view) const override {
    if (Equals(view->GetClassName(), "AssistantCardElementView")) {
      auto* card_view = static_cast<ash::AssistantUiElementView*>(view);
      return card_view->ToStringForTesting();
    }

    return base::nullopt;
  }
};

}  // namespace

// Test mixin for the browser tests that logs in the given user and issues
// refresh and access tokens for this user.
class LoggedInUserMixin : public InProcessBrowserTestMixin {
 public:
  LoggedInUserMixin(InProcessBrowserTestMixinHost* host,
                    InProcessBrowserTest* test_base,
                    LoginManagerMixin::TestUserInfo user,
                    net::EmbeddedTestServer* embedded_test_server)
      : InProcessBrowserTestMixin(host),
        login_manager_(host, {user}),
        test_server_(host, embedded_test_server),
        fake_gaia_(host, embedded_test_server),
        user_(user),
        test_base_(test_base),
        user_context_(LoginManagerMixin::CreateDefaultUserContext(user)) {
    // Tell LoginManagerMixin to launch the browser when the user is logged in.
    login_manager_.set_should_launch_browser(true);
  }

  ~LoggedInUserMixin() override = default;

  void SetAccessToken(std::string token) { access_token_ = token; }

  void SetUpOnMainThread() override {
    // By default, browser tests block anything that doesn't go to localhost, so
    // account.google.com requests would never reach fake GAIA server without
    // this.
    test_base_->host_resolver()->AddRule("*", "127.0.0.1");

    LogIn();
    SetupFakeGaia();

    // Ensure test_base_->browser() returns the browser of the logged in user
    // session.
    test_base_->SelectFirstBrowser();
  }

  void LogIn() {
    user_context_.SetRefreshToken(kRefreshToken);
    bool success = login_manager_.LoginAndWaitForActiveSession(user_context_);
    EXPECT_TRUE(success) << "Failed to log in as test user.";
  }

  void SetupFakeGaia() {
    FakeGaia::AccessTokenInfo token_info;
    token_info.token = access_token_;
    token_info.audience = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
    token_info.email = user_context_.GetAccountId().GetUserEmail();
    token_info.any_scope = true;
    token_info.expires_in = kAccessTokenExpiration;

    fake_gaia_.fake_gaia()->MapEmailToGaiaId(user_.account_id.GetUserEmail(),
                                             user_.account_id.GetGaiaId());
    fake_gaia_.fake_gaia()->IssueOAuthToken(kRefreshToken, token_info);
  }

 private:
  const char* kRefreshToken = FakeGaiaMixin::kFakeRefreshToken;
  const int kAccessTokenExpiration = FakeGaiaMixin::kFakeAccessTokenExpiration;

  LoginManagerMixin login_manager_;
  EmbeddedTestServerSetupMixin test_server_;
  FakeGaiaMixin fake_gaia_;

  LoginManagerMixin::TestUserInfo user_;
  InProcessBrowserTest* const test_base_;
  UserContext user_context_;
  std::string access_token_{FakeGaiaMixin::kFakeAllScopeAccessToken};
};

AssistantTestMixin::AssistantTestMixin(
    InProcessBrowserTestMixinHost* host,
    InProcessBrowserTest* test_base,
    net::EmbeddedTestServer* embedded_test_server,
    FakeS3Mode mode)
    : InProcessBrowserTestMixin(host),
      fake_s3_server_(),
      mode_(mode),
      test_api_(ash::AssistantTestApi::Create()),
      user_mixin_(std::make_unique<LoggedInUserMixin>(host,
                                                      test_base,
                                                      GetTestUserInfo(),
                                                      embedded_test_server)) {
  scoped_feature_list_.InitAndEnableFeature(
      app_list_features::kEnableAssistantLauncherUI);
}

AssistantTestMixin::~AssistantTestMixin() = default;

void AssistantTestMixin::SetUpCommandLine(base::CommandLine* command_line) {
  // Prevent the Assistant setup flow dialog from popping up immediately on user
  // start - otherwise the Assistant can not be started.
  command_line->AppendSwitch(switches::kOobeSkipPostLogin);
}

void AssistantTestMixin::SetUpOnMainThread() {
  fake_s3_server_.Setup(mode_);
  user_mixin_->SetAccessToken(fake_s3_server_.GetAccessToken());
  test_api_->DisableAnimations();
}

void AssistantTestMixin::TearDownOnMainThread() {
  DisableAssistant();
  fake_s3_server_.Teardown();
}

void AssistantTestMixin::StartAssistantAndWaitForReady(
    base::TimeDelta wait_timeout) {
  // Note: You might be tempted to call this function from SetUpOnMainThread(),
  // but that will not work as the Assistant service can not start until
  // |BrowserTestBase| calls InitializeNetworkProcess(), which it only does
  // after SetUpOnMainThread() finishes.

  test_api_->SetAssistantEnabled(true);
  SetPreferVoice(false);

  AssistantStatusWaiter waiter(test_api_->GetAssistantState(),
                               ash::mojom::AssistantState::NEW_READY);
  waiter.Wait(wait_timeout);

  // With the warmer welcome enabled the Assistant service will start an
  // interaction that will never complete (as our tests finish too soon).
  // This in turn causes the FakeS3Server to not remember this interaction when
  // running in |kRecord| mode, which then causes interaction failures in
  // |kReplay| mode, potentially leading to a deadlock (see b/144872676).
  DisableWarmerWelcome();
}

void AssistantTestMixin::SetPreferVoice(bool prefer_voice) {
  test_api_->SetPreferVoice(prefer_voice);
}

void AssistantTestMixin::SendTextQuery(const std::string& query) {
  test_api_->SendTextQuery(query);
}

void AssistantTestMixin::ExpectCardResponse(
    const std::string& expected_response,
    base::TimeDelta wait_timeout) {
  CardResponseWaiter waiter(test_api_->ui_element_container(),
                            {expected_response});
  waiter.Wait(wait_timeout);
}

void AssistantTestMixin::ExpectTextResponse(
    const std::string& expected_response,
    base::TimeDelta wait_timeout) {
  ExpectAnyOfTheseTextResponses({expected_response}, wait_timeout);
}

void AssistantTestMixin::ExpectAnyOfTheseTextResponses(
    const std::vector<std::string>& expected_responses,
    base::TimeDelta wait_timeout) {
  TextResponseWaiter waiter(test_api_->ui_element_container(),
                            expected_responses);
  waiter.Wait(wait_timeout);
}

void AssistantTestMixin::PressAssistantKey() {
  SendKeyPress(ui::VKEY_ASSISTANT);
}

bool AssistantTestMixin::IsVisible() {
  return test_api_->IsVisible();
}

PrefService* AssistantTestMixin::GetUserPreferences() {
  return ProfileManager::GetPrimaryUserProfile()->GetPrefs();
}

void AssistantTestMixin::SendKeyPress(ui::KeyboardCode key) {
  ui::test::EventGenerator event_generator(test_api_->root_window());
  event_generator.PressKey(key, /*flags=*/ui::EF_NONE);
}

void AssistantTestMixin::DisableAssistant() {
  // First disable Assistant in the settings.
  test_api_->SetAssistantEnabled(false);

  // Then wait for the Service to shutdown.
  AssistantStatusWaiter waiter(test_api_->GetAssistantState(),
                               ash::mojom::AssistantState::NOT_READY);
  waiter.Wait(kDefaultWaitTimeout);
}

void AssistantTestMixin::DisableWarmerWelcome() {
  // To disable the warmer welcome, we spoof that it has already been
  // triggered too many times.
  GetUserPreferences()->SetInteger(
      ash::prefs::kAssistantNumWarmerWelcomeTriggered,
      ash::assistant::ui::kWarmerWelcomesMaxTimesTriggered);
}

}  // namespace assistant
}  // namespace chromeos

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/supervision_onboarding_screen.h"

#include <initializer_list>
#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/mixin_based_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/test/embedded_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/local_policy_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/user_policy_mixin.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/supervision/onboarding_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/supervision_onboarding_screen_handler.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace chromeos {

namespace {

chromeos::OobeUI* GetOobeUI() {
  auto* host = chromeos::LoginDisplayHost::default_host();
  return host ? host->GetOobeUI() : nullptr;
}

}  // namespace

// Fake HTTP server that returns the data necessary to render the Supervision
// Onboarding pages. It provides methods to customize the HTTP responses to
// include/omit custom HTTP headers that are expected by the flow.
class FakeSupervisionServer {
 public:
  explicit FakeSupervisionServer(net::EmbeddedTestServer* test_server) {
    test_server_ = test_server;

    test_server_->RegisterRequestHandler(base::BindRepeating(
        &FakeSupervisionServer::HandleRequest, base::Unretained(this)));
  }

  ~FakeSupervisionServer() = default;

  // Sets the custom HTTP header that will be sent back in responses.
  void set_custom_http_header_value(
      const std::string& custom_http_header_value) {
    custom_http_header_value_ = custom_http_header_value;
  }

  // Stops sending the custom header in responses.
  void clear_custom_http_header_value() {
    custom_http_header_value_ = base::nullopt;
  }

  const std::string& last_request_url() { return last_request_url_; }

  size_t GetReceivedRequestsCount() const {
    // It's safe to use the size of the access token list as a proxy to the
    // number of requests. This server asserts that all requests contain an
    // authentication header.
    return received_auth_header_values_.size();
  }

 private:
  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    // We are not interested in other URLs hitting the server at this point.
    // This will filter bogus requests like favicon fetches and stop us from
    // handling requests that are targeting gaia.
    if (!base::StartsWith(request.relative_url, "/kids/deviceonboarding",
                          base::CompareCase::INSENSITIVE_ASCII)) {
      return nullptr;
    }

    UpdateVerificationData(request);
    auto response = std::make_unique<BasicHttpResponse>();
    if (custom_http_header_value_.has_value()) {
      response->AddCustomHeader(supervision::kExperimentHeaderName,
                                custom_http_header_value_.value());
    }

    response->set_code(net::HTTP_OK);
    response->set_content("Test Supervision Onboarding content");
    response->set_content_type("text/plain");
    return std::move(response);
  }

  void UpdateVerificationData(const HttpRequest& request) {
    auto auth_header =
        request.headers.find(net::HttpRequestHeaders::kAuthorization);
    ASSERT_NE(auth_header, request.headers.end());
    ASSERT_EQ(auth_header->second,
              base::StringPrintf("Bearer %s",
                                 FakeGaiaMixin::kFakeAllScopeAccessToken));

    received_auth_header_values_.push_back(auth_header->second);
    last_request_url_ = request.relative_url;
  }

  net::EmbeddedTestServer* test_server_;
  std::vector<std::string> received_auth_header_values_;
  std::string last_request_url_;

  base::Optional<std::string> custom_http_header_value_ = base::nullopt;
};

class SupervisionOnboardingBaseTest : public MixinBasedInProcessBrowserTest {
 public:
  SupervisionOnboardingBaseTest() = default;
  ~SupervisionOnboardingBaseTest() override = default;

  virtual bool IsFeatureOn() const = 0;
  virtual bool IsChild() const = 0;

  void SetUp() override {
    if (IsFeatureOn()) {
      feature_list_.InitWithFeatures(
          {features::kSupervisionOnboardingEligibility,
           features::kSupervisionOnboardingScreens},
          {});
    }

    MixinBasedInProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (IsFeatureOn()) {
      command_line->AppendSwitchASCII(
          switches::kSupervisionOnboardingUrlPrefix,
          embedded_test_server()->base_url().spec());

      // To turn on the feature properly we also ask the server to return the
      // expected custom http header value. Tests that want to simulate other
      // server responses can call these methods again to override this
      // behavior.
      supervision_server()->set_custom_http_header_value(
          supervision::kDeviceOnboardingExperimentName);
    }

    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    AccountId id =
        IsChild() ? child_user_.account_id : regular_user_.account_id;
    fake_gaia_.SetupFakeGaiaForLogin(id.GetUserEmail(), id.GetGaiaId(),
                                     FakeGaiaMixin::kFakeRefreshToken);

    LoginManagerMixin::TestUserInfo user_info =
        IsChild() ? child_user_ : regular_user_;
    UserContext user_context =
        LoginManagerMixin::CreateDefaultUserContext(user_info);
    user_context.SetRefreshToken(FakeGaiaMixin::kFakeRefreshToken);
    login_manager_.LoginAndWaitForActiveSession(user_context);

    ShowLoginWizard(OobeScreen::SCREEN_TEST_NO_WINDOW);
    WizardController::default_controller()
        ->screen_manager()
        ->DeleteScreenForTesting(SupervisionOnboardingScreenView::kScreenId);
    auto supervision_onboarding_screen =
        std::make_unique<SupervisionOnboardingScreen>(
            GetOobeUI()->GetView<SupervisionOnboardingScreenHandler>(),
            base::BindRepeating(
                &SupervisionOnboardingBaseTest::HandleScreenExit,
                base::Unretained(this)));
    supervision_onboarding_screen_ = supervision_onboarding_screen.get();
    WizardController::default_controller()
        ->screen_manager()
        ->SetScreenForTesting(std::move(supervision_onboarding_screen));

    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

  void ShowScreen() { supervision_onboarding_screen_->Show(); }

  void WaitForPageWithUrl(const std::string& requested_url) {
    // Wait for the request...
    while (supervision_server()->last_request_url() != requested_url) {
      base::RunLoop run_loop;
      // Avoid RunLoop::RunUntilIdle() because this is in a loop and
      // could end up being a busy loop when there are no pending tasks.
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, run_loop.QuitClosure(),
          base::TimeDelta::FromMilliseconds(100));
      run_loop.Run();
    }

    // Now wait for the UI to be updated with the response.
    test::OobeJS()
        .CreateVisibilityWaiter(
            true, {"supervision-onboarding", "supervision-onboarding-content"})
        ->Wait();
  }

  void ClickButton(const std::string& button_id) {
    std::initializer_list<base::StringPiece> button_path = {
        "supervision-onboarding", button_id};
    test::OobeJS().CreateVisibilityWaiter(true, button_path)->Wait();
    test::OobeJS().TapOnPath(button_path);
  }

  void WaitForScreenExit() {
    if (screen_exited_)
      return;

    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // Shows the screen and navigates to the start page.
  // This will also expect that we will make the correct requests to load the
  // start page.
  void NavigateToStartPage() {
    ShowScreen();
    OobeScreenWaiter screen_waiter(SupervisionOnboardingScreenView::kScreenId);
    screen_waiter.set_assert_next_screen();
    screen_waiter.Wait();
    WaitForPageWithUrl(supervision::kOnboardingStartPageRelativeUrl);

    EXPECT_EQ(1u, supervision_server()->GetReceivedRequestsCount());
  }

  // Navigates to the details page by first going through the start page.
  void NavigateToDetailsPage() {
    NavigateToStartPage();

    ClickButton("supervision-onboarding-next-button");
    WaitForPageWithUrl(supervision::kOnboardingDetailsPageRelativeUrl);

    EXPECT_EQ(2u, supervision_server()->GetReceivedRequestsCount());
  }

  // Navigates to the "All Set!" page by going through the Start and Details
  // pages.
  void NavigateToAllSetPage() {
    NavigateToDetailsPage();

    ClickButton("supervision-onboarding-next-button");
    WaitForPageWithUrl(supervision::kOnboardingAllSetPageRelativeUrl);

    EXPECT_EQ(3u, supervision_server()->GetReceivedRequestsCount());
  }

  FakeSupervisionServer* supervision_server() { return &supervision_server_; }

  SupervisionOnboardingScreen* supervision_onboarding_screen_;

 private:
  void HandleScreenExit(SupervisionOnboardingScreen::Result result) {
    ASSERT_FALSE(screen_exited_);
    screen_exited_ = true;
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  base::test::ScopedFeatureList feature_list_;
  bool screen_exited_ = false;
  base::OnceClosure screen_exit_callback_;

  const LoginManagerMixin::TestUserInfo regular_user_{
      AccountId::FromUserEmailGaiaId("test-regular-user@gmail.com",
                                     "test-regular-user-gaia-id")};
  const LoginManagerMixin::TestUserInfo child_user_{
      AccountId::FromUserEmailGaiaId("test-child-user@gmail.com",
                                     "test-child-user-gaia-id"),
      user_manager::USER_TYPE_CHILD};

  EmbeddedTestServerSetupMixin embedded_test_server_{&mixin_host_,
                                                     embedded_test_server()};
  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};
  LoginManagerMixin login_manager_{&mixin_host_, {regular_user_, child_user_}};
  LocalPolicyTestServerMixin local_policy_mixin_{&mixin_host_};
  UserPolicyMixin user_policy_{&mixin_host_, child_user_.account_id,
                               &local_policy_mixin_};

  FakeSupervisionServer supervision_server_{embedded_test_server()};
};

class SupervisionOnboardingRegularUserTest
    : public SupervisionOnboardingBaseTest {
 public:
  SupervisionOnboardingRegularUserTest() = default;
  ~SupervisionOnboardingRegularUserTest() override = default;

  bool IsFeatureOn() const override { return true; }

  bool IsChild() const override { return false; }
};

IN_PROC_BROWSER_TEST_F(SupervisionOnboardingRegularUserTest,
                       FlowExitsImmediately) {
  ShowScreen();

  WaitForScreenExit();
  EXPECT_EQ(0u, supervision_server()->GetReceivedRequestsCount());
}

class SupervisionOnboardingFeatureTurnedOffTest
    : public SupervisionOnboardingBaseTest {
 public:
  SupervisionOnboardingFeatureTurnedOffTest() = default;
  ~SupervisionOnboardingFeatureTurnedOffTest() override = default;

  bool IsFeatureOn() const override { return false; }

  bool IsChild() const override { return true; }
};

IN_PROC_BROWSER_TEST_F(SupervisionOnboardingFeatureTurnedOffTest,
                       FlowExitsImmediately) {
  ShowScreen();

  WaitForScreenExit();
  EXPECT_EQ(0u, supervision_server()->GetReceivedRequestsCount());
}

class SupervisionOnboardingTest : public SupervisionOnboardingBaseTest {
 public:
  SupervisionOnboardingTest() = default;
  ~SupervisionOnboardingTest() override = default;

  bool IsFeatureOn() const override { return true; }

  bool IsChild() const override { return true; }
};

IN_PROC_BROWSER_TEST_F(SupervisionOnboardingTest,
                       ExitWhenServerDoesNotReturnHeader) {
  supervision_server()->clear_custom_http_header_value();

  ShowScreen();
  WaitForScreenExit();

  EXPECT_EQ(1u, supervision_server()->GetReceivedRequestsCount());
}

IN_PROC_BROWSER_TEST_F(SupervisionOnboardingTest,
                       ExitWhenServerSendsWrongHeader) {
  supervision_server()->set_custom_http_header_value("wrong_header_value");

  ShowScreen();
  WaitForScreenExit();

  EXPECT_EQ(1u, supervision_server()->GetReceivedRequestsCount());
}

IN_PROC_BROWSER_TEST_F(SupervisionOnboardingTest,
                       NavigateToStartPageAndSkipFlow) {
  NavigateToStartPage();

  ClickButton("supervision-onboarding-skip-button");
  WaitForScreenExit();
}

// TODO(crbug.com/971696) Re-enable this test when the setup time is within
// acceptable ranges.
IN_PROC_BROWSER_TEST_F(SupervisionOnboardingTest,
                       DISABLED_NavigateToDetailsPageAndBack) {
  NavigateToDetailsPage();

  ClickButton("supervision-onboarding-back-button");
  WaitForPageWithUrl(supervision::kOnboardingStartPageRelativeUrl);

  ClickButton("supervision-onboarding-skip-button");
  WaitForScreenExit();
}

// TODO(crbug.com/971696) Re-enable this test when the setup time is within
// acceptable ranges.
IN_PROC_BROWSER_TEST_F(SupervisionOnboardingTest,
                       DISABLED_NavigateToAllSetPageAndBack) {
  NavigateToAllSetPage();

  ClickButton("supervision-onboarding-back-button");
  WaitForPageWithUrl(supervision::kOnboardingDetailsPageRelativeUrl);

  ClickButton("supervision-onboarding-back-button");
  WaitForPageWithUrl(supervision::kOnboardingStartPageRelativeUrl);

  ClickButton("supervision-onboarding-skip-button");
  WaitForScreenExit();
}

// TODO(crbug.com/971696) Re-enable this test when the setup time is within
// acceptable ranges.
IN_PROC_BROWSER_TEST_F(SupervisionOnboardingTest,
                       DISABLED_NavigateToAllSetPageAndFinishFlow) {
  NavigateToAllSetPage();

  ClickButton("supervision-onboarding-next-button");
  WaitForScreenExit();
}

}  // namespace chromeos

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/hash/sha1.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/session/arc_service_launcher.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/arc_terms_of_service_screen.h"
#include "chrome/browser/chromeos/login/test/embedded_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/webview_content_extractor.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/consent_auditor/consent_auditor_test_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/arc_terms_of_service_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/arc/arc_prefs.h"
#include "components/consent_auditor/fake_consent_auditor.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/base/l10n/l10n_util.h"

using consent_auditor::FakeConsentAuditor;
using sync_pb::UserConsentTypes;
using ArcPlayTermsOfServiceConsent =
    sync_pb::UserConsentTypes::ArcPlayTermsOfServiceConsent;
using ArcBackupAndRestoreConsent =
    sync_pb::UserConsentTypes::ArcBackupAndRestoreConsent;
using ArcGoogleLocationServiceConsent =
    sync_pb::UserConsentTypes::ArcGoogleLocationServiceConsent;
using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace chromeos {

namespace {

constexpr char kTosPath[] = "/about/play-terms.html";
constexpr char kTosContent[] = "Arc TOS for test.";

constexpr char kPrivacyPolicyPath[] = "/policies/privacy/";
constexpr char kPrivacyPolicyContent[] = "Arc Privarcy Policy for test.";

ArcPlayTermsOfServiceConsent BuildArcPlayTermsOfServiceConsent(bool accepted) {
  ArcPlayTermsOfServiceConsent play_consent;
  play_consent.set_status(accepted ? sync_pb::UserConsentTypes::GIVEN
                                   : UserConsentTypes::NOT_GIVEN);
  play_consent.set_confirmation_grd_id(IDS_ARC_OOBE_TERMS_BUTTON_ACCEPT);
  play_consent.set_consent_flow(ArcPlayTermsOfServiceConsent::SETUP);
  play_consent.set_play_terms_of_service_text_length(strlen(kTosContent));
  play_consent.set_play_terms_of_service_hash(
      base::SHA1HashString(kTosContent));
  return play_consent;
}

ArcBackupAndRestoreConsent BuildArcBackupAndRestoreConsent(bool accepted) {
  ArcBackupAndRestoreConsent backup_and_restore_consent;
  backup_and_restore_consent.set_confirmation_grd_id(
      IDS_ARC_OOBE_TERMS_BUTTON_ACCEPT);
  backup_and_restore_consent.add_description_grd_ids(
      IDS_ARC_OPT_IN_DIALOG_BACKUP_RESTORE);
  backup_and_restore_consent.set_status(accepted ? UserConsentTypes::GIVEN
                                                 : UserConsentTypes::NOT_GIVEN);
  return backup_and_restore_consent;
}

ArcGoogleLocationServiceConsent BuildArcGoogleLocationServiceConsent(
    bool accepted) {
  ArcGoogleLocationServiceConsent location_service_consent;
  location_service_consent.set_confirmation_grd_id(
      IDS_ARC_OOBE_TERMS_BUTTON_ACCEPT);
  location_service_consent.add_description_grd_ids(
      IDS_ARC_OPT_IN_LOCATION_SETTING);
  location_service_consent.set_status(accepted ? UserConsentTypes::GIVEN
                                               : UserConsentTypes::NOT_GIVEN);
  return location_service_consent;
}

// Helper class that waits for a single 'contentload' event from the
// specified webview.
// This class can only wait for a single load event per object lifetime.
class WebViewLoadWaiter {
 public:
  explicit WebViewLoadWaiter(
      std::initializer_list<base::StringPiece> element_ids)
      : expected_message_(base::GenerateGUID()) {
    std::string element_id = test::GetOobeElementPath(element_ids);
    std::string js = base::StringPrintf(
        R"(
          (function() {
            var policy_webview = %s;
            var f = function() {
              policy_webview.removeEventListener('contentload', f);
              window.domAutomationController.send('%s');
            };
            policy_webview.addEventListener('contentload', f);
          })()
        )",
        element_id.c_str(), expected_message_.c_str());
    test::OobeJS().Evaluate(js);
  }

  ~WebViewLoadWaiter() = default;

  void Wait() {
    std::string message;
    do {
      ASSERT_TRUE(message_queue_.WaitForMessage(&message));
    } while (message !=
             base::StringPrintf("\"%s\"", expected_message_.c_str()));
  }

 private:
  std::string expected_message_;
  content::DOMMessageQueue message_queue_;

  DISALLOW_COPY_AND_ASSIGN(WebViewLoadWaiter);
};

}  // namespace

class ArcTermsOfServiceScreenTest : public OobeBaseTest {
 public:
  ArcTermsOfServiceScreenTest() {
    // To reuse existing wizard controller in the flow.
    feature_list_.InitAndEnableFeature(
        chromeos::features::kOobeScreensPriority);
  }
  ~ArcTermsOfServiceScreenTest() override = default;

  void RegisterAdditionalRequestHandlers() override {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &ArcTermsOfServiceScreenTest::HandleRequest, base::Unretained(this)));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kArcAvailability,
                                    "officially-supported");
    command_line->AppendSwitch(chromeos::switches::kDisableEncryptionMigration);
    command_line->AppendSwitchASCII(switches::kArcTosHostForTests,
                                    TestServerBaseUrl());
    OobeBaseTest::SetUpCommandLine(command_line);
  }

  void TriggerArcTosScreen() {
    ArcTermsOfServiceScreen* terms_of_service_screen =
        static_cast<ArcTermsOfServiceScreen*>(
            WizardController::default_controller()->screen_manager()->GetScreen(
                ArcTermsOfServiceScreenView::kScreenId));
    terms_of_service_screen->set_exit_callback_for_testing(
        base::BindRepeating(&ArcTermsOfServiceScreenTest::ScreenExitCallback,
                            base::Unretained(this)));
    // Enable ARC for testing.
    arc::ArcServiceLauncher::Get()->ResetForTesting();
    login_manager_mixin_.LoginAsNewReguarUser();
    OobeScreenExitWaiter(GaiaView::kScreenId).Wait();
    LoginDisplayHost::default_host()->StartWizard(
        ArcTermsOfServiceScreenView::kScreenId);
  }

 protected:
  // When enabled serves terms of service with a footer that contains a link
  // to the privacy policy.
  void set_serve_tos_with_privacy_policy_footer(bool serve_with_footer) {
    serve_tos_with_privacy_policy_footer_ = serve_with_footer;
  }

  void set_on_screen_exit_called(base::OnceClosure on_screen_exit_called) {
    on_screen_exit_called_ = std::move(on_screen_exit_called);
  }

  const base::Optional<ArcTermsOfServiceScreen::Result>& screen_exit_result()
      const {
    return screen_exit_result_;
  }

  void WaitForTermsOfServiceWebViewToLoad() {
    test::OobeJS()
        .CreateHasClassWaiter(true, "arc-tos-loaded",
                              {"arc-tos-root", "arcTosDialog"})
        ->Wait();
  }

  void WaitForScreenExitResult() {
    if (screen_exit_result_.has_value())
      return;

    base::RunLoop run_loop;
    on_screen_exit_called_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  void ScreenExitCallback(ArcTermsOfServiceScreen::Result result) {
    ASSERT_FALSE(screen_exit_result_.has_value());
    screen_exit_result_ = result;
    std::move(on_screen_exit_called_).Run();
  }

  // Returns the base URL of the embedded test server.
  // The string will have the format "http://127.0.0.1:${PORT_NUMBER}" where
  // PORT_NUMBER is a randomly assigned port number.
  std::string TestServerBaseUrl() {
    return base::TrimString(
               embedded_test_server()->base_url().GetOrigin().spec(), "/",
               base::TrimPositions::TRIM_TRAILING)
        .as_string();
  }

  // Handles both Terms of Service and Privacy policy requests.
  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    if (!(request.relative_url == kTosPath ||
          request.relative_url == kPrivacyPolicyPath)) {
      return std::unique_ptr<HttpResponse>();
    }

    if (request.relative_url == kPrivacyPolicyPath)
      return BuildHttpResponse(kPrivacyPolicyContent);

    // The terms of service screen determines the URL of the privacy policy
    // by scanning the terms of service http response. It looks for an <a> tag
    // with with href that matches '/policies/privacy/' that is also a child of
    // an element with class 'play-footer'.
    std::string content = kTosContent;
    if (serve_tos_with_privacy_policy_footer_) {
      std::string href = TestServerBaseUrl() + "/policies/privacy/";
      std::string footer = base::StringPrintf(
          "<div class='play-footer'><a href='%s'>", href.c_str());
      content += footer;
    }
    return BuildHttpResponse(content);
  }

  // Returns a successful |BasicHttpResponse| with |content|.
  std::unique_ptr<BasicHttpResponse> BuildHttpResponse(
      const std::string& content) {
    std::unique_ptr<BasicHttpResponse> http_response =
        std::make_unique<BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("text/html");
    http_response->set_content(content);
    return http_response;
  }

  bool serve_tos_with_privacy_policy_footer_ = false;

  base::Optional<ArcTermsOfServiceScreen::Result> screen_exit_result_;

  base::OnceClosure on_screen_exit_called_ = base::DoNothing();

  LoginManagerMixin login_manager_mixin_{&mixin_host_};

  base::test::ScopedFeatureList feature_list_;
  DISALLOW_COPY_AND_ASSIGN(ArcTermsOfServiceScreenTest);
};

// Tests that screen fetches the terms of service from the specified URL
// and the content is displayed as a <webview>.
IN_PROC_BROWSER_TEST_F(ArcTermsOfServiceScreenTest, TermsOfServiceContent) {
  TriggerArcTosScreen();
  ASSERT_NO_FATAL_FAILURE(WaitForTermsOfServiceWebViewToLoad());
  EXPECT_EQ(kTosContent,
            test::GetWebViewContents({"arc-tos-root", "arcTosView"}));

  EXPECT_FALSE(screen_exit_result().has_value());
}

// Tests that clicking on "More" button unhides some terms of service paragraphs
// of the screen.
IN_PROC_BROWSER_TEST_F(ArcTermsOfServiceScreenTest, ClickOnMore) {
  TriggerArcTosScreen();
  ASSERT_NO_FATAL_FAILURE(WaitForTermsOfServiceWebViewToLoad());
  // By default, these paragraphs should be hidden.
  test::OobeJS().ExpectHiddenPath({"arc-tos-root", "arcExtraContent"});
  test::OobeJS().ExpectHiddenPath({"arc-tos-root", "arcTosAcceptButton"});

  // Click on 'More' button.
  test::OobeJS().ClickOnPath({"arc-tos-root", "arcTosNextButton"});

  // Paragraphs should now be visible.
  test::OobeJS().ExpectHiddenPath({"arc-tos-root", "arcTosNextButton"});
  test::OobeJS().ExpectVisiblePath({"arc-tos-root", "arcExtraContent"});

  EXPECT_FALSE(screen_exit_result().has_value());
}

// Tests that all "learn more" links opens correct popup dialog.
IN_PROC_BROWSER_TEST_F(ArcTermsOfServiceScreenTest, LearnMoreDialogs) {
  TriggerArcTosScreen();
  ASSERT_NO_FATAL_FAILURE(WaitForTermsOfServiceWebViewToLoad());
  test::OobeJS().ClickOnPath({"arc-tos-root", "arcTosNextButton"});

  // List of pairs of {html element ids, html pop up dialog id}.
  std::vector<std::pair<std::string, std::string>> learn_more_links{
      {"learnMoreLinkMetrics", "arcMetricsPopup"},
      {"learnMoreLinkBackupRestore", "arcBackupRestorePopup"},
      {"learnMoreLinkLocationService", "arcLocationServicePopup"},
      {"learnMoreLinkPai", "arcPaiPopup"}};

  for (const auto& pair : learn_more_links) {
    std::string html_element_id;
    std::string popup_html_element_id;
    std::tie(html_element_id, popup_html_element_id) = pair;
    test::OobeJS().ExpectHasNoAttribute(
        "open", {"arc-tos-root", popup_html_element_id, "helpDialog"});
    test::OobeJS().ClickOnPath({"arc-tos-root", html_element_id});
    test::OobeJS().ExpectHasAttribute(
        "open", {"arc-tos-root", popup_html_element_id, "helpDialog"});
    test::OobeJS().ClickOnPath(
        {"arc-tos-root", popup_html_element_id, "closeButton"});
    test::OobeJS().ExpectHasNoAttribute(
        "open", {"arc-tos-root", popup_html_element_id, "helpDialog"});
  }
  EXPECT_FALSE(screen_exit_result().has_value());
}

// Test that checking the "review after signing" checkbox updates pref
// kShowArcSettingsOnSessionStart.
IN_PROC_BROWSER_TEST_F(ArcTermsOfServiceScreenTest, ReviewPlayOptions) {
  TriggerArcTosScreen();
  ASSERT_NO_FATAL_FAILURE(WaitForTermsOfServiceWebViewToLoad());

  Profile* profile = ProfileManager::GetActiveUserProfile();
  EXPECT_FALSE(
      profile->GetPrefs()->GetBoolean(prefs::kShowArcSettingsOnSessionStart));

  test::OobeJS().ClickOnPath({"arc-tos-root", "arcTosNextButton"});
  test::OobeJS().ClickOnPath({"arc-tos-root", "arcReviewSettingsCheckbox"});
  test::OobeJS().ClickOnPath({"arc-tos-root", "arcTosAcceptButton"});

  EXPECT_TRUE(
      profile->GetPrefs()->GetBoolean(prefs::kShowArcSettingsOnSessionStart));

  WaitForScreenExitResult();
  EXPECT_EQ(screen_exit_result(), ArcTermsOfServiceScreen::Result::ACCEPTED);
}

// Test whether google privacy policy can be loaded.
IN_PROC_BROWSER_TEST_F(ArcTermsOfServiceScreenTest, PrivacyPolicy) {
  // Privacy policy link is parsed from the footer of the TOS content response.
  set_serve_tos_with_privacy_policy_footer(true);
  TriggerArcTosScreen();
  ASSERT_NO_FATAL_FAILURE(WaitForTermsOfServiceWebViewToLoad());

  WebViewLoadWaiter waiter({"arc-tos-root", "arcTosOverlayWebview"});
  test::OobeJS().ClickOnPath({"arc-tos-root", "arcTosNextButton"});
  test::OobeJS().ClickOnPath({"arc-tos-root", "arcPolicyLink"});
  waiter.Wait();
  EXPECT_EQ(test::GetWebViewContents({"arc-tos-root", "arcTosOverlayWebview"}),
            kPrivacyPolicyContent);

  EXPECT_FALSE(screen_exit_result().has_value());
}

// There are two checkboxes for enabling/disabling arc backup restore and
// arc location service. This parameterized test executes all 4 combinations
// of enabled/disabled states and checks that advancing to the next screen by
// accepting.
class ParameterizedArcTermsOfServiceScreenTest
    : public ArcTermsOfServiceScreenTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  ParameterizedArcTermsOfServiceScreenTest() = default;
  ~ParameterizedArcTermsOfServiceScreenTest() = default;

  void SetUp() override {
    std::tie(accept_backup_restore_, accept_location_service_) = GetParam();
    ArcTermsOfServiceScreenTest::SetUp();
  }

  // Common routine that enables/disables checkboxes based on test parameters.
  // When |accept| is true, advances to next screen by clicking on the "Accept"
  // button.
  // |play_consent|, |backup_and_restore_consent| and |location_service_consent|
  // are the expected consents recordings.
  void AdvanceNextScreenWithExpectations(
      bool accept,
      ArcPlayTermsOfServiceConsent play_consent,
      ArcBackupAndRestoreConsent backup_and_restore_consent,
      ArcGoogleLocationServiceConsent location_service_consent) {
    ASSERT_NO_FATAL_FAILURE(WaitForTermsOfServiceWebViewToLoad());
    test::OobeJS().ClickOnPath({"arc-tos-root", "arcTosNextButton"});

    // Wait for checkboxes to become visible.
    test::OobeJS()
        .CreateVisibilityWaiter(true, {"arc-tos-root", "arcLocationService"})
        ->Wait();

    Profile* profile = ProfileManager::GetActiveUserProfile();
    FakeConsentAuditor* auditor = static_cast<FakeConsentAuditor*>(
        ConsentAuditorFactory::GetInstance()->SetTestingFactoryAndUse(
            profile, base::BindRepeating(&BuildFakeConsentAuditor)));

    if (!accept_backup_restore_)
      test::OobeJS().ClickOnPath({"arc-tos-root", "arcEnableBackupRestore"});

    if (!accept_location_service_) {
      test::OobeJS().ClickOnPath({"arc-tos-root", "arcEnableLocationService"});
    }

    EXPECT_CALL(*auditor, RecordArcPlayConsent(
                              testing::_,
                              consent_auditor::ArcPlayConsentEq(play_consent)));
    EXPECT_CALL(*auditor,
                RecordArcBackupAndRestoreConsent(
                    testing::_, consent_auditor::ArcBackupAndRestoreConsentEq(
                                    backup_and_restore_consent)));
    EXPECT_CALL(
        *auditor,
        RecordArcGoogleLocationServiceConsent(
            testing::_, consent_auditor::ArcGoogleLocationServiceConsentEq(
                            location_service_consent)));

    if (accept)
      test::OobeJS().ClickOnPath({"arc-tos-root", "arcTosAcceptButton"});
  }

 protected:
  bool accept_backup_restore_;
  bool accept_location_service_;

  DISALLOW_COPY_AND_ASSIGN(ParameterizedArcTermsOfServiceScreenTest);
};

// Tests that clicking on "Accept" button records the expected consents.
// When TOS are accepted we should also record whether backup restores and
// location services are enabled.
IN_PROC_BROWSER_TEST_P(ParameterizedArcTermsOfServiceScreenTest, ClickAccept) {
  TriggerArcTosScreen();
  ASSERT_NO_FATAL_FAILURE(WaitForTermsOfServiceWebViewToLoad());
  ArcPlayTermsOfServiceConsent play_consent =
      BuildArcPlayTermsOfServiceConsent(true);
  ArcBackupAndRestoreConsent backup_and_restore_consent =
      BuildArcBackupAndRestoreConsent(accept_backup_restore_);
  ArcGoogleLocationServiceConsent location_service_consent =
      BuildArcGoogleLocationServiceConsent(accept_location_service_);

  AdvanceNextScreenWithExpectations(
      true, play_consent, backup_and_restore_consent, location_service_consent);

  WaitForScreenExitResult();
  EXPECT_EQ(screen_exit_result(), ArcTermsOfServiceScreen::Result::ACCEPTED);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ParameterizedArcTermsOfServiceScreenTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

}  // namespace chromeos

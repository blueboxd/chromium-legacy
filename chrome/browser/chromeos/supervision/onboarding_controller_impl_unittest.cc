// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/supervision/onboarding_controller_impl.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/ash_pref_names.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/supervision/mojom/onboarding_controller.mojom.h"
#include "chrome/browser/chromeos/supervision/onboarding_constants.h"
#include "chrome/browser/chromeos/supervision/onboarding_delegate.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/net_errors.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace supervision {
namespace {

const char kTestAccountId[] = "test-account-id";
const char kFakeAccessToken[] = "fake-access-token";

}  // namespace

class FakeOnboardingDelegate : public OnboardingDelegate {
 public:
  ~FakeOnboardingDelegate() override {}

  bool finished_onboarding() { return finished_; }

  bool skipped_onboarding() { return skipped_; }

 private:
  // OnboardingDelegate:
  void SkipOnboarding() override {
    ASSERT_FALSE(finished_);
    ASSERT_FALSE(skipped_);
    skipped_ = true;
  }

  void FinishOnboarding() override {
    ASSERT_FALSE(finished_);
    ASSERT_FALSE(skipped_);
    finished_ = true;
  }

  bool skipped_ = false;
  bool finished_ = false;
};

class FakeOnboardingWebviewHost : mojom::OnboardingWebviewHost {
 public:
  explicit FakeOnboardingWebviewHost(
      mojom::OnboardingWebviewHostRequest request)
      : binding_(this, std::move(request)) {}

  void ExpectPresentations(const std::vector<mojom::OnboardingPresentation>&
                               expected_presentations) {
    ASSERT_EQ(presentations_.size(), expected_presentations.size());

    for (std::size_t i = 0; i < expected_presentations.size(); ++i) {
      const auto& expected_presentation = expected_presentations[i];
      const auto& presentation = presentations_[i];
      EXPECT_TRUE(presentation->Equals(expected_presentation));

      // To yield more readable errors, we also test each property individually.
      EXPECT_EQ(expected_presentation.state, presentation->state);
      EXPECT_EQ(expected_presentation.can_show_next_page,
                presentation->can_show_next_page);
      EXPECT_EQ(expected_presentation.can_show_previous_page,
                presentation->can_show_previous_page);
      EXPECT_EQ(expected_presentation.can_skip_flow,
                presentation->can_skip_flow);
    }
  }

  // Flushes the internal mojo message pipe.
  void Flush() { binding_.FlushForTesting(); }

  void Reset() {
    page_loaded_ = base::nullopt;
    presentations_.clear();
  }

  const base::Optional<mojom::OnboardingPage>& page_loaded() {
    return page_loaded_;
  }

  void set_load_page_result(const mojom::OnboardingLoadPageResult& result) {
    load_page_result_ = result;
  }

 private:
  void SetPresentation(mojom::OnboardingPresentationPtr presentation) override {
    presentations_.push_back(std::move(presentation));
  }

  void LoadPage(mojom::OnboardingPagePtr page,
                LoadPageCallback callback) override {
    page_loaded_ = *page;

    std::move(callback).Run(load_page_result_.Clone());
  }

  mojo::Binding<mojom::OnboardingWebviewHost> binding_;

  mojom::OnboardingLoadPageResult load_page_result_{
      net::Error::OK, kDeviceOnboardingExperimentName};

  std::vector<mojom::OnboardingPresentationPtr> presentations_;
  base::Optional<mojom::OnboardingPage> page_loaded_;

  DISALLOW_COPY_AND_ASSIGN(FakeOnboardingWebviewHost);
};

class OnboardingControllerBaseTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment();

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
    identity_test_env()->MakeAccountAvailable(kTestAccountId);
    identity_test_env()->SetPrimaryAccount(kTestAccountId);

    // Setting up an extension service, because some observers need to load
    // extensions.
    extensions::TestExtensionSystem* extension_system(
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile())));
    extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(),
        /*autoupdate_enabled=*/false);

    delegate_ = std::make_unique<FakeOnboardingDelegate>();
    controller_impl_ =
        std::make_unique<OnboardingControllerImpl>(profile(), delegate());
    controller_impl_->BindRequest(mojo::MakeRequest(&controller_));
  }

  void BindWebviewHost() {
    mojom::OnboardingWebviewHostPtr webview_host_proxy;
    webview_host_ = std::make_unique<FakeOnboardingWebviewHost>(
        mojo::MakeRequest(&webview_host_proxy));

    controller_->BindWebviewHost(std::move(webview_host_proxy));
  }

  void SetUpPageLoad(const mojom::OnboardingLoadPageResult& result) {
    webview_host()->set_load_page_result(result);
    identity_test_env()
        ->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
            kFakeAccessToken,
            base::Time::Now() + base::TimeDelta::FromHours(1));

    Flush();
  }

  void BindHostAndSetupFailedAuth() {
    BindWebviewHost();

    identity_test_env()
        ->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
            GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));

    Flush();
  }

  void BindHostAndReturnLoadPageSuccess() {
    mojom::OnboardingLoadPageResult result;
    result.net_error = net::Error::OK;
    result.custom_header_value = kDeviceOnboardingExperimentName;

    BindWebviewHost();
    SetUpPageLoad(result);
  }

  void BindHostAndReturnLoadPageError() {
    mojom::OnboardingLoadPageResult result;
    result.net_error = net::Error::ERR_FAILED;
    result.custom_header_value = kDeviceOnboardingExperimentName;

    BindWebviewHost();
    SetUpPageLoad(result);
  }

  void BindHostAndReturnMissingCustomHeader() {
    mojom::OnboardingLoadPageResult result;
    result.net_error = net::Error::OK;
    result.custom_header_value = base::nullopt;

    BindWebviewHost();
    SetUpPageLoad(result);
  }

  void BindHostAndReturnWrongCustomHeader() {
    mojom::OnboardingLoadPageResult result;
    result.net_error = net::Error::OK;
    result.custom_header_value = "clearly-wrong-header-value";

    BindWebviewHost();
    SetUpPageLoad(result);
  }

  void HandleAction(mojom::OnboardingAction action) {
    controller_->HandleAction(action);
    Flush();
  }

  void Flush() {
    controller_.FlushForTesting();
    webview_host()->Flush();
  }

  Profile* profile() { return profile_.get(); }

  identity::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  FakeOnboardingDelegate* delegate() { return delegate_.get(); }

  FakeOnboardingWebviewHost* webview_host() { return webview_host_.get(); }

 private:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  std::unique_ptr<FakeOnboardingDelegate> delegate_;
  std::unique_ptr<OnboardingControllerImpl> controller_impl_;
  mojom::OnboardingControllerPtr controller_;
  std::unique_ptr<FakeOnboardingWebviewHost> webview_host_;
};

class OnboardingControllerFlowDisabledTest
    : public OnboardingControllerBaseTest {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndDisableFeature(
        features::kSupervisionOnboardingScreens);

    OnboardingControllerBaseTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(OnboardingControllerFlowDisabledTest, ExitFlowWhenFlowIsDisabled) {
  BindHostAndReturnLoadPageSuccess();

  EXPECT_TRUE(delegate()->skipped_onboarding());
}

TEST_F(OnboardingControllerFlowDisabledTest,
       PresentOnlyLoadingStateWhenFlowIsDisabled) {
  BindHostAndReturnLoadPageSuccess();

  mojom::OnboardingPresentation loading;
  loading.state = mojom::OnboardingPresentationState::kLoading;

  webview_host()->ExpectPresentations({loading});
}

TEST_F(OnboardingControllerFlowDisabledTest,
       SetEligibleForKioskNextWhenFlowIsDisabled) {
  BindHostAndReturnLoadPageSuccess();

  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(ash::prefs::kKioskNextShellEligible));
}

class OnboardingControllerTest : public OnboardingControllerBaseTest {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kSupervisionOnboardingScreens);

    OnboardingControllerBaseTest::SetUp();
  }

  // Navigates to the details page by first loading the Start page, then faking
  // a user pressing the "Next" button. It can optionally fake a failed page
  // load for the Details page.
  // Note: To make tests simpler we also reset the data from the
  // FakeWebviewHost, this way we can write tests that only focus on the page
  // being handled.
  void NavigateToDetailsPage(bool return_error = false) {
    BindHostAndReturnLoadPageSuccess();
    webview_host()->Reset();

    HandleAction(mojom::OnboardingAction::kShowNextPage);

    mojom::OnboardingLoadPageResult result;
    result.net_error = return_error ? net::Error::ERR_FAILED : net::Error::OK;
    SetUpPageLoad(result);
  }

  // Navigates to the "All Set!" page by first navigating to the Details page,
  // then faking a user pressing the "Next" button. It can optionally fake a
  // failed page load for the "All Set!" page.
  // Note: To make tests simpler we also reset the data from the
  // FakeWebviewHost, this way we can write tests that only focus on the page
  // being handled.
  void NavigateToAllSetPage(bool return_error = false) {
    NavigateToDetailsPage();
    webview_host()->Reset();

    HandleAction(mojom::OnboardingAction::kShowNextPage);
    mojom::OnboardingLoadPageResult result;
    result.net_error = return_error ? net::Error::ERR_FAILED : net::Error::OK;
    SetUpPageLoad(result);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(OnboardingControllerTest, RequestWebviewHostToLoadStartPageCorrectly) {
  BindHostAndReturnLoadPageSuccess();

  ASSERT_TRUE(webview_host()->page_loaded().has_value());
  EXPECT_EQ(webview_host()->page_loaded()->url,
            GURL("https://families.google.com/kids/deviceonboarding/start"));
  EXPECT_EQ(webview_host()->page_loaded()->access_token, kFakeAccessToken);
  EXPECT_EQ(webview_host()->page_loaded()->custom_header_name,
            kExperimentHeaderName);
  EXPECT_EQ(webview_host()->page_loaded()->allowed_urls_prefix,
            "https://families.google.com/");
}

TEST_F(OnboardingControllerTest, OverridePageUrlsByCommandLine) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kSupervisionOnboardingUrlPrefix,
      "https://example.com/");

  BindHostAndReturnLoadPageSuccess();

  ASSERT_TRUE(webview_host()->page_loaded().has_value());
  EXPECT_EQ(webview_host()->page_loaded()->url,
            GURL("https://example.com/kids/deviceonboarding/start"));
  EXPECT_EQ(webview_host()->page_loaded()->access_token, kFakeAccessToken);
  EXPECT_EQ(webview_host()->page_loaded()->custom_header_name,
            kExperimentHeaderName);
  EXPECT_EQ(webview_host()->page_loaded()->allowed_urls_prefix,
            "https://example.com/");
}

TEST_F(OnboardingControllerTest, StayInFlowWhenLoadSucceeds) {
  BindHostAndReturnLoadPageSuccess();

  EXPECT_FALSE(delegate()->skipped_onboarding());
  EXPECT_FALSE(delegate()->finished_onboarding());
}

TEST_F(OnboardingControllerTest, PresentReadyStateWhenLoadSucceeds) {
  BindHostAndReturnLoadPageSuccess();

  mojom::OnboardingPresentation loading;
  loading.state = mojom::OnboardingPresentationState::kLoading;

  mojom::OnboardingPresentation ready;
  ready.state = mojom::OnboardingPresentationState::kReady;
  ready.can_show_next_page = true;
  ready.can_skip_flow = true;

  webview_host()->ExpectPresentations({loading, ready});
}

TEST_F(OnboardingControllerTest, SetEligibleForKioskNextWhenLoadSucceeds) {
  BindHostAndReturnLoadPageSuccess();

  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(ash::prefs::kKioskNextShellEligible));
}

TEST_F(OnboardingControllerTest, StayInFlowOnAuthError) {
  BindHostAndSetupFailedAuth();

  EXPECT_FALSE(webview_host()->page_loaded().has_value());
  EXPECT_FALSE(delegate()->skipped_onboarding());
}

TEST_F(OnboardingControllerTest, PresentRetryStateOnAuthError) {
  BindHostAndSetupFailedAuth();

  mojom::OnboardingPresentation loading;
  loading.state = mojom::OnboardingPresentationState::kLoading;

  mojom::OnboardingPresentation retry;
  retry.state = mojom::OnboardingPresentationState::kPageLoadFailed;
  retry.can_retry_page_load = true;
  webview_host()->ExpectPresentations({loading, retry});
}

TEST_F(OnboardingControllerTest, SetNotEligibleForKioskNextOnAuthError) {
  BindHostAndSetupFailedAuth();

  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(ash::prefs::kKioskNextShellEligible));
}

TEST_F(OnboardingControllerTest, StayInFlowOnLoadPageError) {
  BindHostAndReturnLoadPageError();

  EXPECT_FALSE(delegate()->skipped_onboarding());
  EXPECT_FALSE(delegate()->finished_onboarding());
}

TEST_F(OnboardingControllerTest, PresentRetryStateOnLoadPageError) {
  BindHostAndReturnLoadPageError();

  mojom::OnboardingPresentation loading;
  loading.state = mojom::OnboardingPresentationState::kLoading;

  mojom::OnboardingPresentation retry;
  retry.state = mojom::OnboardingPresentationState::kPageLoadFailed;
  retry.can_retry_page_load = true;
  webview_host()->ExpectPresentations({loading, retry});
}

TEST_F(OnboardingControllerTest, PresentSkipButtonIfLoadFailsTooManyTimes) {
  BindHostAndReturnLoadPageError();

  mojom::OnboardingLoadPageResult result;
  result.net_error = net::Error::ERR_FAILED;
  HandleAction(mojom::OnboardingAction::kRetryPageLoad);
  SetUpPageLoad(result);
  HandleAction(mojom::OnboardingAction::kRetryPageLoad);
  SetUpPageLoad(result);

  mojom::OnboardingPresentation loading;
  loading.state = mojom::OnboardingPresentationState::kLoading;

  mojom::OnboardingPresentation retry;
  retry.state = mojom::OnboardingPresentationState::kPageLoadFailed;
  retry.can_retry_page_load = true;

  mojom::OnboardingPresentation retry_with_skip;
  retry_with_skip.state = mojom::OnboardingPresentationState::kPageLoadFailed;
  retry_with_skip.can_retry_page_load = true;
  retry_with_skip.can_skip_flow = true;

  webview_host()->ExpectPresentations({
      // First try.
      loading,
      retry,
      // Second try.
      loading,
      retry,
      // On the third try we now show the skip button.
      loading,
      retry_with_skip,
  });
}

TEST_F(OnboardingControllerTest, SetNotEligibleForKioskNextOnLoadPageError) {
  BindHostAndReturnLoadPageError();

  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(ash::prefs::kKioskNextShellEligible));
}

TEST_F(OnboardingControllerTest, ExitFlowWhenHeaderValueIsMissing) {
  BindHostAndReturnMissingCustomHeader();

  EXPECT_TRUE(delegate()->skipped_onboarding());
}

TEST_F(OnboardingControllerTest,
       PresentOnlyLoadingStateWhenHeaderValueIsMissing) {
  BindHostAndReturnMissingCustomHeader();

  mojom::OnboardingPresentation loading;
  loading.state = mojom::OnboardingPresentationState::kLoading;

  webview_host()->ExpectPresentations({loading});
}

TEST_F(OnboardingControllerTest,
       SetNotEligibleForKioskNextWhenHeaderValueIsMissing) {
  BindHostAndReturnMissingCustomHeader();

  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(ash::prefs::kKioskNextShellEligible));
}

TEST_F(OnboardingControllerTest, ExitFlowWhenHeaderValueIsWrong) {
  BindHostAndReturnWrongCustomHeader();

  EXPECT_TRUE(delegate()->skipped_onboarding());
}

TEST_F(OnboardingControllerTest,
       PresentOnlyLoadingStateWhenHeaderValueIsWrong) {
  BindHostAndReturnWrongCustomHeader();

  mojom::OnboardingPresentation loading;
  loading.state = mojom::OnboardingPresentationState::kLoading;

  webview_host()->ExpectPresentations({loading});
}

TEST_F(OnboardingControllerTest,
       SetNotEligibleForKioskNextWhenHeaderValueIsWrong) {
  BindHostAndReturnWrongCustomHeader();

  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(ash::prefs::kKioskNextShellEligible));
}

TEST_F(OnboardingControllerTest, ExitFlowWhenHandlingSkipAction) {
  BindHostAndReturnLoadPageSuccess();

  HandleAction(mojom::OnboardingAction::kSkipFlow);

  EXPECT_TRUE(delegate()->skipped_onboarding());
}

TEST_F(OnboardingControllerTest, StayInFlowWhenNavigatingToDetailsPage) {
  NavigateToDetailsPage();

  EXPECT_FALSE(delegate()->skipped_onboarding());
  EXPECT_FALSE(delegate()->finished_onboarding());
}

TEST_F(OnboardingControllerTest, DetailsPageStaysInFlowOnFailedPageLoad) {
  NavigateToDetailsPage(/*return_error=*/true);

  EXPECT_FALSE(delegate()->skipped_onboarding());
  EXPECT_FALSE(delegate()->finished_onboarding());
}

TEST_F(OnboardingControllerTest, DetailsPageIsPresentedCorrectly) {
  NavigateToDetailsPage();

  mojom::OnboardingPresentation loading;
  loading.state = mojom::OnboardingPresentationState::kLoading;

  mojom::OnboardingPresentation ready;
  ready.state = mojom::OnboardingPresentationState::kReady;
  ready.can_show_next_page = true;
  ready.can_show_previous_page = true;

  webview_host()->ExpectPresentations({loading, ready});
}

TEST_F(OnboardingControllerTest, DetailsPageIsLoadedCorrectly) {
  NavigateToDetailsPage();

  ASSERT_TRUE(webview_host()->page_loaded().has_value());
  EXPECT_EQ(webview_host()->page_loaded()->url,
            GURL("https://families.google.com/kids/deviceonboarding/details"));
  EXPECT_EQ(webview_host()->page_loaded()->access_token, kFakeAccessToken);
  EXPECT_EQ(webview_host()->page_loaded()->custom_header_name, base::nullopt);
  EXPECT_EQ(webview_host()->page_loaded()->allowed_urls_prefix,
            "https://families.google.com/");
}

TEST_F(OnboardingControllerTest, StayInFlowWhenNavigatingToAllSetPage) {
  NavigateToAllSetPage();

  EXPECT_FALSE(delegate()->skipped_onboarding());
  EXPECT_FALSE(delegate()->finished_onboarding());
}

TEST_F(OnboardingControllerTest, AllSetPageStaysInFlowOnFailedPageLoad) {
  NavigateToAllSetPage(/*return_error=*/true);

  EXPECT_FALSE(delegate()->skipped_onboarding());
  EXPECT_FALSE(delegate()->finished_onboarding());
}

TEST_F(OnboardingControllerTest, AllSetPageIsPresentedCorrectly) {
  NavigateToAllSetPage();

  mojom::OnboardingPresentation loading;
  loading.state = mojom::OnboardingPresentationState::kLoading;

  mojom::OnboardingPresentation ready;
  ready.state = mojom::OnboardingPresentationState::kReady;
  ready.can_show_next_page = true;
  ready.can_show_previous_page = true;

  webview_host()->ExpectPresentations({loading, ready});
}

TEST_F(OnboardingControllerTest, AllSetPageIsLoadedCorrectly) {
  NavigateToAllSetPage();

  ASSERT_TRUE(webview_host()->page_loaded().has_value());
  EXPECT_EQ(webview_host()->page_loaded()->url,
            GURL("https://families.google.com/kids/deviceonboarding/allset"));
  EXPECT_EQ(webview_host()->page_loaded()->access_token, kFakeAccessToken);
  EXPECT_EQ(webview_host()->page_loaded()->custom_header_name, base::nullopt);
  EXPECT_EQ(webview_host()->page_loaded()->allowed_urls_prefix,
            "https://families.google.com/");
}

TEST_F(OnboardingControllerTest, AllSetPageCanFinishFlow) {
  NavigateToAllSetPage();

  EXPECT_FALSE(delegate()->finished_onboarding());
  HandleAction(mojom::OnboardingAction::kShowNextPage);
  EXPECT_TRUE(delegate()->finished_onboarding());
}

TEST_F(OnboardingControllerTest, AllSetPageEnablesKioskNext) {
  NavigateToAllSetPage();

  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(ash::prefs::kKioskNextShellEnabled));
  HandleAction(mojom::OnboardingAction::kShowNextPage);

  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(ash::prefs::kKioskNextShellEnabled));
}

}  // namespace supervision
}  // namespace chromeos

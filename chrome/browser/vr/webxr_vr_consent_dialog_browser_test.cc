// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/vr/test/fake_xr_session_request_consent_manager.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"

namespace vr {

#if defined(OS_WIN)

// Browser test class exclusively to test the consent dialog that's shown when
// attempting to enter VR.
class WebXrVrConsentDialogBrowserTest : public WebXrVrBrowserTestBase {
 public:
  WebXrVrConsentDialogBrowserTest();

  // Must be called first thing in the test. This cannot be part of SetUp()
  // because the XRSessionRequestConsentManager::SetInstance() is not called
  // in ChromeBrowserMain code by that time.
  void SetupFakeConsentManager(
      FakeXRSessionRequestConsentManager::UserResponse user_response);

 private:
  std::unique_ptr<FakeXRSessionRequestConsentManager> fake_consent_manager_;
};

WebXrVrConsentDialogBrowserTest::WebXrVrConsentDialogBrowserTest() {
  enable_features_.push_back(features::kOpenVR);
  enable_features_.push_back(features::kWebXr);
#if BUILDFLAG(ENABLE_WINDOWS_MR)
  disable_features_.push_back(features::kWindowsMixedReality);
#endif
}

void WebXrVrConsentDialogBrowserTest::SetupFakeConsentManager(
    FakeXRSessionRequestConsentManager::UserResponse user_response) {
  fake_consent_manager_.reset(new FakeXRSessionRequestConsentManager(
      XRSessionRequestConsentManager::Instance(), user_response));
  XRSessionRequestConsentManager::SetInstanceForTesting(
      fake_consent_manager_.get());
}

IN_PROC_BROWSER_TEST_F(
    WebXrVrConsentDialogBrowserTest,
    TestWebXrVrSucceedsWhenUserClicksConsentDialogAllowButton) {
  SetupFakeConsentManager(
      FakeXRSessionRequestConsentManager::UserResponse::kClickAllowButton);

  LoadUrlAndAwaitInitialization(
      GetFileUrlForHtmlTestFile("generic_webxr_page"));

  // These two functions below are exactly the same as
  // WebXrVrBrowserTestBase::EnterSessionWithUserGesture, except that the base
  // class' implementation replaces the FakeXRSessionRequestConsentManager
  // set in the SetupFakeConsentManager call above, which should be avoided.
  RunJavaScriptOrFail("onRequestSession()", GetCurrentWebContents());
  PollJavaScriptBooleanOrFail(
      "sessionInfos[sessionTypes.IMMERSIVE].currentSession != null",
      kPollTimeoutLong, GetCurrentWebContents());

  RunJavaScriptOrFail(
      "sessionInfos[sessionTypes.IMMERSIVE].currentSession.end()",
      GetCurrentWebContents());
}

IN_PROC_BROWSER_TEST_F(
    WebXrVrConsentDialogBrowserTest,
    TestWebXrVrFailsWhenUserClicksConsentDialogCancelButton) {
  SetupFakeConsentManager(
      FakeXRSessionRequestConsentManager::UserResponse::kClickCancelButton);

  LoadUrlAndAwaitInitialization(GetFileUrlForHtmlTestFile(
      "webxr_test_presentation_promise_rejected_if_consent_not_granted"));
  ExecuteStepAndWait("onImmersiveRequestWithConsent()");
  EndTest();
}

IN_PROC_BROWSER_TEST_F(WebXrVrConsentDialogBrowserTest,
                       TestWebXrVrFailsWhenUserClosesConsentDialog) {
  SetupFakeConsentManager(
      FakeXRSessionRequestConsentManager::UserResponse::kCloseDialog);

  LoadUrlAndAwaitInitialization(GetFileUrlForHtmlTestFile(
      "webxr_test_presentation_promise_rejected_if_consent_not_granted"));
  ExecuteStepAndWait("onImmersiveRequestWithConsent()");
  EndTest();
}

#endif

}  // namespace vr

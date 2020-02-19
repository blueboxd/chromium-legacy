// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/vr/test/multi_class_browser_test.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"

namespace vr {

// Helper macro to wrap consent flow tests that should be run with the custom
// consent flow as well as the standard permissions prompt.
#define CONSENT_FLOW_TEST_F(test_name)                          \
  IN_PROC_MULTI_CLASS_BROWSER_TEST_F2(                          \
      WebXrVrConsentBrowserTest, WebXrVrPermissionsBrowserTest, \
      WebXrVrConsentBrowserTestBase, test_name)

// The consent flow isn't specific to any particular runtime; so it's okay to
// keep it specific to one runtime, especially while the additional setup steps
// are needed. Once we've consolidated this down to a single flow and don't need
// additional setup, ShownCount and SetupObservers could move to a higher level
// and tests could be run on all supported runtimes if desired.
class WebXrVrConsentBrowserTestBase : public WebXrVrWmrBrowserTestBase {
 public:
  WebXrVrConsentBrowserTestBase() {}
  ~WebXrVrConsentBrowserTestBase() override = default;
  virtual uint32_t ShownCount() = 0;
  virtual void SetupObservers() {}
};

class WebXrVrConsentBrowserTest : public WebXrVrConsentBrowserTestBase {
 public:
  WebXrVrConsentBrowserTest() {
    disable_features_.push_back(features::kWebXrPermissionsApi);
  }
  ~WebXrVrConsentBrowserTest() override = default;

  uint32_t ShownCount() override { return fake_consent_manager_->ShownCount(); }
};

class WebXrVrPermissionsBrowserTest
    : public WebXrVrConsentBrowserTestBase,
      public permissions::PermissionRequestManager::Observer {
 public:
  WebXrVrPermissionsBrowserTest() {
    enable_features_.push_back(features::kWebXrPermissionsApi);
  }
  ~WebXrVrPermissionsBrowserTest() override = default;

  uint32_t ShownCount() override { return shown_count_; }

 private:
  void OnBubbleAdded() override { shown_count_++; }

  void SetupObservers() override {
    GetPermissionRequestManager()->AddObserver(this);
  }

  uint32_t shown_count_ = 0u;
};

// Tests that WebXR sessions can be created when the "Allow" button is pressed.
CONSENT_FLOW_TEST_F(TestConsentAllowCreatesSession) {
  t->SetupFakeConsentManager(
      FakeXRSessionRequestConsentManager::UserResponse::kClickAllowButton);

  t->LoadFileAndAwaitInitialization("generic_webxr_page");
  t->SetupObservers();

  t->EnterSessionWithUserGestureOrFail();

  ASSERT_EQ(t->ShownCount(), 1u)
      << "Consent Dialog should have been shown once";
}

// Tests that a session is not created if the user explicitly clicks the
// cancel button.
CONSENT_FLOW_TEST_F(TestConsentCancelFailsSessionCreation) {
  t->SetupFakeConsentManager(
      FakeXRSessionRequestConsentManager::UserResponse::kClickCancelButton);

  t->LoadFileAndAwaitInitialization("test_webxr_consent");
  t->SetupObservers();
  t->EnterSessionWithUserGesture();
  t->PollJavaScriptBooleanOrFail(
      "sessionInfos[sessionTypes.IMMERSIVE].error != null",
      WebXrVrBrowserTestBase::kPollTimeoutMedium);
  t->RunJavaScriptOrFail("verifySessionConsentError(sessionTypes.IMMERSIVE)");
  t->AssertNoJavaScriptErrors();

  ASSERT_EQ(t->ShownCount(), 1u)
      << "Consent Dialog should have been shown once";
}

// Tests that a session is not created if the user explicitly closes the
// dialog.
CONSENT_FLOW_TEST_F(TestConsentCloseFailsSessionCreation) {
  t->SetupFakeConsentManager(
      FakeXRSessionRequestConsentManager::UserResponse::kCloseDialog);

  t->LoadFileAndAwaitInitialization("test_webxr_consent");
  t->SetupObservers();
  t->EnterSessionWithUserGesture();
  t->PollJavaScriptBooleanOrFail(
      "sessionInfos[sessionTypes.IMMERSIVE].error != null",
      WebXrVrBrowserTestBase::kPollTimeoutMedium);
  t->RunJavaScriptOrFail("verifySessionConsentError(sessionTypes.IMMERSIVE)");
  t->AssertNoJavaScriptErrors();

  ASSERT_EQ(t->ShownCount(), 1u)
      << "Consent Dialog should have been shown once";
}

// Tests that requesting a session with the same required level of consent
// without a page reload, only prompts once.
CONSENT_FLOW_TEST_F(TestConsentPersistsSameLevel) {
  t->SetupFakeConsentManager(
      FakeXRSessionRequestConsentManager::UserResponse::kClickAllowButton);

  t->LoadFileAndAwaitInitialization("generic_webxr_page");
  t->SetupObservers();

  t->EnterSessionWithUserGestureOrFail();
  t->EndSessionOrFail();

  // Since the consent from the earlier prompt should be persisted, requesting
  // an XR session a second time should not prompt the user, but should create
  // a valid session.
  t->EnterSessionWithUserGestureOrFail();

  // Validate that the consent prompt has only been shown once since the start
  // of this test.
  ASSERT_EQ(t->ShownCount(), 1u)
      << "Consent Dialog should have only been shown once";
}

// Verify that inline with no session parameters doesn't prompt for consent.
CONSENT_FLOW_TEST_F(TestConsentNotNeededForInline) {
  // This ensures that we have a fresh consent manager with a new count.
  t->SetupFakeConsentManager(
      FakeXRSessionRequestConsentManager::UserResponse::kClickAllowButton);

  t->LoadFileAndAwaitInitialization("test_webxr_consent");
  t->SetupObservers();
  t->RunJavaScriptOrFail("requestMagicWindowSession()");

  t->PollJavaScriptBooleanOrFail(
      "sessionInfos[sessionTypes.MAGIC_WINDOW].currentSession != null",
      WebXrVrBrowserTestBase::kPollTimeoutLong);

  // Validate that the consent prompt has not been shown since this test began.
  ASSERT_EQ(t->ShownCount(), 0u) << "Consent Dialog should not have been shown";
}

// Verify that if a higher level of consent is granted (e.g. bounded), that the
// lower level does not re-prompt for consent.
IN_PROC_BROWSER_TEST_F(WebXrVrConsentBrowserTest,
                       TestConsentPersistsLowerLevel) {
  // This ensures that we have a fresh consent manager with a new count.
  SetupFakeConsentManager(
      FakeXRSessionRequestConsentManager::UserResponse::kClickAllowButton);

  LoadFileAndAwaitInitialization("test_webxr_consent");
  SetupObservers();

  // Setup to ensure that we request a session that requires a high level of
  // consent.
  RunJavaScriptOrFail("setupImmersiveSessionToRequestBounded()");

  EnterSessionWithUserGestureOrFail();
  EndSessionOrFail();

  // Since the (higher) consent from the earlier prompt should be persisted,
  // requesting an XR session a second time, with a lower level of permissions
  // expected should not prompt the user, but should create a valid session.
  RunJavaScriptOrFail("setupImmersiveSessionToRequestHeight()");
  EnterSessionWithUserGestureOrFail();

  // Validate that the consent prompt has only been shown once since the start
  // of this test.
  ASSERT_EQ(ShownCount(), 1u)
      << "Consent Dialog should have only been shown once";
}

// Tests that if a higher level of consent than was previously granted is needed
// that the user is re-prompted.
IN_PROC_BROWSER_TEST_F(WebXrVrConsentBrowserTest,
                       TestConsentRepromptsHigherLevel) {
  // This ensures that we have a fresh consent manager with a new count.
  SetupFakeConsentManager(
      FakeXRSessionRequestConsentManager::UserResponse::kClickAllowButton);

  LoadFileAndAwaitInitialization("test_webxr_consent");
  SetupObservers();

  // First request an immersive session with a medium level of consent.
  RunJavaScriptOrFail("setupImmersiveSessionToRequestHeight()");
  EnterSessionWithUserGestureOrFail();
  EndSessionOrFail();

  // Now set up to request a session with a higher level of consent than
  // previously granted.  This should cause the prompt to appear again, and then
  // a new session be created.
  RunJavaScriptOrFail("setupImmersiveSessionToRequestBounded()");
  EnterSessionWithUserGestureOrFail();

  // Validate that both request sessions showed a consent prompt.
  ASSERT_EQ(ShownCount(), 2u) << "Consent Dialog should have been shown twice";
}

IN_PROC_BROWSER_TEST_F(WebXrVrConsentBrowserTest,
                       TestConsentRepromptsAfterReload) {
  SetupFakeConsentManager(
      FakeXRSessionRequestConsentManager::UserResponse::kClickAllowButton);

  LoadFileAndAwaitInitialization("generic_webxr_page");
  SetupObservers();

  EnterSessionWithUserGestureOrFail();
  EndSessionOrFail();

  LoadFileAndAwaitInitialization("generic_webxr_page");
  SetupObservers();

  EnterSessionWithUserGestureOrFail();

  // Validate that both request sessions showed a consent prompt.
  ASSERT_EQ(ShownCount(), 2u) << "Consent Dialog should have been shown twice";
}

}  // namespace vr

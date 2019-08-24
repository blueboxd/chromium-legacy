// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>

#include "chrome/browser/vr/test/webvr_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Invoke;

namespace vr {

bool WebVrBrowserTestBase::XrDeviceFound(content::WebContents* web_contents) {
  return RunJavaScriptAndExtractBoolOrFail("vrDisplay != null", web_contents);
}

void WebVrBrowserTestBase::EnterSessionWithUserGesture(
    content::WebContents* web_contents) {
  XRSessionRequestConsentManager::SetInstanceForTesting(&consent_manager_);
  EXPECT_CALL(consent_manager_, ShowDialogAndGetConsent(_, _, _)).Times(0);

  // ExecuteScript runs with a user gesture, so we can just directly call
  // requestPresent instead of having to do the hacky workaround the
  // instrumentation tests use of actually sending a click event to the canvas.
  RunJavaScriptOrFail("onVrRequestPresent()", web_contents);
}

void WebVrBrowserTestBase::EnterSessionWithUserGestureOrFail(
    content::WebContents* web_contents) {
  EnterSessionWithUserGesture(web_contents);
  PollJavaScriptBooleanOrFail("vrDisplay.isPresenting", kPollTimeoutLong,
                              web_contents);
}

void WebVrBrowserTestBase::EndSession(content::WebContents* web_contents) {
  XRSessionRequestConsentManager::SetInstanceForTesting(nullptr);
  RunJavaScriptOrFail("vrDisplay.exitPresent()", web_contents);
}

void WebVrBrowserTestBase::EndSessionOrFail(
    content::WebContents* web_contents) {
  EndSession(web_contents);
  PollJavaScriptBooleanOrFail("vrDisplay.isPresenting == false",
                              kPollTimeoutLong, web_contents);
}

}  // namespace vr

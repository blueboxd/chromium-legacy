// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>

#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Invoke;

namespace vr {

void WebXrVrBrowserTestBase::EnterSessionWithUserGesture(
    content::WebContents* web_contents) {
#if defined(OS_WIN)
  XRSessionRequestConsentManager::SetInstanceForTesting(&consent_manager_);
  ON_CALL(consent_manager_, ShowDialogAndGetConsent(_, _))
      .WillByDefault(Invoke(
          [](content::WebContents*, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(true);
            return nullptr;
          }));
#endif
  // ExecuteScript runs with a user gesture, so we can just directly call
  // requestSession instead of having to do the hacky workaround the
  // instrumentation tests use of actually sending a click event to the canvas.
  RunJavaScriptOrFail("onRequestSession()", web_contents);
}

void WebXrVrBrowserTestBase::EnterSessionWithUserGestureOrFail(
    content::WebContents* web_contents) {
  EnterSessionWithUserGesture(web_contents);
  PollJavaScriptBooleanOrFail(
      "sessionInfos[sessionTypes.IMMERSIVE].currentSession != null",
      kPollTimeoutLong, web_contents);

#if defined(OS_WIN)
  // For WMR, creating a session may take foreground from us, and Windows may
  // not return it when the session terminates. This means subsequent requests
  // to enter an immersive session may fail. The fix for testing is to call
  // SetForegroundWindow manually. In real code, we'll have foreground if there
  // was a user gesture to enter VR.
  SetForegroundWindow(hwnd_);
#endif
}

void WebXrVrBrowserTestBase::EndSession(content::WebContents* web_contents) {
#if defined(OS_WIN)
  XRSessionRequestConsentManager::SetInstanceForTesting(nullptr);
#endif
  RunJavaScriptOrFail(
      "sessionInfos[sessionTypes.IMMERSIVE].currentSession.end()",
      web_contents);
}

void WebXrVrBrowserTestBase::EndSessionOrFail(
    content::WebContents* web_contents) {
  EndSession(web_contents);
  PollJavaScriptBooleanOrFail(
      "sessionInfos[sessionTypes.IMMERSIVE].currentSession == null",
      kPollTimeoutLong, web_contents);
}

gfx::Vector3dF WebXrVrBrowserTestBase::GetControllerOffset() const {
  return gfx::Vector3dF();
}

#if defined(OS_WIN)
XrBrowserTestBase::RuntimeType WebXrVrOpenVrBrowserTestBase::GetRuntimeType()
    const {
  return XrBrowserTestBase::RuntimeType::RUNTIME_OPENVR;
}

gfx::Vector3dF WebXrVrOpenVrBrowserTestBase::GetControllerOffset() const {
  // The 0.08f comes from the slight adjustment we perform in
  // openvr_render_loop.cc to account for OpenVR reporting the controller
  // position at the tip, but WebXR using the position at the grip.
  return gfx::Vector3dF(0, 0, 0.08f);
}

WebXrVrWmrBrowserTestBase::WebXrVrWmrBrowserTestBase() {}

WebXrVrWmrBrowserTestBase::~WebXrVrWmrBrowserTestBase() = default;

void WebXrVrWmrBrowserTestBase::PreRunTestOnMainThread() {
  dummy_hook_ = std::make_unique<MockXRDeviceHookBase>();
  WebXrVrBrowserTestBase::PreRunTestOnMainThread();
}

XrBrowserTestBase::RuntimeType WebXrVrWmrBrowserTestBase::GetRuntimeType()
    const {
  return XrBrowserTestBase::RuntimeType::RUNTIME_WMR;
}
#endif  // OS_WIN

}  // namespace vr

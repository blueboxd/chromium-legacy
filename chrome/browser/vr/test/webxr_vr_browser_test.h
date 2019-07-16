// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_WEBXR_VR_BROWSER_TEST_H_
#define CHROME_BROWSER_VR_TEST_WEBXR_VR_BROWSER_TEST_H_

#include "build/build_config.h"
#include "chrome/browser/vr/test/conditional_skipping.h"
#include "chrome/browser/vr/test/mock_xr_device_hook_base.h"
#include "chrome/browser/vr/test/webxr_browser_test.h"
#include "chrome/browser/vr/test/xr_browser_test.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "device/base/features.h"
#include "device/vr/buildflags/buildflags.h"
#include "ui/gfx/geometry/vector3d_f.h"

#if defined(OS_WIN)
#include "chrome/browser/vr/test/mock_xr_session_request_consent_manager.h"
#include "services/service_manager/sandbox/features.h"
#endif

namespace vr {

// WebXR for VR-specific test base class without any particular runtime.
class WebXrVrBrowserTestBase : public WebXrBrowserTestBase {
 public:
  WebXrVrBrowserTestBase() { enable_features_.push_back(features::kWebXr); }
  void EnterSessionWithUserGesture(content::WebContents* web_contents) override;
  void EnterSessionWithUserGestureOrFail(
      content::WebContents* web_contents) override;
  void EndSession(content::WebContents* web_contents) override;
  void EndSessionOrFail(content::WebContents* web_contents) override;

  virtual gfx::Vector3dF GetControllerOffset() const;

  // Necessary to use the WebContents-less versions of functions.
  using WebXrBrowserTestBase::XrDeviceFound;
  using WebXrBrowserTestBase::EnterSessionWithUserGesture;
  using WebXrBrowserTestBase::EnterSessionWithUserGestureAndWait;
  using WebXrBrowserTestBase::EnterSessionWithUserGestureOrFail;
  using WebXrBrowserTestBase::EndSession;
  using WebXrBrowserTestBase::EndSessionOrFail;

#if defined(OS_WIN)
  MockXRSessionRequestConsentManager consent_manager_;
#endif
};

// Test class with OpenVR disabled.
class WebXrVrRuntimelessBrowserTest : public WebXrVrBrowserTestBase {
 public:
  WebXrVrRuntimelessBrowserTest() {
#if BUILDFLAG(ENABLE_WINDOWS_MR)
    disable_features_.push_back(features::kWindowsMixedReality);
#endif
  }
};

// WebXrOrientationSensorDevice is only defined when the enable_vr flag is set.
#if BUILDFLAG(ENABLE_VR)
class WebXrVrRuntimelessBrowserTestSensorless
    : public WebXrVrRuntimelessBrowserTest {
 public:
  WebXrVrRuntimelessBrowserTestSensorless() {
    disable_features_.push_back(device::kWebXrOrientationSensorDevice);
  }
};
#endif  // BUILDFLAG(ENABLE_VR)

// OpenVR and WMR feature only defined on Windows.
#ifdef OS_WIN
// OpenVR-specific subclass of WebXrVrBrowserTestBase.
class WebXrVrOpenVrBrowserTestBase : public WebXrVrBrowserTestBase {
 public:
  WebXrVrOpenVrBrowserTestBase() {
    enable_features_.push_back(features::kOpenVR);
#if BUILDFLAG(ENABLE_WINDOWS_MR)
    disable_features_.push_back(features::kWindowsMixedReality);
#endif
  }
  XrBrowserTestBase::RuntimeType GetRuntimeType() const override;
  gfx::Vector3dF GetControllerOffset() const override;
};

// WMR-specific subclass of WebXrVrBrowserTestBase.
class WebXrVrWmrBrowserTestBase : public WebXrVrBrowserTestBase {
 public:
  WebXrVrWmrBrowserTestBase();
  ~WebXrVrWmrBrowserTestBase() override;
  void PreRunTestOnMainThread() override;
  // WMR enabled by default, so no need to add anything in the constructor.
  XrBrowserTestBase::RuntimeType GetRuntimeType() const override;

 private:
  // We create this before the test starts so that a test hook is always
  // registered, and thus the mock WMR wrappers are always used in tests. If a
  // test needs to actually use the test hook for input, then the one the test
  // creates will simply be registered over this one.
  std::unique_ptr<MockXRDeviceHookBase> dummy_hook_;
};

// Test class with standard features enabled: WebXR and OpenVR.
class WebXrVrOpenVrBrowserTest : public WebXrVrOpenVrBrowserTestBase {
 public:
  WebXrVrOpenVrBrowserTest() {
    // We know at this point that we're going to be running with both OpenVR and
    // WebXR enabled, so enforce the DirectX 11.1 requirement.
    runtime_requirements_.push_back(XrTestRequirement::DIRECTX_11_1);
  }
};

class WebXrVrWmrBrowserTest : public WebXrVrWmrBrowserTestBase {
 public:
  WebXrVrWmrBrowserTest() {
    // WMR already enabled by default.
    runtime_requirements_.push_back(XrTestRequirement::DIRECTX_11_1);
  }
};

// Test classes with WebXR disabled.
class WebXrVrOpenVrBrowserTestWebXrDisabled
    : public WebXrVrOpenVrBrowserTestBase {
 public:
  WebXrVrOpenVrBrowserTestWebXrDisabled() {
    disable_features_.push_back(features::kWebXr);
  }
};

class WebXrVrWmrBrowserTestWebXrDisabled : public WebXrVrWmrBrowserTestBase {
 public:
  WebXrVrWmrBrowserTestWebXrDisabled() {
    disable_features_.push_back(features::kWebXr);
  }
};
#endif  // OS_WIN

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_WEBXR_VR_BROWSER_TEST_H_

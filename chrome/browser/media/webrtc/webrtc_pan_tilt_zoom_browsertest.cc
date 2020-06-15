// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/stringprintf.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace {

struct TestConfig {
  const char* constraints;
  const char* expected_microphone;
  const char* expected_camera;
  const char* expected_pan_tilt_zoom;
};

static const char kMainHtmlPage[] = "/webrtc/webrtc_pan_tilt_zoom_test.html";

}  // namespace

class WebRtcPanTiltZoomBrowserTest
    : public WebRtcTestBase,
      public testing::WithParamInterface<TestConfig> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "MediaCapturePanTilt");
  }

  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();
  }
};

IN_PROC_BROWSER_TEST_P(WebRtcPanTiltZoomBrowserTest,
                       TestRequestPanTiltZoomPermission) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);

  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(),
      base::StringPrintf("runGetUserMedia(%s);", GetParam().constraints),
      &result));
  EXPECT_EQ(result, "runGetUserMedia-success");

  std::string microphone;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(), "getMicrophonePermission();", &microphone));
  EXPECT_EQ(microphone, GetParam().expected_microphone);

  std::string camera;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(), "getCameraPermission();", &camera));
  EXPECT_EQ(camera, GetParam().expected_camera);

  std::string pan_tilt_zoom;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(), "getPanTiltZoomPermission();", &pan_tilt_zoom));
  EXPECT_EQ(pan_tilt_zoom, GetParam().expected_pan_tilt_zoom);
}

INSTANTIATE_TEST_SUITE_P(
    RequestPanTiltZoomPermission,
    WebRtcPanTiltZoomBrowserTest,
    testing::Values(
        // no pan, tilt, zoom in audio and video constraints
        TestConfig{"{ video: true }", "prompt", "granted", "prompt"},
        TestConfig{"{ audio: true }", "granted", "prompt", "prompt"},
        TestConfig{"{ audio: true, video: true }", "granted", "granted",
                   "prompt"},
        // pan, tilt, zoom in audio constraints
        TestConfig{"{ audio: { pan : false } }", "granted", "prompt", "prompt"},
        TestConfig{"{ audio: { tilt : false } }", "granted", "prompt",
                   "prompt"},
        TestConfig{"{ audio: { zoom : false } }", "granted", "prompt",
                   "prompt"},
        TestConfig{"{ audio: { pan : {} } }", "granted", "prompt", "prompt"},
        TestConfig{"{ audio: { tilt : {} } }", "granted", "prompt", "prompt"},
        TestConfig{"{ audio: { zoom : {} } }", "granted", "prompt", "prompt"},
        TestConfig{"{ audio: { pan : 1 } }", "granted", "prompt", "prompt"},
        TestConfig{"{ audio: { tilt : 1 } }", "granted", "prompt", "prompt"},
        TestConfig{"{ audio: { zoom : 1 } }", "granted", "prompt", "prompt"},
        TestConfig{"{ audio: { pan : true } }", "granted", "prompt", "prompt"},
        TestConfig{"{ audio: { tilt : true } }", "granted", "prompt", "prompt"},
        TestConfig{"{ audio: { zoom : true } }", "granted", "prompt", "prompt"},
        // pan, tilt, zoom in basic video constraints if no audio
        TestConfig{"{ video: { pan : false } }", "prompt", "granted", "prompt"},
        TestConfig{"{ video: { tilt : false } }", "prompt", "granted",
                   "prompt"},
        TestConfig{"{ video: { zoom : false } }", "prompt", "granted",
                   "prompt"},
        TestConfig{"{ video: { pan : {} } }", "prompt", "granted", "granted"},
        TestConfig{"{ video: { tilt : {} } }", "prompt", "granted", "granted"},
        TestConfig{"{ video: { zoom : {} } }", "prompt", "granted", "granted"},
        TestConfig{"{ video: { pan : 1 } }", "prompt", "granted", "granted"},
        TestConfig{"{ video: { tilt : 1 } }", "prompt", "granted", "granted"},
        TestConfig{"{ video: { zoom : 1 } }", "prompt", "granted", "granted"},
        TestConfig{"{ video: { pan : true } }", "prompt", "granted", "granted"},
        TestConfig{"{ video: { tilt : true } }", "prompt", "granted",
                   "granted"},
        TestConfig{"{ video: { zoom : true } }", "prompt", "granted",
                   "granted"},
        // pan, tilt, zoom in advanced video constraints if no audio
        TestConfig{"{ video: { advanced: [{ pan : false }] } }", "prompt",
                   "granted", "prompt"},
        TestConfig{"{ video: { advanced: [{ tilt : false }] } }", "prompt",
                   "granted", "prompt"},
        TestConfig{"{ video: { advanced: [{ zoom : false }] } }", "prompt",
                   "granted", "prompt"},
        TestConfig{"{ video: { advanced: [{ pan : {} }] } }", "prompt",
                   "granted", "granted"},
        TestConfig{"{ video: { advanced: [{ tilt : {} }] } }", "prompt",
                   "granted", "granted"},
        TestConfig{"{ video: { advanced: [{ zoom : {} }] } }", "prompt",
                   "granted", "granted"},
        TestConfig{"{ video: { advanced: [{ pan : 1 }] } }", "prompt",
                   "granted", "granted"},
        TestConfig{"{ video: { advanced: [{ tilt : 1 }] } }", "prompt",
                   "granted", "granted"},
        TestConfig{"{ video: { advanced: [{ zoom : 1 }] } }", "prompt",
                   "granted", "granted"},
        TestConfig{"{ video: { advanced: [{ pan : true }] } }", "prompt",
                   "granted", "granted"},
        TestConfig{"{ video: { advanced: [{ tilt : true }] } }", "prompt",
                   "granted", "granted"},
        TestConfig{"{ video: { advanced: [{ zoom : true }] } }", "prompt",
                   "granted", "granted"},
        // pan, tilt, zoom in basic video constraints if audio
        TestConfig{"{ audio: true, video: { pan : false } }", "granted",
                   "granted", "prompt"},
        TestConfig{"{ audio: true, video: { tilt : false } }", "granted",
                   "granted", "prompt"},
        TestConfig{"{ audio: true, video: { zoom : false } }", "granted",
                   "granted", "prompt"},
        TestConfig{"{ audio: true, video: { pan : {} } }", "granted", "granted",
                   "granted"},
        TestConfig{"{ audio: true, video: { tilt : {} } }", "granted",
                   "granted", "granted"},
        TestConfig{"{ audio: true, video: { zoom : {} } }", "granted",
                   "granted", "granted"},
        TestConfig{"{ audio: true, video: { pan : 1 } }", "granted", "granted",
                   "granted"},
        TestConfig{"{ audio: true, video: { tilt : 1 } }", "granted", "granted",
                   "granted"},
        TestConfig{"{ audio: true, video: { zoom : 1 } }", "granted", "granted",
                   "granted"},
        TestConfig{"{ audio: true, video: { pan : true } }", "granted",
                   "granted", "granted"},
        TestConfig{"{ audio: true, video: { tilt : true } }", "granted",
                   "granted", "granted"},
        TestConfig{"{ audio: true, video: { zoom : true } }", "granted",
                   "granted", "granted"},
        // pan, tilt, zoom in advanced video constraints if audio
        TestConfig{"{ audio: true, video: { advanced: [{ pan : false }] } }",
                   "granted", "granted", "prompt"},
        TestConfig{"{ audio: true, video: { advanced: [{ tilt : false }] } }",
                   "granted", "granted", "prompt"},
        TestConfig{"{ audio: true, video: { advanced: [{ zoom : false }] } }",
                   "granted", "granted", "prompt"},
        TestConfig{"{ audio: true, video: { advanced: [{ pan : {} }] } }",
                   "granted", "granted", "granted"},
        TestConfig{"{ audio: true, video: { advanced: [{ tilt : {} }] } }",
                   "granted", "granted", "granted"},
        TestConfig{"{ audio: true, video: { advanced: [{ zoom : {} }] } }",
                   "granted", "granted", "granted"},
        TestConfig{"{ audio: true, video: { advanced: [{ pan : 1 }] } }",
                   "granted", "granted", "granted"},
        TestConfig{"{ audio: true, video: { advanced: [{ tilt : 1 }] } }",
                   "granted", "granted", "granted"},
        TestConfig{"{ audio: true, video: { advanced: [{ zoom : 1 }] } }",
                   "granted", "granted", "granted"},
        TestConfig{"{ audio: true, video: { advanced: [{ pan : true }] } }",
                   "granted", "granted", "granted"},
        TestConfig{"{ audio: true, video: { advanced: [{ tilt : true }] } }",
                   "granted", "granted", "granted"},
        TestConfig{"{ audio: true, video: { advanced: [{ zoom : true }] } }",
                   "granted", "granted", "granted"}));

class WebRtcPanTiltZoomPermissionRequestBrowserTest
    : public WebRtcTestBase,
      public ::testing::WithParamInterface<
          bool /* IsPanTiltZoomSupported() */> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kEnableBlinkFeatures,
        "MediaCapturePanTilt,PermissionsRequestRevoke");
  }

  bool IsPanTiltZoomSupported() const { return GetParam(); }

  void SetUpOnMainThread() override {
    WebRtcTestBase::SetUpOnMainThread();

    blink::MediaStreamDevices video_devices;
    blink::MediaStreamDevice fake_video_device(
        blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, "fake_video_dev",
        "Fake Video Device", media::MEDIA_VIDEO_FACING_NONE, base::nullopt,
        IsPanTiltZoomSupported());
    video_devices.push_back(fake_video_device);
    MediaCaptureDevicesDispatcher::GetInstance()->SetTestVideoCaptureDevices(
        video_devices);
  }

  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();
  }
};

IN_PROC_BROWSER_TEST_P(WebRtcPanTiltZoomPermissionRequestBrowserTest,
                       TestRequestPanTiltZoomPermission) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);

  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(), "runRequestPanTiltZoom();", &result));
  EXPECT_EQ(result, "runRequestPanTiltZoom-success");

  std::string camera;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(), "getCameraPermission();", &camera));
  EXPECT_EQ(camera, "granted");

  std::string pan_tilt_zoom;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(), "getPanTiltZoomPermission();", &pan_tilt_zoom));
  EXPECT_EQ(pan_tilt_zoom, IsPanTiltZoomSupported() ? "granted" : "prompt");
}

INSTANTIATE_TEST_SUITE_P(RequestPanTiltZoomPermission,
                         WebRtcPanTiltZoomPermissionRequestBrowserTest,
                         ::testing::Bool() /* IsPanTiltZoomSupported() */);

class WebRtcPanTiltZoomCameraDevicesBrowserTest : public WebRtcTestBase {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kEnableBlinkFeatures,
        "MediaCapturePanTilt,PermissionsRequestRevoke");
  }

  void SetVideoCaptureDevice(bool pan_tilt_zoom_supported) {
    blink::MediaStreamDevices video_devices;
    blink::MediaStreamDevice fake_video_device(
        blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, "fake_video_dev",
        "Fake Video Device", media::MEDIA_VIDEO_FACING_NONE, base::nullopt,
        pan_tilt_zoom_supported);
    video_devices.push_back(fake_video_device);
    MediaCaptureDevicesDispatcher::GetInstance()->SetTestVideoCaptureDevices(
        video_devices);
  }

  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();
  }
};

IN_PROC_BROWSER_TEST_F(WebRtcPanTiltZoomCameraDevicesBrowserTest,
                       TestCameraPanTiltZoomPermissionIsNotGrantedAfterCamera) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);

  // Simulate camera device with no PTZ support and request PTZ camera
  // permission.
  SetVideoCaptureDevice(false /* pan_tilt_zoom_supported */);
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(), "runRequestPanTiltZoom();", &result));
  EXPECT_EQ(result, "runRequestPanTiltZoom-success");

  // Camera permission should be granted.
  std::string camera;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(), "getCameraPermission();", &camera));
  EXPECT_EQ(camera, "granted");

  // Camera PTZ permission should not be granted.
  std::string pan_tilt_zoom;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(), "getPanTiltZoomPermission();", &pan_tilt_zoom));
  EXPECT_EQ(pan_tilt_zoom, "prompt");

  // Simulate camera device with PTZ support.
  SetVideoCaptureDevice(true /* pan_tilt_zoom_supported */);

  // Camera PTZ permission should still not be granted.
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(), "getPanTiltZoomPermission();", &pan_tilt_zoom));
  EXPECT_EQ(pan_tilt_zoom, "prompt");
}

IN_PROC_BROWSER_TEST_F(WebRtcPanTiltZoomCameraDevicesBrowserTest,
                       TestCameraPanTiltZoomPermissionPersists) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* tab = OpenTestPageInNewTab(kMainHtmlPage);

  // Simulate camera device with PTZ support and request PTZ camera permission.
  SetVideoCaptureDevice(true /* pan_tilt_zoom_supported */);
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(), "runRequestPanTiltZoom();", &result));
  EXPECT_EQ(result, "runRequestPanTiltZoom-success");

  // Camera permission should be granted.
  std::string camera;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(), "getCameraPermission();", &camera));
  EXPECT_EQ(camera, "granted");

  // Camera PTZ permission should be granted.
  std::string pan_tilt_zoom;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(), "getPanTiltZoomPermission();", &pan_tilt_zoom));
  EXPECT_EQ(pan_tilt_zoom, "granted");

  // Simulate camera device with no PTZ support.
  SetVideoCaptureDevice(false /* pan_tilt_zoom_supported */);

  // Camera PTZ permission should still be granted.
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetMainFrame(), "getPanTiltZoomPermission();", &pan_tilt_zoom));
  EXPECT_EQ(pan_tilt_zoom, "granted");
}

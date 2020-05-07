// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/stringprintf.h"
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
        TestConfig{"{ video: true }", "prompt", "granted", "prompt"},
        TestConfig{"{ audio: true }", "granted", "prompt", "prompt"},
        TestConfig{"{ audio: true, video: true }", "granted", "granted",
                   "prompt"},
        TestConfig{"{ video: { pan : {} } }", "prompt", "granted", "prompt"},
        TestConfig{"{ video: { tilt : {} } }", "prompt", "granted", "prompt"},
        TestConfig{"{ video: { zoom : {} } }", "prompt", "granted", "prompt"},
        TestConfig{"{ video: { advanced: [{ pan : {} }] } }", "prompt",
                   "granted", "prompt"},
        TestConfig{"{ video: { advanced: [{ tilt : {} }] } }", "prompt",
                   "granted", "prompt"},
        TestConfig{"{ video: { advanced: [{ zoom : {} }] } }", "prompt",
                   "granted", "prompt"},
        TestConfig{"{ audio: { pan : {} } }", "granted", "prompt", "prompt"},
        TestConfig{"{ audio: { tilt : {} } }", "granted", "prompt", "prompt"},
        TestConfig{"{ audio: { zoom : {} } }", "granted", "prompt", "prompt"},
        TestConfig{"{ video: { pan : 1 } }", "prompt", "granted", "granted"},
        TestConfig{"{ video: { tilt : 1 } }", "prompt", "granted", "granted"},
        TestConfig{"{ video: { zoom : 1 } }", "prompt", "granted", "granted"},
        TestConfig{"{ video: { advanced: [{ pan : 1 }] } }", "prompt",
                   "granted", "granted"},
        TestConfig{"{ video: { advanced: [{ tilt : 1 }] } }", "prompt",
                   "granted", "granted"},
        TestConfig{"{ video: { advanced: [{ zoom : 1 }] } }", "prompt",
                   "granted", "granted"},
        TestConfig{"{ audio: true, video: { pan : 1 } }", "granted", "granted",
                   "granted"},
        TestConfig{"{ audio: true, video: { tilt : 1 } }", "granted", "granted",
                   "granted"},
        TestConfig{"{ audio: true, video: { zoom : 1 } }", "granted", "granted",
                   "granted"}));

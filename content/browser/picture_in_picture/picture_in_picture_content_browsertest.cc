// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/picture_in_picture/picture_in_picture_service_impl.h"
#include "content/browser/picture_in_picture/picture_in_picture_window_controller_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/picture_in_picture/picture_in_picture.mojom.h"

using testing::Mock;
using testing::NiceMock;

namespace content {

namespace {

class TestOverlayWindow : public OverlayWindow {
 public:
  TestOverlayWindow() = default;
  ~TestOverlayWindow() override = default;

  static std::unique_ptr<OverlayWindow> Create(
      PictureInPictureWindowController* controller) {
    return std::unique_ptr<OverlayWindow>(new TestOverlayWindow());
  }

  bool IsActive() override { return false; }
  void Close() override {}
  void ShowInactive() override {}
  void Hide() override {}
  bool IsVisible() override { return false; }
  bool IsAlwaysOnTop() override { return false; }
  gfx::Rect GetBounds() override { return gfx::Rect(size_); }
  void UpdateVideoSize(const gfx::Size& natural_size) override {
    size_ = natural_size;
  }
  void SetPlaybackState(PlaybackState playback_state) override {}
  void SetAlwaysHidePlayPauseButton(bool is_visible) override {}
  void SetSkipAdButtonVisibility(bool is_visible) override {}
  void SetNextTrackButtonVisibility(bool is_visible) override {}
  void SetPreviousTrackButtonVisibility(bool is_visible) override {}
  void SetSurfaceId(const viz::SurfaceId& surface_id) override {}
  cc::Layer* GetLayerForTesting() override { return nullptr; }

 private:
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(TestOverlayWindow);
};

class TestContentBrowserClient : public ContentBrowserClient {
 public:
  TestContentBrowserClient() = default;
  ~TestContentBrowserClient() override = default;

  std::unique_ptr<OverlayWindow> CreateWindowForPictureInPicture(
      PictureInPictureWindowController* controller) override {
    return TestOverlayWindow::Create(controller);
  }
};

class TestWebContentsDelegate : public WebContentsDelegate {
 public:
  TestWebContentsDelegate() = default;
  ~TestWebContentsDelegate() override = default;

  PictureInPictureResult EnterPictureInPicture(
      WebContents* web_contents,
      const viz::SurfaceId&,
      const gfx::Size& natural_size) override {
    return PictureInPictureResult::kSuccess;
  }

  MOCK_METHOD0(ExitPictureInPicture, void());
};

class PictureInPictureContentBrowserTest : public ContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "PictureInPictureAPI");
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");

    old_browser_client_ = SetBrowserClientForTesting(&content_browser_client_);
    shell()->web_contents()->SetDelegate(&web_contents_delegate_);
  }

  void TearDownOnMainThread() override {
    SetBrowserClientForTesting(old_browser_client_);

    ContentBrowserTest::TearDownOnMainThread();
  }

 protected:
  NiceMock<TestWebContentsDelegate> web_contents_delegate_;

 private:
  ContentBrowserClient* old_browser_client_ = nullptr;
  TestContentBrowserClient content_browser_client_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(PictureInPictureContentBrowserTest,
                       RequestSecondVideoInSameRFHDoesNotCloseWindow) {
  EXPECT_CALL(web_contents_delegate_, ExitPictureInPicture()).Times(0);

  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "two-videos.html")));

  // Play first video.
  ASSERT_TRUE(ExecuteScript(shell()->web_contents(), "videos[0].play();"));

  base::string16 expected_title = base::ASCIIToUTF16("videos[0] playing");
  EXPECT_EQ(
      expected_title,
      TitleWatcher(shell()->web_contents(), expected_title).WaitAndGetTitle());

  // Play second video.
  ASSERT_TRUE(ExecuteScript(shell()->web_contents(), "videos[1].play();"));

  expected_title = base::ASCIIToUTF16("videos[1] playing");
  EXPECT_EQ(
      expected_title,
      TitleWatcher(shell()->web_contents(), expected_title).WaitAndGetTitle());

  // Send first video in Picture-in-Picture.
  ASSERT_TRUE(ExecuteScript(shell()->web_contents(),
                            "videos[0].requestPictureInPicture();"));

  expected_title = base::ASCIIToUTF16("videos[0] entered picture-in-picture");
  EXPECT_EQ(
      expected_title,
      TitleWatcher(shell()->web_contents(), expected_title).WaitAndGetTitle());

  // Send second video in Picture-in-Picture.
  ASSERT_TRUE(ExecuteScript(shell()->web_contents(),
                            "videos[1].requestPictureInPicture();"));

  expected_title = base::ASCIIToUTF16("videos[1] entered picture-in-picture");
  EXPECT_EQ(
      expected_title,
      TitleWatcher(shell()->web_contents(), expected_title).WaitAndGetTitle());

  // The session should still be active and ExitPictureInPicture() never called.
  EXPECT_NE(nullptr, PictureInPictureWindowControllerImpl::FromWebContents(
                         shell()->web_contents())
                         ->active_session_for_testing());

  Mock::VerifyAndClearExpectations(&web_contents_delegate_);
}

IN_PROC_BROWSER_TEST_F(PictureInPictureContentBrowserTest,
                       RequestSecondVideoInDifferentRFHDoesNotCloseWindow) {
  EXPECT_CALL(web_contents_delegate_, ExitPictureInPicture()).Times(0);

  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL(
          "example.com", "/media/picture_in_picture/two-videos.html")));

  base::string16 expected_title = base::ASCIIToUTF16("iframe loaded");
  EXPECT_EQ(
      expected_title,
      TitleWatcher(shell()->web_contents(), expected_title).WaitAndGetTitle());

  // Play first video.
  ASSERT_TRUE(ExecuteScript(shell()->web_contents(), "videos[0].play();"));

  expected_title = base::ASCIIToUTF16("videos[0] playing");
  EXPECT_EQ(
      expected_title,
      TitleWatcher(shell()->web_contents(), expected_title).WaitAndGetTitle());

  // Play second video (in iframe).
  ASSERT_TRUE(
      ExecuteScript(shell()->web_contents(), "iframeVideos[0].play();"));

  expected_title = base::ASCIIToUTF16("iframeVideos[0] playing");
  EXPECT_EQ(
      expected_title,
      TitleWatcher(shell()->web_contents(), expected_title).WaitAndGetTitle());

  // Send first video in Picture-in-Picture.
  ASSERT_TRUE(ExecuteScript(shell()->web_contents(),
                            "videos[0].requestPictureInPicture();"));

  expected_title = base::ASCIIToUTF16("videos[0] entered picture-in-picture");
  EXPECT_EQ(
      expected_title,
      TitleWatcher(shell()->web_contents(), expected_title).WaitAndGetTitle());

  // Send second video in Picture-in-Picture.
  ASSERT_TRUE(ExecuteScript(shell()->web_contents(),
                            "iframeVideos[0].requestPictureInPicture();"));

  expected_title =
      base::ASCIIToUTF16("iframeVideos[0] entered picture-in-picture");
  EXPECT_EQ(
      expected_title,
      TitleWatcher(shell()->web_contents(), expected_title).WaitAndGetTitle());

  // The session should still be active and ExitPictureInPicture() never called.
  EXPECT_NE(nullptr, PictureInPictureWindowControllerImpl::FromWebContents(
                         shell()->web_contents())
                         ->active_session_for_testing());

  Mock::VerifyAndClearExpectations(&web_contents_delegate_);
}

}  // namespace content

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/decorators/helpers/page_live_state_decorator_helper.h"

#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/performance_manager/performance_manager_tab_helper.h"
#include "components/performance_manager/performance_manager_test_harness.h"
#include "components/performance_manager/test_support/page_live_state_decorator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

class PageLiveStateDecoratorHelperTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  PageLiveStateDecoratorHelperTest() = default;
  ~PageLiveStateDecoratorHelperTest() override = default;
  PageLiveStateDecoratorHelperTest(
      const PageLiveStateDecoratorHelperTest& other) = delete;
  PageLiveStateDecoratorHelperTest& operator=(
      const PageLiveStateDecoratorHelperTest&) = delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    perf_man_ = PerformanceManagerImpl::Create(base::DoNothing());
    indicator_ = MediaCaptureDevicesDispatcher::GetInstance()
                     ->GetMediaStreamCaptureIndicator();
    auto contents = CreateTestWebContents();
    helper_ = std::make_unique<PageLiveStateDecoratorHelper>();
    SetContents(std::move(contents));
  }

  void TearDown() override {
    helper_.reset();
    indicator_.reset();
    DeleteContents();
    // Have the performance manager destroy itself.
    PerformanceManagerImpl::Destroy(std::move(perf_man_));
    task_environment()->RunUntilIdle();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<content::WebContents> CreateTestWebContents() {
    std::unique_ptr<content::WebContents> contents =
        ChromeRenderViewHostTestHarness::CreateTestWebContents();
    PerformanceManagerTabHelper::CreateForWebContents(contents.get());
    return contents;
  }

  MediaStreamCaptureIndicator* indicator() { return indicator_.get(); }

  void EndToEndStreamPropertyTest(
      blink::mojom::MediaStreamType stream_type,
      bool (PageLiveStateDecorator::Data::*pm_getter)() const);

 private:
  scoped_refptr<MediaStreamCaptureIndicator> indicator_;
  std::unique_ptr<PerformanceManagerImpl> perf_man_;
  std::unique_ptr<PageLiveStateDecoratorHelper> helper_;
};

void PageLiveStateDecoratorHelperTest::EndToEndStreamPropertyTest(
    blink::mojom::MediaStreamType stream_type,
    bool (PageLiveStateDecorator::Data::*pm_getter)() const) {
  // By default all properties are set to false.
  TestPageLiveStatePropertyOnPMSequence(web_contents(), pm_getter, false);

  // Create the fake stream device and start it, this should set the property to
  // true.
  blink::MediaStreamDevices devices{
      blink::MediaStreamDevice(stream_type, "fake_device", "fake_device")};
  std::unique_ptr<content::MediaStreamUI> ui =
      indicator()->RegisterMediaStream(web_contents(), devices);
  ui->OnStarted(base::OnceClosure(), content::MediaStreamUI::SourceCallback());
  TestPageLiveStatePropertyOnPMSequence(web_contents(), pm_getter, true);

  // Switch back to the default state.
  ui.reset();
  TestPageLiveStatePropertyOnPMSequence(web_contents(), pm_getter, false);
}

}  // namespace

TEST_F(PageLiveStateDecoratorHelperTest, OnIsCapturingVideoChanged) {
  EndToEndStreamPropertyTest(
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
      &PageLiveStateDecorator::Data::IsCapturingVideo);
}

TEST_F(PageLiveStateDecoratorHelperTest, OnIsCapturingAudioChanged) {
  EndToEndStreamPropertyTest(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      &PageLiveStateDecorator::Data::IsCapturingAudio);
}

TEST_F(PageLiveStateDecoratorHelperTest, OnIsBeingMirroredChanged) {
  EndToEndStreamPropertyTest(
      blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE,
      &PageLiveStateDecorator::Data::IsBeingMirrored);
}

TEST_F(PageLiveStateDecoratorHelperTest, OnIsCapturingDesktopChanged) {
  EndToEndStreamPropertyTest(
      blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
      &PageLiveStateDecorator::Data::IsCapturingDesktop);
}

}  // namespace performance_manager

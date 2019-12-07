// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"

#include "base/bind.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"

namespace {

class LenientMockObserver : public MediaStreamCaptureIndicator::Observer {
 public:
  LenientMockObserver() = default;
  ~LenientMockObserver() override {}

  // Helper functions used to set the expectations of the mock methods. This
  // allows passing function pointers to
  // MediaStreamCaptureIndicatorTest::TestObserverMethod.

  void SetOnIsCapturingVideoChangedExpectation(content::WebContents* contents,
                                               bool is_capturing_video) {
    EXPECT_CALL(*this, OnIsCapturingVideoChanged(contents, is_capturing_video));
  }

  void SetOnIsCapturingAudioChangedExpectation(content::WebContents* contents,
                                               bool is_capturing_audio) {
    EXPECT_CALL(*this, OnIsCapturingAudioChanged(contents, is_capturing_audio));
  }

  void SetOnIsBeingMirroredChangedExpectation(content::WebContents* contents,
                                              bool is_being_mirrored) {
    EXPECT_CALL(*this, OnIsBeingMirroredChanged(contents, is_being_mirrored));
  }

  void SetOnIsCapturingDesktopChangedExpectation(content::WebContents* contents,
                                                 bool is_capturing_desktop) {
    EXPECT_CALL(*this,
                OnIsCapturingDesktopChanged(contents, is_capturing_desktop));
  }

 private:
  MOCK_METHOD2(OnIsCapturingVideoChanged,
               void(content::WebContents* contents, bool is_capturing_video));
  MOCK_METHOD2(OnIsCapturingAudioChanged,
               void(content::WebContents* contents, bool is_capturing_audio));
  MOCK_METHOD2(OnIsBeingMirroredChanged,
               void(content::WebContents* contents, bool is_being_mirrored));
  MOCK_METHOD2(OnIsCapturingDesktopChanged,
               void(content::WebContents* contents, bool is_capturing_desktop));

  DISALLOW_COPY_AND_ASSIGN(LenientMockObserver);
};
using MockObserver = testing::StrictMock<LenientMockObserver>;

typedef void (MockObserver::*MockObserverSetExpectationsMethod)(
    content::WebContents* web_contents,
    bool value);
typedef bool (MediaStreamCaptureIndicator::*AccessorMethod)(
    content::WebContents* web_contents) const;

class MediaStreamCaptureIndicatorTest : public ChromeRenderViewHostTestHarness {
 public:
  MediaStreamCaptureIndicatorTest() {}
  ~MediaStreamCaptureIndicatorTest() override {}
  MediaStreamCaptureIndicatorTest(const MediaStreamCaptureIndicatorTest&) =
      delete;
  MediaStreamCaptureIndicatorTest& operator=(
      const MediaStreamCaptureIndicatorTest&) = delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    indicator_ = MediaCaptureDevicesDispatcher::GetInstance()
                     ->GetMediaStreamCaptureIndicator();
    observer_ = std::make_unique<MockObserver>();
    indicator_->AddObserver(observer());
    contents_ = CreateTestWebContents();
  }

  void TearDown() override {
    contents_.reset();
    indicator_->RemoveObserver(observer());
    observer_.reset();
    indicator_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  MediaStreamCaptureIndicator* indicator() { return indicator_.get(); }
  content::WebContents* contents() { return contents_.get(); }
  MockObserver* observer() { return observer_.get(); }

  // Test a MediaStreamCaptureIndicator accessor and ensure that the
  // corresponding observer method gets called.
  void TestObserverMethod(blink::mojom::MediaStreamType stream_type,
                          MockObserverSetExpectationsMethod observer_method,
                          AccessorMethod accessor_method);

 private:
  std::unique_ptr<MockObserver> observer_;
  std::unique_ptr<content::WebContents> contents_;
  scoped_refptr<MediaStreamCaptureIndicator> indicator_;
};

}  // namespace

void MediaStreamCaptureIndicatorTest::TestObserverMethod(
    blink::mojom::MediaStreamType stream_type,
    MockObserverSetExpectationsMethod observer_set_expectations_method,
    AccessorMethod accessor_method) {
  // Create the fake stream device.
  blink::MediaStreamDevices devices{
      blink::MediaStreamDevice(stream_type, "fake_device", "fake_device")};

  // By default all accessors should return false as there's no stream device.
  EXPECT_FALSE((indicator()->*accessor_method)(contents()));
  std::unique_ptr<content::MediaStreamUI> ui =
      indicator()->RegisterMediaStream(contents(), devices);

  // Make sure that the observer gets called and that the corresponding accessor
  // gets called when |OnStarted| is called.
  (observer()->*observer_set_expectations_method)(contents(), true);
  ui->OnStarted(base::OnceClosure(), content::MediaStreamUI::SourceCallback());
  EXPECT_TRUE((indicator()->*accessor_method)(contents()));
  ::testing::Mock::VerifyAndClear(observer());

  // Removing the stream device should cause the observer to be notified that
  // the observed property is now set to false.
  (observer()->*observer_set_expectations_method)(contents(), false);
  ui.reset();
  EXPECT_FALSE((indicator()->*accessor_method)(contents()));
  ::testing::Mock::VerifyAndClear(observer());
}

TEST_F(MediaStreamCaptureIndicatorTest, IsCapturingVideo) {
  TestObserverMethod(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
                     &MockObserver::SetOnIsCapturingVideoChangedExpectation,
                     &MediaStreamCaptureIndicator::IsCapturingVideo);
}

TEST_F(MediaStreamCaptureIndicatorTest, IsCapturingAudio) {
  TestObserverMethod(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
                     &MockObserver::SetOnIsCapturingAudioChangedExpectation,
                     &MediaStreamCaptureIndicator::IsCapturingAudio);
}

TEST_F(MediaStreamCaptureIndicatorTest, IsBeingMirrored) {
  TestObserverMethod(blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE,
                     &MockObserver::SetOnIsBeingMirroredChangedExpectation,
                     &MediaStreamCaptureIndicator::IsBeingMirrored);
}

TEST_F(MediaStreamCaptureIndicatorTest, IsCapturingDesktop) {
  TestObserverMethod(blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
                     &MockObserver::SetOnIsCapturingDesktopChangedExpectation,
                     &MediaStreamCaptureIndicator::IsCapturingDesktop);
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller.h"

#include <memory>

#include "base/test/scoped_task_environment.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller_delegate.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using media_session::mojom::MediaSessionInfo;
using media_session::mojom::MediaSessionInfoPtr;

namespace {

class MockMediaToolbarButtonControllerDelegate
    : public MediaToolbarButtonControllerDelegate {
 public:
  MockMediaToolbarButtonControllerDelegate() = default;
  ~MockMediaToolbarButtonControllerDelegate() override = default;

  // MediaToolbarButtonControllerDelegate implementation.
  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());
  MOCK_METHOD0(Enable, void());
  MOCK_METHOD0(Disable, void());
};

}  // anonymous namespace

class MediaToolbarButtonControllerTest : public testing::Test {
 public:
  MediaToolbarButtonControllerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME,
                          base::test::TaskEnvironment::MainThreadType::UI) {}
  ~MediaToolbarButtonControllerTest() override = default;

  void SetUp() override {
    controller_ =
        std::make_unique<MediaToolbarButtonController>(nullptr, &delegate_);
  }

 protected:
  void SimulatePlayingControllableMedia() {
    MediaSessionInfoPtr session_info(MediaSessionInfo::New());
    session_info->is_controllable = true;
    controller_->MediaSessionInfoChanged(std::move(session_info));
  }

  void SimulateMediaStopped() { controller_->MediaSessionInfoChanged(nullptr); }

  void AdvanceClockMilliseconds(int milliseconds) {
    task_environment_.FastForwardBy(
        base::TimeDelta::FromMilliseconds(milliseconds));
  }

  MockMediaToolbarButtonControllerDelegate& delegate() { return delegate_; }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MediaToolbarButtonController> controller_;
  MockMediaToolbarButtonControllerDelegate delegate_;

  DISALLOW_COPY_AND_ASSIGN(MediaToolbarButtonControllerTest);
};

TEST_F(MediaToolbarButtonControllerTest, CallsShowForControllableMedia) {
  EXPECT_CALL(delegate(), Show());
  SimulatePlayingControllableMedia();
}

TEST_F(MediaToolbarButtonControllerTest, HidesAfterTimeoutAndShowsAgainOnPlay) {
  // First, show the button.
  EXPECT_CALL(delegate(), Show());
  SimulatePlayingControllableMedia();
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // Then, stop playing media so the button is disabled, but hasn't been hidden
  // yet.
  EXPECT_CALL(delegate(), Disable());
  EXPECT_CALL(delegate(), Hide()).Times(0);
  SimulateMediaStopped();
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // If the time hasn't elapsed yet, we should still not be hidden.
  EXPECT_CALL(delegate(), Hide()).Times(0);
  AdvanceClockMilliseconds(2400);
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // Once the time is elapsed, we should be hidden.
  EXPECT_CALL(delegate(), Hide());
  AdvanceClockMilliseconds(200);
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // If media starts playing again, we should show and enable the button.
  EXPECT_CALL(delegate(), Show());
  EXPECT_CALL(delegate(), Enable());
  SimulatePlayingControllableMedia();
  testing::Mock::VerifyAndClearExpectations(&delegate());
}

TEST_F(MediaToolbarButtonControllerTest,
       DoesNotHideIfMediaStartsPlayingWithinTimeout) {
  // First, show the button.
  EXPECT_CALL(delegate(), Show());
  SimulatePlayingControllableMedia();
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // Then, stop playing media so the button is disabled, but hasn't been hidden
  // yet.
  EXPECT_CALL(delegate(), Disable());
  EXPECT_CALL(delegate(), Hide()).Times(0);
  SimulateMediaStopped();
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // If the time hasn't elapsed yet, we should still not be hidden.
  EXPECT_CALL(delegate(), Hide()).Times(0);
  AdvanceClockMilliseconds(2400);
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // If media starts playing again, we should show and enable the button.
  EXPECT_CALL(delegate(), Show());
  EXPECT_CALL(delegate(), Enable());
  SimulatePlayingControllableMedia();
  testing::Mock::VerifyAndClearExpectations(&delegate());
}

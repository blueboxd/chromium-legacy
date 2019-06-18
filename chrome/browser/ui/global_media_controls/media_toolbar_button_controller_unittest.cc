// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller.h"

#include <memory>

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
};

}  // anonymous namespace

class MediaToolbarButtonControllerTest : public testing::Test {
 public:
  MediaToolbarButtonControllerTest() = default;
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

  MockMediaToolbarButtonControllerDelegate& delegate() { return delegate_; }

 private:
  std::unique_ptr<MediaToolbarButtonController> controller_;
  MockMediaToolbarButtonControllerDelegate delegate_;

  DISALLOW_COPY_AND_ASSIGN(MediaToolbarButtonControllerTest);
};

TEST_F(MediaToolbarButtonControllerTest, CallsShowForControllableMedia) {
  EXPECT_CALL(delegate(), Show());
  SimulatePlayingControllableMedia();
}

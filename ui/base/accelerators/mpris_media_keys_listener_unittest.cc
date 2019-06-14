// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/mpris_media_keys_listener.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/mpris/mock_mpris_service.h"

using testing::_;
using testing::WithArg;

namespace ui {

namespace {

class MockMediaKeysListenerDelegate : public MediaKeysListener::Delegate {
 public:
  MockMediaKeysListenerDelegate() = default;
  ~MockMediaKeysListenerDelegate() override = default;

  // MediaKeysListener::Delegate implementation.
  MOCK_METHOD1(OnMediaKeysAccelerator, void(const Accelerator& accelerator));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMediaKeysListenerDelegate);
};

}  // anonymous namespace

class MprisMediaKeysListenerTest : public testing::Test {
 public:
  MprisMediaKeysListenerTest() = default;
  ~MprisMediaKeysListenerTest() override = default;

  void SetUp() override {
    listener_ = std::make_unique<MprisMediaKeysListener>(&delegate_);
    listener_->SetMprisServiceForTesting(&mock_mpris_service_);
  }

 protected:
  mpris::MockMprisService& mock_mpris_service() { return mock_mpris_service_; }
  MockMediaKeysListenerDelegate& delegate() { return delegate_; }
  MprisMediaKeysListener* listener() { return listener_.get(); }

 private:
  mpris::MockMprisService mock_mpris_service_;
  MockMediaKeysListenerDelegate delegate_;
  std::unique_ptr<MprisMediaKeysListener> listener_;

  DISALLOW_COPY_AND_ASSIGN(MprisMediaKeysListenerTest);
};

TEST_F(MprisMediaKeysListenerTest, ListensToMprisService) {
  EXPECT_CALL(mock_mpris_service(), AddObserver(listener()));
  listener()->Initialize();
}

TEST_F(MprisMediaKeysListenerTest, SimplePlayPauseTest) {
  // Should be set to true when we start listening for the key.
  EXPECT_CALL(mock_mpris_service(), SetCanPlay(true));
  EXPECT_CALL(mock_mpris_service(), SetCanPause(true));

  EXPECT_CALL(delegate(), OnMediaKeysAccelerator(_))
      .WillOnce(WithArg<0>([](const Accelerator& accelerator) {
        EXPECT_EQ(ui::VKEY_MEDIA_PLAY_PAUSE, accelerator.key_code());
      }));

  listener()->Initialize();
  listener()->StartWatchingMediaKey(ui::VKEY_MEDIA_PLAY_PAUSE);

  // Simulate media key press.
  listener()->OnPlayPause();
}

TEST_F(MprisMediaKeysListenerTest, KeyCanBeReRegistered) {
  EXPECT_CALL(mock_mpris_service(), SetCanGoNext(true)).Times(2);
  EXPECT_CALL(mock_mpris_service(), SetCanGoNext(false));
  EXPECT_CALL(delegate(), OnMediaKeysAccelerator(_))
      .WillOnce(WithArg<0>([](const Accelerator& accelerator) {
        EXPECT_EQ(ui::VKEY_MEDIA_NEXT_TRACK, accelerator.key_code());
      }));

  listener()->Initialize();

  // Start listening to register the key.
  listener()->StartWatchingMediaKey(ui::VKEY_MEDIA_NEXT_TRACK);

  // Stop listening to unregister the key.
  listener()->StopWatchingMediaKey(ui::VKEY_MEDIA_NEXT_TRACK);

  // Start listening to re-register the key.
  listener()->StartWatchingMediaKey(ui::VKEY_MEDIA_NEXT_TRACK);

  // Simulate media key press.
  listener()->OnNext();
}

TEST_F(MprisMediaKeysListenerTest, ListenForMultipleKeys) {
  // Should be set to true when we start listening for the key.
  EXPECT_CALL(mock_mpris_service(), SetCanPlay(true));
  EXPECT_CALL(mock_mpris_service(), SetCanPause(true));
  EXPECT_CALL(mock_mpris_service(), SetCanGoPrevious(true));

  // Should receive the key presses.
  EXPECT_CALL(delegate(), OnMediaKeysAccelerator(_)).Times(2);

  listener()->Initialize();
  listener()->StartWatchingMediaKey(ui::VKEY_MEDIA_PLAY_PAUSE);
  listener()->StartWatchingMediaKey(ui::VKEY_MEDIA_PREV_TRACK);

  // Simulate media key press.
  listener()->OnPlayPause();
  listener()->OnPrevious();
}

TEST_F(MprisMediaKeysListenerTest, DoesNotFirePlayPauseOnPauseEventWhenPaused) {
  // Should be set to true when we start listening for the key.
  EXPECT_CALL(mock_mpris_service(), SetCanPlay(true));
  EXPECT_CALL(mock_mpris_service(), SetCanPause(true));
  EXPECT_CALL(delegate(), OnMediaKeysAccelerator(_)).Times(0);

  listener()->Initialize();
  listener()->StartWatchingMediaKey(ui::VKEY_MEDIA_PLAY_PAUSE);
  listener()->SetIsMediaPlaying(false);

  // Simulate media key press.
  listener()->OnPause();
}

TEST_F(MprisMediaKeysListenerTest, DoesNotFirePlayPauseOnPlayEventWhenPlaying) {
  // Should be set to true when we start listening for the key.
  EXPECT_CALL(mock_mpris_service(), SetCanPlay(true));
  EXPECT_CALL(mock_mpris_service(), SetCanPause(true));
  EXPECT_CALL(delegate(), OnMediaKeysAccelerator(_)).Times(0);

  listener()->Initialize();
  listener()->StartWatchingMediaKey(ui::VKEY_MEDIA_PLAY_PAUSE);
  listener()->SetIsMediaPlaying(true);

  // Simulate media key press.
  listener()->OnPlay();
}

}  // namespace ui

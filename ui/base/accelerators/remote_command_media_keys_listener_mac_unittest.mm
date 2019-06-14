// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/remote_command_media_keys_listener_mac.h"

#include "base/callback.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/now_playing/mock_remote_command_center_delegate.h"
#include "ui/events/event.h"

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

class RemoteCommandMediaKeysListenerMacTest : public testing::Test {
 public:
  RemoteCommandMediaKeysListenerMacTest() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteCommandMediaKeysListenerMacTest);
};

TEST_F(RemoteCommandMediaKeysListenerMacTest, SimplePlayPauseTest) {
  if (@available(macOS 10.12.2, *)) {
    now_playing::MockRemoteCommandCenterDelegate rcc_delegate;
    MockMediaKeysListenerDelegate delegate;
    RemoteCommandMediaKeysListenerMac listener(&delegate);
    listener.SetRemoteCommandCenterDelegateForTesting(&rcc_delegate);

    EXPECT_CALL(rcc_delegate, AddObserver(&listener));
    EXPECT_CALL(rcc_delegate, SetCanPlayPause(true));
    EXPECT_CALL(delegate, OnMediaKeysAccelerator(_))
        .WillOnce(WithArg<0>([](const Accelerator& accelerator) {
          EXPECT_EQ(ui::VKEY_MEDIA_PLAY_PAUSE, accelerator.key_code());
        }));

    listener.Initialize();
    listener.StartWatchingMediaKey(ui::VKEY_MEDIA_PLAY_PAUSE);

    // Simulate media key press.
    listener.OnPlayPause();
  }
}

TEST_F(RemoteCommandMediaKeysListenerMacTest, KeyCanBeReRegistered) {
  if (@available(macOS 10.12.2, *)) {
    now_playing::MockRemoteCommandCenterDelegate rcc_delegate;
    MockMediaKeysListenerDelegate delegate;
    RemoteCommandMediaKeysListenerMac listener(&delegate);
    listener.SetRemoteCommandCenterDelegateForTesting(&rcc_delegate);

    EXPECT_CALL(rcc_delegate, AddObserver(&listener));
    EXPECT_CALL(rcc_delegate, SetCanGoNextTrack(true)).Times(2);
    EXPECT_CALL(rcc_delegate, SetCanGoNextTrack(false));
    EXPECT_CALL(delegate, OnMediaKeysAccelerator(_))
        .WillOnce(WithArg<0>([](const Accelerator& accelerator) {
          EXPECT_EQ(ui::VKEY_MEDIA_NEXT_TRACK, accelerator.key_code());
        }));

    listener.Initialize();

    // Start listening to register the key.
    listener.StartWatchingMediaKey(ui::VKEY_MEDIA_NEXT_TRACK);

    // Stop listening to unregister the key.
    listener.StopWatchingMediaKey(ui::VKEY_MEDIA_NEXT_TRACK);

    // Start listening to re-register the key.
    listener.StartWatchingMediaKey(ui::VKEY_MEDIA_NEXT_TRACK);

    // Simulate media key press.
    listener.OnNext();
  }
}

TEST_F(RemoteCommandMediaKeysListenerMacTest, ListenForMultipleKeys) {
  if (@available(macOS 10.12.2, *)) {
    now_playing::MockRemoteCommandCenterDelegate rcc_delegate;
    MockMediaKeysListenerDelegate delegate;
    RemoteCommandMediaKeysListenerMac listener(&delegate);
    listener.SetRemoteCommandCenterDelegateForTesting(&rcc_delegate);

    EXPECT_CALL(rcc_delegate, AddObserver(&listener));
    EXPECT_CALL(rcc_delegate, SetCanPlayPause(true));
    EXPECT_CALL(rcc_delegate, SetCanStop(true));
    EXPECT_CALL(delegate, OnMediaKeysAccelerator(_)).Times(2);

    listener.Initialize();
    listener.StartWatchingMediaKey(ui::VKEY_MEDIA_PLAY_PAUSE);
    listener.StartWatchingMediaKey(ui::VKEY_MEDIA_STOP);

    // Simulate media key press.
    listener.OnPlayPause();
    listener.OnStop();
  }
}

TEST_F(RemoteCommandMediaKeysListenerMacTest,
       DoesNotFirePlayPauseOnPauseEventWhenPaused) {
  if (@available(macOS 10.12.2, *)) {
    now_playing::MockRemoteCommandCenterDelegate rcc_delegate;
    MockMediaKeysListenerDelegate delegate;
    RemoteCommandMediaKeysListenerMac listener(&delegate);
    listener.SetRemoteCommandCenterDelegateForTesting(&rcc_delegate);

    EXPECT_CALL(rcc_delegate, AddObserver(&listener));
    EXPECT_CALL(rcc_delegate, SetCanPlayPause(true));
    EXPECT_CALL(rcc_delegate, SetCanPlay(true));
    EXPECT_CALL(rcc_delegate, SetCanPause(true));
    EXPECT_CALL(delegate, OnMediaKeysAccelerator(_)).Times(0);

    listener.Initialize();
    listener.StartWatchingMediaKey(ui::VKEY_MEDIA_PLAY_PAUSE);
    listener.SetIsMediaPlaying(false);

    // Simulate media key press.
    listener.OnPause();
  }
}

TEST_F(RemoteCommandMediaKeysListenerMacTest,
       DoesNotFirePlayPauseOnPlayEventWhenPlaying) {
  if (@available(macOS 10.12.2, *)) {
    now_playing::MockRemoteCommandCenterDelegate rcc_delegate;
    MockMediaKeysListenerDelegate delegate;
    RemoteCommandMediaKeysListenerMac listener(&delegate);
    listener.SetRemoteCommandCenterDelegateForTesting(&rcc_delegate);

    EXPECT_CALL(rcc_delegate, AddObserver(&listener));
    EXPECT_CALL(rcc_delegate, SetCanPlayPause(true));
    EXPECT_CALL(rcc_delegate, SetCanPlay(true));
    EXPECT_CALL(rcc_delegate, SetCanPause(true));
    EXPECT_CALL(delegate, OnMediaKeysAccelerator(_)).Times(0);

    listener.Initialize();
    listener.StartWatchingMediaKey(ui::VKEY_MEDIA_PLAY_PAUSE);
    listener.SetIsMediaPlaying(true);

    // Simulate media key press.
    listener.OnPlay();
  }
}

}  // namespace ui

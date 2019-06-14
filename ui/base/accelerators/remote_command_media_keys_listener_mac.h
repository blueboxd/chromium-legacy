// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_REMOTE_COMMAND_MEDIA_KEYS_LISTENER_MAC_H_
#define UI_BASE_ACCELERATORS_REMOTE_COMMAND_MEDIA_KEYS_LISTENER_MAC_H_

#include "base/containers/flat_set.h"
#include "ui/base/accelerators/media_keys_listener.h"
#include "ui/base/now_playing/remote_command_center_delegate_observer.h"
#include "ui/base/ui_base_export.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace now_playing {
class RemoteCommandCenterDelegate;
}  // namespace now_playing

namespace ui {

// Implementation of MediaKeysListener that uses MPRemoteCommandCenter to
// globally listen for media key presses. It only allows for a single instance
// to be created in order to prevent conflicts from multiple listeners.
class UI_BASE_EXPORT API_AVAILABLE(macos(10.12.2))
    RemoteCommandMediaKeysListenerMac
    : public MediaKeysListener,
      public now_playing::RemoteCommandCenterDelegateObserver {
 public:
  explicit RemoteCommandMediaKeysListenerMac(
      MediaKeysListener::Delegate* delegate);
  ~RemoteCommandMediaKeysListenerMac() override;

  static bool has_instance() { return has_instance_; }

  // Connects with the RemoteCommandCenterDelegate.
  void Initialize();

  // MediaKeysListener implementation.
  bool StartWatchingMediaKey(KeyboardCode key_code) override;
  void StopWatchingMediaKey(KeyboardCode key_code) override;
  void SetIsMediaPlaying(bool is_playing) override;

  // now_playing::RemoteCommandCenterDelegateObserver implementation.
  void OnNext() override;
  void OnPrevious() override;
  void OnPause() override;
  void OnPlayPause() override;
  void OnStop() override;
  void OnPlay() override;

  void SetRemoteCommandCenterDelegateForTesting(
      now_playing::RemoteCommandCenterDelegate* delegate) {
    remote_command_center_delegate_ = delegate;
  }

 private:
  static bool has_instance_;

  // Sends |key_code| to the delegate if it's being listened to.
  void MaybeSend(KeyboardCode key_code);

  MediaKeysListener::Delegate* delegate_;
  base::flat_set<KeyboardCode> key_codes_;
  now_playing::RemoteCommandCenterDelegate* remote_command_center_delegate_ =
      nullptr;
  bool is_media_playing_ = false;

  DISALLOW_COPY_AND_ASSIGN(RemoteCommandMediaKeysListenerMac);
};

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_REMOTE_COMMAND_MEDIA_KEYS_LISTENER_MAC_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_MPRIS_MEDIA_KEYS_LISTENER_H_
#define UI_BASE_ACCELERATORS_MPRIS_MEDIA_KEYS_LISTENER_H_

#include "base/containers/flat_set.h"
#include "ui/base/accelerators/media_keys_listener.h"
#include "ui/base/mpris/mpris_service_observer.h"
#include "ui/base/ui_base_export.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace mpris {
class MprisService;
}  // namespace mpris

namespace ui {

// Implementation of MediaKeysListener that uses MprisService to globally listen
// for media key presses. It only allows for a single instance to be created in
// order to prevent conflicts from multiple listeners.
class UI_BASE_EXPORT MprisMediaKeysListener
    : public MediaKeysListener,
      public mpris::MprisServiceObserver {
 public:
  explicit MprisMediaKeysListener(MediaKeysListener::Delegate* delegate);
  ~MprisMediaKeysListener() override;

  static bool has_instance() { return has_instance_; }

  // Connects with the MprisService.
  void Initialize();

  // MediaKeysListener implementation.
  bool StartWatchingMediaKey(KeyboardCode key_code) override;
  void StopWatchingMediaKey(KeyboardCode key_code) override;
  void SetIsMediaPlaying(bool is_playing) override;

  // mpris::MprisServiceObserver implementation.
  void OnNext() override;
  void OnPrevious() override;
  void OnPause() override;
  void OnPlayPause() override;
  void OnStop() override;
  void OnPlay() override;

  void SetMprisServiceForTesting(mpris::MprisService* service) {
    service_ = service;
  }

 private:
  static bool has_instance_;

  // Sends the key code to the delegate if the delegate has asked for it.
  void MaybeSendKeyCode(KeyboardCode key_code);

  MediaKeysListener::Delegate* delegate_;
  base::flat_set<KeyboardCode> key_codes_;
  mpris::MprisService* service_ = nullptr;
  bool is_media_playing_ = false;

  DISALLOW_COPY_AND_ASSIGN(MprisMediaKeysListener);
};

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_MPRIS_MEDIA_KEYS_LISTENER_H_

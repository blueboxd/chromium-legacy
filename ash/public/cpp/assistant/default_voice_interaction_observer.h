// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_DEFAULT_VOICE_INTERACTION_OBSERVER_H_
#define ASH_PUBLIC_CPP_ASSISTANT_DEFAULT_VOICE_INTERACTION_OBSERVER_H_

#include <string>

#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "base/macros.h"
#include "base/observer_list_types.h"

namespace ash {

// Provides a default empty implementation of
// ash::mojom::VoiceInteractionObserver interface. Child class only need to
// override the methods they are actually interested in.
class ASH_PUBLIC_EXPORT DefaultVoiceInteractionObserver
    : public mojom::VoiceInteractionObserver,
      public base::CheckedObserver {
 public:
  ~DefaultVoiceInteractionObserver() override = default;

  // mojom::VoiceInteractionObserver:
  void OnVoiceInteractionStatusChanged(
      ash::mojom::VoiceInteractionState state) override {}
  void OnVoiceInteractionSettingsEnabled(bool enabled) override {}
  void OnVoiceInteractionContextEnabled(bool enabled) override {}
  void OnVoiceInteractionHotwordEnabled(bool enabled) override {}
  void OnAssistantFeatureAllowedChanged(
      ash::mojom::AssistantAllowedState state) override {}
  void OnLocaleChanged(const std::string& locale) override {}
  void OnArcPlayStoreEnabledChanged(bool enabled) override {}
  void OnLockedFullScreenStateChanged(bool enabled) override {}

 protected:
  DefaultVoiceInteractionObserver() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultVoiceInteractionObserver);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_DEFAULT_VOICE_INTERACTION_OBSERVER_H_

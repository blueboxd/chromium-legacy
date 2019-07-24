// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_VOICE_INTERACTION_CONTROLLER_H_
#define ASH_PUBLIC_CPP_VOICE_INTERACTION_CONTROLLER_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/assistant/assistant_state_base.h"
#include "ash/public/cpp/assistant/default_voice_interaction_observer.h"
#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

namespace ash {

class ASH_PUBLIC_EXPORT VoiceInteractionController
    : public mojom::VoiceInteractionController,
      public AssistantStateBase {
 public:
  static VoiceInteractionController* Get();

  VoiceInteractionController();
  ~VoiceInteractionController() override;

  void BindRequest(mojom::VoiceInteractionControllerRequest request);

  // Called when the voice interaction state is changed.
  virtual void NotifyStatusChanged(mojom::VoiceInteractionState state);

  // Called when the voice interaction settings is enabled/disabled.
  virtual void NotifySettingsEnabled(bool enabled);

  // Called when the voice interaction context is enabled/disabled.
  // If context is enabled the screenshot will be passed in voice
  // interaction session.
  virtual void NotifyContextEnabled(bool enabled);

  // Called when the hotword listening is enabled/disabled.
  virtual void NotifyHotwordEnabled(bool enabled);

  // Notify if voice interaction feature is allowed or not. e.g. not allowed
  // if disabled by policy.
  virtual void NotifyFeatureAllowed(mojom::AssistantAllowedState state);

  // Called when the locale is changed.
  virtual void NotifyLocaleChanged(const std::string& locale);

  // Called when Google Play Store is enabled/disabled.
  virtual void NotifyArcPlayStoreEnabledChanged(bool enabled);

  // Called when locked full screen state is enabled/disabled.
  virtual void NotifyLockedFullScreenStateChanged(bool enabled);

  // ash::mojom::VoiceInteractionController:
  void AddObserver(mojom::VoiceInteractionObserverPtr observer) override;

  // Adding local observers in the same process.
  void AddLocalObserver(DefaultVoiceInteractionObserver* observer);
  void RemoveLocalObserver(DefaultVoiceInteractionObserver* observer);
  void InitObserver(mojom::VoiceInteractionObserver* observer);

  void FlushForTesting();

 private:
  mojo::BindingSet<mojom::VoiceInteractionController> bindings_;

  mojo::InterfacePtrSet<mojom::VoiceInteractionObserver> observers_;

  base::ObserverList<DefaultVoiceInteractionObserver> local_observers_;

  DISALLOW_COPY_AND_ASSIGN(VoiceInteractionController);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_VOICE_INTERACTION_CONTROLLER_H_

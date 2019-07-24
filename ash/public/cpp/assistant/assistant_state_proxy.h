// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_STATE_PROXY_H_
#define ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_STATE_PROXY_H_

#include <string>
#include <vector>

#include "ash/public/cpp/assistant/assistant_state_base.h"
#include "ash/public/cpp/assistant/default_voice_interaction_observer.h"
#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace ash {

// Provides a convenient client to access various Assistant states. The state
// information can be accessed through direct accessors which returns
// |base::Optional<>| or observers. When adding an observer, all change events
// will fire if this client already have data.
class ASH_PUBLIC_EXPORT AssistantStateProxy
    : public AssistantStateBase,
      public mojom::VoiceInteractionObserver {
 public:
  AssistantStateProxy();
  ~AssistantStateProxy() override;

  void Init(service_manager::Connector* connector);
  void AddObserver(DefaultVoiceInteractionObserver* observer);
  void RemoveObserver(DefaultVoiceInteractionObserver* observer);

 private:
  // mojom::VoiceInteractionObserver:
  void OnVoiceInteractionStatusChanged(
      mojom::VoiceInteractionState state) override;
  void OnVoiceInteractionSettingsEnabled(bool enabled) override;
  void OnVoiceInteractionContextEnabled(bool enabled) override;
  void OnVoiceInteractionHotwordEnabled(bool enabled) override;
  void OnAssistantFeatureAllowedChanged(
      mojom::AssistantAllowedState state) override;
  void OnLocaleChanged(const std::string& locale) override;
  void OnArcPlayStoreEnabledChanged(bool enabled) override;
  void OnLockedFullScreenStateChanged(bool enabled) override;

  base::ObserverList<DefaultVoiceInteractionObserver> observers_;

  mojom::VoiceInteractionControllerPtr voice_interaction_controller_;
  mojo::Binding<mojom::VoiceInteractionObserver>
      voice_interaction_observer_binding_;

  DISALLOW_COPY_AND_ASSIGN(AssistantStateProxy);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_STATE_PROXY_H_

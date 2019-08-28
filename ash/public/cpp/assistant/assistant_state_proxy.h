// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_STATE_PROXY_H_
#define ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_STATE_PROXY_H_

#include <string>
#include <vector>

#include "ash/public/cpp/assistant/assistant_state_base.h"
#include "ash/public/mojom/assistant_state_controller.mojom.h"
#include "ash/public/mojom/voice_interaction_controller.mojom.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash {

// Provides a convenient client to access various Assistant states. The state
// information can be accessed through direct accessors which returns
// |base::Optional<>| or observers. When adding an observer, all change events
// will fire if this client already have data.
class ASH_PUBLIC_EXPORT AssistantStateProxy
    : public AssistantStateBase,
      public mojom::AssistantStateObserver {
 public:
  AssistantStateProxy();
  ~AssistantStateProxy() override;

  void Init(mojo::PendingRemote<mojom::AssistantStateController>
                assistant_state_controller);

 private:
  // mojom::AssistantStateObserver:
  void OnAssistantStatusChanged(mojom::VoiceInteractionState state) override;
  void OnAssistantSettingsEnabled(bool enabled) override;
  void OnAssistantHotwordEnabled(bool enabled) override;
  void OnAssistantFeatureAllowedChanged(
      mojom::AssistantAllowedState state) override;
  void OnLocaleChanged(const std::string& locale) override;
  void OnArcPlayStoreEnabledChanged(bool enabled) override;
  void OnLockedFullScreenStateChanged(bool enabled) override;

  mojom::AssistantStateControllerPtr assistant_state_controller_;
  mojo::Binding<mojom::AssistantStateObserver>
      assistant_state_observer_binding_;

  DISALLOW_COPY_AND_ASSIGN(AssistantStateProxy);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_STATE_PROXY_H_

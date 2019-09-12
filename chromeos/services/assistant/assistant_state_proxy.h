// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_STATE_PROXY_H_
#define CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_STATE_PROXY_H_

#include <string>
#include <vector>

#include "ash/public/cpp/assistant/assistant_state_base.h"
#include "ash/public/mojom/assistant_state_controller.mojom.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/services/assistant/pref_connection_delegate.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace assistant {

// Provides a convenient client to access various Assistant states. The state
// information can be accessed through direct accessors which returns
// |base::Optional<>| or observers. When adding an observer, all change events
// will fire if this client already have data.
// Also connect to pref service to register for preference changes.
class COMPONENT_EXPORT(ASSISTANT_SERVICE) AssistantStateProxy
    : public ash::AssistantStateBase,
      public ash::mojom::AssistantStateObserver {
 public:
  AssistantStateProxy();
  ~AssistantStateProxy() override;

  void Init(mojom::ClientProxy* client);

  void SetPrefConnectionDelegateForTesting(
      std::unique_ptr<PrefConnectionDelegate> pref_connection_delegate);

 private:
  // AssistantStateObserver:
  void OnAssistantStatusChanged(ash::mojom::AssistantState state) override;
  void OnAssistantFeatureAllowedChanged(
      ash::mojom::AssistantAllowedState state) override;
  void OnLocaleChanged(const std::string& locale) override;
  void OnArcPlayStoreEnabledChanged(bool enabled) override;
  void OnLockedFullScreenStateChanged(bool enabled) override;

  void OnPrefServiceConnected(std::unique_ptr<::PrefService> pref_service);

  mojo::Remote<ash::mojom::AssistantStateController>
      assistant_state_controller_;
  mojo::Receiver<ash::mojom::AssistantStateObserver>
      assistant_state_observer_receiver_{this};

  std::unique_ptr<PrefService> pref_service_;

  std::unique_ptr<PrefConnectionDelegate> pref_connection_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AssistantStateProxy);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_STATE_PROXY_H_

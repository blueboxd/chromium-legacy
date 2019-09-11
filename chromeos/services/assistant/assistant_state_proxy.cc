// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/assistant_state_proxy.h"

#include <algorithm>
#include <utility>

#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {
namespace assistant {

AssistantStateProxy::AssistantStateProxy()
    : assistant_state_observer_binding_(this),
      pref_connection_delegate_(std::make_unique<PrefConnectionDelegate>()) {}

AssistantStateProxy::~AssistantStateProxy() {
  // Reset pref change registar.
  RegisterPrefChanges(nullptr);
}

void AssistantStateProxy::Init(mojom::ClientProxy* client) {
  // Bind to AssistantStateController.
  mojo::PendingRemote<ash::mojom::AssistantStateController> remote_controller;
  client->RequestAssistantStateController(
      remote_controller.InitWithNewPipeAndPassReceiver());
  assistant_state_controller_.Bind(std::move(remote_controller));

  ash::mojom::AssistantStateObserverPtr ptr;
  assistant_state_observer_binding_.Bind(mojo::MakeRequest(&ptr));
  assistant_state_controller_->AddMojomObserver(std::move(ptr));

  // Connect to pref service.
  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
  prefs::RegisterProfilePrefsForeign(pref_registry.get());
  mojo::PendingRemote<::prefs::mojom::PrefStoreConnector> remote_connector;
  client->RequestPrefStoreConnector(
      remote_connector.InitWithNewPipeAndPassReceiver());
  pref_connection_delegate_->ConnectToPrefService(
      std::move(remote_connector), std::move(pref_registry),
      base::Bind(&AssistantStateProxy::OnPrefServiceConnected,
                 base::Unretained(this)));
}

void AssistantStateProxy::SetPrefConnectionDelegateForTesting(
    std::unique_ptr<PrefConnectionDelegate> pref_connection_delegate) {
  pref_connection_delegate_ = std::move(pref_connection_delegate);
}

void AssistantStateProxy::OnAssistantStatusChanged(
    ash::mojom::AssistantState state) {
  UpdateAssistantStatus(state);
}

void AssistantStateProxy::OnAssistantFeatureAllowedChanged(
    ash::mojom::AssistantAllowedState state) {
  UpdateFeatureAllowedState(state);
}

void AssistantStateProxy::OnLocaleChanged(const std::string& locale) {
  UpdateLocale(locale);
}

void AssistantStateProxy::OnArcPlayStoreEnabledChanged(bool enabled) {
  UpdateArcPlayStoreEnabled(enabled);
}

void AssistantStateProxy::OnLockedFullScreenStateChanged(bool enabled) {
  UpdateLockedFullScreenState(enabled);
}

void AssistantStateProxy::OnPrefServiceConnected(
    std::unique_ptr<::PrefService> pref_service) {
  // TODO(b/110211045): Add testing support for Assistant prefs.
  if (!pref_service)
    return;

  pref_service_ = std::move(pref_service);
  RegisterPrefChanges(pref_service_.get());
}

}  // namespace assistant
}  // namespace chromeos

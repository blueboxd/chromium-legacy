// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <utility>

#include "ash/public/cpp/assistant/assistant_state_proxy.h"
#include "ash/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace ash {

AssistantStateProxy::AssistantStateProxy()
    : assistant_state_observer_binding_(this) {}

AssistantStateProxy::~AssistantStateProxy() = default;

void AssistantStateProxy::Init(
    mojo::PendingRemote<mojom::AssistantStateController>
        assistant_state_controller) {
  assistant_state_controller_.Bind(std::move(assistant_state_controller));

  mojom::AssistantStateObserverPtr ptr;
  assistant_state_observer_binding_.Bind(mojo::MakeRequest(&ptr));
  assistant_state_controller_->AddMojomObserver(std::move(ptr));
}

void AssistantStateProxy::OnAssistantStatusChanged(
    ash::mojom::VoiceInteractionState state) {
  voice_interaction_state_ = state;
  for (auto& observer : observers_)
    observer.OnAssistantStatusChanged(voice_interaction_state_);
}

void AssistantStateProxy::OnAssistantSettingsEnabled(bool enabled) {
  settings_enabled_ = enabled;
  for (auto& observer : observers_)
    observer.OnAssistantSettingsEnabled(settings_enabled_.value());
}

void AssistantStateProxy::OnAssistantHotwordEnabled(bool enabled) {
  hotword_enabled_ = enabled;
  for (auto& observer : observers_)
    observer.OnAssistantHotwordEnabled(hotword_enabled_.value());
}

void AssistantStateProxy::OnAssistantFeatureAllowedChanged(
    ash::mojom::AssistantAllowedState state) {
  allowed_state_ = state;
  for (auto& observer : observers_)
    observer.OnAssistantFeatureAllowedChanged(allowed_state_.value());
}

void AssistantStateProxy::OnLocaleChanged(const std::string& locale) {
  locale_ = locale;
  for (auto& observer : observers_)
    observer.OnLocaleChanged(locale_.value());
}

void AssistantStateProxy::OnArcPlayStoreEnabledChanged(bool enabled) {
  arc_play_store_enabled_ = enabled;
  for (auto& observer : observers_)
    observer.OnArcPlayStoreEnabledChanged(arc_play_store_enabled_.value());
}

void AssistantStateProxy::OnLockedFullScreenStateChanged(bool enabled) {
  locked_full_screen_enabled_ = enabled;
  for (auto& observer : observers_) {
    observer.OnLockedFullScreenStateChanged(
        locked_full_screen_enabled_.value());
  }
}

}  // namespace ash

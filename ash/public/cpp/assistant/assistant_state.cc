// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/assistant/assistant_state.h"

#include <ostream>
#include <sstream>

namespace ash {
namespace {

AssistantState* g_assistant_state = nullptr;

}  // namespace

// static
AssistantState* AssistantState::Get() {
  return g_assistant_state;
}

AssistantState::AssistantState() {
  DCHECK(!g_assistant_state);
  g_assistant_state = this;
}

AssistantState::~AssistantState() {
  DCHECK_EQ(g_assistant_state, this);
  g_assistant_state = nullptr;
}

void AssistantState::BindRequest(
    mojom::AssistantStateControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void AssistantState::NotifyStatusChanged(mojom::AssistantState state) {
  if (assistant_state_ == state)
    return;

  UpdateAssistantStatus(state);
  remote_observers_.ForAllPtrs(
      [state](auto* observer) { observer->OnAssistantStatusChanged(state); });
}

void AssistantState::NotifyFeatureAllowed(mojom::AssistantAllowedState state) {
  if (allowed_state_ == state)
    return;

  UpdateFeatureAllowedState(state);
  remote_observers_.ForAllPtrs([state](auto* observer) {
    observer->OnAssistantFeatureAllowedChanged(state);
  });
}

void AssistantState::NotifyLocaleChanged(const std::string& locale) {
  if (locale_ == locale)
    return;

  UpdateLocale(locale);
  remote_observers_.ForAllPtrs(
      [locale](auto* observer) { observer->OnLocaleChanged(locale); });
}

void AssistantState::NotifyArcPlayStoreEnabledChanged(bool enabled) {
  if (arc_play_store_enabled_ == enabled)
    return;

  UpdateArcPlayStoreEnabled(enabled);
  remote_observers_.ForAllPtrs([enabled](auto* observer) {
    observer->OnArcPlayStoreEnabledChanged(enabled);
  });
}

void AssistantState::NotifyLockedFullScreenStateChanged(bool enabled) {
  if (locked_full_screen_enabled_ == enabled)
    return;

  UpdateLockedFullScreenState(enabled);
  remote_observers_.ForAllPtrs([enabled](auto* observer) {
    observer->OnLockedFullScreenStateChanged(enabled);
  });
}

void AssistantState::AddMojomObserver(
    mojom::AssistantStateObserverPtr observer) {
  auto* observer_ptr = observer.get();
  remote_observers_.AddPtr(std::move(observer));
  InitializeObserverMojom(observer_ptr);
}

}  // namespace ash

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ambient/ambient_mode_state.h"

namespace ash {

namespace {

AmbientModeState* g_ambient_mode_state = nullptr;

}  // namespace

// static
AmbientModeState* AmbientModeState::Get() {
  return g_ambient_mode_state;
}

AmbientModeState::AmbientModeState() {
  DCHECK(!g_ambient_mode_state);
  g_ambient_mode_state = this;
}

AmbientModeState::~AmbientModeState() {
  DCHECK_EQ(g_ambient_mode_state, this);
  g_ambient_mode_state = nullptr;
}

void AmbientModeState::AddObserver(AmbientModeStateObserver* observer) {
  observers_.AddObserver(observer);
}

void AmbientModeState::RemoveObserver(AmbientModeStateObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AmbientModeState::SetAmbientModeEnabled(bool enabled) {
  if (enabled_ == enabled)
    return;

  enabled_ = enabled;
  NotifyAmbientModeEnabled();
}

void AmbientModeState::NotifyAmbientModeEnabled() {
  for (auto& observer : observers_)
    observer.OnAmbientModeEnabled(enabled_);
}

}  // namespace ash

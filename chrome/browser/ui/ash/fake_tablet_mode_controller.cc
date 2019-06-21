// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/fake_tablet_mode_controller.h"

#include <utility>

FakeTabletModeController::FakeTabletModeController() = default;

FakeTabletModeController::~FakeTabletModeController() = default;

void FakeTabletModeController::SetTabletModeToggleObserver(
    ash::TabletModeToggleObserver* observer) {
  observer_ = observer;
}

bool FakeTabletModeController::InTabletMode() const {
  return enabled_;
}

void FakeTabletModeController::SetEnabledForTest(bool enabled) {
  enabled_ = enabled;
}

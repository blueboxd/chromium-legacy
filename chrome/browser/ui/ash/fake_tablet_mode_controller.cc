// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/fake_tablet_mode_controller.h"

#include <utility>
#include "base/logging.h"

FakeTabletModeController::FakeTabletModeController() = default;

FakeTabletModeController::~FakeTabletModeController() = default;

void FakeTabletModeController::AddObserver(ash::TabletModeObserver* observer) {
  observer_ = observer;
}

void FakeTabletModeController::RemoveObserver(
    ash::TabletModeObserver* observer) {
  DCHECK_EQ(observer_, observer);
  observer_ = nullptr;
}

bool FakeTabletModeController::InTabletMode() const {
  return enabled_;
}

void FakeTabletModeController::SetEnabledForTest(bool enabled) {
  enabled_ = enabled;
}

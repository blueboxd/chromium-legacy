// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/test_tablet_mode.h"

namespace ash {

TestTabletMode::TestTabletMode() = default;
TestTabletMode::~TestTabletMode() = default;

void TestTabletMode::AddObserver(TabletModeObserver* observer) {}
void TestTabletMode::RemoveObserver(TabletModeObserver* observer) {}

bool TestTabletMode::InTabletMode() const {
  return in_tablet_mode_;
}

void TestTabletMode::ForceUiTabletModeState(base::Optional<bool> enabled) {}

void TestTabletMode::SetEnabledForTest(bool enabled) {
  in_tablet_mode_ = enabled;
}

}  // namespace ash
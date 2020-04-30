// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_TEST_TABLET_MODE_H_
#define ASH_PUBLIC_CPP_TEST_TEST_TABLET_MODE_H_

#include "ash/public/cpp/tablet_mode.h"

namespace ash {

// TabletMode implementation for test support.
class TestTabletMode : public TabletMode {
 public:
  TestTabletMode();
  ~TestTabletMode() override;

  // TabletMode:
  void AddObserver(ash::TabletModeObserver* observer) override;
  void RemoveObserver(ash::TabletModeObserver* observer) override;
  bool InTabletMode() const override;
  void ForceUiTabletModeState(base::Optional<bool> enabled) override;
  void SetEnabledForTest(bool enabled) override;

 private:
  bool in_tablet_mode_ = false;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_TEST_TABLET_MODE_H_
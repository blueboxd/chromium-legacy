// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_FAKE_TABLET_MODE_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_FAKE_TABLET_MODE_CONTROLLER_H_

#include "ash/public/cpp/tablet_mode.h"
#include "base/macros.h"

// Simulates the TabletModeController in ash.
class FakeTabletModeController : public ash::TabletMode {
 public:
  FakeTabletModeController();

  ~FakeTabletModeController() override;

  bool has_observer() const { return !!observer_; }

  // ash::TabletMode:
  void AddObserver(ash::TabletModeObserver* observer) override;
  void RemoveObserver(ash::TabletModeObserver* observer) override;
  bool InTabletMode() const override;
  void SetEnabledForTest(bool enabled) override;

 private:
  bool enabled_ = false;
  ash::TabletModeObserver* observer_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeTabletModeController);
};

#endif  // CHROME_BROWSER_UI_ASH_FAKE_TABLET_MODE_CONTROLLER_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TABLET_MODE_TOGGLE_OBSERVER_H_
#define ASH_PUBLIC_CPP_TABLET_MODE_TOGGLE_OBSERVER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"

namespace ash {

// A simplified observer which allows Ash to inform Chrome when tablet mode has
// been enabled or disabled.
class ASH_PUBLIC_EXPORT TabletModeToggleObserver
    : public base::CheckedObserver {
 public:
  // Fired after the tablet mode has been toggled.
  virtual void OnTabletModeToggled(bool enabled) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TABLET_MODE_TOGGLE_OBSERVER_H_

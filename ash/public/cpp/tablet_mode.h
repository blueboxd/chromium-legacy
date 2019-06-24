// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TABLET_MODE_H_
#define ASH_PUBLIC_CPP_TABLET_MODE_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

class TabletModeToggleObserver;

// An interface implemented by Ash that allows Chrome to be informed of changes
// to tablet mode state.
class ASH_PUBLIC_EXPORT TabletMode {
 public:
  // Returns the singleton instance.
  static TabletMode* Get();

  virtual void SetTabletModeToggleObserver(
      TabletModeToggleObserver* observer) = 0;

  // Returns true if the system is in tablet mode.
  virtual bool InTabletMode() const = 0;

  virtual void SetEnabledForTest(bool enabled) = 0;

 protected:
  TabletMode();
  virtual ~TabletMode();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TABLET_MODE_H_

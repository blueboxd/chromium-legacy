// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_WALLPAPER_CONTROLLER_H_
#define ASH_WM_OVERVIEW_OVERVIEW_WALLPAPER_CONTROLLER_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/macros.h"

namespace ash {

// Class that controls when and how to apply blur and dimming wallpaper upon
// entering and exiting overview mode. Blurs the wallpaper automatically if the
// wallpaper is not visible prior to entering overview mode (covered by a
// window), otherwise animates the blur and dim.
class ASH_EXPORT OverviewWallpaperController {
 public:
  OverviewWallpaperController() = default;
  ~OverviewWallpaperController() = default;

  // There is no need to blur or dim the wallpaper for tests.
  static void SetDoNotChangeWallpaperForTests();

  void Blur(bool animate_only);
  void Unblur();

 private:
  // Called when the wallpaper is to be changed. Checks to see which root
  // windows should have their wallpaper blurs animated.
  void OnBlurChange(bool should_blur, bool animate_only);

  DISALLOW_COPY_AND_ASSIGN(OverviewWallpaperController);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_WALLPAPER_CONTROLLER_H_

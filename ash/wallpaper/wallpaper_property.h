// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_PROPERTY_H_
#define ASH_WALLPAPER_WALLPAPER_PROPERTY_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/login_constants.h"
#include "ash/wm/overview/overview_constants.h"

namespace ash {

// Defines the blur sigma and opacity values used when painting
// wallpaper.
struct ASH_EXPORT WallpaperProperty {
  float blur_sigma;
  float opacity;

  bool operator==(const WallpaperProperty& p) const {
    return blur_sigma == p.blur_sigma && opacity == p.opacity;
  }
};

namespace wallpaper_constants {

// Blur sigma and opacity used for normal wallpaper.
constexpr WallpaperProperty kClear{0.f, 1.f};
// Blur sigma and opacity used in normal overview mode.
constexpr WallpaperProperty kOverviewState{overview_constants::kBlurSigma,
                                           overview_constants::kOpacity};
// Blur sigma and opacity used in tablet overview mode.
constexpr WallpaperProperty kOverviewInTabletState{
    overview_constants::kBlurSigma, 1.f};
// Blur sigma and opacity used in lock/login screen.
constexpr WallpaperProperty kLockState{login_constants::kBlurSigma, 1.f};

}  // namespace wallpaper_constants

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_PROPERTY_H_

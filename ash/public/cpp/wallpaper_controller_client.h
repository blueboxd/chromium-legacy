// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WALLPAPER_CONTROLLER_CLIENT_H_
#define ASH_PUBLIC_CPP_WALLPAPER_CONTROLLER_CLIENT_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// Used by ash to control a Chrome client of the WallpaperController.
class ASH_PUBLIC_EXPORT WallpaperControllerClient {
 public:
  // Opens the wallpaper picker window.
  virtual void OpenWallpaperPicker() = 0;

  // Signals to the client that ash is ready to set wallpapers. The client is
  // able to decide whatever the first wallpaper it wants to display.
  virtual void OnReadyToSetWallpaper() = 0;

  // Notifies the client that the animation of the first wallpaper since the
  // controller initialization has completed.
  // TODO(crbug.com/875128): Remove this after web-ui login code is completely
  // removed.
  virtual void OnFirstWallpaperAnimationFinished() = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WALLPAPER_CONTROLLER_CLIENT_H_

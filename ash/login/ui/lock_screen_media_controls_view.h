// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOCK_SCREEN_MEDIA_CONTROLS_VIEW_H_
#define ASH_LOGIN_UI_LOCK_SCREEN_MEDIA_CONTROLS_VIEW_H_

#include "ash/ash_export.h"
#include "ash/login/ui/lock_screen.h"
#include "ui/views/view.h"

namespace ash {

class LoginExpandedPublicAccountView;

class LockScreenMediaControlsView : public views::View {
 public:
  LockScreenMediaControlsView();
  ~LockScreenMediaControlsView() override;

  static bool AreMediaControlsEnabled(
      LockScreen::ScreenType screen_type,
      LoginExpandedPublicAccountView* expanded_view);

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  const char* GetClassName() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LockScreenMediaControlsView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOCK_SCREEN_MEDIA_CONTROLS_VIEW_H_

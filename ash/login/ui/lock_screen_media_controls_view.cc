// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_screen_media_controls_view.h"
#include "ash/login/ui/login_expanded_public_account_view.h"
#include "ash/media/media_controller_impl.h"
#include "ash/public/cpp/ash_features.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/background.h"

namespace ash {

namespace {
// Total width of the media controls view.
constexpr int kMediaControlsTotalWidthDp = 320;

// Total height of the media controls view.
constexpr int kMediaControlsTotalHeightDp = 400;

constexpr const char kLockScreenMediaControlsViewName[] =
    "LockScreenMediaControlsView";
}  // namespace

LockScreenMediaControlsView::LockScreenMediaControlsView() {
  SetBackground(views::CreateSolidBackground(SK_ColorBLACK));
}

LockScreenMediaControlsView::~LockScreenMediaControlsView() = default;

// static
bool LockScreenMediaControlsView::AreMediaControlsEnabled(
    LockScreen::ScreenType screen_type,
    LoginExpandedPublicAccountView* expanded_view) {
  return MediaControllerImpl::AreLockScreenMediaKeysEnabled() &&
         base::FeatureList::IsEnabled(features::kLockScreenMediaControls) &&
         screen_type == LockScreen::ScreenType::kLock &&
         !expanded_view->GetVisible();
}

// views::View:
gfx::Size LockScreenMediaControlsView::CalculatePreferredSize() const {
  return gfx::Size(kMediaControlsTotalWidthDp, kMediaControlsTotalHeightDp);
}

const char* LockScreenMediaControlsView::GetClassName() const {
  return kLockScreenMediaControlsViewName;
}

}  // namespace ash

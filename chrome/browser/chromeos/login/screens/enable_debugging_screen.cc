// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/enable_debugging_screen.h"

#include "base/check.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"

namespace chromeos {

EnableDebuggingScreen::EnableDebuggingScreen(
    EnableDebuggingScreenView* view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(EnableDebuggingScreenView::kScreenId,
                 OobeScreenPriority::SCREEN_ENABLE_DEBUGGING),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  if (view_)
    view_->SetDelegate(this);
}

EnableDebuggingScreen::~EnableDebuggingScreen() {
  if (view_)
    view_->SetDelegate(nullptr);
}

void EnableDebuggingScreen::OnExit(bool success) {
  if (is_hidden())
    return;
  exit_callback_.Run();
}

void EnableDebuggingScreen::OnViewDestroyed(EnableDebuggingScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void EnableDebuggingScreen::ShowImpl() {
  if (view_)
    view_->Show();
}

void EnableDebuggingScreen::HideImpl() {
  if (view_)
    view_->Hide();
}

}  // namespace chromeos

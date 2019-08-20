// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/update_required_screen.h"

#include <algorithm>

#include "base/bind.h"
#include "chrome/browser/ui/webui/chromeos/login/update_required_screen_handler.h"

namespace chromeos {

UpdateRequiredScreen::UpdateRequiredScreen(UpdateRequiredView* view)
    : BaseScreen(UpdateRequiredView::kScreenId), view_(view) {
  if (view_)
    view_->Bind(this);
}

UpdateRequiredScreen::~UpdateRequiredScreen() {
  if (view_)
    view_->Unbind();
}

void UpdateRequiredScreen::OnViewDestroyed(UpdateRequiredView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void UpdateRequiredScreen::Show() {
  is_shown_ = true;

  if (view_)
    view_->Show();
}

void UpdateRequiredScreen::Hide() {
  if (view_)
    view_->Hide();
  is_shown_ = false;
}

}  // namespace chromeos

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/packaged_license_screen.h"

#include "chrome/browser/ui/webui/chromeos/login/packaged_license_screen_handler.h"

namespace {

constexpr const char kUserActionEnrollButtonClicked[] = "enroll";
constexpr const char kUserActionDontEnrollButtonClicked[] = "dont-enroll";

}  // namespace

namespace chromeos {

PackagedLicenseScreen::PackagedLicenseScreen(
    PackagedLicenseView* view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(PackagedLicenseView::kScreenId),
      view_(view),
      exit_callback_(exit_callback) {
  if (view_)
    view_->Bind(this);
}

PackagedLicenseScreen::~PackagedLicenseScreen() {
  if (view_)
    view_->Unbind();
}

void PackagedLicenseScreen::Show() {
  if (view_)
    view_->Show();
}

void PackagedLicenseScreen::Hide() {
  if (view_)
    view_->Hide();
}

void PackagedLicenseScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionEnrollButtonClicked)
    exit_callback_.Run(Result::ENROLL);
  else if (action_id == kUserActionDontEnrollButtonClicked)
    exit_callback_.Run(Result::DONT_ENROLL);
  else
    BaseScreen::OnUserAction(action_id);
}

}  // namespace chromeos

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/fingerprint_setup_screen.h"

#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/fingerprint_setup_screen_handler.h"

namespace chromeos {
namespace {

constexpr char kUserActionClose[] = "fingerprint-setup-done";

}  // namespace

// static
std::string FingerprintSetupScreen::GetResultString(Result result) {
  switch (result) {
    case Result::NEXT:
      return "Next";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
}

FingerprintSetupScreen* FingerprintSetupScreen::Get(ScreenManager* manager) {
  return static_cast<FingerprintSetupScreen*>(
      manager->GetScreen(FingerprintSetupScreenView::kScreenId));
}

FingerprintSetupScreen::FingerprintSetupScreen(
    FingerprintSetupScreenView* view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(FingerprintSetupScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  view_->Bind(this);
}

FingerprintSetupScreen::~FingerprintSetupScreen() {
  view_->Bind(nullptr);
}

bool FingerprintSetupScreen::MaybeSkip() {
  if (!chromeos::quick_unlock::IsFingerprintEnabled(
          ProfileManager::GetActiveUserProfile()) ||
      chrome_user_manager_util::IsPublicSessionOrEphemeralLogin()) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }
  return false;
}

void FingerprintSetupScreen::ShowImpl() {
  view_->Show();
}

void FingerprintSetupScreen::HideImpl() {
  view_->Hide();
}

void FingerprintSetupScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionClose) {
    exit_callback_.Run(Result::NEXT);
    return;
  }
  BaseScreen::OnUserAction(action_id);
}

}  // namespace chromeos

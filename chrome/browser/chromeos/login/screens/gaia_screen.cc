// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/gaia_screen.h"

#include "chrome/browser/chromeos/login/screen_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "components/account_id/account_id.h"

namespace chromeos {

GaiaScreen::GaiaScreen()
    : BaseScreen(GaiaView::kScreenId, OobeScreenPriority::DEFAULT) {}

// static
GaiaScreen* GaiaScreen::Get(ScreenManager* manager) {
  return static_cast<GaiaScreen*>(manager->GetScreen(GaiaView::kScreenId));
}

void GaiaScreen::MaybePreloadAuthExtension() {
  view_->MaybePreloadAuthExtension();
}

void GaiaScreen::LoadOnline(const AccountId& account) {
  view_->LoadGaiaAsync(account);
}

void GaiaScreen::LoadOffline(const AccountId& account) {
  view_->LoadOfflineGaia(account);
}

void GaiaScreen::ShowImpl() {
  view_->Show();
}

void GaiaScreen::HideImpl() {
  view_->LoadGaiaAsync(EmptyAccountId());
  view_->Hide();
}

}  // namespace chromeos

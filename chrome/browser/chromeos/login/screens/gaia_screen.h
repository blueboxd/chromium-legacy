// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_GAIA_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_GAIA_SCREEN_H_

#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"

namespace chromeos {

class GaiaView;
class ScreenManager;

// This class represents GAIA screen: login screen that is responsible for
// GAIA-based sign-in.
class GaiaScreen : public BaseScreen {
 public:
  GaiaScreen();
  ~GaiaScreen() override = default;

  static GaiaScreen* Get(ScreenManager* manager);

  void set_view(GaiaView* view) { view_ = view; }

  void MaybePreloadAuthExtension();
  // Loads online Gaia into the webview.
  void LoadOnline(const AccountId& account);
  // Loads offline version of Gaia.
  void LoadOffline(const AccountId& account);

 private:
  void ShowImpl() override;
  void HideImpl() override;

  GaiaView* view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(GaiaScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_GAIA_SCREEN_H_

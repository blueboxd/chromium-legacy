// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MARKETING_OPT_IN_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MARKETING_OPT_IN_SCREEN_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "components/prefs/pref_change_registrar.h"

namespace chromeos {

class MarketingOptInScreenView;

// This is Sync settings screen that is displayed as a part of user first
// sign-in flow.
class MarketingOptInScreen : public BaseScreen {
 public:
  MarketingOptInScreen(MarketingOptInScreenView* view,
                       const base::RepeatingClosure& exit_callback);
  ~MarketingOptInScreen() override;

  // On "All set" button pressed.
  void OnAllSet(bool play_communications_opt_in,
                bool tips_communications_opt_in);

  void set_exit_callback_for_testing(
      const base::RepeatingClosure& exit_callback) {
    exit_callback_ = exit_callback;
  }

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;

 private:
  // Exits the screen.
  void ExitScreen();

  void OnA11yShelfNavigationButtonPrefChanged();

  MarketingOptInScreenView* const view_;

  // Whether the screen is shown and exit callback has not been run.
  bool active_ = false;

  base::RepeatingClosure exit_callback_;

  std::unique_ptr<PrefChangeRegistrar> active_user_pref_change_registrar_;

  base::WeakPtrFactory<MarketingOptInScreen> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MarketingOptInScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MARKETING_OPT_IN_SCREEN_H_

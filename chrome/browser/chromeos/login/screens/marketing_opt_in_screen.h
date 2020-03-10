// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MARKETING_OPT_IN_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MARKETING_OPT_IN_SCREEN_H_

#include "ash/public/cpp/shelf_config.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "components/prefs/pref_change_registrar.h"

namespace chromeos {

class MarketingOptInScreenView;

// This is Sync settings screen that is displayed as a part of user first
// sign-in flow.
class MarketingOptInScreen : public BaseScreen,
                             public ash::ShelfConfig::Observer {
 public:
  MarketingOptInScreen(MarketingOptInScreenView* view,
                       bool is_fullscreen,
                       const base::RepeatingClosure& exit_callback);
  ~MarketingOptInScreen() override;

  // On "All set" button pressed.
  void OnAllSet(bool play_communications_opt_in,
                bool tips_communications_opt_in);

  // Called when the visibility of the accessibility page within the screen
  // changes.
  void OnAccessibilityPageVisibilityChanged(bool shown);

  // ash::ShelfCondif::Observer:
  void OnShelfConfigUpdated() override;

  void set_exit_callback_for_testing(
      const base::RepeatingClosure& exit_callback) {
    exit_callback_ = exit_callback;
  }

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;

 private:
  // Enables or disables shelf gesture handling depending on the current state.
  // Gesture handling should be disabled if shelf navigation buttons should be
  // shown in this context, or if the screen is on the accessibility settings
  // page.
  // Gesture handling is only supported if the screen is shown in fullscreen
  // OOBE.
  void UpdateShelfGestureHandlingState();

  // Called when a fling from the shelf is detected - it exits the screen.
  // This is the fling callback passed to
  // LoginScreen::SetLoginShelfGestureHandler().
  void HandleFlingFromShelf();

  // Called when the login shelf gesture detection stops.
  // This is the exit callback passed to
  // LoginScreen::SetLoginShelfGestureHandler().
  void OnShelfGestureDetectionStopped();

  // Exits the screen - it clears the login shelf gesture handler, and runs the
  // exit callback as needed.
  void ExitScreen();

  // Clears login shelf gesture handler if the screen is handling shelf
  // gestures.
  void ClearLoginShelfGestureHandler();

  void OnA11yShelfNavigationButtonPrefChanged();

  MarketingOptInScreenView* const view_;

  // Whether the screen is shown as part the first run with OOBE display type,
  // i.e. if the screen UI is shown fullscreen. Note that the shelf swipe
  // gesture will only be enabled in fullscreen mode.
  const bool is_fullscreen_;

  // Whether the screen is shown and exit callback has not been run.
  bool active_ = false;

  // Whether the screen has set a login shelf gesture handler.
  bool handling_shelf_gestures_ = false;

  // Whether the accessibility page (that contains an option to make the shelf
  // navigation buttons show in tablet mode) is currently shown.
  bool accessibility_page_shown_ = false;

  base::RepeatingClosure exit_callback_;

  ScopedObserver<ash::ShelfConfig, ash::ShelfConfig::Observer>
      shelf_config_observer_{this};

  std::unique_ptr<PrefChangeRegistrar> active_user_pref_change_registrar_;

  base::WeakPtrFactory<MarketingOptInScreen> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MarketingOptInScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MARKETING_OPT_IN_SCREEN_H_

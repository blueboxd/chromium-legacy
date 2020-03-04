// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_GESTURE_NAVIGATION_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_GESTURE_NAVIGATION_SCREEN_H_

#include <string>

#include "base/callback.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/gesture_navigation_screen_handler.h"

namespace chromeos {

// The OOBE screen dedicated to gesture navigation education.
class GestureNavigationScreen : public BaseScreen {
 public:
  GestureNavigationScreen(GestureNavigationScreenView* view,
                          const base::RepeatingClosure& exit_callback);
  ~GestureNavigationScreen() override;

  GestureNavigationScreen(const GestureNavigationScreen&) = delete;
  GestureNavigationScreen operator=(const GestureNavigationScreen&) = delete;

  void set_exit_callback_for_testing(
      const base::RepeatingClosure& exit_callback) {
    exit_callback_ = exit_callback;
  }

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;

 private:
  GestureNavigationScreenView* view_;
  base::RepeatingClosure exit_callback_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_GESTURE_NAVIGATION_SCREEN_H_

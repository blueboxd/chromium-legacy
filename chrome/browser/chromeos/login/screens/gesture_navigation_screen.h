// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_GESTURE_NAVIGATION_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_GESTURE_NAVIGATION_SCREEN_H_

#include <map>
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

  // Called when the currently shown page is changed.
  void GesturePageChange(const std::string& new_page);

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;

 private:
  // Record metrics for the elapsed time that each page was shown for.
  void RecordPageShownTimeMetrics();

  GestureNavigationScreenView* view_;
  base::RepeatingClosure exit_callback_;

  // Used to keep track of the current elapsed time that each page has been
  // shown for.
  std::map<std::string, base::TimeDelta> page_times_;

  // The current page that is shown on the gesture navigation screen.
  std::string current_page_;

  // The starting time for the most recently shown page.
  base::TimeTicks start_time_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_GESTURE_NAVIGATION_SCREEN_H_

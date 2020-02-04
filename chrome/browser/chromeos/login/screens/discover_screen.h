// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_DISCOVER_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_DISCOVER_SCREEN_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"

namespace chromeos {

class DiscoverScreenView;

class DiscoverScreen : public BaseScreen {
 public:
  DiscoverScreen(DiscoverScreenView* view,
                 const base::RepeatingClosure& exit_callback);
  ~DiscoverScreen() override;

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;

 private:
  DiscoverScreenView* const view_;
  base::RepeatingClosure exit_callback_;

  DISALLOW_COPY_AND_ASSIGN(DiscoverScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_DISCOVER_SCREEN_H_

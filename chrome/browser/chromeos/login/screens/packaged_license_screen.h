// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_PACKAGED_LICENSE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_PACKAGED_LICENSE_SCREEN_H_

#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"

namespace chromeos {

class PackagedLicenseView;

// Screen which is shown before login and enterprise screens.
// It advertises the packaged license which allows user enroll device.
class PackagedLicenseScreen : public BaseScreen {
 public:
  enum class Result {
    // Show login screen
    DONT_ENROLL,
    // Show enterprise enrollment screen
    ENROLL
  };
  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  PackagedLicenseScreen(PackagedLicenseView* view,
                        const ScreenExitCallback& exit_callback);
  PackagedLicenseScreen(const PackagedLicenseScreen&) = delete;
  PackagedLicenseScreen& operator=(const PackagedLicenseScreen&) = delete;
  ~PackagedLicenseScreen() override;

  // BaseScreen
  void Show() override;
  void Hide() override;
  void OnUserAction(const std::string& action_id) override;

 private:
  PackagedLicenseView* view_ = nullptr;

  ScreenExitCallback exit_callback_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_PACKAGED_LICENSE_SCREEN_H_

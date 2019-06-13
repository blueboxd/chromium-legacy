// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SUPERVISION_ONBOARDING_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SUPERVISION_ONBOARDING_SCREEN_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/supervision/onboarding_delegate.h"

namespace chromeos {

class SupervisionOnboardingScreenView;

class SupervisionOnboardingScreen : public BaseScreen,
                                    public supervision::OnboardingDelegate {
 public:
  enum class Result {
    // User reached the end of the flow and exited successfully.
    kFinished,
    // User chose to skip the flow or we skipped the flow for internal reasons.
    kSkipped,
  };
  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  SupervisionOnboardingScreen(SupervisionOnboardingScreenView* view,
                              const ScreenExitCallback& exit_callback);
  ~SupervisionOnboardingScreen() override;

  // BaseScreen:
  void Show() override;
  void Hide() override;

  // Called when view is destroyed so there's no dead reference to it.
  void OnViewDestroyed(SupervisionOnboardingScreenView* view);

 private:
  // supervision::OnboardingDelegate:
  void SkipOnboarding() override;
  void FinishOnboarding() override;

  SupervisionOnboardingScreenView* view_;
  ScreenExitCallback exit_callback_;

  DISALLOW_COPY_AND_ASSIGN(SupervisionOnboardingScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SUPERVISION_ONBOARDING_SCREEN_H_

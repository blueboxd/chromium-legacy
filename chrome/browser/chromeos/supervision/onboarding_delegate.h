// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SUPERVISION_ONBOARDING_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_SUPERVISION_ONBOARDING_DELEGATE_H_

namespace chromeos {
namespace supervision {

// Interface for classes that host the Supervision Onboarding flow. Since the
// flow might be hosted by different WebUIs, this is the common interface used
// to communicate between internal onboarding classes and their WebUI handlers.
class OnboardingDelegate {
 public:
  virtual ~OnboardingDelegate() {}

  // Called when we want to skip the onboarding flow. This can happen if the
  // user actively skipped the flow or we decided that the flow should be
  // skipped for other reasons (errors, missing flags, eligibility, etc).
  virtual void SkipOnboarding() = 0;

  // Called when the user successfully finishes the onboarding flow by reaching
  // its conclusion.
  virtual void FinishOnboarding() = 0;
};

}  // namespace supervision
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SUPERVISION_ONBOARDING_DELEGATE_H_

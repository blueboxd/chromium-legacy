// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SUPERVISION_KIOSK_NEXT_FLOW_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_SUPERVISION_KIOSK_NEXT_FLOW_OBSERVER_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/supervision/mojom/onboarding_controller.mojom.h"
#include "chrome/browser/chromeos/supervision/onboarding_flow_model.h"

class Profile;

namespace chromeos {
namespace supervision {

// Observes the Supervision Onboarding flow and sets prefs related to
// Kiosk Next.
class KioskNextFlowObserver : public OnboardingFlowModel::Observer {
 public:
  explicit KioskNextFlowObserver(Profile* profile,
                                 OnboardingFlowModel* flow_model);
  ~KioskNextFlowObserver() override;

 private:
  // OnboardingFlowModel::Observer:
  void StepFinishedLoading(OnboardingFlowModel::Step step) override;
  void WillExitFlow(OnboardingFlowModel::Step step,
                    OnboardingFlowModel::ExitReason reason) override;

  // Enables Kiosk Next by loading its extension and setting the relevant
  // prefs.
  void EnableKioskNext();

  Profile* profile_;
  OnboardingFlowModel* flow_model_;

  DISALLOW_COPY_AND_ASSIGN(KioskNextFlowObserver);
};

}  // namespace supervision
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SUPERVISION_KIOSK_NEXT_FLOW_OBSERVER_H_

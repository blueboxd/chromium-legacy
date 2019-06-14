// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SUPERVISION_ONBOARDING_LOGGER_H_
#define CHROME_BROWSER_CHROMEOS_SUPERVISION_ONBOARDING_LOGGER_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/supervision/mojom/onboarding_controller.mojom.h"
#include "chrome/browser/chromeos/supervision/onboarding_flow_model.h"

namespace chromeos {
namespace supervision {

// Records onboarding flow changes.
//
// TODO(crbug.com/958995): Update UMA metrics as well. We want to see how many
// users get errors, have missing header values or actually end up seeing the
// page.
class OnboardingLogger : public OnboardingFlowModel::Observer {
 public:
  explicit OnboardingLogger(OnboardingFlowModel* flow_model);
  ~OnboardingLogger() override;

 private:
  // OnboardingFlowModel::Observer:
  void StepStartedLoading(OnboardingFlowModel::Step step) override;
  void StepFinishedLoading(OnboardingFlowModel::Step step) override;
  void StepFailedToLoadDueToAuthError(OnboardingFlowModel::Step step,
                                      GoogleServiceAuthError error) override;
  void StepFailedToLoadDueToNetworkError(OnboardingFlowModel::Step step,
                                         net::Error error) override;
  void WillExitFlow(OnboardingFlowModel::Step step,
                    OnboardingFlowModel::ExitReason reason) override;

  OnboardingFlowModel* flow_model_;

  DISALLOW_COPY_AND_ASSIGN(OnboardingLogger);
};

}  // namespace supervision
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SUPERVISION_ONBOARDING_LOGGER_H_

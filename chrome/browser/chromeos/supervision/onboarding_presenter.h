// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SUPERVISION_ONBOARDING_PRESENTER_H_
#define CHROME_BROWSER_CHROMEOS_SUPERVISION_ONBOARDING_PRESENTER_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/supervision/mojom/onboarding_controller.mojom.h"
#include "chrome/browser/chromeos/supervision/onboarding_flow_model.h"
#include "net/base/net_errors.h"

class GoogleServiceAuthError;

namespace chromeos {
namespace supervision {

// Sets the onboarding presentation based on observed flow changes.
class OnboardingPresenter : public OnboardingFlowModel::Observer {
 public:
  explicit OnboardingPresenter(OnboardingFlowModel* flow_model);
  ~OnboardingPresenter() override;

 private:
  // OnboardingFlowModel::Observer:
  void StepStartedLoading(OnboardingFlowModel::Step step) override;
  void StepFinishedLoading(OnboardingFlowModel::Step step) override;
  void StepFailedToLoadDueToAuthError(OnboardingFlowModel::Step step,
                                      GoogleServiceAuthError error) override;
  void StepFailedToLoadDueToNetworkError(OnboardingFlowModel::Step step,
                                         net::Error error) override;

  void PresentErrorState();

  OnboardingFlowModel* flow_model_;
  int failed_loads_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(OnboardingPresenter);
};

}  // namespace supervision
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SUPERVISION_ONBOARDING_PRESENTER_H_

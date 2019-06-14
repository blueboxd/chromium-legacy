// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/supervision/onboarding_logger.h"

#include "base/logging.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace chromeos {
namespace supervision {
namespace {

const char* StepDescription(OnboardingFlowModel::Step step) {
  switch (step) {
    case OnboardingFlowModel::Step::kStart:
      return "Start";
    case OnboardingFlowModel::Step::kDetails:
      return "Details";
    case OnboardingFlowModel::Step::kAllSet:
      return "AllSet";
  }
}

const char* ExitReasonDescription(OnboardingFlowModel::ExitReason reason) {
  switch (reason) {
    case OnboardingFlowModel::ExitReason::kUserReachedEnd:
      return "User reached the end of the flow successfuly.";
    case OnboardingFlowModel::ExitReason::kFlowSkipped:
      return "User chose to skip the flow.";
    case OnboardingFlowModel::ExitReason::kUserNotEligible:
      return "User is not eligible to go through the flow.";
    case OnboardingFlowModel::ExitReason::kFeatureDisabled:
      return "Feature is disabled, we can't present flow pages.";
  }
}

}  // namespace

OnboardingLogger::OnboardingLogger(OnboardingFlowModel* flow_model)
    : flow_model_(flow_model) {
  flow_model_->AddObserver(this);
}

OnboardingLogger::~OnboardingLogger() {
  flow_model_->RemoveObserver(this);
}

void OnboardingLogger::StepStartedLoading(OnboardingFlowModel::Step step) {
  DVLOG(1) << "Supervision Onboarding started loading step: "
           << StepDescription(step);
}

void OnboardingLogger::StepFinishedLoading(OnboardingFlowModel::Step step) {
  DVLOG(1) << "Supervision Onboarding successfuly loaded step: "
           << StepDescription(step);
}

void OnboardingLogger::StepFailedToLoadDueToAuthError(
    OnboardingFlowModel::Step step,
    GoogleServiceAuthError error) {
  LOG(WARNING)
      << "Supervision Onboarding step failed to load due to auth error. "
      << "Step: " << StepDescription(step)
      << " Error: " << error.error_message();
}

void OnboardingLogger::StepFailedToLoadDueToNetworkError(
    OnboardingFlowModel::Step step,
    net::Error error) {
  LOG(WARNING) << "Supervision Onboarding step failed to load due to a network "
               << "error. Step: " << StepDescription(step)
               << " Error: " << net::ErrorToString(error);
}

void OnboardingLogger::WillExitFlow(OnboardingFlowModel::Step step,
                                    OnboardingFlowModel::ExitReason reason) {
  DVLOG(1) << "Supervision Onboarding exiting. "
           << ExitReasonDescription(reason);
}

}  // namespace supervision
}  // namespace chromeos

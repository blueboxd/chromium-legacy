// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SUPERVISION_ONBOARDING_FLOW_MODEL_H_
#define CHROME_BROWSER_CHROMEOS_SUPERVISION_ONBOARDING_FLOW_MODEL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/supervision/mojom/onboarding_controller.mojom.h"
#include "services/identity/public/cpp/identity_manager.h"

class Profile;

namespace identity {
class PrimaryAccountAccessTokenFetcher;
}

namespace chromeos {
namespace supervision {

class OnboardingDelegate;

// Class that manages the onboarding flow state, handling user actions and
// loading new pages. It notifies its observers of flow changes.
class OnboardingFlowModel {
 public:
  explicit OnboardingFlowModel(Profile* profile, OnboardingDelegate* delegate);
  ~OnboardingFlowModel();

  // Represents each onboarding flow step.
  enum class Step {
    // First page, informs the user about supervision features. Has a button to
    // skip the whole flow.
    kStart,
    // Second page, shows additional details about supervision.
    kDetails,
    // Third and final page, presented when all previous steps have been
    // successful.
    kAllSet,
  };

  // Represents possible reasons for the flow to exit.
  enum class ExitReason {
    // The user navigated through the whole flow and exited successfully.
    kUserReachedEnd,
    // User chose to skip the flow during its introduction.
    kFlowSkipped,
    // We found an authentication error before we attempted to load the first
    // onboarding page.
    kAuthError,
    // The webview that presents the pages found a network problem.
    kWebviewNetworkError,
    // The user is not eligible to see the flow.
    kUserNotEligible,
    // The onboarding flow screens should not be shown since their feature is
    // disabled.
    kFeatureDisabled,
  };

  class Observer : public base::CheckedObserver {
   public:
    // Step loading notifications. They are called before and after a step has
    // successfully loaded, which includes fetching an access token and
    // loading a new onboarding page. It is safe to call other flow model
    // methods after receiving these notifications.
    virtual void StepStartedLoading(Step step) {}
    virtual void StepFinishedLoading(Step step) {}

    // If we are exiting the flow for any reason, we first notify our observers
    // through this method. Observers should *NOT* call other methods in the
    // flow model while receiving this notification.
    virtual void WillExitFlow(Step current_step, ExitReason reason) {}
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void StartWithWebviewHost(mojom::OnboardingWebviewHostPtr webview_host);
  void HandleAction(mojom::OnboardingAction action);
  void ExitFlow(ExitReason reason);

  mojom::OnboardingWebviewHost& GetWebviewHost();

 private:
  mojom::OnboardingPagePtr MakePage(Step step, const std::string& access_token);
  void ShowNextPage();
  void ShowPreviousPage();
  void SkipFlow();

  void LoadStep(Step step);

  void AccessTokenCallback(GoogleServiceAuthError error,
                           identity::AccessTokenInfo access_token_info);

  void LoadPageCallback(mojom::OnboardingLoadPageResultPtr result);

  Profile* profile_;
  OnboardingDelegate* delegate_;
  mojom::OnboardingWebviewHostPtr webview_host_;
  Step current_step_ = Step::kStart;
  base::ObserverList<Observer> observer_list_;
  std::unique_ptr<identity::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(OnboardingFlowModel);
};

}  // namespace supervision
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SUPERVISION_ONBOARDING_FLOW_MODEL_H_

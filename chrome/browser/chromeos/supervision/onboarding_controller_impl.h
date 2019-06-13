// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SUPERVISION_ONBOARDING_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_SUPERVISION_ONBOARDING_CONTROLLER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chromeos/supervision/mojom/onboarding_controller.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

class Profile;

namespace chromeos {
namespace supervision {

class OnboardingDelegate;
class OnboardingFlowModel;
class OnboardingPresenter;
class OnboardingLogger;
class KioskNextFlowObserver;

class OnboardingControllerImpl : public mojom::OnboardingController {
 public:
  explicit OnboardingControllerImpl(Profile* profile,
                                    OnboardingDelegate* delegate);
  ~OnboardingControllerImpl() override;

  void BindRequest(mojom::OnboardingControllerRequest request);

 private:
  // mojom::OnboardingController:
  void BindWebviewHost(mojom::OnboardingWebviewHostPtr webview_host) override;
  void HandleAction(mojom::OnboardingAction action) override;

  mojo::BindingSet<mojom::OnboardingController> bindings_;

  std::unique_ptr<OnboardingFlowModel> flow_model_;
  std::unique_ptr<OnboardingPresenter> presenter_;
  std::unique_ptr<OnboardingLogger> logger_;
  std::unique_ptr<KioskNextFlowObserver> kiosk_next_observer_;

  DISALLOW_COPY_AND_ASSIGN(OnboardingControllerImpl);
};

}  // namespace supervision
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SUPERVISION_ONBOARDING_CONTROLLER_IMPL_H_

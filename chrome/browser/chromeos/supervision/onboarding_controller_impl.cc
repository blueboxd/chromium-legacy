// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/supervision/onboarding_controller_impl.h"

#include <utility>

#include "base/logging.h"
#include "chrome/browser/chromeos/supervision/kiosk_next_flow_observer.h"
#include "chrome/browser/chromeos/supervision/onboarding_flow_model.h"
#include "chrome/browser/chromeos/supervision/onboarding_logger.h"
#include "chrome/browser/chromeos/supervision/onboarding_presenter.h"

namespace chromeos {
namespace supervision {

OnboardingControllerImpl::OnboardingControllerImpl(Profile* profile,
                                                   OnboardingDelegate* delegate)
    : flow_model_(std::make_unique<OnboardingFlowModel>(profile, delegate)),
      presenter_(std::make_unique<OnboardingPresenter>(flow_model_.get())),
      logger_(std::make_unique<OnboardingLogger>(flow_model_.get())),
      kiosk_next_observer_(
          std::make_unique<KioskNextFlowObserver>(profile, flow_model_.get())) {
  DCHECK(profile);
}

OnboardingControllerImpl::~OnboardingControllerImpl() = default;

void OnboardingControllerImpl::BindRequest(
    mojom::OnboardingControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void OnboardingControllerImpl::BindWebviewHost(
    mojom::OnboardingWebviewHostPtr webview_host) {
  flow_model_->StartWithWebviewHost(std::move(webview_host));
}

void OnboardingControllerImpl::HandleAction(mojom::OnboardingAction action) {
  flow_model_->HandleAction(action);
}

}  // namespace supervision
}  // namespace chromeos

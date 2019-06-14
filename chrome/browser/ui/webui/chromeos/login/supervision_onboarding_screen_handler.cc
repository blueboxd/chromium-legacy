// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/supervision_onboarding_screen_handler.h"

#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/supervision_onboarding_screen.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

constexpr StaticOobeScreenId SupervisionOnboardingScreenView::kScreenId;

SupervisionOnboardingScreenHandler::SupervisionOnboardingScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {}

SupervisionOnboardingScreenHandler::~SupervisionOnboardingScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void SupervisionOnboardingScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("supervisionOnboardingWaitMessage",
               IDS_SUPERVISION_ONBOARDING_WAIT_MESSAGE);
  builder->Add("supervisionOnboardingNextButtonLabel",
               IDS_SUPERVISION_ONBOARDING_NEXT_BUTTON);
  builder->Add("supervisionOnboardingSkipButtonLabel",
               IDS_SUPERVISION_ONBOARDING_SKIP_BUTTON);
  builder->Add("supervisionOnboardingBackButtonLabel",
               IDS_SUPERVISION_ONBOARDING_BACK_BUTTON);
  builder->Add("supervisionOnboardingRetryButtonLabel",
               IDS_SUPERVISION_ONBOARDING_RETRY_BUTTON);
  builder->Add("supervisionOnboardingRetryDialogTitle",
               IDS_SUPERVISION_ONBOARDING_RETRY_DIALOG_TITLE);
  builder->Add("supervisionOnboardingRetryDialogMessage",
               IDS_SUPERVISION_ONBOARDING_RETRY_DIALOG_MESSAGE);
}

void SupervisionOnboardingScreenHandler::Bind(
    SupervisionOnboardingScreen* screen) {
  BaseScreenHandler::SetBaseScreen(screen);
  screen_ = screen;
}

void SupervisionOnboardingScreenHandler::Unbind() {
  supervision_onboarding_controller_.reset();
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

void SupervisionOnboardingScreenHandler::Show() {
  ShowScreen(kScreenId);

  GetOobeUI()->AddHandlerToRegistry(base::BindRepeating(
      &SupervisionOnboardingScreenHandler::BindSupervisionOnboardingController,
      base::Unretained(this)));

  CallJS("login.SupervisionOnboardingScreen.setupMojo");
}

void SupervisionOnboardingScreenHandler::Hide() {}

void SupervisionOnboardingScreenHandler::Initialize() {}

void SupervisionOnboardingScreenHandler::BindSupervisionOnboardingController(
    supervision::mojom::OnboardingControllerRequest request) {
  DCHECK(screen_);
  if (!supervision_onboarding_controller_) {
    supervision_onboarding_controller_ =
        std::make_unique<supervision::OnboardingControllerImpl>(
            ProfileManager::GetPrimaryUserProfile(), screen_);
  }

  supervision_onboarding_controller_->BindRequest(std::move(request));
}

}  // namespace chromeos

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_answers/quick_answers_ui_controller.h"

#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "ash/quick_answers/quick_answers_controller_impl.h"
#include "ash/quick_answers/ui/quick_answers_view.h"
#include "ash/quick_answers/ui/user_consent_view.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/optional.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

using chromeos::quick_answers::QuickAnswer;
namespace ash {

QuickAnswersUiController::QuickAnswersUiController(
    QuickAnswersControllerImpl* controller)
    : controller_(controller) {}

QuickAnswersUiController::~QuickAnswersUiController() {
  CloseQuickAnswersView();
  CloseUserConsentView();
}

void QuickAnswersUiController::CreateQuickAnswersView(
    const gfx::Rect& bounds,
    const std::string& title) {
  DCHECK(!quick_answers_view_);
  DCHECK(!user_consent_view_);
  SetActiveQuery(title);
  quick_answers_view_ = new QuickAnswersView(bounds, title, this);
  quick_answers_view_->GetWidget()->ShowInactive();
}

void QuickAnswersUiController::OnQuickAnswersViewPressed() {
  CloseQuickAnswersView();
  ash::AssistantInteractionController::Get()->StartTextInteraction(
      query_, /*allow_tts=*/false,
      chromeos::assistant::mojom::AssistantQuerySource::kQuickAnswers);
  controller_->OnQuickAnswerClick();
}

void QuickAnswersUiController::CloseQuickAnswersView() {
  if (quick_answers_view_) {
    quick_answers_view_->GetWidget()->Close();
    quick_answers_view_ = nullptr;
  }
}

void QuickAnswersUiController::OnRetryLabelPressed() {
  controller_->OnRetryQuickAnswersRequest();
}

void QuickAnswersUiController::RenderQuickAnswersViewWithResult(
    const gfx::Rect& anchor_bounds,
    const QuickAnswer& quick_answer) {
  if (!quick_answers_view_)
    return;

  // QuickAnswersView was initiated with a loading page and will be updated
  // when quick answers result from server side is ready.
  quick_answers_view_->UpdateView(anchor_bounds, quick_answer);
}

void QuickAnswersUiController::SetActiveQuery(const std::string& query) {
  query_ = query;
}

void QuickAnswersUiController::ShowRetry() {
  if (!quick_answers_view_)
    return;

  quick_answers_view_->ShowRetryView();
}

void QuickAnswersUiController::UpdateQuickAnswersBounds(
    const gfx::Rect& anchor_bounds) {
  if (quick_answers_view_)
    quick_answers_view_->UpdateAnchorViewBounds(anchor_bounds);

  if (user_consent_view_)
    user_consent_view_->UpdateAnchorViewBounds(anchor_bounds);
}

void QuickAnswersUiController::CreateUserConsentView(
    const gfx::Rect& anchor_bounds) {
  DCHECK(!quick_answers_view_);
  DCHECK(!user_consent_view_);
  user_consent_view_ = new quick_answers::UserConsentView(anchor_bounds, this);
  user_consent_view_->GetWidget()->ShowInactive();
}

void QuickAnswersUiController::CloseUserConsentView() {
  if (user_consent_view_) {
    user_consent_view_->GetWidget()->Close();
    user_consent_view_ = nullptr;
  }
}

void QuickAnswersUiController::OnConsentGrantedButtonPressed() {
  controller_->OnUserConsentGranted();
}

void QuickAnswersUiController::OnManageSettingsButtonPressed() {
  controller_->OnConsentSettingsRequestedByUser();
}

void QuickAnswersUiController::OnDogfoodButtonPressed() {
  controller_->OpenQuickAnswersDogfoodLink();
}

}  // namespace ash

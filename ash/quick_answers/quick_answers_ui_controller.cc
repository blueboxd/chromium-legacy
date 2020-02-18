// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_answers/quick_answers_ui_controller.h"

#include "ash/public/cpp/assistant/assistant_interface_binder.h"
#include "ash/quick_answers/ui/quick_answers_view.h"
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

QuickAnswersUiController::~QuickAnswersUiController() {
  Close();
}

void QuickAnswersUiController::CreateQuickAnswersView(
    const gfx::Rect& bounds,
    const std::string& title) {
  DCHECK(!quick_answers_view_);
  SetActiveQuery(title);
  quick_answers_view_ = new QuickAnswersView(bounds, title, this);
  quick_answers_view_->GetWidget()->ShowInactive();
}

void QuickAnswersUiController::OnQuickAnswersViewPressed() {
  Close();
  mojo::Remote<chromeos::assistant::mojom::AssistantController>
      assistant_controller;
  ash::AssistantInterfaceBinder::GetInstance()->BindController(
      assistant_controller.BindNewPipeAndPassReceiver());
  assistant_controller->StartTextInteraction(
      query_, /*allow_tts=*/false,
      chromeos::assistant::mojom::AssistantQuerySource::kQuickAnswers);
}

void QuickAnswersUiController::Close() {
  if (!quick_answers_view_)
    return;

  quick_answers_view_->GetWidget()->Close();
  quick_answers_view_ = nullptr;
}

void QuickAnswersUiController::RenderQuickAnswersViewWithResult(
    const gfx::Rect& bounds,
    const QuickAnswer& quick_answer) {
  if (!quick_answers_view_)
    return;

  // QuickAnswersView was initiated with a loading page and will be updated
  // when quick answers result from server side is ready.
  quick_answers_view_->UpdateView(bounds, quick_answer);
}

void QuickAnswersUiController::SetActiveQuery(const std::string& query) {
  query_ = query;
}

void QuickAnswersUiController::UpdateQuickAnswersBounds(
    const gfx::Rect& bounds) {
  if (!quick_answers_view_)
    return;

  quick_answers_view_->UpdateAnchorViewBounds(bounds);
}
}  // namespace ash

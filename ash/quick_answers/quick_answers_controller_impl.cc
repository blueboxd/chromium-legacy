// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_answers/quick_answers_controller_impl.h"

#include "ash/quick_answers/quick_answers_ui_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"

// TODO(yanxiao):Add a unit test for QuickAnswersControllerImpl.
namespace {
using chromeos::quick_answers::QuickAnswer;
using chromeos::quick_answers::QuickAnswersClient;
using chromeos::quick_answers::QuickAnswersRequest;
using chromeos::quick_answers::ResultType;

// TODO:(yanxiao) move the string to grd source file.
constexpr char kNoResult[] = "See result in Assistant";
}  // namespace

namespace ash {

QuickAnswersControllerImpl::QuickAnswersControllerImpl()
    : quick_answers_ui_controller_(
          std::make_unique<ash::QuickAnswersUiController>(this)) {}

QuickAnswersControllerImpl::~QuickAnswersControllerImpl() = default;

void QuickAnswersControllerImpl::SetClient(
    std::unique_ptr<QuickAnswersClient> client) {
  quick_answers_client_ = std::move(client);
}

void QuickAnswersControllerImpl::CreateQuickAnswersView(
    const gfx::Rect& anchor_bounds,
    const std::string& title) {
  DCHECK(quick_answers_client_);
  DCHECK(quick_answers_ui_controller_);

  if (!is_eligible_)
    return;

  anchor_bounds_ = anchor_bounds;
  query_ = title;
  quick_answers_ui_controller_->CreateQuickAnswersView(anchor_bounds, title);

  // Fetch Quick Answer.
  QuickAnswersRequest request;
  request.selected_text = title;
  quick_answers_client_->SendRequest(request);
}

void QuickAnswersControllerImpl::DismissQuickAnswersView() {
  quick_answers_ui_controller_->Close();
}

chromeos::quick_answers::QuickAnswersDelegate*
QuickAnswersControllerImpl::GetQuickAnswersDelegate() {
  return this;
}
void QuickAnswersControllerImpl::OnQuickAnswerReceived(
    std::unique_ptr<QuickAnswer> quick_answer) {
  if (quick_answer) {
    if (quick_answer->title.empty()) {
      quick_answer->title.push_back(
          std::make_unique<chromeos::quick_answers::QuickAnswerText>(query_));
    }
    quick_answers_ui_controller_->RenderQuickAnswersViewWithResult(
        anchor_bounds_, *quick_answer);
  } else {
    chromeos::quick_answers::QuickAnswer quick_answer_with_no_result;
    quick_answer_with_no_result.title.push_back(
        std::make_unique<chromeos::quick_answers::QuickAnswerText>(query_));
    quick_answer_with_no_result.first_answer_row.push_back(
        std::make_unique<chromeos::quick_answers::QuickAnswerText>(kNoResult));
    quick_answers_ui_controller_->RenderQuickAnswersViewWithResult(
        anchor_bounds_, quick_answer_with_no_result);
  }
}

void QuickAnswersControllerImpl::OnEligibilityChanged(bool eligible) {
  is_eligible_ = eligible;
}

void QuickAnswersControllerImpl::OnNetworkError() {
  // Notify quick_answers_ui_controller_ to show retry UI.
  quick_answers_ui_controller_->ShowRetry();
}

void QuickAnswersControllerImpl::OnRetryQuickAnswersRequest() {
  QuickAnswersRequest request;
  request.selected_text = query_;
  quick_answers_client_->SendRequest(request);
}
void QuickAnswersControllerImpl::UpdateQuickAnswersAnchorBounds(
    const gfx::Rect& anchor_bounds) {
  quick_answers_ui_controller_->UpdateQuickAnswersBounds(anchor_bounds);
}

}  // namespace ash

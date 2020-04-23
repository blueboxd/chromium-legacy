// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_answers/quick_answers_controller_impl.h"

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/quick_answers/quick_answers_ui_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "chromeos/components/quick_answers/quick_answers_consents.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "url/gurl.h"

// TODO(yanxiao):Add a unit test for QuickAnswersControllerImpl.
namespace {
using chromeos::quick_answers::QuickAnswer;
using chromeos::quick_answers::QuickAnswersClient;
using chromeos::quick_answers::QuickAnswersRequest;
using chromeos::quick_answers::ResultType;

constexpr char kAssistantRelatedInfoUrl[] =
    "chrome://os-settings/googleAssistant";
constexpr char kDogfoodUrl[] = "https://go/quick-answers-dogfood-bugs";

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
  consent_controller_ =
      std::make_unique<chromeos::quick_answers::QuickAnswersConsent>(
          Shell::Get()->session_controller()->GetPrimaryUserPrefService());
}

void QuickAnswersControllerImpl::MaybeShowQuickAnswers(
    const gfx::Rect& anchor_bounds,
    const std::string& title) {
  DCHECK(quick_answers_client_);
  DCHECK(quick_answers_ui_controller_);

  if (!is_eligible_)
    return;

  // Cache anchor-bounds and query.
  anchor_bounds_ = anchor_bounds;
  query_ = title;

  // Show user-consent notice informing user about the feature if required.
  if (consent_controller_->ShouldShowConsent()) {
    if (!quick_answers_ui_controller_->is_showing_user_consent_view()) {
      quick_answers_ui_controller_->CreateUserConsentView(anchor_bounds);
      consent_controller_->StartConsent();
    }

    // Quick-Answers will only be displayed after explicit or tacit consent is
    // obtained.
    return;
  }

  // Create Quick-Answers view and fetch Quick-Answer.
  quick_answers_ui_controller_->CreateQuickAnswersView(anchor_bounds, title);
  QuickAnswersRequest request;
  request.selected_text = title;
  quick_answers_client_->SendRequest(request);
}

void QuickAnswersControllerImpl::DismissQuickAnswers(bool is_active) {
  MaybeDismissQuickAnswersConsent();
  quick_answers_ui_controller_->CloseQuickAnswersView();
  quick_answers_client_->OnQuickAnswersDismissed(
      quick_answer_ ? quick_answer_->result_type : ResultType::kNoResult,
      is_active);
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
        std::make_unique<chromeos::quick_answers::QuickAnswerResultText>(
            kNoResult));
    quick_answers_ui_controller_->RenderQuickAnswersViewWithResult(
        anchor_bounds_, quick_answer_with_no_result);
  }

  quick_answer_ = std::move(quick_answer);
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

void QuickAnswersControllerImpl::OnQuickAnswerClick() {
  quick_answers_client_->OnQuickAnswerClick(
      quick_answer_ ? quick_answer_->result_type : ResultType::kNoResult);
}

void QuickAnswersControllerImpl::UpdateQuickAnswersAnchorBounds(
    const gfx::Rect& anchor_bounds) {
  anchor_bounds_ = anchor_bounds;
  quick_answers_ui_controller_->UpdateQuickAnswersBounds(anchor_bounds);
}

void QuickAnswersControllerImpl::OnUserConsentGranted() {
  quick_answers_ui_controller_->CloseUserConsentView();
  consent_controller_->AcceptConsent(
      chromeos::quick_answers::ConsentInteractionType::kAccept);

  // Display Quick-Answer for the cached query when user consents.
  MaybeShowQuickAnswers(anchor_bounds_, query_);
}

void QuickAnswersControllerImpl::OnConsentSettingsRequestedByUser() {
  quick_answers_ui_controller_->CloseUserConsentView();
  consent_controller_->AcceptConsent(
      chromeos::quick_answers::ConsentInteractionType::kManageSettings);
  NewWindowDelegate::GetInstance()->NewTabWithUrl(
      GURL(kAssistantRelatedInfoUrl), /*from_user_interaction=*/true);
}

void QuickAnswersControllerImpl::MaybeDismissQuickAnswersConsent() {
  if (quick_answers_ui_controller_->is_showing_user_consent_view())
    consent_controller_->DismissConsent();
  quick_answers_ui_controller_->CloseUserConsentView();
}

void QuickAnswersControllerImpl::OpenQuickAnswersDogfoodLink() {
  NewWindowDelegate::GetInstance()->NewTabWithUrl(
      GURL(kDogfoodUrl), /*from_user_interaction=*/true);
}

}  // namespace ash

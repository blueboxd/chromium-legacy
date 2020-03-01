// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_ANSWERS_QUICK_ANSWERS_CONTROLLER_IMPL_H_
#define ASH_QUICK_ANSWERS_QUICK_ANSWERS_CONTROLLER_IMPL_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/quick_answers_controller.h"
#include "chromeos/components/quick_answers/quick_answers_client.h"
#include "ui/gfx/geometry/rect.h"

namespace chromeos {
namespace quick_answers {
struct QuickAnswer;
}  // namespace quick_answers
}  // namespace chromeos

namespace ash {
class QuickAnswersUiController;

// Implementation of QuickAnswerController. It fetches quick answers
// result via QuickAnswersClient and manages quick answers UI.
class ASH_EXPORT QuickAnswersControllerImpl
    : public QuickAnswersController,
      public chromeos::quick_answers::QuickAnswersDelegate {
 public:
  explicit QuickAnswersControllerImpl();
  QuickAnswersControllerImpl(const QuickAnswersControllerImpl&) = delete;
  QuickAnswersControllerImpl& operator=(const QuickAnswersControllerImpl&) =
      delete;
  ~QuickAnswersControllerImpl() override;

  // QuickAnswersController:
  void SetClient(std::unique_ptr<chromeos::quick_answers::QuickAnswersClient>
                     client) override;

  // SetClient is required to be called before using these methods.
  // TODO(yanxiao): refactor to delegate to browser.
  void CreateQuickAnswersView(const gfx::Rect& anchor_bounds,
                              const std::string& title) override;

  void DismissQuickAnswersView() override;

  chromeos::quick_answers::QuickAnswersDelegate* GetQuickAnswersDelegate()
      override;

  // QuickAnswersDelegate:
  void OnQuickAnswerReceived(
      std::unique_ptr<chromeos::quick_answers::QuickAnswer> answer) override;
  void OnEligibilityChanged(bool eligible) override;
  void OnNetworkError() override;

  // Retry sending quick answers request to backend.
  void OnRetryQuickAnswersRequest();

  // Update the bounds of the anchor view.
  void UpdateQuickAnswersAnchorBounds(const gfx::Rect& anchor_bounds);

 private:
  void SendAssistantQuery(const std::string& query);

  // Bounds of the anchor view.
  gfx::Rect anchor_bounds_;

  // Query used to retrieve quick answer.
  std::string query_;

  std::unique_ptr<chromeos::quick_answers::QuickAnswersClient>
      quick_answers_client_;

  // Whether the feature is enabled and all eligibility criteria are met (
  // locale, consents, etc).
  bool is_eligible_ = false;

  std::unique_ptr<QuickAnswersUiController> quick_answers_ui_controller_;
};

}  // namespace ash
#endif  // ASH_QUICK_ANSWERS_QUICK_ANSWERS_CONTROLLER_IMPL_H_

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_ANSWERS_QUICK_ANSWERS_UI_CONTROLLER_H_
#define ASH_QUICK_ANSWERS_QUICK_ANSWERS_UI_CONTROLLER_H_

#include <string>

#include "ash/ash_export.h"
#include "ui/gfx/geometry/rect.h"

namespace chromeos {
namespace quick_answers {
struct QuickAnswer;
}  // namespace quick_answers
}  // namespace chromeos

namespace ash {
class QuickAnswersView;
class QuickAnswersControllerImpl;

namespace quick_answers {
class UserConsentView;
}  // namespace quick_answers

// A controller to show/hide and handle interactions for quick
// answers view.
class ASH_EXPORT QuickAnswersUiController {
 public:
  explicit QuickAnswersUiController(QuickAnswersControllerImpl* controller);
  ~QuickAnswersUiController();

  QuickAnswersUiController(const QuickAnswersUiController&) = delete;
  QuickAnswersUiController& operator=(const QuickAnswersUiController&) = delete;

  void Close();
  // Constructs/resets |quick_answers_view_|.
  void CreateQuickAnswersView(const gfx::Rect& anchor_bounds,
                              const std::string& title);

  void OnQuickAnswersViewPressed();

  void OnRetryLabelPressed();

  // |bounds| is the bound of context menu.
  void RenderQuickAnswersViewWithResult(
      const gfx::Rect& bounds,
      const chromeos::quick_answers::QuickAnswer& quick_answer);

  void SetActiveQuery(const std::string& query);

  // Show retry option in the quick answers view.
  void ShowRetry();

  void UpdateQuickAnswersBounds(const gfx::Rect& anchor_bounds);

  // Creates a view for user-consent for Quick Answers vertically aligned to the
  // anchor.
  void CreateUserConsentView(const gfx::Rect& anchor_bounds);

  // Invoked when user clicks the consent button to grant consent for using
  // Quick Answers.
  void OnConsentGrantedButtonPressed();

  // Invoked when user clicks the settings button related to consent for Quick
  // Answers.
  void OnManageSettingsButtonPressed();

 private:
  QuickAnswersControllerImpl* controller_ = nullptr;

  // Owned by view hierarchy.
  QuickAnswersView* quick_answers_view_ = nullptr;
  quick_answers::UserConsentView* user_consent_view_ = nullptr;
  std::string query_;
};

}  // namespace ash

#endif  // ASH_QUICK_ANSWERS_QUICK_ANSWERS_UI_CONTROLLER_H_

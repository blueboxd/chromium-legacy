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

// A controller to show/hide and handle interactions for quick
// answers view.
class ASH_EXPORT QuickAnswersUiController {
 public:
  QuickAnswersUiController() = default;
  ~QuickAnswersUiController();

  QuickAnswersUiController(const QuickAnswersUiController&) = delete;
  QuickAnswersUiController& operator=(const QuickAnswersUiController&) = delete;

  void Close();
  // Constructs/resets |quick_answers_view_|.
  void CreateQuickAnswersView(const gfx::Rect& bounds,
                              const std::string& title);

  void OnQuickAnswersViewPressed();

  // |bounds| is the bound of context menu.
  void RenderQuickAnswersViewWithResult(
      const gfx::Rect& bounds,
      const chromeos::quick_answers::QuickAnswer& quick_answer);

  void SetActiveQuery(const std::string& query);

  void UpdateQuickAnswersBounds(const gfx::Rect& bounds);

 private:
  // Owned by view hierarchy.
  QuickAnswersView* quick_answers_view_ = nullptr;
  std::string query_;
};

}  // namespace ash

#endif  // ASH_QUICK_ANSWERS_QUICK_ANSWERS_UI_CONTROLLER_H_

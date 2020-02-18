// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_ANSWERS_UI_QUICK_ANSWERS_VIEW_H_
#define ASH_QUICK_ANSWERS_UI_QUICK_ANSWERS_VIEW_H_

#include <vector>

#include "ash/ash_export.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/events/event_handler.h"
#include "ui/views/controls/button/button.h"

namespace ash {
class QuickAnswersUiController;
class QuickAnswersViewHandler;

// A bubble style view to show QuickAnswer.
class ASH_EXPORT QuickAnswersView : public views::View {
 public:
  QuickAnswersView(const gfx::Rect& anchor_view_bounds,
                   const std::string& title,
                   QuickAnswersUiController* controller);
  ~QuickAnswersView() override;

  QuickAnswersView(const QuickAnswersView&) = delete;
  QuickAnswersView& operator=(const QuickAnswersView&) = delete;

  // views::View:
  const char* GetClassName() const override;

  void UpdateAnchorViewBounds(const gfx::Rect& anchor_view_bounds);

  // Update the quick answers view with quick answers result.
  void UpdateView(const gfx::Rect& anchor_view_bounds,
                  const chromeos::quick_answers::QuickAnswer& quick_answer);

  void SendQuickAnswersQuery();

  void SetBackgroundColor(SkColor color);

 private:
  int GetPreferredHeight();
  void InitLayout();
  void InitWidget();
  void UpdateBounds();
  void UpdateChildViews(
      const chromeos::quick_answers::QuickAnswer& quick_answer);
  void UpdateOneRowAnswer(
      const std::vector<std::unique_ptr<
          chromeos::quick_answers::QuickAnswerUiElement>>& answers,
      int y);

  gfx::Rect anchor_view_bounds_;
  QuickAnswersUiController* const controller_;
  bool has_second_row_answer_ = false;
  std::string title_;
  SkColor background_color_ = SK_ColorWHITE;
  std::unique_ptr<QuickAnswersViewHandler> quick_answers_view_handler_;
  base::WeakPtrFactory<QuickAnswersView> weak_factory_{this};
};
}  // namespace ash

#endif  // ASH_QUICK_ANSWERS_UI_QUICK_ANSWERS_VIEW_H_

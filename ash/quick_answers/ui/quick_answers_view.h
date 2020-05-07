// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_ANSWERS_UI_QUICK_ANSWERS_VIEW_H_
#define ASH_QUICK_ANSWERS_UI_QUICK_ANSWERS_VIEW_H_

#include <vector>

#include "ash/ash_export.h"
#include "ui/events/event_handler.h"
#include "ui/views/controls/button/button.h"

namespace chromeos {
namespace quick_answers {
struct QuickAnswer;
}  // namespace quick_answers
}  // namespace chromeos

namespace views {
class ImageButton;
class Label;
class LabelButton;
}  // namespace views

namespace ash {

class QuickAnswersUiController;
class QuickAnswersPreTargetHandler;

// A bubble style view to show QuickAnswer.
class ASH_EXPORT QuickAnswersView : public views::Button,
                                    public views::ButtonListener {
 public:
  QuickAnswersView(const gfx::Rect& anchor_view_bounds,
                   const std::string& title,
                   QuickAnswersUiController* controller);
  ~QuickAnswersView() override;

  QuickAnswersView(const QuickAnswersView&) = delete;
  QuickAnswersView& operator=(const QuickAnswersView&) = delete;

  // views::View:
  const char* GetClassName() const override;

  // views::Button:
  void StateChanged(views::Button::ButtonState old_state) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Called when a click happens to trigger Assistant Query.
  void SendQuickAnswersQuery();

  void UpdateAnchorViewBounds(const gfx::Rect& anchor_view_bounds);

  // Update the quick answers view with quick answers result.
  void UpdateView(const gfx::Rect& anchor_view_bounds,
                  const chromeos::quick_answers::QuickAnswer& quick_answer);

  void ShowRetryView();

  bool preempt_last_click_event() const { return preempt_last_click_event_; }

 private:
  void InitLayout();
  void InitWidget();
  void AddDogfoodButton();
  void AddAssistantIcon();
  void ResetContentView();
  void UpdateBounds();
  void UpdateQuickAnswerResult(
      const chromeos::quick_answers::QuickAnswer& quick_answer);

  // Buttons should fire on mouse-press instead of default behavior (waiting for
  // mouse-release), since events of former type dismiss the accompanying menu.
  void SetButtonNotifyActionToOnPress(views::Button* button);

  gfx::Rect anchor_view_bounds_;
  QuickAnswersUiController* const controller_;
  bool has_second_row_answer_ = false;
  bool preempt_last_click_event_ = false;
  std::string title_;
  views::View* main_view_ = nullptr;
  views::View* content_view_ = nullptr;
  views::Label* first_answer_label_ = nullptr;
  views::LabelButton* retry_label_ = nullptr;
  views::ImageButton* dogfood_button_ = nullptr;
  std::unique_ptr<QuickAnswersPreTargetHandler> quick_answers_view_handler_;
  base::WeakPtrFactory<QuickAnswersView> weak_factory_{this};
};
}  // namespace ash

#endif  // ASH_QUICK_ANSWERS_UI_QUICK_ANSWERS_VIEW_H_

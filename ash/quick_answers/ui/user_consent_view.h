// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_ANSWERS_UI_USER_CONSENT_VIEW_H_
#define ASH_QUICK_ANSWERS_UI_USER_CONSENT_VIEW_H_

#include <memory>

#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
class LabelButton;
}  // namespace views

namespace ash {

class QuickAnswersUiController;

namespace quick_answers {

// TODO(siabhijeet): Investigate BubbleDialogDelegateView as a common view for
// UserConsentView and QuickAnswersView
class UserConsentView : public views::View, public views::ButtonListener {
 public:
  UserConsentView(const gfx::Rect& anchor_view_bounds,
                  QuickAnswersUiController* ui_controller);

  // Disallow copy and assign.
  UserConsentView(const UserConsentView&) = delete;
  UserConsentView& operator=(const UserConsentView&) = delete;

  ~UserConsentView() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  void UpdateAnchorViewBounds(const gfx::Rect& anchor_view_bounds);

 private:
  void InitLayout();
  void InitContent();
  void InitButtonBar();
  void InitWidget();
  void AddDogfoodButton();
  void UpdateWidgetBounds();

  // Cached bounds of the anchor this view is tied to.
  gfx::Rect anchor_view_bounds_;

  std::unique_ptr<EventHandler> event_handler_;
  QuickAnswersUiController* const ui_controller_;

  // Owned by view hierarchy.
  views::View* main_view_ = nullptr;
  views::View* content_ = nullptr;
  views::ImageButton* dogfood_button_ = nullptr;
  views::LabelButton* settings_button_ = nullptr;
  views::LabelButton* consent_button_ = nullptr;
};

}  // namespace quick_answers
}  // namespace ash

#endif  // ASH_QUICK_ANSWERS_UI_USER_CONSENT_VIEW_H_

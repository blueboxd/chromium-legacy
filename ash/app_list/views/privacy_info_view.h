// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_PRIVACY_INFO_VIEW_H_
#define ASH_APP_LIST_VIEWS_PRIVACY_INFO_VIEW_H_

#include "ui/views/controls/button/button.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
class ImageView;
class StyledLabel;
}  // namespace views

namespace ash {

// View representing privacy info in Launcher.
class PrivacyInfoView : public views::View,
                        public views::ButtonListener,
                        public views::StyledLabelListener {
 public:
  ~PrivacyInfoView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnKeyEvent(ui::KeyEvent* event) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

 protected:
  PrivacyInfoView(int info_string_id, int link_string_id);

 private:
  void InitLayout();
  void InitInfoIcon();
  void InitText();
  void InitCloseButton();

  virtual void LinkClicked() = 0;
  virtual void CloseButtonPressed() = 0;

  views::ImageView* info_icon_ = nullptr;       // Owned by view hierarchy.
  views::StyledLabel* text_view_ = nullptr;     // Owned by view hierarchy.
  views::ImageButton* close_button_ = nullptr;  // Owned by view hierarchy.

  const int info_string_id_;
  const int link_string_id_;

  DISALLOW_COPY_AND_ASSIGN(PrivacyInfoView);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_PRIVACY_INFO_VIEW_H_

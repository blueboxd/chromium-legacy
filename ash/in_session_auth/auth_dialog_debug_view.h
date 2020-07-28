// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IN_SESSION_AUTH_AUTH_DIALOG_DEBUG_VIEW_H_
#define ASH_IN_SESSION_AUTH_AUTH_DIALOG_DEBUG_VIEW_H_

#include <string>

#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class BoxLayout;
class Label;
class LabelButton;
}  // namespace views

namespace ash {

// Contains the debug views that allows the developer to interact with the
// AuthDialogController.
class AuthDialogDebugView : public views::View, public views::ButtonListener {
 public:
  AuthDialogDebugView();
  AuthDialogDebugView(const AuthDialogDebugView&) = delete;
  AuthDialogDebugView& operator=(const AuthDialogDebugView&) = delete;
  ~AuthDialogDebugView() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  // Add a view for dialog title.
  void AddTitleView();

  // Add a view for the prompt message.
  void AddPromptView();

  // Add a vertical spacing view.
  void AddVerticalSpacing(int height);

  // Add a view for action buttons.
  void AddActionButtonsView();

  // Creates a button on the debug row that cannot be focused.
  views::LabelButton* AddButton(const std::string& text,
                                int id,
                                views::View* container);

  // Debug container which holds the entire debug UI.
  views::View* container_ = nullptr;

  // Layout for |container_|.
  views::BoxLayout* main_layout_ = nullptr;

  // Title of the auth dialog.
  views::Label* title_ = nullptr;

  // Prompt message to the user.
  views::Label* prompt_ = nullptr;

  // Show other authentication mechanisms if more than one.
  views::LabelButton* more_options_button_ = nullptr;

  // Cancel all operations and close th dialog.
  views::LabelButton* cancel_button_ = nullptr;

  // Container which holds action buttons.
  views::View* action_view_container_ = nullptr;
};

}  // namespace ash

#endif  // ASH_IN_SESSION_AUTH_AUTH_DIALOG_DEBUG_VIEW_H_

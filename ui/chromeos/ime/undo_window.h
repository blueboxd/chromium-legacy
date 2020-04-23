// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_IME_UNDO_WINDOW_H_
#define UI_CHROMEOS_IME_UNDO_WINDOW_H_

#include "ui/chromeos/ime/assistive_delegate.h"
#include "ui/chromeos/ui_chromeos_export.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/label_button.h"

namespace ui {
namespace ime {

// Pop up UI for users to undo an autocorrected word.
class UI_CHROMEOS_EXPORT UndoWindow : public views::BubbleDialogDelegateView,
                                      public views::ButtonListener {
 public:
  explicit UndoWindow(gfx::NativeView parent, AssistiveDelegate* delegate);
  ~UndoWindow() override;

  views::Widget* InitWidget();
  void Hide();
  void Show();

  // Set the position of the undo window at the start of the autocorrected word.
  void SetBounds(const gfx::Rect& word_bounds);

 private:
  // views::BubbleDialogDelegateView:
  const char* GetClassName() const override;

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  AssistiveDelegate* delegate_;
  views::LabelButton* undo_button_;

  DISALLOW_COPY_AND_ASSIGN(UndoWindow);
};

}  // namespace ime
}  // namespace ui

#endif  // UI_CHROMEOS_IME_UNDO_WINDOW_H_

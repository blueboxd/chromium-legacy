// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_ASSISTIVE_WINDOW_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_ASSISTIVE_WINDOW_CONTROLLER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chromeos/input_method/assistive_window_controller.h"
#include "ui/base/ime/ime_assistive_window_handler_interface.h"
#include "ui/chromeos/ime/assistive_delegate.h"
#include "ui/chromeos/ime/suggestion_window_view.h"
#include "ui/chromeos/ime/undo_window.h"

namespace views {
class Widget;
}  // namespace views

namespace chromeos {
struct AssistiveWindowProperties;

namespace input_method {

// The implementation of AssistiveWindowController.
// AssistiveWindowController controls different assistive windows.
class AssistiveWindowControllerImpl : public AssistiveWindowController,
                                      public views::WidgetObserver,
                                      public IMEAssistiveWindowHandlerInterface,
                                      public ui::ime::AssistiveDelegate {
 public:
  AssistiveWindowControllerImpl();
  ~AssistiveWindowControllerImpl() override;

 private:
  // IMEAssistiveWindowHandlerInterface implementation.
  void SetBounds(const gfx::Rect& cursor_bounds) override;
  void SetAssistiveWindowProperties(
      const AssistiveWindowProperties& window) override;
  void ShowSuggestion(const base::string16& text,
                      const size_t confirmed_length,
                      const bool show_tab) override;
  void HideSuggestion() override;
  base::string16 GetSuggestionText() const override;
  size_t GetConfirmedLength() const override;
  void FocusStateChanged() override;
  void OnWidgetClosing(views::Widget* widget) override;

  // ui::ime::AssistiveDelegate implementation.
  void AssistiveWindowClicked(ui::ime::ButtonId id,
                              ui::ime::AssistiveWindowType type) override;

  void InitSuggestionWindow();
  void InitUndoWindow();

  ui::ime::SuggestionWindowView* suggestion_window_view_ = nullptr;
  ui::ime::UndoWindow* undo_window_ = nullptr;
  base::string16 suggestion_text_;
  size_t confirmed_length_;

  DISALLOW_COPY_AND_ASSIGN(AssistiveWindowControllerImpl);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_ASSISTIVE_WINDOW_CONTROLLER_IMPL_H_

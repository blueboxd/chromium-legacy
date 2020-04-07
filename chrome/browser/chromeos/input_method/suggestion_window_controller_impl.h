// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_SUGGESTION_WINDOW_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_SUGGESTION_WINDOW_CONTROLLER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chromeos/input_method/suggestion_window_controller.h"
#include "ui/base/ime/ime_suggestion_window_handler_interface.h"
#include "ui/chromeos/ime/suggestion_window_view.h"

namespace views {
class Widget;
}  // namespace views

namespace chromeos {
namespace input_method {

// The implementation of SuggestionWindowController.
// SuggestionWindowController controls the SuggestionWindow.
class SuggestionWindowControllerImpl
    : public SuggestionWindowController,
      public views::WidgetObserver,
      public IMESuggestionWindowHandlerInterface {
 public:
  SuggestionWindowControllerImpl();
  ~SuggestionWindowControllerImpl() override;

  // IMESuggestionWindowHandlerInterface implementation.
  void SetBounds(const gfx::Rect& cursor_bounds) override;
  void Show(const base::string16& text,
            const base::string16& confirmed_text,
            const bool show_tab) override;
  void Hide() override;
  base::string16 GetText() const override;
  void FocusStateChanged() override;
  void OnWidgetClosing(views::Widget* widget) override;

  void Init();

  // The suggestion window view.
  ui::ime::SuggestionWindowView* suggestion_window_view_ = nullptr;

  bool is_focused_ = false;
  base::string16 suggestion_text_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionWindowControllerImpl);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_SUGGESTION_WINDOW_CONTROLLER_IMPL_H_

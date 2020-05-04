// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/assistive_window_controller.h"

#include <string>
#include <vector>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"           // mash-ok
#include "ash/wm/window_util.h"  // mash-ok
#include "chrome/browser/chromeos/input_method/assistive_window_properties.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/views/widget/widget.h"

namespace chromeos {
namespace input_method {

namespace {
gfx::NativeView GetParentView() {
  gfx::NativeView parent = nullptr;

  aura::Window* active_window = ash::window_util::GetActiveWindow();
  // Use VirtualKeyboardContainer so that it works even with a system modal
  // dialog.
  parent = ash::Shell::GetContainer(
      active_window ? active_window->GetRootWindow()
                    : ash::Shell::GetRootWindowForNewWindows(),
      ash::kShellWindowId_VirtualKeyboardContainer);
  return parent;
}
}  // namespace

AssistiveWindowController::AssistiveWindowController() = default;

AssistiveWindowController::~AssistiveWindowController() {
  if (suggestion_window_view_ && suggestion_window_view_->GetWidget())
    suggestion_window_view_->GetWidget()->RemoveObserver(this);
  if (undo_window_ && undo_window_->GetWidget())
    undo_window_->GetWidget()->RemoveObserver(this);
}

void AssistiveWindowController::InitSuggestionWindow() {
  if (suggestion_window_view_)
    return;
  // suggestion_window_view_ is deleted by DialogDelegateView::DeleteDelegate.
  suggestion_window_view_ = new ui::ime::SuggestionWindowView(GetParentView());
  views::Widget* widget = suggestion_window_view_->InitWidget();
  widget->AddObserver(this);
  widget->Show();
}

void AssistiveWindowController::InitUndoWindow() {
  if (undo_window_)
    return;
  // undo_window is deleted by DialogDelegateView::DeleteDelegate.
  undo_window_ = new ui::ime::UndoWindow(GetParentView(), this);
  views::Widget* widget = undo_window_->InitWidget();
  widget->AddObserver(this);
  widget->Show();
}

void AssistiveWindowController::OnWidgetClosing(views::Widget* widget) {
  if (suggestion_window_view_ &&
      widget == suggestion_window_view_->GetWidget()) {
    widget->RemoveObserver(this);
    suggestion_window_view_ = nullptr;
  }
  if (undo_window_ && widget == undo_window_->GetWidget()) {
    widget->RemoveObserver(this);
    undo_window_ = nullptr;
  }
}

void AssistiveWindowController::HideSuggestion() {
  suggestion_text_ = base::EmptyString16();
  if (suggestion_window_view_)
    suggestion_window_view_->GetWidget()->Close();
}

void AssistiveWindowController::SetBounds(const gfx::Rect& cursor_bounds) {
  if (suggestion_window_view_ && confirmed_length_ == 0)
    suggestion_window_view_->SetBounds(cursor_bounds);
  if (undo_window_)
    undo_window_->SetBounds(cursor_bounds);
}

void AssistiveWindowController::FocusStateChanged() {
  if (suggestion_window_view_)
    HideSuggestion();
  if (undo_window_)
    undo_window_->Hide();
}

void AssistiveWindowController::ShowSuggestion(const base::string16& text,
                                               const size_t confirmed_length,
                                               const bool show_tab) {
  if (!suggestion_window_view_)
    InitSuggestionWindow();
  suggestion_text_ = text;
  confirmed_length_ = confirmed_length;
  suggestion_window_view_->Show(text, confirmed_length, show_tab);
}

base::string16 AssistiveWindowController::GetSuggestionText() const {
  return suggestion_text_;
}

size_t AssistiveWindowController::GetConfirmedLength() const {
  return confirmed_length_;
}

void AssistiveWindowController::SetAssistiveWindowProperties(
    const AssistiveWindowProperties& window) {
  switch (window.type) {
    case ui::ime::AssistiveWindowType::kUndoWindow:
      if (!undo_window_)
        InitUndoWindow();
      window.visible ? undo_window_->Show() : undo_window_->Hide();
  }
}

void AssistiveWindowController::AssistiveWindowClicked(
    ui::ime::ButtonId id,
    ui::ime::AssistiveWindowType type) {}

ui::ime::SuggestionWindowView*
AssistiveWindowController::GetSuggestionWindowViewForTesting() {
  return suggestion_window_view_;
}

}  // namespace input_method
}  // namespace chromeos

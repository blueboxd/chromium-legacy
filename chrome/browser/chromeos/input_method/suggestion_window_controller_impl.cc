// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/suggestion_window_controller_impl.h"

#include <string>
#include <vector>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"           // mash-ok
#include "ash/wm/window_util.h"  // mash-ok
#include "base/logging.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/chromeos/ime/infolist_window.h"
#include "ui/views/widget/widget.h"

namespace chromeos {
namespace input_method {

namespace {}  // namespace

SuggestionWindowControllerImpl::SuggestionWindowControllerImpl() {
  ui::IMEBridge::Get()->SetSuggestionWindowHandler(this);
}

SuggestionWindowControllerImpl::~SuggestionWindowControllerImpl() {
  ui::IMEBridge::Get()->SetSuggestionWindowHandler(nullptr);
}

void SuggestionWindowControllerImpl::Init() {
  if (suggestion_window_view_)
    return;

  gfx::NativeView parent = nullptr;

  aura::Window* active_window = ash::window_util::GetActiveWindow();
  // Use VirtualKeyboardContainer so that it works even with a system modal
  // dialog.
  parent = ash::Shell::GetContainer(
      active_window ? active_window->GetRootWindow()
                    : ash::Shell::GetRootWindowForNewWindows(),
      ash::kShellWindowId_VirtualKeyboardContainer);
  // suggestion_window_view_ is deleted by DialogDelegateView::DeleteDelegate.
  suggestion_window_view_ = new ui::ime::SuggestionWindowView(
      parent, ash::kShellWindowId_VirtualKeyboardContainer);
  views::Widget* widget = suggestion_window_view_->InitWidget();
  widget->AddObserver(this);
  widget->Show();
}

void SuggestionWindowControllerImpl::OnWidgetClosing(views::Widget* widget) {
  if (suggestion_window_view_ &&
      widget == suggestion_window_view_->GetWidget()) {
    widget->RemoveObserver(this);
    suggestion_window_view_ = nullptr;
  }
}

void SuggestionWindowControllerImpl::Hide() {
  suggestion_text_ = base::EmptyString16();
  if (suggestion_window_view_)
    suggestion_window_view_->GetWidget()->Close();
}

void SuggestionWindowControllerImpl::SetBounds(const gfx::Rect& cursor_bounds) {
  if (suggestion_window_view_)
    suggestion_window_view_->SetBounds(cursor_bounds);
}

void SuggestionWindowControllerImpl::FocusStateChanged() {
  if (suggestion_window_view_)
    Hide();
}

void SuggestionWindowControllerImpl::Show(const base::string16& text) {
  if (!suggestion_window_view_)
    Init();
  suggestion_text_ = text;
  suggestion_window_view_->Show(text);
}

base::string16 SuggestionWindowControllerImpl::GetText() const {
  return suggestion_text_;
}

}  // namespace input_method
}  // namespace chromeos

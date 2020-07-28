// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/in_session_auth_dialog.h"

#include "ash/in_session_auth/auth_dialog_debug_view.h"
#include "base/command_line.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

constexpr gfx::Size kDefaultSize(340, 224);

class AuthDialogWidgetDelegate : public views::WidgetDelegate {
 public:
  AuthDialogWidgetDelegate() = default;
  AuthDialogWidgetDelegate(const AuthDialogWidgetDelegate&) = delete;
  AuthDialogWidgetDelegate& operator=(const AuthDialogWidgetDelegate&) = delete;
  ~AuthDialogWidgetDelegate() override = default;

  // views::WidgetDelegate:
  views::View* GetInitiallyFocusedView() override {
    return GetWidget()->GetContentsView();
  }
  void DeleteDelegate() override { delete this; }
  ui::ModalType GetModalType() const override { return ui::MODAL_TYPE_SYSTEM; }
};

std::unique_ptr<views::Widget> CreateAuthDialogWidget(aura::Window* parent) {
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.delegate = new AuthDialogWidgetDelegate();
  params.show_state = ui::SHOW_STATE_NORMAL;
  params.parent = parent;
  params.name = "AuthDialogWidget";
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.shadow_elevation = 3;
  gfx::Rect bounds = display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  bounds.ClampToCenteredSize(kDefaultSize);
  params.bounds = bounds;

  std::unique_ptr<views::Widget> widget = std::make_unique<views::Widget>();
  widget->Init(std::move(params));
  widget->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);
  return widget;
}

}  // namespace

InSessionAuthDialog::InSessionAuthDialog() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kShowAuthDialogDevOverlay)) {
    widget_ = CreateAuthDialogWidget(nullptr);
    widget_->SetContentsView(std::make_unique<AuthDialogDebugView>());

    widget_->Show();
  }
}

InSessionAuthDialog::~InSessionAuthDialog() = default;

}  // namespace ash

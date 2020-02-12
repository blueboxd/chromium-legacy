// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/parent_access_widget.h"

#include <utility>

#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/window_dimmer.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

ParentAccessWidget* instance_ = nullptr;

}  // namespace

ParentAccessWidget::TestApi::TestApi(ParentAccessWidget* widget)
    : parent_access_widget_(widget) {}

ParentAccessWidget::TestApi::~TestApi() = default;

ParentAccessView* ParentAccessWidget::TestApi::parent_access_view() {
  return static_cast<ParentAccessView*>(
      parent_access_widget_->widget_->widget_delegate());
}

// static
void ParentAccessWidget::Show(ParentAccessRequest request,
                              ParentAccessView::Delegate* delegate) {
  DCHECK(!instance_);
  instance_ = new ParentAccessWidget(std::move(request), delegate);
}

// static
ParentAccessWidget* ParentAccessWidget::Get() {
  return instance_;
}

void ParentAccessWidget::UpdateState(ParentAccessRequestViewState state,
                                     const base::string16& title,
                                     const base::string16& description) {
  DCHECK_EQ(instance_, this);
  static_cast<ParentAccessView*>(widget_->widget_delegate())
      ->UpdateState(state, title, description);
}

void ParentAccessWidget::Close(bool success) {
  DCHECK_EQ(instance_, this);
  ParentAccessWidget* instance = instance_;
  instance_ = nullptr;
  std::move(on_parent_access_done_).Run(success);
  widget_->Close();
  delete instance;
}

ParentAccessWidget::ParentAccessWidget(ParentAccessRequest request,
                                       ParentAccessView::Delegate* delegate)
    : on_parent_access_done_(std::move(request.on_parent_access_done)) {
  views::Widget::InitParams widget_params;
  // Using window frameless to be able to get focus on the view input fields,
  // which does not work with popup type.
  widget_params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  widget_params.ownership =
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget_params.opacity =
      views::Widget::InitParams::WindowOpacity::kTranslucent;
  widget_params.accept_events = true;

  ShellWindowId parent_window_id =
      Shell::Get()->session_controller()->GetSessionState() ==
              session_manager::SessionState::ACTIVE
          ? kShellWindowId_SystemModalContainer
          : kShellWindowId_LockSystemModalContainer;
  widget_params.parent =
      Shell::GetPrimaryRootWindow()->GetChildById(parent_window_id);
  if (request.extra_dimmer)
    dimmer_ = std::make_unique<WindowDimmer>(widget_params.parent);
  request.on_parent_access_done =
      base::BindOnce(&ParentAccessWidget::Close, weak_factory_.GetWeakPtr());
  widget_params.delegate = new ParentAccessView(std::move(request), delegate);

  widget_ = std::make_unique<views::Widget>();
  widget_->Init(std::move(widget_params));

  Show();
}

ParentAccessWidget::~ParentAccessWidget() = default;

void ParentAccessWidget::Show() {
  if (dimmer_)
    dimmer_->window()->Show();

  DCHECK(widget_);
  widget_->Show();

  auto* keyboard_controller = Shell::Get()->keyboard_controller();
  if (keyboard_controller && keyboard_controller->IsKeyboardEnabled())
    keyboard_controller->HideKeyboard(HideReason::kSystem);
}

}  // namespace ash

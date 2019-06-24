// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/parent_access_widget.h"

#include <utility>

#include "ash/login/ui/parent_access_view.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/window_dimmer.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/views/widget/widget.h"

namespace ash {

ParentAccessWidget::ParentAccessWidget(const AccountId& account_id,
                                       const OnExitCallback& callback,
                                       ParentAccessRequestReason reason,
                                       bool use_extra_dimmer)
    : callback_(callback) {
  views::Widget::InitParams widget_params;
  // Using window frameless to be able to get focus on the view input fields,
  // which does not work with popup type.
  widget_params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  widget_params.ownership =
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget_params.opacity = views::Widget::InitParams::OPAQUE_WINDOW;
  widget_params.accept_events = true;

  ShellWindowId parent_window_id =
      Shell::Get()->session_controller()->GetSessionState() ==
              session_manager::SessionState::ACTIVE
          ? ash::kShellWindowId_SystemModalContainer
          : ash::kShellWindowId_LockSystemModalContainer;
  widget_params.parent =
      ash::Shell::GetPrimaryRootWindow()->GetChildById(parent_window_id);

  ParentAccessView::Callbacks callbacks;
  callbacks.on_finished = base::BindRepeating(&ParentAccessWidget::OnExit,
                                              weak_factory_.GetWeakPtr());
  widget_params.delegate = new ParentAccessView(account_id, callbacks, reason);

  if (use_extra_dimmer) {
    dimmer_ = std::make_unique<WindowDimmer>(widget_params.parent);
    dimmer_->window()->Show();
  }

  widget_ = std::make_unique<views::Widget>();
  widget_->Init(widget_params);
  widget_->Show();
}

ParentAccessWidget::~ParentAccessWidget() = default;

void ParentAccessWidget::OnExit(bool success) {
  callback_.Run(success);
  widget_->Close();
  dimmer_.reset();
}

}  // namespace ash

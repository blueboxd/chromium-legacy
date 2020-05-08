// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_confirmation_dialog.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

namespace ash {

AcceleratorConfirmationDialog::AcceleratorConfirmationDialog(
    int window_title_text_id,
    int dialog_text_id,
    base::OnceClosure on_accept_callback,
    base::OnceClosure on_cancel_callback) {
  SetTitle(l10n_util::GetStringUTF16(window_title_text_id));
  DialogDelegate::SetButtonLabel(
      ui::DIALOG_BUTTON_OK, l10n_util::GetStringUTF16(IDS_ASH_CONTINUE_BUTTON));
  DialogDelegate::SetAcceptCallback(std::move(on_accept_callback));
  DialogDelegate::SetCancelCallback(std::move(on_cancel_callback));

  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetBorder(views::CreateEmptyBorder(
      views::LayoutProvider::Get()->GetDialogInsetsForContentType(
          views::TEXT, views::TEXT)));
  AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(dialog_text_id)));

  // Parent the dialog widget to the LockSystemModalContainer to ensure that it
  // will get displayed on respective lock/signin or OOBE screen.
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  int container_id = kShellWindowId_SystemModalContainer;
  if (session_controller->IsUserSessionBlocked() ||
      session_controller->GetSessionState() ==
          session_manager::SessionState::OOBE) {
    container_id = kShellWindowId_LockSystemModalContainer;
  }

  views::Widget* widget = CreateDialogWidget(
      this, nullptr,
      Shell::GetContainer(Shell::GetPrimaryRootWindow(), container_id));
  widget->Show();
}

AcceleratorConfirmationDialog::~AcceleratorConfirmationDialog() = default;

ui::ModalType AcceleratorConfirmationDialog::GetModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

base::WeakPtr<AcceleratorConfirmationDialog>
AcceleratorConfirmationDialog::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash

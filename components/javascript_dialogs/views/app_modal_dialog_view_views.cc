// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/javascript_dialogs/views/app_modal_dialog_view_views.h"

#include "base/strings/utf_string_conversions.h"
#include "components/javascript_dialogs/app_modal_dialog_controller.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"

namespace javascript_dialogs {

////////////////////////////////////////////////////////////////////////////////
// AppModalDialogViewViews, public:

AppModalDialogViewViews::AppModalDialogViewViews(
    AppModalDialogController* controller)
    : controller_(controller) {
  int options = views::MessageBoxView::DETECT_DIRECTIONALITY;
  if (controller->javascript_dialog_type() ==
      content::JAVASCRIPT_DIALOG_TYPE_PROMPT)
    options |= views::MessageBoxView::HAS_PROMPT_FIELD;

  views::MessageBoxView::InitParams params(controller->message_text());
  params.options = options;
  params.default_prompt = controller->default_prompt_text();
  message_box_view_ = new views::MessageBoxView(params);
  DCHECK(message_box_view_);

  message_box_view_->AddAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_CONTROL_DOWN));
  if (controller->display_suppress_checkbox()) {
    message_box_view_->SetCheckBoxLabel(
        l10n_util::GetStringUTF16(IDS_JAVASCRIPT_MESSAGEBOX_SUPPRESS_OPTION));
  }

  DialogDelegate::SetButtons(
      controller_->javascript_dialog_type() ==
              content::JAVASCRIPT_DIALOG_TYPE_ALERT
          ? ui::DIALOG_BUTTON_OK
          : (ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL));

  if (controller_->is_before_unload_dialog()) {
    DialogDelegate::SetButtonLabel(
        ui::DIALOG_BUTTON_OK,
        l10n_util::GetStringUTF16(
            controller_->is_reload()
                ? IDS_BEFORERELOAD_MESSAGEBOX_OK_BUTTON_LABEL
                : IDS_BEFOREUNLOAD_MESSAGEBOX_OK_BUTTON_LABEL));
  }
}

AppModalDialogViewViews::~AppModalDialogViewViews() = default;

////////////////////////////////////////////////////////////////////////////////
// AppModalDialogViewViews, AppModalDialogView implementation:

void AppModalDialogViewViews::ShowAppModalDialog() {
  GetWidget()->Show();
}

void AppModalDialogViewViews::ActivateAppModalDialog() {
  GetWidget()->Show();
  GetWidget()->Activate();
}

void AppModalDialogViewViews::CloseAppModalDialog() {
  GetWidget()->Close();
}

void AppModalDialogViewViews::AcceptAppModalDialog() {
  AcceptDialog();
}

void AppModalDialogViewViews::CancelAppModalDialog() {
  CancelDialog();
}

bool AppModalDialogViewViews::IsShowing() const {
  return GetWidget()->IsVisible();
}

//////////////////////////////////////////////////////////////////////////////
// AppModalDialogViewViews, views::DialogDelegate implementation:

base::string16 AppModalDialogViewViews::GetWindowTitle() const {
  return controller_->title();
}

void AppModalDialogViewViews::DeleteDelegate() {
  delete this;
}

bool AppModalDialogViewViews::Cancel() {
  controller_->OnCancel(message_box_view_->IsCheckBoxSelected());
  return true;
}

bool AppModalDialogViewViews::Accept() {
  controller_->OnAccept(message_box_view_->GetInputText(),
                        message_box_view_->IsCheckBoxSelected());
  return true;
}

ui::ModalType AppModalDialogViewViews::GetModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

views::View* AppModalDialogViewViews::GetContentsView() {
  return message_box_view_;
}

views::View* AppModalDialogViewViews::GetInitiallyFocusedView() {
  if (message_box_view_->text_box())
    return message_box_view_->text_box();
  return views::DialogDelegate::GetInitiallyFocusedView();
}

bool AppModalDialogViewViews::ShouldShowCloseButton() const {
  return false;
}

void AppModalDialogViewViews::WindowClosing() {
  controller_->OnClose();
}

views::Widget* AppModalDialogViewViews::GetWidget() {
  return message_box_view_->GetWidget();
}

const views::Widget* AppModalDialogViewViews::GetWidget() const {
  return message_box_view_->GetWidget();
}

}  // namespace javascript_dialogs

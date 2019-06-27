// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/add_supervision/confirm_signout_dialog.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace chromeos {

namespace {
// The width of the text body in the dialog.
const int kDialogBodyTextWidth = 250;
}  // namespace

ConfirmSignoutDialog::ConfirmSignoutDialog() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetBorder(views::CreateEmptyBorder(
      views::LayoutProvider::Get()->GetDialogInsetsForContentType(
          views::DialogContentType::TEXT, views::DialogContentType::TEXT)));

  base::string16 given_name =
      user_manager::UserManager::Get()->GetPrimaryUser()->GetGivenName();

  // |body_| will be owned by the views system.
  views::Label* body = new views::Label;
  body->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  body->SetMultiLine(true);
  body->SetText(l10n_util::GetStringFUTF16(IDS_ADD_SUPERVISION_EXIT_DIALOG_BODY,
                                           given_name));
  body->SizeToFit(kDialogBodyTextWidth);
  AddChildView(body);
}

ConfirmSignoutDialog::~ConfirmSignoutDialog() = default;

ui::ModalType ConfirmSignoutDialog::GetModalType() const {
  return ui::ModalType::MODAL_TYPE_SYSTEM;
}

base::string16 ConfirmSignoutDialog::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_ADD_SUPERVISION_EXIT_DIALOG_TITLE);
}

bool ConfirmSignoutDialog::Accept() {
  chrome::AttemptUserExit();
  return true;
}

int ConfirmSignoutDialog::GetDialogButtons() const {
  return ui::DialogButton::DIALOG_BUTTON_OK |
         ui::DialogButton::DIALOG_BUTTON_CANCEL;
}

base::string16 ConfirmSignoutDialog::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DialogButton::DIALOG_BUTTON_OK) {
    return l10n_util::GetStringUTF16(
        IDS_ADD_SUPERVISION_EXIT_DIALOG_SIGNOUT_BUTTON_LABEL);
  }
  return l10n_util::GetStringUTF16(
      IDS_ADD_SUPERVISION_EXIT_DIALOG_CANCEL_BUTTON_LABEL);
}

// static
void ConfirmSignoutDialog::Show() {
  // Ownership of the ConfirmSignoutDialog is passed to the views system.
  // Dialog is system-modal, so no parent window is needed.
  constrained_window::CreateBrowserModalDialogViews(new ConfirmSignoutDialog(),
                                                    nullptr /* parent window */)
      ->Show();
}

}  // namespace chromeos

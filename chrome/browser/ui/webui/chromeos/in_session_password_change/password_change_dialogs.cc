// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/in_session_password_change/password_change_dialogs.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/saml/password_expiry_notification.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/confirm_password_change_handler.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/password_change_handler.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/urgent_password_expiry_notification_handler.h"
#include "chrome/browser/ui/webui/localized_string.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/login/auth/saml_password_attributes.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui_data_source.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/strings/grit/ui_strings.h"

namespace chromeos {

namespace {

PasswordChangeDialog* g_dialog = nullptr;

ConfirmPasswordChangeDialog* g_confirm_dialog = nullptr;

UrgentPasswordExpiryNotificationDialog* g_notification_dialog = nullptr;

constexpr gfx::Size kPasswordChangeDialogDesiredSize(768, 640);

// TODO(https://crbug.com/930109): Change these numbers depending on what is
// shown in the dialog.
constexpr gfx::Size kConfirmPasswordChangeDialogDesiredSize(520, 380);

constexpr gfx::Size kNotificationDesiredSize = kPasswordChangeDialogDesiredSize;

// Given a desired size, returns the same size if it fits on screen,
// or the closest possible size that will fit on the screen.
gfx::Size FitSizeToDisplay(const gfx::Size& desired) {
  const display::Display display =
      display::Screen::GetScreen()->GetPrimaryDisplay();

  gfx::Size display_size = display.size();

  if (display.rotation() == display::Display::ROTATE_90 ||
      display.rotation() == display::Display::ROTATE_270) {
    display_size = gfx::Size(display_size.height(), display_size.width());
  }

  return gfx::Size(std::min(desired.width(), display_size.width()),
                   std::min(desired.height(), display_size.height()));
}

}  // namespace

BasePasswordDialog::BasePasswordDialog(GURL url, gfx::Size desired_size)
    : SystemWebDialogDelegate(url, /*title=*/base::string16()),
      desired_size_(desired_size) {}

BasePasswordDialog::~BasePasswordDialog() {}

void BasePasswordDialog::GetDialogSize(gfx::Size* size) const {
  *size = FitSizeToDisplay(desired_size_);
}

void BasePasswordDialog::AdjustWidgetInitParams(
    views::Widget::InitParams* params) {
  params->type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
}

ui::ModalType BasePasswordDialog::GetDialogModalType() const {
  return ui::ModalType::MODAL_TYPE_SYSTEM;
}

// static
void PasswordChangeDialog::Show() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_dialog) {
    g_dialog->Focus();
    return;
  }
  g_dialog = new PasswordChangeDialog();
  g_dialog->ShowSystemDialog();
}

// static
void PasswordChangeDialog::Dismiss() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_dialog)
    g_dialog->Close();
}

PasswordChangeDialog::PasswordChangeDialog()
    : BasePasswordDialog(GURL(chrome::kChromeUIPasswordChangeUrl),
                         kPasswordChangeDialogDesiredSize) {}

PasswordChangeDialog::~PasswordChangeDialog() {
  DCHECK_EQ(this, g_dialog);
  g_dialog = nullptr;
}

// static
void ConfirmPasswordChangeDialog::Show(const std::string& scraped_old_password,
                                       const std::string& scraped_new_password,
                                       bool show_spinner_initially) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_confirm_dialog) {
    g_confirm_dialog->Focus();
    return;
  }
  g_confirm_dialog = new ConfirmPasswordChangeDialog(
      scraped_old_password, scraped_new_password, show_spinner_initially);
  g_confirm_dialog->ShowSystemDialog();
}

// static
void ConfirmPasswordChangeDialog::Dismiss() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_confirm_dialog)
    g_confirm_dialog->Close();
}

ConfirmPasswordChangeDialog::ConfirmPasswordChangeDialog(
    const std::string& scraped_old_password,
    const std::string& scraped_new_password,
    bool show_spinner_initially)
    : BasePasswordDialog(GURL(chrome::kChromeUIConfirmPasswordChangeUrl),
                         kConfirmPasswordChangeDialogDesiredSize),
      scraped_old_password_(scraped_old_password),
      scraped_new_password_(scraped_new_password),
      show_spinner_initially_(show_spinner_initially) {}

ConfirmPasswordChangeDialog::~ConfirmPasswordChangeDialog() {
  DCHECK_EQ(this, g_confirm_dialog);
  g_confirm_dialog = nullptr;
}

std::string ConfirmPasswordChangeDialog::GetDialogArgs() const {
  // TODO(https://crbug.com/930109): Configure the embedded UI to only display
  // prompts for the passwords that were not scraped.
  std::string data;
  base::Value dialog_args(base::Value::Type::DICTIONARY);
  dialog_args.SetBoolKey("showSpinnerInitially", show_spinner_initially_);
  base::JSONWriter::Write(dialog_args, &data);
  return data;
}

// static
void UrgentPasswordExpiryNotificationDialog::Show() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_notification_dialog) {
    g_notification_dialog->Focus();
    return;
  }
  g_notification_dialog = new UrgentPasswordExpiryNotificationDialog();
  g_notification_dialog->ShowSystemDialog();
}

// static
void UrgentPasswordExpiryNotificationDialog::Dismiss() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_notification_dialog)
    g_notification_dialog->Close();
}

UrgentPasswordExpiryNotificationDialog::UrgentPasswordExpiryNotificationDialog()
    : BasePasswordDialog(
          GURL(chrome::kChromeUIUrgentPasswordExpiryNotificationUrl),
          kNotificationDesiredSize) {}

UrgentPasswordExpiryNotificationDialog::
    ~UrgentPasswordExpiryNotificationDialog() {
  DCHECK_EQ(this, g_notification_dialog);
  g_notification_dialog = nullptr;
}

}  // namespace chromeos

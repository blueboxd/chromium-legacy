// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_PASSWORD_CHANGE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_PASSWORD_CHANGE_UI_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

// System dialog wrapping chrome:://password-change
class PasswordChangeDialog : public SystemWebDialogDelegate {
 public:
  static void Show();
  static void Dismiss();

 protected:
  PasswordChangeDialog();
  ~PasswordChangeDialog() override;

  // ui::WebDialogDelegate:
  void GetDialogSize(gfx::Size* size) const override;
  void AdjustWidgetInitParams(views::Widget::InitParams* params) override;
  ui::ModalType GetDialogModalType() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordChangeDialog);
};

// For chrome:://password-change
class PasswordChangeUI : public ui::WebDialogUI {
 public:
  explicit PasswordChangeUI(content::WebUI* web_ui);
  ~PasswordChangeUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordChangeUI);
};

// System dialog wrapping chrome://confirm-password-change
class ConfirmPasswordChangeDialog : public SystemWebDialogDelegate {
 public:
  static void Show(const std::string& scraped_old_password,
                   const std::string& scraped_new_password,
                   bool show_spinner_initially);
  static void Dismiss();

 protected:
  ConfirmPasswordChangeDialog(const std::string& scraped_old_password,
                              const std::string& scraped_new_password,
                              bool show_spinner_initially);
  ~ConfirmPasswordChangeDialog() override;

  // ui::WebDialogDelegate:
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  void AdjustWidgetInitParams(views::Widget::InitParams* params) override;
  ui::ModalType GetDialogModalType() const override;

 private:
  std::string scraped_old_password_;
  std::string scraped_new_password_;
  bool show_spinner_initially_ = false;

  DISALLOW_COPY_AND_ASSIGN(ConfirmPasswordChangeDialog);
};

// For chrome:://confirm-password-change
class ConfirmPasswordChangeUI : public ui::WebDialogUI {
 public:
  explicit ConfirmPasswordChangeUI(content::WebUI* web_ui);
  ~ConfirmPasswordChangeUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConfirmPasswordChangeUI);
};

// System dialog wrapping chrome://urgent-password-expiry-notification
class UrgentPasswordExpiryNotificationDialog : public SystemWebDialogDelegate {
 public:
  static void Show();
  static void Dismiss();

 protected:
  UrgentPasswordExpiryNotificationDialog();
  ~UrgentPasswordExpiryNotificationDialog() override;

  // ui::WebDialogDelegate:
  void GetDialogSize(gfx::Size* size) const override;
  void AdjustWidgetInitParams(views::Widget::InitParams* params) override;
  ui::ModalType GetDialogModalType() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UrgentPasswordExpiryNotificationDialog);
};

// For chrome:://urgent-password-expiry-notification
class UrgentPasswordExpiryNotificationUI : public ui::WebDialogUI {
 public:
  explicit UrgentPasswordExpiryNotificationUI(content::WebUI* web_ui);
  ~UrgentPasswordExpiryNotificationUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UrgentPasswordExpiryNotificationUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_PASSWORD_CHANGE_UI_H_

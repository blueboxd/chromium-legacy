// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_INSESSION_PASSWORD_CHANGE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_INSESSION_PASSWORD_CHANGE_UI_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

class Profile;

namespace chromeos {

// System dialog wrapping chrome:://password-change
class PasswordChangeDialog : public SystemWebDialogDelegate {
 public:
  static void Show(Profile* profile);
  static void Dismiss();

 protected:
  explicit PasswordChangeDialog(const base::string16& title);
  ~PasswordChangeDialog() override;

  // ui::WebDialogDelegate:
  void GetDialogSize(gfx::Size* size) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordChangeDialog);
};

// For chrome:://password-change
class InSessionPasswordChangeUI : public ui::WebDialogUI {
 public:
  explicit InSessionPasswordChangeUI(content::WebUI* web_ui);
  ~InSessionPasswordChangeUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InSessionPasswordChangeUI);
};

// System dialog wrapping chrome://confirm-password-change
class ConfirmPasswordChangeDialog : public SystemWebDialogDelegate {
 public:
  static void Show();
  static void Dismiss();

 protected:
  ConfirmPasswordChangeDialog();
  ~ConfirmPasswordChangeDialog() override;

  // ui::WebDialogDelegate:
  void GetDialogSize(gfx::Size* size) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConfirmPasswordChangeDialog);
};

// For chrome:://confirm-password-change
class InSessionConfirmPasswordChangeUI : public ui::WebDialogUI {
 public:
  explicit InSessionConfirmPasswordChangeUI(content::WebUI* web_ui);
  ~InSessionConfirmPasswordChangeUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InSessionConfirmPasswordChangeUI);
};

// System dialog wrapping chrome://urgent-password-expiry-notification
class UrgentPasswordExpiryNotificationDialog : public SystemWebDialogDelegate {
 public:
  static void Show(int less_than_n_days);
  static void Dismiss();

 protected:
  explicit UrgentPasswordExpiryNotificationDialog(int less_than_n_days);
  ~UrgentPasswordExpiryNotificationDialog() override;

  // ui::WebDialogDelegate:
  void GetDialogSize(gfx::Size* size) const override;

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

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_INSESSION_PASSWORD_CHANGE_UI_H_

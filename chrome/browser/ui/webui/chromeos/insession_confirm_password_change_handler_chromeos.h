// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_INSESSION_CONFIRM_PASSWORD_CHANGE_HANDLER_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_INSESSION_CONFIRM_PASSWORD_CHANGE_HANDLER_CHROMEOS_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace chromeos {

class InSessionConfirmPasswordChangeHandler
    : public content::WebUIMessageHandler {
 public:
  InSessionConfirmPasswordChangeHandler();
  ~InSessionConfirmPasswordChangeHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // Tries to change the cryptohome password once the confirm-password-change
  // dialog is filled in and the password change is confirmed.
  void HandleChangePassword(const base::ListValue* passwords);

 private:
  base::WeakPtrFactory<InSessionConfirmPasswordChangeHandler> weak_factory_{
      this};
  DISALLOW_COPY_AND_ASSIGN(InSessionConfirmPasswordChangeHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_INSESSION_CONFIRM_PASSWORD_CHANGE_HANDLER_CHROMEOS_H_

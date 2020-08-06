// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_IN_SESSION_AUTH_DIALOG_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_IN_SESSION_AUTH_DIALOG_CLIENT_H_

#include "ash/public/cpp/in_session_auth_dialog_client.h"

// Handles method calls sent from Ash to ChromeOS.
class InSessionAuthDialogClient : public ash::InSessionAuthDialogClient {
 public:
  InSessionAuthDialogClient();
  InSessionAuthDialogClient(const InSessionAuthDialogClient&) = delete;
  InSessionAuthDialogClient& operator=(const InSessionAuthDialogClient&) =
      delete;
  ~InSessionAuthDialogClient() override;

  static bool HasInstance();
  static InSessionAuthDialogClient* Get();

  // ash::InSessionAuthDialogClient:
  void AuthenticateUserWithPasswordOrPin(
      const std::string& password,
      bool authenticated_by_pin,
      base::OnceCallback<void(bool)> callback) override;
};

#endif  // CHROME_BROWSER_UI_ASH_IN_SESSION_AUTH_DIALOG_CLIENT_H_

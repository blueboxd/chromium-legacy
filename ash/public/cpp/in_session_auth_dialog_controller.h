// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_IN_SESSION_AUTH_DIALOG_CONTROLLER_H_
#define ASH_PUBLIC_CPP_IN_SESSION_AUTH_DIALOG_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/in_session_auth_dialog_client.h"

namespace ash {

// InSessionAuthDialogController manages the in-session auth dialog.
class ASH_PUBLIC_EXPORT InSessionAuthDialogController {
 public:
  // Return the singleton instance.
  static InSessionAuthDialogController* Get();

  // Sets the client that will handle authentication.
  virtual void SetClient(InSessionAuthDialogClient* client) = 0;

  // Displays the authentication dialog.
  virtual void ShowAuthenticationDialog() = 0;

  // Destroys the authentication dialog.
  virtual void DestroyAuthenticationDialog() = 0;

 protected:
  InSessionAuthDialogController();
  virtual ~InSessionAuthDialogController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_IN_SESSION_AUTH_DIALOG_CONTROLLER_H_

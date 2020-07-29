// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_IN_SESSION_AUTH_DIALOG_CONTROLLER_H_
#define ASH_PUBLIC_CPP_IN_SESSION_AUTH_DIALOG_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// InSessionAuthDialogController manages the in-session auth dialog.
class ASH_PUBLIC_EXPORT InSessionAuthDialogController {
 public:
  // Return the singleton instance.
  static InSessionAuthDialogController* Get();

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

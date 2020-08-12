// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IN_SESSION_AUTH_IN_SESSION_AUTH_DIALOG_CONTROLLER_IMPL_H_
#define ASH_IN_SESSION_AUTH_IN_SESSION_AUTH_DIALOG_CONTROLLER_IMPL_H_

#include <memory>

#include "ash/in_session_auth/in_session_auth_dialog.h"
#include "ash/public/cpp/in_session_auth_dialog_controller.h"
#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"

namespace ash {

class InSessionAuthDialogClient;

// InSessionAuthDialogControllerImpl persists as long as UI is running.
class InSessionAuthDialogControllerImpl : public InSessionAuthDialogController {
 public:
  // Callback for authentication checks. |success| is nullopt if an
  // authentication check did not run, otherwise it is true/false if auth
  // succeeded/failed.
  using OnAuthenticateCallback =
      base::OnceCallback<void(base::Optional<bool> success)>;

  InSessionAuthDialogControllerImpl();
  InSessionAuthDialogControllerImpl(const InSessionAuthDialogControllerImpl&) =
      delete;
  InSessionAuthDialogControllerImpl& operator=(
      const InSessionAuthDialogControllerImpl&) = delete;
  ~InSessionAuthDialogControllerImpl() override;

  // InSessionAuthDialogController overrides
  void SetClient(InSessionAuthDialogClient* client) override;
  void ShowAuthenticationDialog() override;
  void DestroyAuthenticationDialog() override;
  void AuthenticateUserWithPasswordOrPin(
      const std::string& password,
      OnAuthenticateCallback callback) override;

 private:
  // Callback to execute when auth on ChromeOS side completes.
  void OnAuthenticateComplete(OnAuthenticateCallback callback, bool success);

  InSessionAuthDialogClient* client_ = nullptr;

  std::unique_ptr<InSessionAuthDialog> dialog_;

  base::WeakPtrFactory<InSessionAuthDialogControllerImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_IN_SESSION_AUTH_IN_SESSION_AUTH_DIALOG_CONTROLLER_IMPL_H_

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/in_session_auth_dialog_controller_impl.h"

#include "ash/public/cpp/in_session_auth_dialog_client.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string_util.h"

namespace ash {

InSessionAuthDialogControllerImpl::InSessionAuthDialogControllerImpl() =
    default;

InSessionAuthDialogControllerImpl::~InSessionAuthDialogControllerImpl() =
    default;

void InSessionAuthDialogControllerImpl::SetClient(
    InSessionAuthDialogClient* client) {
  client_ = client;
}

void InSessionAuthDialogControllerImpl::ShowAuthenticationDialog() {
  dialog_ = std::make_unique<InSessionAuthDialog>();
}

void InSessionAuthDialogControllerImpl::DestroyAuthenticationDialog() {
  dialog_.reset();
}

void InSessionAuthDialogControllerImpl::AuthenticateUserWithPasswordOrPin(
    const std::string& password,
    OnAuthenticateCallback callback) {
  DCHECK(client_);

  // TODO(b/156258540): Check that PIN is enabled / set up for this user.
  bool authenticated_by_pin = base::ContainsOnlyChars(password, "0123456789");

  client_->AuthenticateUserWithPasswordOrPin(
      password, authenticated_by_pin,
      base::BindOnce(&InSessionAuthDialogControllerImpl::OnAuthenticateComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void InSessionAuthDialogControllerImpl::OnAuthenticateComplete(
    OnAuthenticateCallback callback,
    bool success) {
  std::move(callback).Run(success);
  // TODO(b/156258540): send status to UserAuthenticationServiceProvider for
  // dbus response.
  DestroyAuthenticationDialog();
}

}  // namespace ash

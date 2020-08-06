// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/in_session_auth_dialog_controller_impl.h"

#include "ash/public/cpp/in_session_auth_dialog_client.h"

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

}  // namespace ash

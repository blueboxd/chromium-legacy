// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/in_session_auth_dialog_client.h"

#include <utility>

#include "ash/public/cpp/in_session_auth_dialog_controller.h"
#include "base/callback.h"

namespace {
InSessionAuthDialogClient* g_auth_dialog_client_instance = nullptr;
}  // namespace

InSessionAuthDialogClient::InSessionAuthDialogClient() {
  // Register this object as the client interface implementation.
  ash::InSessionAuthDialogController::Get()->SetClient(this);

  DCHECK(!g_auth_dialog_client_instance);
  g_auth_dialog_client_instance = this;
}

InSessionAuthDialogClient::~InSessionAuthDialogClient() {
  ash::InSessionAuthDialogController::Get()->SetClient(nullptr);
  DCHECK_EQ(this, g_auth_dialog_client_instance);
  g_auth_dialog_client_instance = nullptr;
}

// static
bool InSessionAuthDialogClient::HasInstance() {
  return !!g_auth_dialog_client_instance;
}

// static
InSessionAuthDialogClient* InSessionAuthDialogClient::Get() {
  DCHECK(g_auth_dialog_client_instance);
  return g_auth_dialog_client_instance;
}

void InSessionAuthDialogClient::AuthenticateUserWithPasswordOrPin(
    const std::string& password,
    bool authenticated_by_pin,
    base::OnceCallback<void(bool)> callback) {
  // TODO(yichengli): Implement.
  std::move(callback).Run(false);
}

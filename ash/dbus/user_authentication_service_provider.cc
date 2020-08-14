// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/dbus/user_authentication_service_provider.h"

#include "ash/public/cpp/in_session_auth_dialog_controller.h"
#include "base/bind.h"
#include "base/logging.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

UserAuthenticationServiceProvider::UserAuthenticationServiceProvider() =
    default;

UserAuthenticationServiceProvider::~UserAuthenticationServiceProvider() =
    default;

void UserAuthenticationServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      chromeos::kUserAuthenticationServiceInterface,
      chromeos::kUserAuthenticationServiceShowAuthDialogMethod,
      base::BindRepeating(&UserAuthenticationServiceProvider::ShowAuthDialog,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&UserAuthenticationServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
}

void UserAuthenticationServiceProvider::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
  }
}

void UserAuthenticationServiceProvider::ShowAuthDialog(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  auto* auth_dialog_controller = InSessionAuthDialogController::Get();
  auth_dialog_controller->ShowAuthenticationDialog(base::BindOnce(
      &UserAuthenticationServiceProvider::OnAuthFlowComplete,
      weak_ptr_factory_.GetWeakPtr(), method_call, std::move(response_sender)));
}

void UserAuthenticationServiceProvider::OnAuthFlowComplete(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender,
    bool success) {
  DCHECK(method_call && !response_sender.is_null());

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendBool(success);
  std::move(response_sender).Run(std::move(response));
}

}  // namespace ash

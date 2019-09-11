// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/android/dev_ui_loader/dev_ui_loader_message_handler.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/android/dev_ui/dev_ui_module_provider.h"

DevUiLoaderMessageHandler::DevUiLoaderMessageHandler() = default;

DevUiLoaderMessageHandler::~DevUiLoaderMessageHandler() = default;

void DevUiLoaderMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getDevUiDfmState",
      base::BindRepeating(&DevUiLoaderMessageHandler::HandleGetDevUiDfmState,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "installAndLoadDevUiDfm",
      base::BindRepeating(
          &DevUiLoaderMessageHandler::HandleInstallAndLoadDevUiDfm,
          weak_ptr_factory_.GetWeakPtr()));
}

void DevUiLoaderMessageHandler::HandleGetDevUiDfmState(
    const base::ListValue* args) {
  const base::Value* callback_id = nullptr;
  CHECK(args->Get(0, &callback_id));
  const char* response = "ready";
  if (!dev_ui::DevUiModuleProvider::GetInstance().ModuleInstalled())
    response = "not-installed";
  else if (!dev_ui::DevUiModuleProvider::GetInstance().ModuleLoaded())
    response = "not-loaded";
  AllowJavascript();
  ResolveJavascriptCallback(*callback_id, base::Value(response));
}

void DevUiLoaderMessageHandler::ReplyToJavaScript(
    const base::Value& callback_id,
    const char* return_value) {
  AllowJavascript();
  base::ListValue response;
  response.Append(base::Value(return_value));
  ResolveJavascriptCallback(callback_id, response);
}

void DevUiLoaderMessageHandler::HandleInstallAndLoadDevUiDfm(
    const base::ListValue* args) {
  const base::Value* callback_id = nullptr;
  CHECK(args->Get(0, &callback_id));

  if (!dev_ui::DevUiModuleProvider::GetInstance().ModuleInstalled()) {
    dev_ui::DevUiModuleProvider::GetInstance().InstallModule(base::BindOnce(
        &DevUiLoaderMessageHandler::OnDevUiDfmInstallWithStatus,
        weak_ptr_factory_.GetWeakPtr(), callback_id->GetString()));

  } else if (!dev_ui::DevUiModuleProvider::GetInstance().ModuleLoaded()) {
    dev_ui::DevUiModuleProvider::GetInstance().LoadModule(base::BindOnce(
        &DevUiLoaderMessageHandler::OnDevUiResourceLoaded,
        weak_ptr_factory_.GetWeakPtr(), callback_id->GetString()));

  } else {
    ReplyToJavaScript(*callback_id, "noop");
  }
}

void DevUiLoaderMessageHandler::OnDevUiDfmInstallWithStatus(
    std::string callback_id_string,
    bool success) {
  if (success) {
    dev_ui::DevUiModuleProvider::GetInstance().LoadModule(
        base::BindOnce(&DevUiLoaderMessageHandler::OnDevUiResourceLoaded,
                       weak_ptr_factory_.GetWeakPtr(), callback_id_string));
  } else {
    ReplyToJavaScript(base::Value(callback_id_string), "failure");
  }
}

void DevUiLoaderMessageHandler::OnDevUiResourceLoaded(
    std::string callback_id_string) {
  ReplyToJavaScript(base::Value(callback_id_string), "success");
}

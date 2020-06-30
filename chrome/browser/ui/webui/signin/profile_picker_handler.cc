// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/profile_picker_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"

ProfilePickerHandler::ProfilePickerHandler() = default;

ProfilePickerHandler::~ProfilePickerHandler() = default;

void ProfilePickerHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "mainViewInitialize",
      base::BindRepeating(&ProfilePickerHandler::HandleMainViewInitialize,
                          base::Unretained(this)));
}

void ProfilePickerHandler::HandleMainViewInitialize(
    const base::ListValue* args) {
  AllowJavascript();
  PushProfilesList();
}

void ProfilePickerHandler::PushProfilesList() {
  FireWebUIListener("profiles-list-changed", GetProfilesList());
}

base::Value ProfilePickerHandler::GetProfilesList() {
  // TODO(msalama): Return profiles list.
  return base::Value(base::Value::Type::LIST);
}

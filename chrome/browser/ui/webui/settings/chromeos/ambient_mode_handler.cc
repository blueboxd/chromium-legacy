// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/ambient_mode_handler.h"

#include "base/bind.h"
#include "base/values.h"

namespace chromeos {
namespace settings {

AmbientModeHandler::AmbientModeHandler() = default;

AmbientModeHandler::~AmbientModeHandler() = default;

void AmbientModeHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "onAmbientModePageReady",
      base::BindRepeating(&AmbientModeHandler::HandleInitialized,
                          base::Unretained(this)));
}

void AmbientModeHandler::HandleInitialized(const base::ListValue* args) {
  CHECK(args);
  CHECK(args->empty());

  AllowJavascript();
}

}  // namespace settings
}  // namespace chromeos

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/printing/printing_api.h"

#include "chrome/browser/chromeos/extensions/printing/printing_api_handler.h"

namespace extensions {

PrintingGetPrintersFunction::~PrintingGetPrintersFunction() = default;

ExtensionFunction::ResponseAction PrintingGetPrintersFunction::Run() {
  return RespondNow(ArgumentList(api::printing::GetPrinters::Results::Create(
      PrintingAPIHandler::Get(browser_context())->GetPrinters())));
}

}  // namespace extensions

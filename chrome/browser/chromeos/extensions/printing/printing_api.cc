// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/printing/printing_api.h"

#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/printing/printing_api_handler.h"

namespace extensions {

PrintingSubmitJobFunction::~PrintingSubmitJobFunction() = default;

ExtensionFunction::ResponseAction PrintingSubmitJobFunction::Run() {
  std::unique_ptr<api::printing::SubmitJob::Params> params(
      api::printing::SubmitJob::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  PrintingAPIHandler::Get(browser_context())
      ->SubmitJob(extension_id(), std::move(params),
                  base::BindOnce(
                      &PrintingSubmitJobFunction::OnPrintJobSubmitted, this));

  return RespondLater();
}

void PrintingSubmitJobFunction::OnPrintJobSubmitted(
    base::Optional<api::printing::SubmitJobStatus> status,
    std::unique_ptr<std::string> job_id,
    base::Optional<std::string> error) {
  if (error.has_value()) {
    Respond(Error(error.value()));
    return;
  }
  api::printing::SubmitJobResponse response;
  DCHECK(status.has_value());
  response.status = status.value();
  response.job_id = std::move(job_id);
  Respond(OneArgument(response.ToValue()));
}

PrintingGetPrintersFunction::~PrintingGetPrintersFunction() = default;

ExtensionFunction::ResponseAction PrintingGetPrintersFunction::Run() {
  return RespondNow(ArgumentList(api::printing::GetPrinters::Results::Create(
      PrintingAPIHandler::Get(browser_context())->GetPrinters())));
}

PrintingGetPrinterInfoFunction::~PrintingGetPrinterInfoFunction() = default;

ExtensionFunction::ResponseAction PrintingGetPrinterInfoFunction::Run() {
  std::unique_ptr<api::printing::GetPrinterInfo::Params> params(
      api::printing::GetPrinterInfo::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  PrintingAPIHandler::Get(browser_context())
      ->GetPrinterInfo(
          params->printer_id,
          base::BindOnce(
              &PrintingGetPrinterInfoFunction::OnPrinterInfoRetrieved, this));

  return RespondLater();
}

void PrintingGetPrinterInfoFunction::OnPrinterInfoRetrieved(
    base::Optional<base::Value> capabilities,
    base::Optional<api::printing::PrinterStatus> status,
    base::Optional<std::string> error) {
  if (error.has_value()) {
    Respond(Error(error.value()));
    return;
  }
  api::printing::GetPrinterInfoResponse response;
  if (capabilities.has_value()) {
    response.capabilities =
        std::make_unique<api::printing::GetPrinterInfoResponse::Capabilities>();
    base::Value capabilities_value = std::move(capabilities.value());
    CHECK(capabilities_value.is_dict());
    // It's safe just to swap values here as |capabilities_value| stores exactly
    // the same object as |response.capabilities| expects.
    response.capabilities->additional_properties.Swap(
        static_cast<base::DictionaryValue*>(&capabilities_value));
  }
  DCHECK(status.has_value());
  response.status = status.value();
  Respond(OneArgument(response.ToValue()));
}

}  // namespace extensions

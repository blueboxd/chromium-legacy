// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_PRINTING_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_PRINTING_API_H_

#include <memory>
#include <string>

#include "base/optional.h"
#include "chrome/common/extensions/api/printing.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace base {
class Value;
}  // namespace base

namespace extensions {

class PrintingSubmitJobFunction : public ExtensionFunction {
 protected:
  ~PrintingSubmitJobFunction() override;

  // ExtensionFunction:
  void GetQuotaLimitHeuristics(QuotaLimitHeuristics* heuristics) const override;
  ResponseAction Run() override;

 private:
  void OnPrintJobSubmitted(
      base::Optional<api::printing::SubmitJobStatus> status,
      std::unique_ptr<std::string> job_id,
      base::Optional<std::string> error);
  DECLARE_EXTENSION_FUNCTION("printing.submitJob", PRINTING_SUBMITJOB)
};

class PrintingCancelJobFunction : public ExtensionFunction {
 protected:
  ~PrintingCancelJobFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("printing.cancelJob", PRINTING_CANCELJOB)
};

class PrintingGetPrintersFunction : public ExtensionFunction {
 protected:
  ~PrintingGetPrintersFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("printing.getPrinters", PRINTING_GETPRINTERS)
};

class PrintingGetPrinterInfoFunction : public ExtensionFunction {
 protected:
  ~PrintingGetPrinterInfoFunction() override;

  // ExtensionFunction:
  void GetQuotaLimitHeuristics(QuotaLimitHeuristics* heuristics) const override;
  ResponseAction Run() override;

 private:
  void OnPrinterInfoRetrieved(
      base::Optional<base::Value> capabilities,
      base::Optional<api::printing::PrinterStatus> status,
      base::Optional<std::string> error);
  DECLARE_EXTENSION_FUNCTION("printing.getPrinterInfo", PRINTING_GETPRINTERINFO)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_PRINTING_API_H_

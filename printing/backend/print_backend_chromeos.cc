// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend.h"

#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/values.h"

#if defined(USE_CUPS)
#include "printing/backend/cups_ipp_utils.h"
#include "printing/backend/print_backend_cups_ipp.h"
#endif  // defined(USE_CUPS)

namespace printing {

// Provides either a stubbed out PrintBackend implementation or a CUPS IPP
// implementation for use on ChromeOS.
class PrintBackendChromeOS : public PrintBackend {
 public:
  explicit PrintBackendChromeOS(const std::string& locale);

  // PrintBackend implementation.
  bool EnumeratePrinters(PrinterList* printer_list) override;
  std::string GetDefaultPrinterName() override;
  bool GetPrinterBasicInfo(const std::string& printer_name,
                           PrinterBasicInfo* printer_info) override;
  bool GetPrinterCapsAndDefaults(const std::string& printer_name,
                                 PrinterCapsAndDefaults* printer_info) override;
  bool GetPrinterSemanticCapsAndDefaults(
      const std::string& printer_name,
      PrinterSemanticCapsAndDefaults* printer_info) override;
  std::string GetPrinterDriverInfo(const std::string& printer_name) override;
  bool IsValidPrinter(const std::string& printer_name) override;

 protected:
  ~PrintBackendChromeOS() override = default;
};

PrintBackendChromeOS::PrintBackendChromeOS(const std::string& locale)
    : PrintBackend(locale) {}

bool PrintBackendChromeOS::EnumeratePrinters(PrinterList* printer_list) {
  return true;
}

bool PrintBackendChromeOS::GetPrinterBasicInfo(const std::string& printer_name,
                                               PrinterBasicInfo* printer_info) {
  return false;
}

bool PrintBackendChromeOS::GetPrinterCapsAndDefaults(
    const std::string& printer_name,
    PrinterCapsAndDefaults* printer_info) {
  NOTREACHED();
  return false;
}

bool PrintBackendChromeOS::GetPrinterSemanticCapsAndDefaults(
    const std::string& printer_name,
    PrinterSemanticCapsAndDefaults* printer_info) {
  NOTREACHED();
  return false;
}

std::string PrintBackendChromeOS::GetPrinterDriverInfo(
    const std::string& printer_name) {
  NOTREACHED();
  return std::string();
}

std::string PrintBackendChromeOS::GetDefaultPrinterName() {
  return std::string();
}

bool PrintBackendChromeOS::IsValidPrinter(const std::string& printer_name) {
  NOTREACHED();
  return true;
}

// static
scoped_refptr<PrintBackend> PrintBackend::CreateInstanceImpl(
    const base::DictionaryValue* print_backend_settings,
    const std::string& locale,
    bool /*for_cloud_print*/) {
#if defined(USE_CUPS)
  return base::MakeRefCounted<PrintBackendCupsIpp>(
      CreateConnection(print_backend_settings), locale);
#else
  return base::MakeRefCounted<PrintBackendChromeOS>(locale);
#endif  // defined(USE_CUPS)
}

}  // namespace printing

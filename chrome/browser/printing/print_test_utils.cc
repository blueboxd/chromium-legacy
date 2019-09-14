// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_test_utils.h"

#include <string>
#include <utility>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"
#include "printing/print_job_constants.h"

namespace printing {

const char kDummyPrinterName[] = "DefaultPrinter";

base::Value GetPrintTicket(PrinterType type, bool cloud) {
  bool is_privet_printer = !cloud && type == kPrivetPrinter;
  bool is_extension_printer = !cloud && type == kExtensionPrinter;

  base::Value ticket(base::Value::Type::DICTIONARY);

  // Letter
  base::Value media_size(base::Value::Type::DICTIONARY);
  media_size.SetBoolKey(kSettingMediaSizeIsDefault, true);
  media_size.SetIntKey(kSettingMediaSizeWidthMicrons, 215900);
  media_size.SetIntKey(kSettingMediaSizeHeightMicrons, 279400);
  ticket.SetKey(kSettingMediaSize, std::move(media_size));

  ticket.SetIntKey(kSettingPreviewPageCount, 1);
  ticket.SetBoolKey(kSettingLandscape, false);
  ticket.SetIntKey(kSettingColor, 2);  // color printing
  ticket.SetBoolKey(kSettingHeaderFooterEnabled, false);
  ticket.SetIntKey(kSettingMarginsType, 0);  // default margins
  ticket.SetIntKey(kSettingDuplexMode, LONG_EDGE);
  ticket.SetIntKey(kSettingCopies, 1);
  ticket.SetBoolKey(kSettingCollate, true);
  ticket.SetBoolKey(kSettingShouldPrintBackgrounds, false);
  ticket.SetBoolKey(kSettingShouldPrintSelectionOnly, false);
  ticket.SetBoolKey(kSettingPreviewModifiable, true);
  ticket.SetBoolKey(kSettingPreviewIsPDF, false);
  ticket.SetBoolKey(kSettingPrintToPDF, !cloud && type == kPdfPrinter);
  ticket.SetBoolKey(kSettingCloudPrintDialog, cloud);
  ticket.SetBoolKey(kSettingPrintWithPrivet, is_privet_printer);
  ticket.SetBoolKey(kSettingPrintWithExtension, is_extension_printer);
  ticket.SetBoolKey(kSettingRasterizePdf, false);
  ticket.SetIntKey(kSettingScaleFactor, 100);
  ticket.SetIntKey(kSettingPagesPerSheet, 1);
  ticket.SetIntKey(kSettingDpiHorizontal, kTestPrinterDpi);
  ticket.SetIntKey(kSettingDpiVertical, kTestPrinterDpi);
  ticket.SetStringKey(kSettingDeviceName, kDummyPrinterName);
  ticket.SetBoolKey(kSettingFitToPageEnabled, true);
  ticket.SetIntKey(kSettingPageWidth, 215900);
  ticket.SetIntKey(kSettingPageHeight, 279400);
  ticket.SetBoolKey(kSettingShowSystemDialog, false);

  if (cloud)
    ticket.SetStringKey(kSettingCloudPrintId, kDummyPrinterName);

  if (is_privet_printer || is_extension_printer) {
    base::Value capabilities(base::Value::Type::DICTIONARY);
    capabilities.SetBoolKey("duplex", true);  // non-empty
    std::string caps_string;
    base::JSONWriter::Write(capabilities, &caps_string);
    ticket.SetStringKey(kSettingCapabilities, caps_string);
    base::Value print_ticket(base::Value::Type::DICTIONARY);
    print_ticket.SetStringKey("version", "1.0");
    print_ticket.SetKey("print", base::Value());
    std::string ticket_string;
    base::JSONWriter::Write(print_ticket, &ticket_string);
    ticket.SetStringKey(kSettingTicket, ticket_string);
  }

  return ticket;
}

}  // namespace printing

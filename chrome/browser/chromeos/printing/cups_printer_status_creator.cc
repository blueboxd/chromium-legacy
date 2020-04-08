// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/cups_printer_status_creator.h"

namespace chromeos {

using CupsReason = CupsPrinterStatus::CupsPrinterStatusReason::Reason;
using CupsSeverity = CupsPrinterStatus::CupsPrinterStatusReason::Severity;
using ReasonFromPrinter = printing::PrinterStatus::PrinterReason::Reason;
using SeverityFromPrinter = printing::PrinterStatus::PrinterReason::Severity;

CupsPrinterStatus PrinterStatusToCupsPrinterStatus(
    const std::string& printer_id,
    const printing::PrinterStatus& printer_status) {
  CupsPrinterStatus cups_printer_status(printer_id);

  for (const auto& reason : printer_status.reasons) {
    cups_printer_status.AddStatusReason(
        PrinterReasonToCupsReason(reason.reason),
        PrinterSeverityToCupsSeverity(reason.severity));
  }

  return cups_printer_status;
}

CupsReason PrinterReasonToCupsReason(const ReasonFromPrinter& reason) {
  switch (reason) {
    case ReasonFromPrinter::CONNECTING_TO_DEVICE:
      return CupsReason::kConnectingToDevice;
    case ReasonFromPrinter::FUSER_OVER_TEMP:
    case ReasonFromPrinter::FUSER_UNDER_TEMP:
    case ReasonFromPrinter::INTERPRETER_RESOURCE_UNAVAILABLE:
    case ReasonFromPrinter::OPC_LIFE_OVER:
    case ReasonFromPrinter::OPC_NEAR_EOL:
      return CupsReason::kDeviceError;
    case ReasonFromPrinter::COVER_OPEN:
    case ReasonFromPrinter::DOOR_OPEN:
    case ReasonFromPrinter::INTERLOCK_OPEN:
      return CupsReason::kDoorOpen;
    case ReasonFromPrinter::DEVELOPER_LOW:
    case ReasonFromPrinter::MARKER_SUPPLY_LOW:
    case ReasonFromPrinter::MARKER_WASTE_ALMOST_FULL:
    case ReasonFromPrinter::TONER_LOW:
      return CupsReason::kLowOnInk;
    case ReasonFromPrinter::MEDIA_LOW:
      return CupsReason::kLowOnPaper;
    case ReasonFromPrinter::NONE:
      return CupsReason::kNoError;
    case ReasonFromPrinter::DEVELOPER_EMPTY:
    case ReasonFromPrinter::MARKER_SUPPLY_EMPTY:
    case ReasonFromPrinter::MARKER_WASTE_FULL:
    case ReasonFromPrinter::TONER_EMPTY:
      return CupsReason::kOutOfInk;
    case ReasonFromPrinter::MEDIA_EMPTY:
    case ReasonFromPrinter::MEDIA_NEEDED:
      return CupsReason::kOutOfPaper;
    case ReasonFromPrinter::OUTPUT_AREA_ALMOST_FULL:
      return CupsReason::kOutputAreaAlmostFull;
    case ReasonFromPrinter::OUTPUT_AREA_FULL:
      return CupsReason::kOutputFull;
    case ReasonFromPrinter::MEDIA_JAM:
      return CupsReason::kPaperJam;
    case ReasonFromPrinter::MOVING_TO_PAUSED:
    case ReasonFromPrinter::PAUSED:
      return CupsReason::kPaused;
    case ReasonFromPrinter::SPOOL_AREA_FULL:
      return CupsReason::kPrinterQueueFull;
    case ReasonFromPrinter::SHUTDOWN:
    case ReasonFromPrinter::TIMED_OUT:
      return CupsReason::kPrinterUnreachable;
    case ReasonFromPrinter::STOPPED_PARTLY:
    case ReasonFromPrinter::STOPPING:
      return CupsReason::kStopped;
    case ReasonFromPrinter::INPUT_TRAY_MISSING:
    case ReasonFromPrinter::OUTPUT_TRAY_MISSING:
      return CupsReason::kTrayMissing;
    case ReasonFromPrinter::UNKNOWN_REASON:
      return CupsReason::kUnknownReason;
  }
}

CupsSeverity PrinterSeverityToCupsSeverity(
    const SeverityFromPrinter& severity) {
  switch (severity) {
    case SeverityFromPrinter::UNKNOWN_SEVERITY:
      return CupsSeverity::kUnknownSeverity;
    case SeverityFromPrinter::REPORT:
      return CupsSeverity::kReport;
    case SeverityFromPrinter::WARNING:
      return CupsSeverity::kWarning;
    case SeverityFromPrinter::ERROR:
      return CupsSeverity::kError;
  }
}

}  // namespace chromeos

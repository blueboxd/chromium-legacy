// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/printer_error_codes.h"

#include "printing/backend/cups_jobs.h"
#include "printing/printer_status.h"

namespace chromeos {

using PrinterReason = ::printing::PrinterStatus::PrinterReason;

PrinterErrorCode PrinterErrorCodeFromPrinterStatusReasons(
    const ::printing::PrinterStatus& printer_status) {
  for (const auto& reason : printer_status.reasons) {
    switch (reason.reason) {
      case PrinterReason::MEDIA_EMPTY:
      case PrinterReason::MEDIA_NEEDED:
      case PrinterReason::MEDIA_LOW:
        return PrinterErrorCode::OUT_OF_PAPER;
      case PrinterReason::MEDIA_JAM:
        return PrinterErrorCode::PAPER_JAM;
      case PrinterReason::TONER_EMPTY:
      case PrinterReason::TONER_LOW:
      case PrinterReason::DEVELOPER_EMPTY:
      case PrinterReason::DEVELOPER_LOW:
      case PrinterReason::MARKER_SUPPLY_EMPTY:
      case PrinterReason::MARKER_SUPPLY_LOW:
      case PrinterReason::MARKER_WASTE_FULL:
      case PrinterReason::MARKER_WASTE_ALMOST_FULL:
        return PrinterErrorCode::OUT_OF_INK;
      case PrinterReason::TIMED_OUT:
      case PrinterReason::SHUTDOWN:
        return PrinterErrorCode::PRINTER_UNREACHABLE;
      case PrinterReason::DOOR_OPEN:
      case PrinterReason::COVER_OPEN:
      case PrinterReason::INTERLOCK_OPEN:
        return PrinterErrorCode::DOOR_OPEN;
      case PrinterReason::INPUT_TRAY_MISSING:
      case PrinterReason::OUTPUT_TRAY_MISSING:
        return PrinterErrorCode::TRAY_MISSING;
      case PrinterReason::OUTPUT_AREA_FULL:
      case PrinterReason::OUTPUT_AREA_ALMOST_FULL:
        return PrinterErrorCode::OUTPUT_FULL;
      case PrinterReason::STOPPING:
      case PrinterReason::STOPPED_PARTLY:
      case PrinterReason::PAUSED:
      case PrinterReason::MOVING_TO_PAUSED:
        return PrinterErrorCode::STOPPED;
      default:
        break;
    }
  }
  return PrinterErrorCode::NO_ERROR;
}

}  // namespace chromeos

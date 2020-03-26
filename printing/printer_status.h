// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTER_STATUS_H_
#define PRINTING_PRINTER_STATUS_H_

#include <cups/cups.h>

#include <string>
#include <vector>

#include "printing/printing_export.h"

namespace printing {

// Represents the status of a printer containing the properties printer-state,
// printer-state-reasons, and printer-state-message.
struct PRINTING_EXPORT PrinterStatus {
  struct PrinterReason {
    // Standardized reasons from RFC2911.
    enum Reason {
      UNKNOWN_REASON,
      NONE,
      MEDIA_NEEDED,
      MEDIA_JAM,
      MOVING_TO_PAUSED,
      PAUSED,
      SHUTDOWN,
      CONNECTING_TO_DEVICE,
      TIMED_OUT,
      STOPPING,
      STOPPED_PARTLY,
      TONER_LOW,
      TONER_EMPTY,
      SPOOL_AREA_FULL,
      COVER_OPEN,
      INTERLOCK_OPEN,
      DOOR_OPEN,
      INPUT_TRAY_MISSING,
      MEDIA_LOW,
      MEDIA_EMPTY,
      OUTPUT_TRAY_MISSING,
      OUTPUT_AREA_ALMOST_FULL,
      OUTPUT_AREA_FULL,
      MARKER_SUPPLY_LOW,
      MARKER_SUPPLY_EMPTY,
      MARKER_WASTE_ALMOST_FULL,
      MARKER_WASTE_FULL,
      FUSER_OVER_TEMP,
      FUSER_UNDER_TEMP,
      OPC_NEAR_EOL,
      OPC_LIFE_OVER,
      DEVELOPER_LOW,
      DEVELOPER_EMPTY,
      INTERPRETER_RESOURCE_UNAVAILABLE
    };

    // Severity of the state-reason.
    enum Severity { UNKNOWN_SEVERITY, REPORT, WARNING, ERROR };

    Reason reason;
    Severity severity;
  };

  PrinterStatus();
  PrinterStatus(const PrinterStatus& other);
  ~PrinterStatus();

  // printer-state
  ipp_pstate_t state;
  // printer-state-reasons
  std::vector<PrinterReason> reasons;
  // printer-state-message
  std::string message;
};

}  // namespace printing

#endif  // PRINTING_PRINTER_STATUS_H_

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
    // This enum is used to record UMA histogram values and should not be
    // reordered. Please keep in sync with PrinterStatusReasons in
    // src/tools/metrics/histograms/enums.xml.
    enum class Reason {
      UNKNOWN_REASON = 0,
      NONE = 1,
      MEDIA_NEEDED = 2,
      MEDIA_JAM = 3,
      MOVING_TO_PAUSED = 4,
      PAUSED = 5,
      SHUTDOWN = 6,
      CONNECTING_TO_DEVICE = 7,
      TIMED_OUT = 8,
      STOPPING = 9,
      STOPPED_PARTLY = 10,
      TONER_LOW = 11,
      TONER_EMPTY = 12,
      SPOOL_AREA_FULL = 13,
      COVER_OPEN = 14,
      INTERLOCK_OPEN = 15,
      DOOR_OPEN = 16,
      INPUT_TRAY_MISSING = 17,
      MEDIA_LOW = 18,
      MEDIA_EMPTY = 19,
      OUTPUT_TRAY_MISSING = 20,
      OUTPUT_AREA_ALMOST_FULL = 21,
      OUTPUT_AREA_FULL = 22,
      MARKER_SUPPLY_LOW = 23,
      MARKER_SUPPLY_EMPTY = 24,
      MARKER_WASTE_ALMOST_FULL = 25,
      MARKER_WASTE_FULL = 26,
      FUSER_OVER_TEMP = 27,
      FUSER_UNDER_TEMP = 28,
      OPC_NEAR_EOL = 29,
      OPC_LIFE_OVER = 30,
      DEVELOPER_LOW = 31,
      DEVELOPER_EMPTY = 32,
      INTERPRETER_RESOURCE_UNAVAILABLE = 33,
      kMaxValue = INTERPRETER_RESOURCE_UNAVAILABLE
    };

    // Severity of the state-reason.
    enum class Severity {
      UNKNOWN_SEVERITY = 0,
      REPORT = 1,
      WARNING = 2,
      ERROR = 3,
    };

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

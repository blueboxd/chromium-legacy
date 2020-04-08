// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/cups_printer_status_creator.h"

#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

using CupsReason = CupsPrinterStatus::CupsPrinterStatusReason::Reason;
using CupsSeverity = CupsPrinterStatus::CupsPrinterStatusReason::Severity;
using ReasonFromPrinter = printing::PrinterStatus::PrinterReason::Reason;
using SeverityFromPrinter = printing::PrinterStatus::PrinterReason::Severity;

TEST(CupsPrinterStatusCreatorTest, PrinterStatusToCupsPrinterStatus) {
  printing::PrinterStatus::PrinterReason reason1;
  reason1.reason = ReasonFromPrinter::NONE;
  reason1.severity = SeverityFromPrinter::REPORT;

  printing::PrinterStatus::PrinterReason reason2;
  reason2.reason = ReasonFromPrinter::COVER_OPEN;
  reason2.severity = SeverityFromPrinter::WARNING;

  printing::PrinterStatus printer_status;
  printer_status.reasons.push_back(reason1);
  printer_status.reasons.push_back(reason2);

  std::string printer_id = "id";
  CupsPrinterStatus cups_printer_status =
      PrinterStatusToCupsPrinterStatus(printer_id, printer_status);

  EXPECT_EQ("id", cups_printer_status.GetPrinterId());
  EXPECT_EQ(2u, cups_printer_status.GetStatusReasons().size());

  std::vector<CupsPrinterStatus::CupsPrinterStatusReason> expected_reasons{
      CupsPrinterStatus::CupsPrinterStatusReason(CupsReason::kNoError,
                                                 CupsSeverity::kReport),
      CupsPrinterStatus::CupsPrinterStatusReason(CupsReason::kDoorOpen,
                                                 CupsSeverity::kWarning)};
  EXPECT_THAT(cups_printer_status.GetStatusReasons(), expected_reasons);
}

TEST(CupsPrinterStatusCreatorTest, PrinterSeverityToCupsSeverity) {
  EXPECT_EQ(
      CupsSeverity::kUnknownSeverity,
      PrinterSeverityToCupsSeverity(SeverityFromPrinter::UNKNOWN_SEVERITY));
  EXPECT_EQ(CupsSeverity::kReport,
            PrinterSeverityToCupsSeverity(SeverityFromPrinter::REPORT));
  EXPECT_EQ(CupsSeverity::kWarning,
            PrinterSeverityToCupsSeverity(SeverityFromPrinter::WARNING));
  EXPECT_EQ(CupsSeverity::kError,
            PrinterSeverityToCupsSeverity(SeverityFromPrinter::ERROR));
}

TEST(CupsPrinterStatusCreatorTest, PrinterReasonToCupsReason) {
  EXPECT_EQ(CupsReason::kConnectingToDevice,
            PrinterReasonToCupsReason(ReasonFromPrinter::CONNECTING_TO_DEVICE));

  EXPECT_EQ(CupsReason::kDeviceError,
            PrinterReasonToCupsReason(ReasonFromPrinter::FUSER_OVER_TEMP));
  EXPECT_EQ(CupsReason::kDeviceError,
            PrinterReasonToCupsReason(ReasonFromPrinter::FUSER_UNDER_TEMP));
  EXPECT_EQ(CupsReason::kDeviceError,
            PrinterReasonToCupsReason(
                ReasonFromPrinter::INTERPRETER_RESOURCE_UNAVAILABLE));
  EXPECT_EQ(CupsReason::kDeviceError,
            PrinterReasonToCupsReason(ReasonFromPrinter::OPC_LIFE_OVER));
  EXPECT_EQ(CupsReason::kDeviceError,
            PrinterReasonToCupsReason(ReasonFromPrinter::OPC_NEAR_EOL));

  EXPECT_EQ(CupsReason::kDoorOpen,
            PrinterReasonToCupsReason(ReasonFromPrinter::COVER_OPEN));
  EXPECT_EQ(CupsReason::kDoorOpen,
            PrinterReasonToCupsReason(ReasonFromPrinter::DOOR_OPEN));
  EXPECT_EQ(CupsReason::kDoorOpen,
            PrinterReasonToCupsReason(ReasonFromPrinter::INTERLOCK_OPEN));

  EXPECT_EQ(CupsReason::kLowOnInk,
            PrinterReasonToCupsReason(ReasonFromPrinter::DEVELOPER_LOW));
  EXPECT_EQ(CupsReason::kLowOnInk,
            PrinterReasonToCupsReason(ReasonFromPrinter::MARKER_SUPPLY_LOW));
  EXPECT_EQ(
      CupsReason::kLowOnInk,
      PrinterReasonToCupsReason(ReasonFromPrinter::MARKER_WASTE_ALMOST_FULL));
  EXPECT_EQ(CupsReason::kLowOnInk,
            PrinterReasonToCupsReason(ReasonFromPrinter::TONER_LOW));

  EXPECT_EQ(CupsReason::kLowOnPaper,
            PrinterReasonToCupsReason(ReasonFromPrinter::MEDIA_LOW));

  EXPECT_EQ(CupsReason::kNoError,
            PrinterReasonToCupsReason(ReasonFromPrinter::NONE));

  EXPECT_EQ(CupsReason::kOutOfInk,
            PrinterReasonToCupsReason(ReasonFromPrinter::DEVELOPER_EMPTY));
  EXPECT_EQ(CupsReason::kOutOfInk,
            PrinterReasonToCupsReason(ReasonFromPrinter::MARKER_SUPPLY_EMPTY));
  EXPECT_EQ(CupsReason::kOutOfInk,
            PrinterReasonToCupsReason(ReasonFromPrinter::MARKER_WASTE_FULL));
  EXPECT_EQ(CupsReason::kOutOfInk,
            PrinterReasonToCupsReason(ReasonFromPrinter::TONER_EMPTY));

  EXPECT_EQ(CupsReason::kOutOfPaper,
            PrinterReasonToCupsReason(ReasonFromPrinter::MEDIA_EMPTY));
  EXPECT_EQ(CupsReason::kOutOfPaper,
            PrinterReasonToCupsReason(ReasonFromPrinter::MEDIA_NEEDED));

  EXPECT_EQ(
      CupsReason::kOutputAreaAlmostFull,
      PrinterReasonToCupsReason(ReasonFromPrinter::OUTPUT_AREA_ALMOST_FULL));

  EXPECT_EQ(CupsReason::kOutputFull,
            PrinterReasonToCupsReason(ReasonFromPrinter::OUTPUT_AREA_FULL));

  EXPECT_EQ(CupsReason::kPaperJam,
            PrinterReasonToCupsReason(ReasonFromPrinter::MEDIA_JAM));

  EXPECT_EQ(CupsReason::kPaused,
            PrinterReasonToCupsReason(ReasonFromPrinter::MOVING_TO_PAUSED));
  EXPECT_EQ(CupsReason::kPaused,
            PrinterReasonToCupsReason(ReasonFromPrinter::PAUSED));

  EXPECT_EQ(CupsReason::kPrinterQueueFull,
            PrinterReasonToCupsReason(ReasonFromPrinter::SPOOL_AREA_FULL));

  EXPECT_EQ(CupsReason::kPrinterUnreachable,
            PrinterReasonToCupsReason(ReasonFromPrinter::SHUTDOWN));
  EXPECT_EQ(CupsReason::kPrinterUnreachable,
            PrinterReasonToCupsReason(ReasonFromPrinter::TIMED_OUT));

  EXPECT_EQ(CupsReason::kStopped,
            PrinterReasonToCupsReason(ReasonFromPrinter::STOPPED_PARTLY));
  EXPECT_EQ(CupsReason::kStopped,
            PrinterReasonToCupsReason(ReasonFromPrinter::STOPPING));

  EXPECT_EQ(CupsReason::kTrayMissing,
            PrinterReasonToCupsReason(ReasonFromPrinter::INPUT_TRAY_MISSING));
  EXPECT_EQ(CupsReason::kTrayMissing,
            PrinterReasonToCupsReason(ReasonFromPrinter::OUTPUT_TRAY_MISSING));

  EXPECT_EQ(CupsReason::kUnknownReason,
            PrinterReasonToCupsReason(ReasonFromPrinter::UNKNOWN_REASON));
}

}  // namespace chromeos

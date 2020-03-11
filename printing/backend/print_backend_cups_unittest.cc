// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend_cups.h"

#include <cups/cups.h>

#include "build/build_config.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

TEST(PrintBackendCupsTest, PrinterBasicInfoFromCUPS) {
  constexpr char kName[] = "printer";
  cups_dest_t* printer = nullptr;
  ASSERT_EQ(
      1, cupsAddDest(kName, /*instance=*/nullptr, /*num_dests=*/0, &printer));

  int num_options = 0;
  cups_option_t* options = nullptr;
#if defined(OS_MACOSX)
  num_options =
      cupsAddOption(kCUPSOptPrinterInfo, "info", num_options, &options);
  num_options = cupsAddOption(kCUPSOptPrinterMakeAndModel, "description",
                              num_options, &options);
#else
  num_options =
      cupsAddOption(kCUPSOptPrinterInfo, "description", num_options, &options);
#endif
  printer->num_options = num_options;
  printer->options = options;

  PrinterBasicInfo printer_info;
  EXPECT_TRUE(
      PrintBackendCUPS::PrinterBasicInfoFromCUPS(*printer, &printer_info));
  cupsFreeDests(/*num_dests=*/1, printer);

  EXPECT_EQ(kName, printer_info.printer_name);
#if defined(OS_MACOSX)
  EXPECT_EQ("info", printer_info.display_name);
#else
  EXPECT_EQ(kName, printer_info.display_name);
#endif
  EXPECT_EQ("description", printer_info.printer_description);
}

}  // namespace printing

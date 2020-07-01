// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_MANAGEMENT_PRINT_MANAGEMENT_UMA_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_MANAGEMENT_PRINT_MANAGEMENT_UMA_H_

// These are used in histograms, do not remove/renumber entries. If you're
// adding to this enum with the intention that it will be logged, update the
// PrintManagementAppEntryPoint enum listing in
// tools/metrics/histograms/enums.xml.
enum class PrintManagementAppEntryPoint {
  kSettings = 0,
  kNotification = 1,
  kLauncher = 2,
  kBrowser = 3,
  kMaxValue = kBrowser,
};

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_MANAGEMENT_PRINT_MANAGEMENT_UMA_H_

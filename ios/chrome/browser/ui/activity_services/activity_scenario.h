// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_SCENARIO_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_SCENARIO_H_

// Enum that contains the list of sharing scenarios. Current values should not
// be renumbered. Please keep in sync with "IOSActivityScenario" in
// src/tools/metrics/histograms/enums.xml.
enum class ActivityScenario {
  TabShareButton = 0,
  QRCodeImage = 1,
  // Highest enumerator. Recommended by Histogram metrics best practices.
  kMaxValue = QRCodeImage
};

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_SCENARIO_H_

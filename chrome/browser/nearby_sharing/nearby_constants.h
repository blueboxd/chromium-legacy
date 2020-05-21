// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_CONSTANTS_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_CONSTANTS_H_

// Represents the advertising bluetooth power for Nearby Connections.
enum class PowerLevel {
  kLowPower,
  kMediumPower,
  kHighPower,
  kMaxValue = kHighPower
};

// Represents the data usage preference.
enum class DataUsage {
  // User is never willing to use the Internet
  kOffline,
  // User is always willing to use the Internet
  kOnline,
  // User is willing to use the Internet on an unmetered connection.
  kWifiOnly,
  kMaxValue = kWifiOnly
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_CONSTANTS_H_

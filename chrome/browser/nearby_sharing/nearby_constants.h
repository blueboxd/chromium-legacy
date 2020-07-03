// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_CONSTANTS_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_CONSTANTS_H_

// Represents the advertising bluetooth power for Nearby Connections.
enum class PowerLevel {
  kUnknown = 0,
  kLowPower = 1,
  kMediumPower = 2,
  kHighPower = 3,
  kMaxValue = kHighPower
};

// Represents the data usage preference.
enum class DataUsage {
  kUnknown = 0,
  // User is never willing to use the Internet
  kOffline = 1,
  // User is always willing to use the Internet
  kOnline = 2,
  // User is willing to use the Internet on an unmetered connection.
  kWifiOnly = 3,
  kMaxValue = kWifiOnly
};

// Represents the visibility of the advertisement.
enum class Visibility {
  kUnknown = 0,
  // The user is not advertising to anyone.
  kNoOne = 1,
  // The user is visible to all contacts.
  kAllContacts = 2,
  // The user is only visible to selected contacts.
  kSelectedContacts = 3,
  kMaxValue = kSelectedContacts,
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_CONSTANTS_H_

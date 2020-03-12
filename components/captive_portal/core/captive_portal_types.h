// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAPTIVE_PORTAL_CORE_CAPTIVE_PORTAL_TYPES_H_
#define COMPONENTS_CAPTIVE_PORTAL_CORE_CAPTIVE_PORTAL_TYPES_H_

#include <string>

#include "components/captive_portal/core/captive_portal_export.h"

namespace captive_portal {

// Possible results of an attempt to detect a captive portal.
enum CaptivePortalResult {
  // There's a confirmed connection to the Internet.
  RESULT_INTERNET_CONNECTED,
  // The URL request received a network or HTTP error, or a non-HTTP response.
  RESULT_NO_RESPONSE,
  // The URL request apparently encountered a captive portal.  It received a
  // a valid HTTP response with a 2xx other than 204, 3xx, or 511 status code.
  RESULT_BEHIND_CAPTIVE_PORTAL,
  RESULT_COUNT
};

// Possible causes of a captive portal probe. These values are persisted to
// logs. Entries should not be renumbered and numeric values should never be
// reused. Please keep in sync with "CaptivePortalProbeReason" in
// src/tools/metrics/histograms/enums.xml.
enum class CaptivePortalProbeReason {
  kUnspecified = 0,
  // Timeout
  kTimeout = 1,
  // Certificate error
  kCertificateError = 2,
  // SSL protocol error
  kSslProtocolError = 3,
  // First load of login tab
  kLoginTabLoad = 4,
  // Secure DNS error
  kSecureDnsError = 5,
  kMaxValue = kSecureDnsError,
};

CAPTIVE_PORTAL_EXPORT extern std::string CaptivePortalResultToString(
    CaptivePortalResult result);

}  // namespace captive_portal

#endif  // COMPONENTS_CAPTIVE_PORTAL_CORE_CAPTIVE_PORTAL_TYPES_H_

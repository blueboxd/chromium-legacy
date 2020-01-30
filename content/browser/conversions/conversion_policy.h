// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_POLICY_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_POLICY_H_

#include <vector>

#include "base/time/time.h"
#include "content/browser/conversions/conversion_report.h"
#include "content/common/content_export.h"

namespace content {

// Class for controlling certain constraints and configurations for handling,
// storing, and sending impressions and conversions.
class CONTENT_EXPORT ConversionPolicy {
 public:
  ConversionPolicy() = default;
  ConversionPolicy(const ConversionPolicy& other) = delete;
  ConversionPolicy& operator=(const ConversionPolicy& other) = delete;
  virtual ~ConversionPolicy() = default;

  // Get the time a conversion report should be sent, by batching reports into
  // set reporting windows based on their impression time. This strictly delays
  // the time a report will be sent.
  virtual base::Time GetReportTimeForConversion(
      const ConversionReport& report) const;

  // Maximum number of times the an impression is allowed to convert.
  virtual int GetMaxConversionsPerImpression() const;

  // Given a set of conversion reports for a single conversion registrations,
  // assigns attribution credits to each one which will be sent at report time.
  // By default, this performs "last click" attribution which assigns the report
  // for the most recent impression a credit of 100, and the rest a credit of 0.
  virtual void AssignAttributionCredits(
      std::vector<ConversionReport>* reports) const;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_POLICY_H_

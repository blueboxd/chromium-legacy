// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_POLICY_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_POLICY_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "content/browser/conversions/conversion_report.h"
#include "content/common/content_export.h"

namespace content {

// Class for controlling certain constraints and configurations for handling,
// storing, and sending impressions and conversions.
class CONTENT_EXPORT ConversionPolicy {
 public:
  // Helper class that generates noised conversion data. Can be overridden to
  // make testing deterministic.
  class CONTENT_EXPORT NoiseProvider {
   public:
    NoiseProvider() = default;
    virtual ~NoiseProvider() = default;

    // Returns a noise value of |conversion_data|. By default, this reports
    // completely random data for 5% of conversions, and sends the real data for
    // 95%. Virtual for testing.
    virtual uint64_t GetNoisedConversionData(uint64_t conversion_data) const;
  };

  static std::unique_ptr<ConversionPolicy> CreateForTesting(
      std::unique_ptr<NoiseProvider> noise_provider);

  ConversionPolicy();
  ConversionPolicy(const ConversionPolicy& other) = delete;
  ConversionPolicy& operator=(const ConversionPolicy& other) = delete;
  virtual ~ConversionPolicy();

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

  // Gets the sanitized conversion data for a conversion. This strips entropy
  // from the provided to data to at most 3 bits of information.
  virtual std::string GetSanitizedConversionData(
      uint64_t conversion_data) const;

 private:
  // For testing only.
  ConversionPolicy(std::unique_ptr<NoiseProvider> noise_provider);

  std::unique_ptr<NoiseProvider> noise_provider_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_POLICY_H_

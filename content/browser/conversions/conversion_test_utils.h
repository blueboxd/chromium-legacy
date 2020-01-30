// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_TEST_UTILS_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_TEST_UTILS_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "content/browser/conversions/conversion_report.h"
#include "content/browser/conversions/conversion_storage.h"
#include "content/browser/conversions/storable_conversion.h"
#include "content/browser/conversions/storable_impression.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/origin.h"

namespace content {

class EmptyStorageDelegate : public ConversionStorage::Delegate {
 public:
  EmptyStorageDelegate() = default;
  ~EmptyStorageDelegate() override = default;

  // ConversionStorage::Delegate
  void ProcessNewConversionReports(
      std::vector<ConversionReport>* reports) override {}

  int GetMaxConversionsPerImpression() const override;
};

// Helper class to construct a StorableImpression for tests using default data.
// StorableImpression members are not mutable after construction requiring a
// builder pattern.
class ImpressionBuilder {
 public:
  ImpressionBuilder(base::Time time);
  ~ImpressionBuilder();

  ImpressionBuilder& SetExpiry(base::TimeDelta delta);

  ImpressionBuilder& SetData(const std::string& data);

  ImpressionBuilder& SetImpressionOrigin(const url::Origin& origin);

  ImpressionBuilder& SetConversionOrigin(const url::Origin& origin);

  ImpressionBuilder& SetReportingOrigin(const url::Origin& origin);

  StorableImpression Build() const;

 private:
  std::string impression_data_;
  base::Time impression_time_;
  base::TimeDelta expiry_;
  url::Origin impression_origin_;
  url::Origin conversion_origin_;
  url::Origin reporting_origin_;
};

// Returns a StorableConversion with default data which matches the default
// impressions created by ImpressionBuilder.
StorableConversion DefaultConversion();

testing::AssertionResult ReportsEqual(
    const std::vector<ConversionReport>& expected,
    const std::vector<ConversionReport>& actual);

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_TEST_UTILS_H_

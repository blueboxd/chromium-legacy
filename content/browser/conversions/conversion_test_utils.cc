// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_test_utils.h"

#include <tuple>

#include "url/gurl.h"

namespace content {

namespace {

const char kDefaultImpressionOrigin[] = "https:/impression.test/";
const char kDefaultConversionOrigin[] = "https:/conversion.test/";
const char kDefaultReportOrigin[] = "https:/report.test/";

// Default expiry time for impressions for testing.
const int64_t kExpiryTime = 30;

}  // namespace

int EmptyStorageDelegate::GetMaxConversionsPerImpression() const {
  return 1;
}

// Builds an impression with default values. This is done as a builder because
// all values needed to be provided at construction time.
ImpressionBuilder::ImpressionBuilder(base::Time time)
    : impression_data_("123"),
      impression_time_(time),
      expiry_(base::TimeDelta::FromMilliseconds(kExpiryTime)),
      impression_origin_(url::Origin::Create(GURL(kDefaultImpressionOrigin))),
      conversion_origin_(url::Origin::Create(GURL(kDefaultConversionOrigin))),
      reporting_origin_(url::Origin::Create(GURL(kDefaultReportOrigin))) {}

ImpressionBuilder::~ImpressionBuilder() = default;

ImpressionBuilder& ImpressionBuilder::SetExpiry(base::TimeDelta delta) {
  expiry_ = delta;
  return *this;
}

ImpressionBuilder& ImpressionBuilder::SetData(const std::string& data) {
  impression_data_ = data;
  return *this;
}

ImpressionBuilder& ImpressionBuilder::SetImpressionOrigin(
    const url::Origin& origin) {
  impression_origin_ = origin;
  return *this;
}

ImpressionBuilder& ImpressionBuilder::SetConversionOrigin(
    const url::Origin& origin) {
  conversion_origin_ = origin;
  return *this;
}

ImpressionBuilder& ImpressionBuilder::SetReportingOrigin(
    const url::Origin& origin) {
  reporting_origin_ = origin;
  return *this;
}

StorableImpression ImpressionBuilder::Build() const {
  return StorableImpression(impression_data_, impression_origin_,
                            conversion_origin_, reporting_origin_,
                            impression_time_,
                            impression_time_ + expiry_ /* expiry_time */,
                            base::nullopt /* impression_id */);
}

StorableConversion DefaultConversion() {
  StorableConversion conversion(
      "111" /* conversion_data */,
      url::Origin::Create(
          GURL(kDefaultConversionOrigin)) /* conversion_origin */,
      url::Origin::Create(GURL(kDefaultReportOrigin)) /* reporting_origin */);
  return conversion;
}

// Custom comparator for comparing two vectors of conversion reports. Does not
// compare impression and conversion id's as they are set by the underlying
// sqlite db and should not be tested.
testing::AssertionResult ReportsEqual(
    const std::vector<ConversionReport>& expected,
    const std::vector<ConversionReport>& actual) {
  const auto tie = [](const ConversionReport& conversion) {
    return std::make_tuple(conversion.impression.impression_data(),
                           conversion.impression.impression_origin(),
                           conversion.impression.conversion_origin(),
                           conversion.impression.reporting_origin(),
                           conversion.impression.impression_time(),
                           conversion.impression.expiry_time(),
                           conversion.conversion_data, conversion.report_time,
                           conversion.attribution_credit);
  };

  if (expected.size() != actual.size())
    return testing::AssertionFailure() << "Expected length " << expected.size()
                                       << ", actual: " << actual.size();

  for (size_t i = 0; i < expected.size(); i++) {
    if (tie(expected[i]) != tie(actual[i])) {
      return testing::AssertionFailure()
             << "Expected " << expected[i] << " at index " << i
             << ", actual: " << actual[i];
    }
  }

  return testing::AssertionSuccess();
}

}  // namespace content

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/request_throttler.h"

#include <memory>

#include "base/test/simple_test_clock.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/v2/config.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

const int kMaximumQueryRequestsPerDay = 20;
const int kMaximumUploadActionsRequestsPerDay = 10;

class FeedRequestThrottlerTest : public testing::Test {
 public:
  FeedRequestThrottlerTest() {
    feed::Config config;
    config.max_action_upload_requests_per_day =
        kMaximumUploadActionsRequestsPerDay;
    config.max_feed_query_requests_per_day = kMaximumQueryRequestsPerDay;
    SetFeedConfigForTesting(config);

    RegisterProfilePrefs(test_prefs_.registry());

    base::Time now;
    EXPECT_TRUE(base::Time::FromString("2018-06-11 12:01AM", &now));
    test_clock_.SetNow(now);
  }

 protected:
  TestingPrefServiceSimple test_prefs_;
  base::SimpleTestClock test_clock_;
  RequestThrottler throttler_{&test_prefs_, &test_clock_};
};

TEST_F(FeedRequestThrottlerTest, RequestQuotaAllAtOnce) {
  for (int i = 0; i < kMaximumQueryRequestsPerDay; ++i) {
    EXPECT_TRUE(throttler_.RequestQuota(NetworkRequestType::kFeedQuery));
  }
  EXPECT_FALSE(throttler_.RequestQuota(NetworkRequestType::kFeedQuery));
}

TEST_F(FeedRequestThrottlerTest, QuotaIsPerDay) {
  for (int i = 0; i < kMaximumUploadActionsRequestsPerDay; ++i) {
    EXPECT_TRUE(throttler_.RequestQuota(NetworkRequestType::kUploadActions));
  }
  // Because we started at 12:01AM, we need to advance 24 hours before making
  // another successful request.
  test_clock_.Advance(base::TimeDelta::FromHours(23));
  EXPECT_FALSE(throttler_.RequestQuota(NetworkRequestType::kUploadActions));
  test_clock_.Advance(base::TimeDelta::FromHours(1));
  EXPECT_TRUE(throttler_.RequestQuota(NetworkRequestType::kUploadActions));
}

}  // namespace
}  // namespace feed

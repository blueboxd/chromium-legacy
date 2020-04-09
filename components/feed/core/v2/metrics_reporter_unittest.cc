// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/metrics_reporter.h"

#include <map>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
using feed::internal::FeedEngagementType;
const base::TimeDelta kEpsilon = base::TimeDelta::FromMilliseconds(1);

class MetricsReporterTest : public testing::Test {
 protected:
  std::map<FeedEngagementType, int> ReportedEngagementType() {
    std::map<FeedEngagementType, int> result;
    for (const auto& bucket :
         histogram_.GetAllSamples("ContentSuggestions.Feed.EngagementType")) {
      result[static_cast<FeedEngagementType>(bucket.min)] += bucket.count;
    }
    return result;
  }

  base::SimpleTestTickClock clock_;
  MetricsReporter reporter_{&clock_};
  base::HistogramTester histogram_;
};

TEST_F(MetricsReporterTest, ScrollingSmall) {
  reporter_.StreamScrolled(100);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedScrolled, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
}

TEST_F(MetricsReporterTest, ScrollingCanTriggerEngaged) {
  reporter_.StreamScrolled(161);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedScrolled, 1},
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
}

TEST_F(MetricsReporterTest, OpeningContentIsInteracting) {
  reporter_.NavigationStarted();

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
}

TEST_F(MetricsReporterTest, RemovingContentIsInteracting) {
  reporter_.ContentRemoved();

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
}

TEST_F(MetricsReporterTest, NotInterestedInIsInteracting) {
  reporter_.NotInterestedIn();

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
}

TEST_F(MetricsReporterTest, ManageInterestsInIsInteracting) {
  reporter_.ManageInterests();

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
}

TEST_F(MetricsReporterTest, VisitsCanLastMoreThanFiveMinutes) {
  reporter_.StreamScrolled(1);
  clock_.Advance(base::TimeDelta::FromMinutes(5) - kEpsilon);
  reporter_.NavigationStarted();
  clock_.Advance(base::TimeDelta::FromMinutes(5) - kEpsilon);
  reporter_.StreamScrolled(1);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedScrolled, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
}

TEST_F(MetricsReporterTest, NewVisitAfterInactivity) {
  reporter_.NavigationStarted();
  reporter_.StreamScrolled(1);
  clock_.Advance(base::TimeDelta::FromMinutes(5) + kEpsilon);
  reporter_.NavigationStarted();
  reporter_.StreamScrolled(1);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 2},
      {FeedEngagementType::kFeedInteracted, 2},
      {FeedEngagementType::kFeedEngagedSimple, 2},
      {FeedEngagementType::kFeedScrolled, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
}

}  // namespace feed

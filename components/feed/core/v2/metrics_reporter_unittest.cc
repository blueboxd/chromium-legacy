// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/metrics_reporter.h"

#include <map>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
using feed::internal::FeedEngagementType;
using feed::internal::FeedUserActionType;
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
  base::UserActionTester user_actions_;
};

TEST_F(MetricsReporterTest, SliceViewedReportsSuggestionShown) {
  reporter_.ContentSliceViewed(5);
  histogram_.ExpectUniqueSample("NewTabPage.ContentSuggestions.Shown", 5, 1);
}

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
  reporter_.OpenAction(5);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
}

TEST_F(MetricsReporterTest, RemovingContentIsInteracting) {
  reporter_.RemoveAction();

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
}

TEST_F(MetricsReporterTest, NotInterestedInIsInteracting) {
  reporter_.NotInterestedInAction();

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
}

TEST_F(MetricsReporterTest, ManageInterestsInIsInteracting) {
  reporter_.ManageInterestsAction();

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
  reporter_.OpenAction(0);
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
  reporter_.OpenAction(0);
  reporter_.StreamScrolled(1);
  clock_.Advance(base::TimeDelta::FromMinutes(5) + kEpsilon);
  reporter_.OpenAction(0);
  reporter_.StreamScrolled(1);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 2},
      {FeedEngagementType::kFeedInteracted, 2},
      {FeedEngagementType::kFeedEngagedSimple, 2},
      {FeedEngagementType::kFeedScrolled, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
}

TEST_F(MetricsReporterTest, ReportsLoadStreamStatus) {
  reporter_.OnLoadStream(LoadStreamStatus::kDataInStoreIsStale,
                         LoadStreamStatus::kLoadedFromNetwork);

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.Initial",
      LoadStreamStatus::kLoadedFromNetwork, 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.InitialFromStore",
      LoadStreamStatus::kDataInStoreIsStale, 1);
}

TEST_F(MetricsReporterTest, ReportsLoadStreamStatusIgnoresNoStatusFromStore) {
  reporter_.OnLoadStream(LoadStreamStatus::kNoStatus,
                         LoadStreamStatus::kLoadedFromNetwork);

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.Initial",
      LoadStreamStatus::kLoadedFromNetwork, 1);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.LoadStreamStatus.InitialFromStore", 0);
}

TEST_F(MetricsReporterTest, ReportsLoadMoreStatus) {
  reporter_.OnLoadMore(LoadStreamStatus::kLoadedFromNetwork);

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.LoadMore",
      LoadStreamStatus::kLoadedFromNetwork, 1);
}

TEST_F(MetricsReporterTest, ReportsBackgroundRefreshStatus) {
  reporter_.OnBackgroundRefresh(LoadStreamStatus::kLoadedFromNetwork);

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.BackgroundRefresh",
      LoadStreamStatus::kLoadedFromNetwork, 1);
}

TEST_F(MetricsReporterTest, OpenAction) {
  reporter_.OpenAction(5);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.Open"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserAction",
                                FeedUserActionType::kTappedOnCard, 1);
  histogram_.ExpectUniqueSample("NewTabPage.ContentSuggestions.Opened", 5, 1);
}

TEST_F(MetricsReporterTest, OpenInNewTabAction) {
  reporter_.OpenInNewTabAction(5);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.OpenInNewTab"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserAction",
                                FeedUserActionType::kTappedOpenInNewTab, 1);
  histogram_.ExpectUniqueSample("NewTabPage.ContentSuggestions.Opened", 5, 1);
}

TEST_F(MetricsReporterTest, OpenInNewIncognitoTabAction) {
  reporter_.OpenInNewIncognitoTabAction();

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.OpenInNewIncognitoTab"));
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.UserAction",
      FeedUserActionType::kTappedOpenInNewIncognitoTab, 1);
  histogram_.ExpectTotalCount("NewTabPage.ContentSuggestions.Opened", 0);
}

TEST_F(MetricsReporterTest, SendFeedbackAction) {
  reporter_.SendFeedbackAction();

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.SendFeedback"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserAction",
                                FeedUserActionType::kTappedSendFeedback, 1);
}

TEST_F(MetricsReporterTest, DownloadAction) {
  reporter_.DownloadAction();

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.Download"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserAction",
                                FeedUserActionType::kTappedDownload, 1);
}

TEST_F(MetricsReporterTest, LearnMoreAction) {
  reporter_.LearnMoreAction();

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.LearnMore"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserAction",
                                FeedUserActionType::kTappedLearnMore, 1);
}

TEST_F(MetricsReporterTest, RemoveAction) {
  reporter_.RemoveAction();

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.HideStory"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserAction",
                                FeedUserActionType::kTappedHideStory, 1);
}

TEST_F(MetricsReporterTest, NotInterestedInAction) {
  reporter_.NotInterestedInAction();

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.NotInterestedIn"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserAction",
                                FeedUserActionType::kTappedNotInterestedIn, 1);
}

TEST_F(MetricsReporterTest, ManageInterestsAction) {
  reporter_.ManageInterestsAction();

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.ManageInterests"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserAction",
                                FeedUserActionType::kTappedManageInterests, 1);
}

TEST_F(MetricsReporterTest, ContextMenuOpened) {
  reporter_.ContextMenuOpened();

  std::map<FeedEngagementType, int> want_empty;
  EXPECT_EQ(want_empty, ReportedEngagementType());
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.ContextMenu"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserAction",
                                FeedUserActionType::kOpenedContextMenu, 1);
}

TEST_F(MetricsReporterTest, SurfaceOpened) {
  reporter_.SurfaceOpened();

  std::map<FeedEngagementType, int> want_empty;
  EXPECT_EQ(want_empty, ReportedEngagementType());
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserAction",
                                FeedUserActionType::kOpenedFeedSurface, 1);
}

}  // namespace feed

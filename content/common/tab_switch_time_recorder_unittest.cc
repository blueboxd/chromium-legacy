// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <strstream>
#include <utility>

#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "content/common/content_to_visible_time_reporter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/presentation_feedback.h"

namespace content {

constexpr char kDurationWithSavedFramesHistogram[] =
    "Browser.Tabs.TotalSwitchDuration.WithSavedFrames";
constexpr char kDurationNoSavedFramesHistogram[] =
    "Browser.Tabs.TotalSwitchDuration.NoSavedFrames_Loaded";
constexpr char kDurationNoSavedFramesNotFrozenHistogram[] =
    "Browser.Tabs.TotalSwitchDuration.NoSavedFrames_Loaded_NotFrozen";
constexpr char kDurationNoSavedFramesFrozenHistogram[] =
    "Browser.Tabs.TotalSwitchDuration.NoSavedFrames_Loaded_Frozen";
constexpr char kDurationNoSavedFramesUnloadedHistogram[] =
    "Browser.Tabs.TotalSwitchDuration.NoSavedFrames_NotLoaded";

constexpr char kIncompleteDurationWithSavedFramesHistogram[] =
    "Browser.Tabs.TotalIncompleteSwitchDuration.WithSavedFrames";
constexpr char kIncompleteDurationNoSavedFramesHistogram[] =
    "Browser.Tabs.TotalIncompleteSwitchDuration.NoSavedFrames_Loaded";
constexpr char kIncompleteDurationNoSavedFramesNotFrozenHistogram[] =
    "Browser.Tabs.TotalIncompleteSwitchDuration.NoSavedFrames_Loaded_NotFrozen";
constexpr char kIncompleteDurationNoSavedFramesFrozenHistogram[] =
    "Browser.Tabs.TotalIncompleteSwitchDuration.NoSavedFrames_Loaded_Frozen";
constexpr char kIncompleteDurationNoSavedFramesUnloadedHistogram[] =
    "Browser.Tabs.TotalIncompleteSwitchDuration.NoSavedFrames_NotLoaded";

constexpr char kResultWithSavedFramesHistogram[] =
    "Browser.Tabs.TabSwitchResult.WithSavedFrames";
constexpr char kResultNoSavedFramesHistogram[] =
    "Browser.Tabs.TabSwitchResult.NoSavedFrames_Loaded";
constexpr char kResultNoSavedFramesNotFrozenHistogram[] =
    "Browser.Tabs.TabSwitchResult.NoSavedFrames_Loaded_NotFrozen";
constexpr char kResultNoSavedFramesFrozenHistogram[] =
    "Browser.Tabs.TabSwitchResult.NoSavedFrames_Loaded_Frozen";
constexpr char kResultNoSavedFramesUnloadedHistogram[] =
    "Browser.Tabs.TabSwitchResult.NoSavedFrames_NotLoaded";
constexpr char kWebContentsUnOccludedHistogram[] =
    "Aura.WebContentsWindowUnOccludedTime";
constexpr char kBfcacheRestoreHistogram[] =
    "BackForwardCache.Restore.NavigationToFirstPaint";

constexpr base::TimeDelta kDuration = base::TimeDelta::FromMilliseconds(42);
constexpr base::TimeDelta kOtherDuration =
    base::TimeDelta::FromMilliseconds(4242);

class ContentToVisibleTimeReporterTest : public testing::Test {
 protected:
  void SetUp() override {
    // Expect all histograms to be empty.
    ExpectHistogramsEmptyExcept({});
  }

  void ExpectHistogramsEmptyExcept(
      std::vector<const char*> histograms_with_values) {
    constexpr const char* kAllHistograms[] = {
        kDurationWithSavedFramesHistogram,
        kDurationNoSavedFramesHistogram,
        kDurationNoSavedFramesNotFrozenHistogram,
        kDurationNoSavedFramesFrozenHistogram,
        kDurationNoSavedFramesUnloadedHistogram,
        kIncompleteDurationWithSavedFramesHistogram,
        kIncompleteDurationNoSavedFramesHistogram,
        kIncompleteDurationNoSavedFramesNotFrozenHistogram,
        kIncompleteDurationNoSavedFramesFrozenHistogram,
        kIncompleteDurationNoSavedFramesUnloadedHistogram,
        kResultWithSavedFramesHistogram,
        kResultNoSavedFramesHistogram,
        kResultNoSavedFramesNotFrozenHistogram,
        kResultNoSavedFramesFrozenHistogram,
        kResultNoSavedFramesUnloadedHistogram,
        kWebContentsUnOccludedHistogram};
    for (const char* histogram : kAllHistograms) {
      if (!base::Contains(histograms_with_values, histogram))
        ExpectTotalSamples(histogram, 0);
    }
  }

  void ExpectTotalSamples(const char* histogram_name, int expected_count) {
    SCOPED_TRACE(base::StringPrintf("Expect %d samples in %s.", expected_count,
                                    histogram_name));
    EXPECT_EQ(static_cast<int>(
                  histogram_tester_.GetAllSamples(histogram_name).size()),
              expected_count);
  }

  void ExpectTimeBucketCount(const char* histogram_name,
                             base::TimeDelta value,
                             int count) {
    histogram_tester_.ExpectTimeBucketCount(histogram_name, value, count);
  }

  void ExpectResultBucketCount(
      const char* histogram_name,
      ContentToVisibleTimeReporter::TabSwitchResult value,
      int count) {
    histogram_tester_.ExpectBucketCount(histogram_name, value, count);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ContentToVisibleTimeReporter tab_switch_time_recorder_;
  base::HistogramTester histogram_tester_;
};

// Time is properly recorded to histogram when we have saved frames and if we
// have a proper matching TabWasShown and callback execution.
TEST_F(ContentToVisibleTimeReporterTest, TimeIsRecordedWithSavedFrames) {
  const auto start = base::TimeTicks::Now();
  auto callback = tab_switch_time_recorder_.TabWasShown(
      true /* has_saved_frames */,
      {start, /* destination_is_loaded */ true,
       /* destination_is_frozen */ false, /* show_reason_tab_switching */ true,
       /* show_reason_unoccluded */ false,
       /* show_reason_bfcache_restore */ false},
      start);
  const auto end = start + kDuration;
  auto presentation_feedback = gfx::PresentationFeedback(
      end, end - start, gfx::PresentationFeedback::Flags::kHWCompletion);
  std::move(callback).Run(presentation_feedback);

  ExpectHistogramsEmptyExcept(
      {kDurationWithSavedFramesHistogram, kResultWithSavedFramesHistogram});

  // Duration.
  ExpectTotalSamples(kDurationWithSavedFramesHistogram, 1);
  ExpectTimeBucketCount(kDurationWithSavedFramesHistogram, kDuration, 1);

  // Result.
  ExpectTotalSamples(kResultWithSavedFramesHistogram, 1);
  ExpectResultBucketCount(
      kResultWithSavedFramesHistogram,
      ContentToVisibleTimeReporter::TabSwitchResult::kSuccess, 1);
}

// Time is properly recorded to histogram when we have no saved frame and if we
// have a proper matching TabWasShown and callback execution.
TEST_F(ContentToVisibleTimeReporterTest, TimeIsRecordedNoSavedFrameNotFrozen) {
  const auto start = base::TimeTicks::Now();
  auto callback = tab_switch_time_recorder_.TabWasShown(
      false /* has_saved_frames */,
      {start, /* destination_is_loaded */ true,
       /* destination_is_frozen */ false, /* show_reason_tab_switching */ true,
       /* show_reason_unoccluded */ false,
       /* show_reason_bfcache_restore */ false},
      start);
  const auto end = start + kDuration;
  auto presentation_feedback = gfx::PresentationFeedback(
      end, end - start, gfx::PresentationFeedback::Flags::kHWCompletion);
  std::move(callback).Run(presentation_feedback);

  ExpectHistogramsEmptyExcept({kDurationNoSavedFramesHistogram,
                               kDurationNoSavedFramesNotFrozenHistogram,
                               kResultNoSavedFramesHistogram,
                               kResultNoSavedFramesNotFrozenHistogram});

  // Duration.
  ExpectTotalSamples(kDurationNoSavedFramesHistogram, 1);
  ExpectTimeBucketCount(kDurationNoSavedFramesHistogram, kDuration, 1);
  ExpectTotalSamples(kDurationNoSavedFramesNotFrozenHistogram, 1);
  ExpectTimeBucketCount(kDurationNoSavedFramesNotFrozenHistogram, kDuration, 1);

  // Result.
  ExpectTotalSamples(kResultNoSavedFramesHistogram, 1);
  ExpectResultBucketCount(
      kResultNoSavedFramesHistogram,
      ContentToVisibleTimeReporter::TabSwitchResult::kSuccess, 1);
  ExpectTotalSamples(kResultNoSavedFramesNotFrozenHistogram, 1);
  ExpectResultBucketCount(
      kResultNoSavedFramesNotFrozenHistogram,
      ContentToVisibleTimeReporter::TabSwitchResult::kSuccess, 1);
}

// Same as TimeIsRecordedNoSavedFrame but with the destination frame frozen.
TEST_F(ContentToVisibleTimeReporterTest, TimeIsRecordedNoSavedFrameFrozen) {
  const auto start = base::TimeTicks::Now();
  auto callback = tab_switch_time_recorder_.TabWasShown(
      false /* has_saved_frames */,
      {start, /* destination_is_loaded */ true,
       /* destination_is_frozen */ true, /* show_reason_tab_switching */ true,
       /* show_reason_unoccluded */ false,
       /* show_reason_bfcache_restore */ false},
      start);
  const auto end = start + kDuration;
  auto presentation_feedback = gfx::PresentationFeedback(
      end, end - start, gfx::PresentationFeedback::Flags::kHWCompletion);
  std::move(callback).Run(presentation_feedback);

  ExpectHistogramsEmptyExcept(
      {kDurationNoSavedFramesHistogram, kDurationNoSavedFramesFrozenHistogram,
       kResultNoSavedFramesHistogram, kResultNoSavedFramesFrozenHistogram});

  // Duration.
  ExpectTotalSamples(kDurationNoSavedFramesHistogram, 1);
  ExpectTimeBucketCount(kDurationNoSavedFramesHistogram, kDuration, 1);
  ExpectTotalSamples(kDurationNoSavedFramesFrozenHistogram, 1);
  ExpectTimeBucketCount(kDurationNoSavedFramesFrozenHistogram, kDuration, 1);

  // Result.
  ExpectTotalSamples(kResultNoSavedFramesHistogram, 1);
  ExpectResultBucketCount(
      kResultNoSavedFramesHistogram,
      ContentToVisibleTimeReporter::TabSwitchResult::kSuccess, 1);
  ExpectTotalSamples(kResultNoSavedFramesFrozenHistogram, 1);
  ExpectResultBucketCount(
      kResultNoSavedFramesFrozenHistogram,
      ContentToVisibleTimeReporter::TabSwitchResult::kSuccess, 1);
}

// Same as TimeIsRecordedNoSavedFrame but with the destination frame unloaded.
TEST_F(ContentToVisibleTimeReporterTest, TimeIsRecordedNoSavedFrameUnloaded) {
  const auto start = base::TimeTicks::Now();
  auto callback = tab_switch_time_recorder_.TabWasShown(
      false /* has_saved_frames */,
      {start, /* destination_is_loaded */ false,
       /* destination_is_frozen */ false, /* show_reason_tab_switching */ true,
       /* show_reason_unoccluded */ false,
       /* show_reason_bfcache_restore */ false},
      start);
  const auto end = start + kDuration;
  auto presentation_feedback = gfx::PresentationFeedback(
      end, end - start, gfx::PresentationFeedback::Flags::kHWCompletion);
  std::move(callback).Run(presentation_feedback);

  ExpectHistogramsEmptyExcept({kDurationNoSavedFramesUnloadedHistogram,
                               kResultNoSavedFramesUnloadedHistogram});

  // Duration.
  ExpectTotalSamples(kDurationNoSavedFramesUnloadedHistogram, 1);
  ExpectTimeBucketCount(kDurationNoSavedFramesUnloadedHistogram, kDuration, 1);

  // Result.
  ExpectTotalSamples(kResultNoSavedFramesUnloadedHistogram, 1);
  ExpectResultBucketCount(
      kResultNoSavedFramesUnloadedHistogram,
      ContentToVisibleTimeReporter::TabSwitchResult::kSuccess, 1);
}

// A failure should be reported if gfx::PresentationFeedback contains the
// kFailure flag.
TEST_F(ContentToVisibleTimeReporterTest, PresentationFailureWithSavedFrames) {
  const auto start = base::TimeTicks::Now();
  auto callback = tab_switch_time_recorder_.TabWasShown(
      true /* has_saved_frames */,
      {start, /* destination_is_loaded */ true,
       /* destination_is_frozen */ false, /* show_reason_tab_switching */ true,
       /* show_reason_unoccluded */ false,
       /* show_reason_bfcache_restore */ false},
      start);
  std::move(callback).Run(gfx::PresentationFeedback::Failure());

  ExpectHistogramsEmptyExcept({kResultWithSavedFramesHistogram});

  // Result (no duration is recorded on presentation failure).
  ExpectTotalSamples(kResultWithSavedFramesHistogram, 1);
  ExpectResultBucketCount(
      kResultWithSavedFramesHistogram,
      ContentToVisibleTimeReporter::TabSwitchResult::kPresentationFailure, 1);
}

// A failure should be reported if gfx::PresentationFeedback contains the
// kFailure flag.
TEST_F(ContentToVisibleTimeReporterTest, PresentationFailureNoSavedFrames) {
  const auto start = base::TimeTicks::Now();
  auto callback = tab_switch_time_recorder_.TabWasShown(
      false /* has_saved_frames */,
      {start, /* destination_is_loaded */ true,
       /* destination_is_frozen */ false, /* show_reason_tab_switching */ true,
       /* show_reason_unoccluded */ false,
       /* show_reason_bfcache_restore */ false},
      start);
  std::move(callback).Run(gfx::PresentationFeedback::Failure());

  ExpectHistogramsEmptyExcept(
      {kResultNoSavedFramesHistogram, kResultNoSavedFramesNotFrozenHistogram});

  // Result (no duration is recorded on presentation failure).
  ExpectTotalSamples(kResultNoSavedFramesHistogram, 1);
  ExpectResultBucketCount(
      kResultNoSavedFramesHistogram,
      ContentToVisibleTimeReporter::TabSwitchResult::kPresentationFailure, 1);
  ExpectTotalSamples(kResultNoSavedFramesNotFrozenHistogram, 1);
  ExpectResultBucketCount(
      kResultNoSavedFramesNotFrozenHistogram,
      ContentToVisibleTimeReporter::TabSwitchResult::kPresentationFailure, 1);
}

// An incomplete tab switch is reported when no frame is shown before a tab is
// hidden.
TEST_F(ContentToVisibleTimeReporterTest,
       HideBeforePresentFrameWithSavedFrames) {
  const auto start1 = base::TimeTicks::Now();
  auto callback1 = tab_switch_time_recorder_.TabWasShown(
      true /* has_saved_frames */,
      {start1, /* destination_is_loaded */ true,
       /* destination_is_frozen */ false, /* show_reason_tab_switching */ true,
       /* show_reason_unoccluded */ false,
       /* show_reason_bfcache_restore */ false},
      start1);

  task_environment_.FastForwardBy(kDuration);
  tab_switch_time_recorder_.TabWasHidden();

  ExpectHistogramsEmptyExcept({kResultWithSavedFramesHistogram,
                               kIncompleteDurationWithSavedFramesHistogram});

  // Duration.
  ExpectTotalSamples(kIncompleteDurationWithSavedFramesHistogram, 1);
  ExpectTimeBucketCount(kIncompleteDurationWithSavedFramesHistogram, kDuration,
                        1);

  // Result.
  ExpectTotalSamples(kResultWithSavedFramesHistogram, 1);
  ExpectResultBucketCount(
      kResultWithSavedFramesHistogram,
      ContentToVisibleTimeReporter::TabSwitchResult::kIncomplete, 1);

  const auto start2 = base::TimeTicks::Now();
  auto callback2 = tab_switch_time_recorder_.TabWasShown(
      true /* has_saved_frames */,
      {start2, /* destination_is_loaded */ true,
       /* destination_is_frozen */ false, /* show_reason_tab_switching */ true,
       /* show_reason_unoccluded */ false,
       /* show_reason_bfcache_restore */ false},
      start2);
  const auto end2 = start2 + kOtherDuration;
  auto presentation_feedback = gfx::PresentationFeedback(
      end2, end2 - start2, gfx::PresentationFeedback::Flags::kHWCompletion);
  std::move(callback2).Run(presentation_feedback);

  ExpectHistogramsEmptyExcept({kDurationWithSavedFramesHistogram,
                               kResultWithSavedFramesHistogram,
                               kIncompleteDurationWithSavedFramesHistogram});

  // Duration.
  ExpectTotalSamples(kIncompleteDurationWithSavedFramesHistogram, 1);
  ExpectTotalSamples(kDurationWithSavedFramesHistogram, 1);
  ExpectTimeBucketCount(kDurationWithSavedFramesHistogram, kOtherDuration, 1);

  // Result.
  ExpectTotalSamples(kResultWithSavedFramesHistogram, 2);
  ExpectResultBucketCount(
      kResultWithSavedFramesHistogram,
      ContentToVisibleTimeReporter::TabSwitchResult::kIncomplete, 1);
  ExpectResultBucketCount(
      kResultWithSavedFramesHistogram,
      ContentToVisibleTimeReporter::TabSwitchResult::kSuccess, 1);
}

// An incomplete tab switch is reported when no frame is shown before a tab is
// hidden.
TEST_F(ContentToVisibleTimeReporterTest, HideBeforePresentFrameNoSavedFrames) {
  const auto start1 = base::TimeTicks::Now();
  auto callback1 = tab_switch_time_recorder_.TabWasShown(
      false /* has_saved_frames */,
      {start1, /* destination_is_loaded */ true,
       /* destination_is_frozen */ false, /* show_reason_tab_switching */ true,
       /* show_reason_unoccluded */ false,
       /* show_reason_bfcache_restore */ false},
      start1);

  task_environment_.FastForwardBy(kDuration);
  tab_switch_time_recorder_.TabWasHidden();

  ExpectHistogramsEmptyExcept(
      {kIncompleteDurationNoSavedFramesHistogram,
       kIncompleteDurationNoSavedFramesNotFrozenHistogram,
       kResultNoSavedFramesHistogram, kResultNoSavedFramesNotFrozenHistogram});

  // Duration.
  ExpectTotalSamples(kIncompleteDurationNoSavedFramesHistogram, 1);
  ExpectTimeBucketCount(kIncompleteDurationNoSavedFramesHistogram, kDuration,
                        1);
  ExpectTotalSamples(kIncompleteDurationNoSavedFramesNotFrozenHistogram, 1);
  ExpectTimeBucketCount(kIncompleteDurationNoSavedFramesNotFrozenHistogram,
                        kDuration, 1);

  // Result.
  ExpectTotalSamples(kResultNoSavedFramesHistogram, 1);
  ExpectResultBucketCount(
      kResultNoSavedFramesHistogram,
      ContentToVisibleTimeReporter::TabSwitchResult::kIncomplete, 1);
  ExpectTotalSamples(kResultNoSavedFramesNotFrozenHistogram, 1);
  ExpectResultBucketCount(
      kResultNoSavedFramesNotFrozenHistogram,
      ContentToVisibleTimeReporter::TabSwitchResult::kIncomplete, 1);

  const auto start2 = base::TimeTicks::Now();
  auto callback2 = tab_switch_time_recorder_.TabWasShown(
      false /* has_saved_frames */,
      {start2, /* destination_is_loaded */ true,
       /* destination_is_frozen */ false, /* show_reason_tab_switching */ true,
       /* show_reason_unoccluded */ false,
       /* show_reason_bfcache_restore */ false},
      start2);
  const auto end2 = start2 + kOtherDuration;

  auto presentation_feedback = gfx::PresentationFeedback(
      end2, end2 - start2, gfx::PresentationFeedback::Flags::kHWCompletion);
  std::move(callback2).Run(presentation_feedback);

  ExpectHistogramsEmptyExcept(
      {kIncompleteDurationNoSavedFramesHistogram,
       kIncompleteDurationNoSavedFramesNotFrozenHistogram,
       kDurationNoSavedFramesHistogram,
       kDurationNoSavedFramesNotFrozenHistogram, kResultNoSavedFramesHistogram,
       kResultNoSavedFramesNotFrozenHistogram});

  // Duration.
  ExpectTotalSamples(kIncompleteDurationNoSavedFramesHistogram, 1);
  ExpectTimeBucketCount(kIncompleteDurationNoSavedFramesHistogram, kDuration,
                        1);
  ExpectTotalSamples(kIncompleteDurationNoSavedFramesNotFrozenHistogram, 1);
  ExpectTimeBucketCount(kIncompleteDurationNoSavedFramesNotFrozenHistogram,
                        kDuration, 1);

  ExpectTotalSamples(kDurationNoSavedFramesHistogram, 1);
  ExpectTimeBucketCount(kDurationNoSavedFramesHistogram, kOtherDuration, 1);
  ExpectTotalSamples(kDurationNoSavedFramesNotFrozenHistogram, 1);
  ExpectTimeBucketCount(kDurationNoSavedFramesNotFrozenHistogram,
                        kOtherDuration, 1);

  // Result.
  ExpectTotalSamples(kResultNoSavedFramesHistogram, 2);
  ExpectResultBucketCount(
      kResultNoSavedFramesHistogram,
      ContentToVisibleTimeReporter::TabSwitchResult::kIncomplete, 1);
  ExpectResultBucketCount(
      kResultNoSavedFramesHistogram,
      ContentToVisibleTimeReporter::TabSwitchResult::kSuccess, 1);
  ExpectTotalSamples(kResultNoSavedFramesNotFrozenHistogram, 2);
  ExpectResultBucketCount(
      kResultNoSavedFramesNotFrozenHistogram,
      ContentToVisibleTimeReporter::TabSwitchResult::kIncomplete, 1);
  ExpectResultBucketCount(
      kResultNoSavedFramesNotFrozenHistogram,
      ContentToVisibleTimeReporter::TabSwitchResult::kSuccess, 1);
}

// Time is properly recorded to histogram when we have unoccluded event.
TEST_F(ContentToVisibleTimeReporterTest, UnoccludedTimeIsRecorded) {
  const auto start = base::TimeTicks::Now();
  auto callback = tab_switch_time_recorder_.TabWasShown(
      true /* has_saved_frames */,
      {start, base::Optional<bool>() /* destination_is_loaded */,
       base::Optional<bool>() /* destination_is_frozen */,
       /* show_reason_tab_switching */ false,
       /* show_reason_unoccluded */ true,
       /* show_reason_bfcache_restore */ false},
      start);
  const auto end = start + kDuration;
  auto presentation_feedback = gfx::PresentationFeedback(
      end, end - start, gfx::PresentationFeedback::Flags::kHWCompletion);
  std::move(callback).Run(presentation_feedback);

  ExpectHistogramsEmptyExcept({kWebContentsUnOccludedHistogram});

  // UnOccluded.
  ExpectTotalSamples(kWebContentsUnOccludedHistogram, 1);
  ExpectTimeBucketCount(kWebContentsUnOccludedHistogram, kDuration, 1);
}

// Time is properly recorded to histogram when we have unoccluded event
// and some other events too.
TEST_F(ContentToVisibleTimeReporterTest,
       TimeIsRecordedWithSavedFramesPlusUnoccludedTimeIsRecorded) {
  const auto start = base::TimeTicks::Now();
  auto callback = tab_switch_time_recorder_.TabWasShown(
      true /* has_saved_frames */,
      {start, /* destination_is_loaded */ true,
       /* destination_is_frozen */ false, /* show_reason_tab_switching */ true,
       /* show_reason_unoccluded */ true,
       /* show_reason_bfcache_restore */ false},
      start);
  const auto end = start + kDuration;
  auto presentation_feedback = gfx::PresentationFeedback(
      end, end - start, gfx::PresentationFeedback::Flags::kHWCompletion);
  std::move(callback).Run(presentation_feedback);

  ExpectHistogramsEmptyExcept({kDurationWithSavedFramesHistogram,
                               kResultWithSavedFramesHistogram,
                               kWebContentsUnOccludedHistogram});

  // Duration.
  ExpectTotalSamples(kDurationWithSavedFramesHistogram, 1);
  ExpectTimeBucketCount(kDurationWithSavedFramesHistogram, kDuration, 1);

  // Result.
  ExpectTotalSamples(kResultWithSavedFramesHistogram, 1);
  ExpectResultBucketCount(
      kResultWithSavedFramesHistogram,
      ContentToVisibleTimeReporter::TabSwitchResult::kSuccess, 1);

  // UnOccluded.
  ExpectTotalSamples(kWebContentsUnOccludedHistogram, 1);
  ExpectTimeBucketCount(kWebContentsUnOccludedHistogram, kDuration, 1);
}

// Time is properly recorded to histogram when we have bfcache restore event.
TEST_F(ContentToVisibleTimeReporterTest, BfcacheRestoreTimeIsRecorded) {
  const auto start = base::TimeTicks::Now();
  auto callback = tab_switch_time_recorder_.TabWasShown(
      false /* has_saved_frames */,
      {start, base::Optional<bool>() /* destination_is_loaded */,
       base::Optional<bool>() /* destination_is_frozen */,
       /* show_reason_tab_switching */ false,
       /* show_reason_unoccluded */ false,
       /* show_reason_bfcache_restore */ true},
      start);
  const auto end = start + kDuration;
  auto presentation_feedback = gfx::PresentationFeedback(
      end, end - start, gfx::PresentationFeedback::Flags::kHWCompletion);
  std::move(callback).Run(presentation_feedback);

  ExpectHistogramsEmptyExcept({kBfcacheRestoreHistogram});

  // Bfcache restore.
  ExpectTotalSamples(kBfcacheRestoreHistogram, 1);
  ExpectTimeBucketCount(kBfcacheRestoreHistogram, kDuration, 1);
}

class RecordContentToVisibleTimeRequestTest : public testing::Test {
 protected:
  // event_start_times are random, so caller is expected to provide failure
  // message useful for debugging timestamps inequality.
  void ExpectEqual(const RecordContentToVisibleTimeRequest& left,
                   const RecordContentToVisibleTimeRequest& right,
                   const std::string& msg) const {
    EXPECT_EQ(left.event_start_time, right.event_start_time) << msg;
    EXPECT_EQ(left.destination_is_loaded, right.destination_is_loaded);
    EXPECT_EQ(left.destination_is_frozen, right.destination_is_frozen);
    EXPECT_EQ(left.show_reason_tab_switching, right.show_reason_tab_switching);
    EXPECT_EQ(left.show_reason_unoccluded, right.show_reason_unoccluded);
    EXPECT_EQ(left.show_reason_bfcache_restore,
              right.show_reason_bfcache_restore);
  }
};

TEST_F(RecordContentToVisibleTimeRequestTest, MergeEmpty) {
  // Merge two empty requests
  RecordContentToVisibleTimeRequest request;
  request.UpdateRequest(RecordContentToVisibleTimeRequest());
  ExpectEqual(request, RecordContentToVisibleTimeRequest(), std::string());
}

// Merge two requests. Tuple represents the parameters of the two requests.
class RecordContentToVisibleTimeRequest_MergeRequestTest
    : public RecordContentToVisibleTimeRequestTest,
      public testing::WithParamInterface<std::tuple<bool,
                                                    bool,
                                                    bool,
                                                    bool,
                                                    bool,
                                                    bool,
                                                    bool,
                                                    bool,
                                                    bool,
                                                    bool>> {
 protected:
  RecordContentToVisibleTimeRequest GetRequest1() const {
    const base::TimeTicks timestamp = RandomRequestTimeTicks();
    return RecordContentToVisibleTimeRequest(
        timestamp,
        std::get<0>(GetParam())
            ? base::Optional<bool>(true)
            : base::Optional<bool>() /* destination_is_loaded */,
        std::get<1>(GetParam())
            ? base::Optional<bool>(true)
            : base::Optional<bool>() /* destination_is_frozen */,
        std::get<2>(GetParam()) /* show_reason_tab_switching */,
        std::get<3>(GetParam()) /* show_reason_unoccluded */,
        std::get<4>(GetParam()) /* show_reason_bfcache_restore */);
  }

  RecordContentToVisibleTimeRequest GetRequest2() const {
    const base::TimeTicks timestamp = RandomRequestTimeTicks();
    return RecordContentToVisibleTimeRequest(
        timestamp,
        std::get<5>(GetParam())
            ? base::Optional<bool>(true)
            : base::Optional<bool>() /* destination_is_loaded */,
        std::get<6>(GetParam())
            ? base::Optional<bool>(true)
            : base::Optional<bool>() /* destination_is_frozen */,
        std::get<7>(GetParam()) /* show_reason_tab_switching */,
        std::get<8>(GetParam()) /* show_reason_unoccluded */,
        std::get<9>(GetParam()) /* show_reason_bfcache_restore */);
  }

  bool isOptionalBoolTrue(const base::Optional<bool>& data) {
    return data.has_value() && data.value();
  }

 private:
  // Returns random moment between (Now) and (Now - random time in the past)
  base::TimeTicks RandomRequestTimeTicks() const {
    uint16_t number;
    base::RandBytes(&number, sizeof(number));
    return base::TimeTicks::Now() - base::TimeDelta::FromMilliseconds(number);
  }
};

TEST_P(RecordContentToVisibleTimeRequest_MergeRequestTest, DoMerge) {
  RecordContentToVisibleTimeRequest request1 = GetRequest1();
  const RecordContentToVisibleTimeRequest request2 = GetRequest2();

  // Timestamps are random, log them in case of failure.
  std::ostringstream buf;
  buf << "Original request1.event_start_time = " << request1.event_start_time
      << ",  merged request2.event_start_time = " << request2.event_start_time
      << ".";

  request1.UpdateRequest(request2);

  // We expect to get minimal timestamp and all the boolean flags set in any
  // request.
  const RecordContentToVisibleTimeRequest expected(
      std::min(request1.event_start_time, request2.event_start_time),
      (request1.destination_is_loaded.has_value() ||
       request2.destination_is_loaded.has_value())
          ? isOptionalBoolTrue(request1.destination_is_loaded) ||
                isOptionalBoolTrue(request2.destination_is_loaded)
          : base::Optional<bool>(),
      (request1.destination_is_frozen.has_value() ||
       request2.destination_is_frozen.has_value())
          ? isOptionalBoolTrue(request1.destination_is_frozen) ||
                isOptionalBoolTrue(request2.destination_is_frozen)
          : base::Optional<bool>(),
      request1.show_reason_tab_switching || request2.show_reason_tab_switching,
      request1.show_reason_unoccluded || request2.show_reason_unoccluded,
      request1.show_reason_bfcache_restore ||
          request2.show_reason_bfcache_restore);

  ExpectEqual(request1, expected, buf.str());
}

INSTANTIATE_TEST_SUITE_P(All,
                         RecordContentToVisibleTimeRequest_MergeRequestTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));
}  // namespace content

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/ukm_manager.h"

#include <vector>

#include "base/time/time.h"
#include "cc/metrics/compositor_frame_reporter.h"
#include "cc/metrics/event_metrics.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/viz/common/frame_timing_details.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cc {
namespace {

const char kTestUrl[] = "https://example.com/foo";
const int64_t kTestSourceId1 = 100;
const int64_t kTestSourceId2 = 200;

const char kUserInteraction[] = "Compositor.UserInteraction";
const char kRendering[] = "Compositor.Rendering";

const char kCheckerboardArea[] = "CheckerboardedContentArea";
const char kCheckerboardAreaRatio[] = "CheckerboardedContentAreaRatio";
const char kMissingTiles[] = "NumMissingTiles";
const char kCheckerboardedImagesCount[] = "CheckerboardedImagesCount";

const char kEventLatency[] = "Graphics.Smoothness.EventLatency";
const char kActivation[] = "Activation";
const char kBeginImplFrameToSendBeginMainFrame[] =
    "BeginImplFrameToSendBeginMainFrame";
const char kBrowserToRendererCompositor[] = "BrowserToRendererCompositor";
const char kCommit[] = "Commit";
const char kEndActivateToSubmitCompositorFrame[] =
    "EndActivateToSubmitCompositorFrame";
const char kEndCommitToActivation[] = "EndCommitToActivation";
const char kEventType[] = "EventType";
const char kScrollInputType[] = "ScrollInputType";
const char kSendBeginMainFrameToCommit[] = "SendBeginMainFrameToCommit";
const char kSubmitCompositorFrameToPresentationCompositorFrame[] =
    "SubmitCompositorFrameToPresentationCompositorFrame";
const char kTotalLatency[] = "TotalLatency";
const char kTotalLatencyToSwapEnd[] = "TotalLatencyToSwapEnd";

class UkmManagerTest : public testing::Test {
 public:
  UkmManagerTest() {
    auto recorder = std::make_unique<ukm::TestUkmRecorder>();
    test_ukm_recorder_ = recorder.get();
    manager_ = std::make_unique<UkmManager>(std::move(recorder));

    // In production, new UKM Source would have been already created, so
    // manager only needs to know the source id.
    test_ukm_recorder_->UpdateSourceURL(kTestSourceId1, GURL(kTestUrl));
    manager_->SetSourceId(kTestSourceId1);
  }

 protected:
  ukm::TestUkmRecorder* test_ukm_recorder_;
  std::unique_ptr<UkmManager> manager_;
};

TEST_F(UkmManagerTest, Basic) {
  manager_->SetUserInteractionInProgress(true);
  manager_->AddCheckerboardStatsForFrame(5, 1, 10);
  manager_->AddCheckerboardStatsForFrame(15, 3, 30);
  manager_->AddCheckerboardedImages(6);
  manager_->SetUserInteractionInProgress(false);

  // We should see a single entry for the interaction above.
  const auto& entries = test_ukm_recorder_->GetEntriesByName(kUserInteraction);
  ukm::SourceId original_id = ukm::kInvalidSourceId;
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    original_id = entry->source_id;
    EXPECT_NE(ukm::kInvalidSourceId, entry->source_id);
    test_ukm_recorder_->ExpectEntrySourceHasUrl(entry, GURL(kTestUrl));
    test_ukm_recorder_->ExpectEntryMetric(entry, kCheckerboardArea, 10);
    test_ukm_recorder_->ExpectEntryMetric(entry, kMissingTiles, 2);
    test_ukm_recorder_->ExpectEntryMetric(entry, kCheckerboardAreaRatio, 50);
    test_ukm_recorder_->ExpectEntryMetric(entry, kCheckerboardedImagesCount, 6);
  }
  test_ukm_recorder_->Purge();

  // Try pushing some stats while no user interaction is happening. No entries
  // should be pushed.
  manager_->AddCheckerboardStatsForFrame(6, 1, 10);
  manager_->AddCheckerboardStatsForFrame(99, 3, 100);
  EXPECT_EQ(0u, test_ukm_recorder_->entries_count());
  manager_->SetUserInteractionInProgress(true);
  EXPECT_EQ(0u, test_ukm_recorder_->entries_count());

  // Record a few entries and change the source before the interaction ends. The
  // stats collected up till this point should be recorded before the source is
  // swapped.
  manager_->AddCheckerboardStatsForFrame(10, 1, 100);
  manager_->AddCheckerboardStatsForFrame(30, 5, 100);

  manager_->SetSourceId(kTestSourceId2);

  const auto& entries2 = test_ukm_recorder_->GetEntriesByName(kUserInteraction);
  EXPECT_EQ(1u, entries2.size());
  for (const auto* entry : entries2) {
    EXPECT_EQ(original_id, entry->source_id);
    test_ukm_recorder_->ExpectEntryMetric(entry, kCheckerboardArea, 20);
    test_ukm_recorder_->ExpectEntryMetric(entry, kMissingTiles, 3);
    test_ukm_recorder_->ExpectEntryMetric(entry, kCheckerboardAreaRatio, 20);
    test_ukm_recorder_->ExpectEntryMetric(entry, kCheckerboardedImagesCount, 0);
  }

  // An entry for rendering is emitted when the URL changes.
  const auto& entries_rendering =
      test_ukm_recorder_->GetEntriesByName(kRendering);
  EXPECT_EQ(1u, entries_rendering.size());
  for (const auto* entry : entries_rendering) {
    EXPECT_EQ(original_id, entry->source_id);
    test_ukm_recorder_->ExpectEntryMetric(entry, kCheckerboardedImagesCount, 6);
  }
}

TEST_F(UkmManagerTest, EventLatency) {
  base::TimeTicks now = base::TimeTicks::Now();

  const base::TimeTicks event_time = now;
  std::unique_ptr<EventMetrics> event_metrics_ptrs[] = {
      EventMetrics::Create(ui::ET_GESTURE_SCROLL_BEGIN, event_time,
                           ui::ScrollInputType::kWheel),
      EventMetrics::Create(ui::ET_GESTURE_SCROLL_UPDATE, event_time,
                           ui::ScrollInputType::kWheel),
      EventMetrics::Create(ui::ET_GESTURE_SCROLL_UPDATE, event_time,
                           ui::ScrollInputType::kWheel),
  };
  EXPECT_THAT(event_metrics_ptrs, ::testing::Each(::testing::NotNull()));
  std::vector<EventMetrics> events_metrics = {
      *event_metrics_ptrs[0], *event_metrics_ptrs[1], *event_metrics_ptrs[2]};

  const base::TimeTicks begin_impl_time =
      (now += base::TimeDelta::FromMicroseconds(10));
  const base::TimeTicks end_activate_time =
      (now += base::TimeDelta::FromMicroseconds(10));
  const base::TimeTicks submit_time =
      (now += base::TimeDelta::FromMicroseconds(10));

  viz::FrameTimingDetails viz_breakdown;
  viz_breakdown.received_compositor_frame_timestamp =
      (now += base::TimeDelta::FromMicroseconds(1));
  viz_breakdown.draw_start_timestamp =
      (now += base::TimeDelta::FromMicroseconds(2));
  viz_breakdown.swap_timings.swap_start =
      (now += base::TimeDelta::FromMicroseconds(3));
  viz_breakdown.swap_timings.swap_end =
      (now += base::TimeDelta::FromMicroseconds(4));
  viz_breakdown.presentation_feedback.timestamp =
      (now += base::TimeDelta::FromMicroseconds(5));

  const base::TimeTicks swap_end_time = viz_breakdown.swap_timings.swap_end;
  const base::TimeTicks present_time =
      viz_breakdown.presentation_feedback.timestamp;

  std::vector<CompositorFrameReporter::StageData> stage_history = {
      {
          CompositorFrameReporter::StageType::
              kBeginImplFrameToSendBeginMainFrame,
          begin_impl_time,
          end_activate_time,
      },
      {
          CompositorFrameReporter::StageType::
              kEndActivateToSubmitCompositorFrame,
          end_activate_time,
          submit_time,
      },
      {
          CompositorFrameReporter::StageType::
              kSubmitCompositorFrameToPresentationCompositorFrame,
          submit_time,
          viz_breakdown.presentation_feedback.timestamp,
      },
      {
          CompositorFrameReporter::StageType::kTotalLatency,
          event_time,
          viz_breakdown.presentation_feedback.timestamp,
      },
  };

  manager_->RecordEventLatencyUKM(events_metrics, stage_history, viz_breakdown);

  const auto& entries = test_ukm_recorder_->GetEntriesByName(kEventLatency);
  ukm::SourceId original_id = ukm::kInvalidSourceId;
  EXPECT_EQ(3u, entries.size());
  for (size_t i = 0; i < entries.size(); i++) {
    const auto* entry = entries[i];
    const auto* event_metrics = event_metrics_ptrs[i].get();

    original_id = entry->source_id;
    EXPECT_NE(ukm::kInvalidSourceId, entry->source_id);
    test_ukm_recorder_->ExpectEntrySourceHasUrl(entry, GURL(kTestUrl));

    test_ukm_recorder_->ExpectEntryMetric(
        entry, kEventType, static_cast<int64_t>(event_metrics->type()));
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kScrollInputType,
        static_cast<int64_t>(*event_metrics->scroll_type()));

    EXPECT_FALSE(test_ukm_recorder_->EntryHasMetric(entry, kActivation));
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kBrowserToRendererCompositor,
        (begin_impl_time - event_time).InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kBeginImplFrameToSendBeginMainFrame,
        (end_activate_time - begin_impl_time).InMicroseconds());
    EXPECT_FALSE(
        test_ukm_recorder_->EntryHasMetric(entry, kSendBeginMainFrameToCommit));
    EXPECT_FALSE(test_ukm_recorder_->EntryHasMetric(entry, kCommit));
    EXPECT_FALSE(
        test_ukm_recorder_->EntryHasMetric(entry, kEndCommitToActivation));
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kEndActivateToSubmitCompositorFrame,
        (submit_time - end_activate_time).InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kSubmitCompositorFrameToPresentationCompositorFrame,
        (present_time - submit_time).InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kTotalLatencyToSwapEnd,
        (swap_end_time - event_time).InMicroseconds());
    test_ukm_recorder_->ExpectEntryMetric(
        entry, kTotalLatency, (present_time - event_time).InMicroseconds());
  }
}

}  // namespace
}  // namespace cc

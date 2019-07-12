// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/compositor_frame_reporter.h"

#include "base/test/metrics/histogram_tester.h"
#include "cc/scheduler/compositor_frame_reporting_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class CompositorFrameReporterTest;

class CompositorFrameReporterTest : public testing::Test {
 public:
  CompositorFrameReporterTest()
      : pipeline_reporter_(std::make_unique<CompositorFrameReporter>()) {
    AdvanceNowByMs(1);
  }

  void AdvanceNowByMs(int advance_ms) {
    now_ += base::TimeDelta::FromMicroseconds(advance_ms);
  }

  base::TimeTicks Now() { return now_; }

 protected:
  std::unique_ptr<CompositorFrameReporter> pipeline_reporter_;
  base::TimeTicks now_;
};

TEST_F(CompositorFrameReporterTest, MainFrameAbortedReportingTest) {
  base::HistogramTester histogram_tester;

  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      Now(), nullptr);
  EXPECT_EQ(0, pipeline_reporter_->StageHistorySizeForTesting());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kSendBeginMainFrameToCommit, Now(),
      nullptr);
  EXPECT_EQ(1, pipeline_reporter_->StageHistorySizeForTesting());

  AdvanceNowByMs(2);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kMainFrameAborted,
      Now());
  EXPECT_EQ(2, pipeline_reporter_->StageHistorySizeForTesting());

  pipeline_reporter_ = nullptr;
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 0);
}

TEST_F(CompositorFrameReporterTest, ReplacedByNewReporterReportingTest) {
  base::HistogramTester histogram_tester;

  pipeline_reporter_->StartStage(CompositorFrameReporter::StageType::kCommit,
                                 Now(), nullptr);
  EXPECT_EQ(0, pipeline_reporter_->StageHistorySizeForTesting());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndCommitToActivation, Now(),
      nullptr);
  EXPECT_EQ(1, pipeline_reporter_->StageHistorySizeForTesting());

  AdvanceNowByMs(2);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kReplacedByNewReporter,
      Now());
  EXPECT_EQ(2, pipeline_reporter_->StageHistorySizeForTesting());

  pipeline_reporter_ = nullptr;
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 0);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    0);
}

TEST_F(CompositorFrameReporterTest, SubmittedFrameReportingTest) {
  base::HistogramTester histogram_tester;

  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kActivation, Now(), nullptr);
  EXPECT_EQ(0, pipeline_reporter_->StageHistorySizeForTesting());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now(), nullptr);
  EXPECT_EQ(1, pipeline_reporter_->StageHistorySizeForTesting());

  AdvanceNowByMs(2);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame, Now());
  EXPECT_EQ(2, pipeline_reporter_->StageHistorySizeForTesting());

  pipeline_reporter_ = nullptr;
  histogram_tester.ExpectTotalCount("CompositorLatency.Activation", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.TotalLatency", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.MissedFrame.Activation",
                                    0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrame.EndActivateToSubmitCompositorFrame", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrame.TotalLatency", 0);

  histogram_tester.ExpectBucketCount("CompositorLatency.Activation", 3, 1);
  histogram_tester.ExpectBucketCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 2, 1);
  histogram_tester.ExpectBucketCount("CompositorLatency.TotalLatency", 5, 1);
}

TEST_F(CompositorFrameReporterTest, SubmittedMissedFrameReportingTest) {
  base::HistogramTester histogram_tester;

  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kSendBeginMainFrameToCommit, Now(),
      nullptr);
  EXPECT_EQ(0, pipeline_reporter_->StageHistorySizeForTesting());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(CompositorFrameReporter::StageType::kCommit,
                                 Now(), nullptr);
  EXPECT_EQ(1, pipeline_reporter_->StageHistorySizeForTesting());

  AdvanceNowByMs(2);
  pipeline_reporter_->MissedSubmittedFrame();
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame, Now());
  EXPECT_EQ(2, pipeline_reporter_->StageHistorySizeForTesting());

  pipeline_reporter_ = nullptr;
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrame.SendBeginMainFrameToCommit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.MissedFrame.Commit", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrame.TotalLatency", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 0);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 0);
  histogram_tester.ExpectTotalCount("CompositorLatency.TotalLatency", 0);

  histogram_tester.ExpectBucketCount(
      "CompositorLatency.MissedFrame.SendBeginMainFrameToCommit", 3, 1);
  histogram_tester.ExpectBucketCount("CompositorLatency.MissedFrame.Commit", 2,
                                     1);
  histogram_tester.ExpectBucketCount(
      "CompositorLatency.MissedFrame.TotalLatency", 5, 1);
}

TEST_F(CompositorFrameReporterTest, MissedFrameLatencyIncreaseReportingTest) {
  base::HistogramTester histogram_tester;
  RollingTimeDeltaHistory time_delta_history(50);
  RollingTimeDeltaHistory time_delta_history2(50);

  // Terminate this frame since it will get destroyed in the for loop.
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kDidNotProduceFrame,
      Now());

  // Submit 19 non-missed frames.
  for (int i = 0; i < 19; ++i) {
    pipeline_reporter_ = std::make_unique<CompositorFrameReporter>();
    pipeline_reporter_->StartStage(CompositorFrameReporter::StageType::kCommit,
                                   Now(), &time_delta_history);
    AdvanceNowByMs(1);
    pipeline_reporter_->StartStage(
        CompositorFrameReporter::StageType::kEndCommitToActivation, Now(),
        &time_delta_history2);
    pipeline_reporter_->TerminateFrame(
        CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame,
        Now());
  }
  pipeline_reporter_ = nullptr;
  EXPECT_EQ((size_t)19, time_delta_history.sample_count());
  EXPECT_EQ((size_t)19, time_delta_history2.sample_count());
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 19);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    19);

  // Submit 3 frames missed frames. This will remove 3 sample from the front of
  // time delta history. And 16 sample will be in the time delta history.
  for (int i = 0; i < 3; ++i) {
    pipeline_reporter_ = std::make_unique<CompositorFrameReporter>();
    pipeline_reporter_->StartStage(CompositorFrameReporter::StageType::kCommit,
                                   Now(), &time_delta_history);
    AdvanceNowByMs(100);
    pipeline_reporter_->StartStage(
        CompositorFrameReporter::StageType::kEndCommitToActivation, Now(),
        &time_delta_history2);
    pipeline_reporter_->MissedSubmittedFrame();
    pipeline_reporter_->TerminateFrame(
        CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame,
        Now());
  }
  pipeline_reporter_ = nullptr;
  EXPECT_EQ((size_t)16, time_delta_history.sample_count());
  EXPECT_EQ((size_t)16, time_delta_history2.sample_count());
  DCHECK_EQ(time_delta_history.sample_count(), (size_t)16);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrameLatencyIncrease.Commit", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrameLatencyIncrease.EndCommitToActivation", 0);
  histogram_tester.ExpectTotalCount("CompositorLatency.MissedFrame.Commit", 3);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrame.EndCommitToActivation", 3);

  // Submit 5 frame so that missed frame duration increases would be reported.
  for (int i = 0; i < 5; ++i) {
    pipeline_reporter_ = std::make_unique<CompositorFrameReporter>();
    pipeline_reporter_->StartStage(CompositorFrameReporter::StageType::kCommit,
                                   Now(), &time_delta_history);
    AdvanceNowByMs(1);
    pipeline_reporter_->StartStage(
        CompositorFrameReporter::StageType::kEndCommitToActivation, Now(),
        &time_delta_history2);
    pipeline_reporter_->TerminateFrame(
        CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame,
        Now());
  }
  pipeline_reporter_ = nullptr;
  EXPECT_EQ((size_t)21, time_delta_history.sample_count());
  EXPECT_EQ((size_t)21, time_delta_history2.sample_count());

  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 24);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    24);

  // Submit missed frame that is not abnormal (more than 95 percentile of the
  // frame history). This brings down the time delta history count to 20.
  pipeline_reporter_ = std::make_unique<CompositorFrameReporter>();
  pipeline_reporter_->StartStage(CompositorFrameReporter::StageType::kCommit,
                                 Now(), &time_delta_history);
  AdvanceNowByMs(1);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndCommitToActivation, Now(),
      &time_delta_history2);
  pipeline_reporter_->MissedSubmittedFrame();
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame, Now());
  pipeline_reporter_ = nullptr;
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrameLatencyIncrease.Commit", 0);
  histogram_tester.ExpectTotalCount("CompositorLatency.MissedFrame.Commit", 4);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrame.EndCommitToActivation", 4);

  EXPECT_EQ((size_t)20, time_delta_history.sample_count());
  EXPECT_EQ((size_t)20, time_delta_history2.sample_count());

  // Submit missed frame that is abnormal (more than 95 percentile of the
  // frame history).
  pipeline_reporter_ = std::make_unique<CompositorFrameReporter>();
  pipeline_reporter_->StartStage(CompositorFrameReporter::StageType::kCommit,
                                 Now(), &time_delta_history);
  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndCommitToActivation, Now(),
      &time_delta_history2);
  pipeline_reporter_->MissedSubmittedFrame();
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame, Now());
  pipeline_reporter_ = nullptr;
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrameLatencyIncrease.Commit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.MissedFrame.Commit", 5);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrame.EndCommitToActivation", 5);

  // Submit not-missed frame with abnormal times.
  pipeline_reporter_ = std::make_unique<CompositorFrameReporter>();
  pipeline_reporter_->StartStage(CompositorFrameReporter::StageType::kCommit,
                                 Now(), &time_delta_history);
  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndCommitToActivation, Now(),
      &time_delta_history2);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame, Now());
  pipeline_reporter_ = nullptr;
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrameLatencyIncrease.Commit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 25);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    25);
}

}  // namespace
}  // namespace cc

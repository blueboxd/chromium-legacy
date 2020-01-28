// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/compositor_frame_reporting_controller.h"

#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class CompositorFrameReportingControllerTest;

class TestCompositorFrameReportingController
    : public CompositorFrameReportingController {
 public:
  TestCompositorFrameReportingController(
      CompositorFrameReportingControllerTest* test)
      : CompositorFrameReportingController(), test_(test) {}

  TestCompositorFrameReportingController(
      const TestCompositorFrameReportingController& controller) = delete;

  TestCompositorFrameReportingController& operator=(
      const TestCompositorFrameReportingController& controller) = delete;

  std::unique_ptr<CompositorFrameReporter>* reporters() { return reporters_; }

  int ActiveReporters() {
    int count = 0;
    for (int i = 0; i < PipelineStage::kNumPipelineStages; ++i) {
      if (reporters_[i])
        ++count;
    }
    return count;
  }

 protected:
  CompositorFrameReportingControllerTest* test_;
};

class CompositorFrameReportingControllerTest : public testing::Test {
 public:
  CompositorFrameReportingControllerTest() : reporting_controller_(this) {
    current_id_ = viz::BeginFrameId(1, 1);
  }

  // The following functions simulate the actions that would
  // occur for each phase of the reporting controller.
  void SimulateBeginImplFrame() {
    reporting_controller_.WillBeginImplFrame(current_id_);
  }

  void SimulateBeginMainFrame() {
    if (!reporting_controller_.reporters()[CompositorFrameReportingController::
                                               PipelineStage::kBeginImplFrame])
      SimulateBeginImplFrame();
    CHECK(
        reporting_controller_.reporters()[CompositorFrameReportingController::
                                              PipelineStage::kBeginImplFrame]);
    reporting_controller_.WillBeginMainFrame(current_id_);
  }

  void SimulateCommit(std::unique_ptr<BeginMainFrameMetrics> blink_breakdown) {
    if (!reporting_controller_
             .reporters()[CompositorFrameReportingController::PipelineStage::
                              kBeginMainFrame]) {
      begin_main_start_ = base::TimeTicks::Now();
      SimulateBeginMainFrame();
    }
    CHECK(
        reporting_controller_.reporters()[CompositorFrameReportingController::
                                              PipelineStage::kBeginMainFrame]);
    reporting_controller_.SetBlinkBreakdown(std::move(blink_breakdown),
                                            begin_main_start_);
    reporting_controller_.WillCommit();
    reporting_controller_.DidCommit();
  }

  void SimulateActivate() {
    if (!reporting_controller_.reporters()
             [CompositorFrameReportingController::PipelineStage::kCommit])
      SimulateCommit(nullptr);
    CHECK(reporting_controller_.reporters()
              [CompositorFrameReportingController::PipelineStage::kCommit]);
    reporting_controller_.WillActivate();
    reporting_controller_.DidActivate();
    last_activated_id_ = viz::BeginFrameId(current_id_);
  }

  void SimulateSubmitCompositorFrame(uint32_t frame_token) {
    if (!reporting_controller_.reporters()
             [CompositorFrameReportingController::PipelineStage::kActivate])
      SimulateActivate();
    CHECK(reporting_controller_.reporters()
              [CompositorFrameReportingController::PipelineStage::kActivate]);
    reporting_controller_.DidSubmitCompositorFrame(frame_token, current_id_,
                                                   last_activated_id_);
  }

  void SimulatePresentCompositorFrame() {
    ++next_token_;
    SimulateSubmitCompositorFrame(*next_token_);
    viz::FrameTimingDetails details = {};
    details.presentation_feedback.timestamp = base::TimeTicks::Now();
    reporting_controller_.DidPresentCompositorFrame(*next_token_, details);
  }

 protected:
  TestCompositorFrameReportingController reporting_controller_;
  viz::BeginFrameId current_id_;
  viz::BeginFrameId last_activated_id_;
  base::TimeTicks begin_main_start_;

 private:
  viz::FrameTokenGenerator next_token_;
};

TEST_F(CompositorFrameReportingControllerTest, ActiveReporterCounts) {
  // Check that there are no leaks with the CompositorFrameReporter
  // objects no matter what the sequence of scheduled actions is
  // Note that due to DCHECKs in WillCommit(), WillActivate(), etc., it
  // is impossible to have 2 reporters both in BMF or Commit

  // Tests Cases:
  // - 2 Reporters at Activate phase
  // - 2 back-to-back BeginImplFrames
  // - 4 Simultaneous Reporters

  // BF
  reporting_controller_.WillBeginImplFrame(current_id_);
  EXPECT_EQ(1, reporting_controller_.ActiveReporters());

  // BF -> BF
  // Should replace previous reporter.
  reporting_controller_.WillBeginImplFrame(current_id_);
  EXPECT_EQ(1, reporting_controller_.ActiveReporters());

  // BF -> BMF -> BF
  // Should add new reporter.
  reporting_controller_.WillBeginMainFrame(current_id_);
  reporting_controller_.WillBeginImplFrame(current_id_);
  EXPECT_EQ(2, reporting_controller_.ActiveReporters());

  // BF -> BMF -> BF -> Commit
  // Should stay same.
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  EXPECT_EQ(2, reporting_controller_.ActiveReporters());

  // BF -> BMF -> BF -> Commit -> BMF -> Activate -> Commit -> Activation
  // Having two reporters at Activate phase should delete the older one.
  reporting_controller_.WillBeginMainFrame(current_id_);
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  last_activated_id_ = viz::BeginFrameId(current_id_);
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  EXPECT_EQ(1, reporting_controller_.ActiveReporters());

  reporting_controller_.DidSubmitCompositorFrame(0, current_id_,
                                                 last_activated_id_);
  EXPECT_EQ(0, reporting_controller_.ActiveReporters());

  // 4 simultaneous reporters active.
  SimulateActivate();

  SimulateCommit(nullptr);

  SimulateBeginMainFrame();

  SimulateBeginImplFrame();
  EXPECT_EQ(4, reporting_controller_.ActiveReporters());

  // Any additional BeginImplFrame's would be ignored.
  SimulateBeginImplFrame();
  EXPECT_EQ(4, reporting_controller_.ActiveReporters());
}

TEST_F(CompositorFrameReportingControllerTest,
       SubmittedFrameHistogramReporting) {
  base::HistogramTester histogram_tester;

  // 2 reporters active.
  SimulateActivate();
  SimulateCommit(nullptr);

  // Submitting and Presenting the next reporter which will be a normal frame.
  SimulatePresentCompositorFrame();

  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrame.BeginImplFrameToSendBeginMainFrame", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrame.SendBeginMainFrameToCommit", 0);
  histogram_tester.ExpectTotalCount("CompositorLatency.MissedFrame.Commit", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrame.EndCommitToActivation", 0);
  histogram_tester.ExpectTotalCount("CompositorLatency.MissedFrame.Activation",
                                    0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrame.EndActivateToSubmitCompositorFrame", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Activation", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 1);

  // Submitting the next reporter will be replaced as a result of a new commit.
  // And this will be reported for all stage before activate as a missed frame.
  SimulateCommit(nullptr);
  // Non Missed frame histogram counts should not change.
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Activation", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 1);

  // Other histograms should be reported updated.
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrame.BeginImplFrameToSendBeginMainFrame", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrame.SendBeginMainFrameToCommit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.MissedFrame.Commit", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrame.EndCommitToActivation", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.MissedFrame.Activation",
                                    0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrame.EndActivateToSubmitCompositorFrame", 0);
}

TEST_F(CompositorFrameReportingControllerTest, ImplFrameCausedNoDamage) {
  base::HistogramTester histogram_tester;

  SimulateBeginImplFrame();
  SimulateBeginImplFrame();
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrame.BeginImplFrameToSendBeginMainFrame", 0);
}

TEST_F(CompositorFrameReportingControllerTest, MainFrameCausedNoDamage) {
  base::HistogramTester histogram_tester;
  viz::BeginFrameId current_id_1_ = viz::BeginFrameId(1, 1);
  viz::BeginFrameId current_id_2_ = viz::BeginFrameId(1, 2);
  viz::BeginFrameId current_id_3_ = viz::BeginFrameId(1, 3);

  reporting_controller_.WillBeginImplFrame(current_id_1_);
  reporting_controller_.WillBeginMainFrame(current_id_1_);
  reporting_controller_.BeginMainFrameAborted(current_id_1_);
  reporting_controller_.OnFinishImplFrame(current_id_1_);
  reporting_controller_.DidNotProduceFrame(current_id_1_);

  reporting_controller_.WillBeginImplFrame(current_id_2_);
  reporting_controller_.WillBeginMainFrame(current_id_2_);
  reporting_controller_.OnFinishImplFrame(current_id_2_);
  reporting_controller_.BeginMainFrameAborted(current_id_2_);
  reporting_controller_.DidNotProduceFrame(current_id_2_);

  reporting_controller_.WillBeginImplFrame(current_id_3_);
  reporting_controller_.WillBeginMainFrame(current_id_3_);

  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrame.BeginImplFrameToSendBeginMainFrame", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrame.SendBeginMainFrameToCommit", 0);
}

TEST_F(CompositorFrameReportingControllerTest, MainFrameAborted) {
  base::HistogramTester histogram_tester;

  reporting_controller_.WillBeginImplFrame(current_id_);
  reporting_controller_.WillBeginMainFrame(current_id_);
  reporting_controller_.BeginMainFrameAborted(current_id_);
  reporting_controller_.OnFinishImplFrame(current_id_);
  reporting_controller_.DidSubmitCompositorFrame(1, current_id_,
                                                 last_activated_id_);

  viz::FrameTimingDetails details = {};
  reporting_controller_.DidPresentCompositorFrame(1, details);

  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 0);
  histogram_tester.ExpectTotalCount("CompositorLatency.Activation", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SubmitCompositorFrameToPresentationCompositorFrame",
      1);
}

TEST_F(CompositorFrameReportingControllerTest, MainFrameAborted2) {
  base::HistogramTester histogram_tester;
  viz::BeginFrameId current_id_1_ = viz::BeginFrameId(1, 1);
  viz::BeginFrameId current_id_2_ = viz::BeginFrameId(1, 2);
  viz::BeginFrameId current_id_3_ = viz::BeginFrameId(1, 3);
  reporting_controller_.WillBeginImplFrame(current_id_1_);
  reporting_controller_.OnFinishImplFrame(current_id_1_);
  reporting_controller_.WillBeginMainFrame(current_id_1_);
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  reporting_controller_.WillBeginImplFrame(current_id_2_);
  reporting_controller_.WillBeginMainFrame(current_id_2_);
  reporting_controller_.OnFinishImplFrame(current_id_2_);
  reporting_controller_.BeginMainFrameAborted(current_id_2_);
  reporting_controller_.DidSubmitCompositorFrame(1, current_id_2_,
                                                 current_id_1_);
  viz::FrameTimingDetails details = {};
  reporting_controller_.DidPresentCompositorFrame(1, details);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrame.BeginImplFrameToSendBeginMainFrame", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Activation", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SubmitCompositorFrameToPresentationCompositorFrame",
      1);
  reporting_controller_.DidSubmitCompositorFrame(2, current_id_2_,
                                                 current_id_1_);
  reporting_controller_.DidPresentCompositorFrame(2, details);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrame.BeginImplFrameToSendBeginMainFrame", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 2);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 2);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Activation", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 2);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SubmitCompositorFrameToPresentationCompositorFrame",
      2);
  reporting_controller_.WillBeginImplFrame(current_id_3_);
  reporting_controller_.OnFinishImplFrame(current_id_3_);
  reporting_controller_.DidSubmitCompositorFrame(3, current_id_3_,
                                                 current_id_1_);
  reporting_controller_.DidPresentCompositorFrame(3, details);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrame.BeginImplFrameToSendBeginMainFrame", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 3);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 2);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Activation", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 3);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SubmitCompositorFrameToPresentationCompositorFrame",
      3);
}

TEST_F(CompositorFrameReportingControllerTest, BlinkBreakdown) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<BeginMainFrameMetrics> blink_breakdown =
      std::make_unique<BeginMainFrameMetrics>();
  blink_breakdown->handle_input_events = base::TimeDelta::FromMicroseconds(10);
  blink_breakdown->animate = base::TimeDelta::FromMicroseconds(9);
  blink_breakdown->style_update = base::TimeDelta::FromMicroseconds(8);
  blink_breakdown->layout_update = base::TimeDelta::FromMicroseconds(7);
  blink_breakdown->prepaint = base::TimeDelta::FromMicroseconds(6);
  blink_breakdown->composite = base::TimeDelta::FromMicroseconds(5);
  blink_breakdown->paint = base::TimeDelta::FromMicroseconds(4);
  blink_breakdown->scrolling_coordinator = base::TimeDelta::FromMicroseconds(3);
  blink_breakdown->composite_commit = base::TimeDelta::FromMicroseconds(2);
  blink_breakdown->update_layers = base::TimeDelta::FromMicroseconds(1);

  SimulateActivate();
  SimulateCommit(std::move(blink_breakdown));
  SimulatePresentCompositorFrame();

  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 1);
  histogram_tester.ExpectUniqueSample(
      "CompositorLatency.SendBeginMainFrameToCommit.HandleInputEvents",
      base::TimeDelta::FromMicroseconds(10).InMilliseconds(), 1);
  histogram_tester.ExpectUniqueSample(
      "CompositorLatency.SendBeginMainFrameToCommit.Animate",
      base::TimeDelta::FromMicroseconds(9).InMilliseconds(), 1);
  histogram_tester.ExpectUniqueSample(
      "CompositorLatency.SendBeginMainFrameToCommit.StyleUpdate",
      base::TimeDelta::FromMicroseconds(8).InMilliseconds(), 1);
  histogram_tester.ExpectUniqueSample(
      "CompositorLatency.SendBeginMainFrameToCommit.LayoutUpdate",
      base::TimeDelta::FromMicroseconds(7).InMilliseconds(), 1);
  histogram_tester.ExpectUniqueSample(
      "CompositorLatency.SendBeginMainFrameToCommit.Prepaint",
      base::TimeDelta::FromMicroseconds(6).InMilliseconds(), 1);
  histogram_tester.ExpectUniqueSample(
      "CompositorLatency.SendBeginMainFrameToCommit.Composite",
      base::TimeDelta::FromMicroseconds(5).InMilliseconds(), 1);
  histogram_tester.ExpectUniqueSample(
      "CompositorLatency.SendBeginMainFrameToCommit.Paint",
      base::TimeDelta::FromMicroseconds(4).InMilliseconds(), 1);
  histogram_tester.ExpectUniqueSample(
      "CompositorLatency.SendBeginMainFrameToCommit.ScrollingCoordinator",
      base::TimeDelta::FromMicroseconds(3).InMilliseconds(), 1);
  histogram_tester.ExpectUniqueSample(
      "CompositorLatency.SendBeginMainFrameToCommit.CompositeCommit",
      base::TimeDelta::FromMicroseconds(2).InMilliseconds(), 1);
  histogram_tester.ExpectUniqueSample(
      "CompositorLatency.SendBeginMainFrameToCommit.UpdateLayers",
      base::TimeDelta::FromMicroseconds(1).InMilliseconds(), 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit.BeginMainSentToStarted", 1);
}

}  // namespace
}  // namespace cc

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/compositor_frame_reporting_controller.h"

#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "cc/input/scroll_input_type.h"
#include "cc/metrics/event_metrics.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

MATCHER(IsWhitelisted,
        base::StrCat({negation ? "isn't" : "is", " whitelisted"})) {
  return arg.IsWhitelisted();
}

class TestCompositorFrameReportingController
    : public CompositorFrameReportingController {
 public:
  TestCompositorFrameReportingController()
      : CompositorFrameReportingController(/*should_report_metrics=*/true) {}

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
};

class CompositorFrameReportingControllerTest : public testing::Test {
 public:
  CompositorFrameReportingControllerTest() : current_id_(1, 1) {
    args_ = SimulateBeginFrameArgs(current_id_);
  }

  // The following functions simulate the actions that would
  // occur for each phase of the reporting controller.
  void SimulateBeginImplFrame() {
    reporting_controller_.WillBeginImplFrame(args_);
  }

  void SimulateBeginMainFrame() {
    if (!reporting_controller_.reporters()[CompositorFrameReportingController::
                                               PipelineStage::kBeginImplFrame])
      SimulateBeginImplFrame();
    CHECK(
        reporting_controller_.reporters()[CompositorFrameReportingController::
                                              PipelineStage::kBeginImplFrame]);
    reporting_controller_.WillBeginMainFrame(args_);
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
    last_activated_id_ = current_id_;
  }

  void SimulateSubmitCompositorFrame(uint32_t frame_token,
                                     EventMetricsSet events_metrics) {
    if (!reporting_controller_.reporters()
             [CompositorFrameReportingController::PipelineStage::kActivate])
      SimulateActivate();
    CHECK(reporting_controller_.reporters()
              [CompositorFrameReportingController::PipelineStage::kActivate]);
    reporting_controller_.DidSubmitCompositorFrame(frame_token, current_id_,
                                                   last_activated_id_,
                                                   std::move(events_metrics));
  }

  void SimulatePresentCompositorFrame() {
    ++next_token_;
    SimulateSubmitCompositorFrame(*next_token_, {});
    viz::FrameTimingDetails details = {};
    details.presentation_feedback.timestamp = base::TimeTicks::Now();
    reporting_controller_.DidPresentCompositorFrame(*next_token_, details);
  }

  viz::BeginFrameArgs SimulateBeginFrameArgs(
      viz::BeginFrameId frame_id,
      base::TimeTicks frame_time = base::TimeTicks::Now(),
      base::TimeDelta interval = base::TimeDelta::FromMilliseconds(16)) {
    args_ = viz::BeginFrameArgs();
    args_.frame_id = frame_id;
    args_.frame_time = frame_time;
    args_.interval = interval;
    return args_;
  }

  void IncrementCurrentId() {
    current_id_.sequence_number++;
    args_.frame_id = current_id_;
  }

 protected:
  TestCompositorFrameReportingController reporting_controller_;
  viz::BeginFrameArgs args_;
  viz::BeginFrameId current_id_;
  viz::BeginFrameId last_activated_id_;
  base::TimeTicks begin_main_start_;
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

  viz::BeginFrameId current_id_1(1, 1);
  viz::BeginFrameArgs args_1 = SimulateBeginFrameArgs(current_id_1);

  viz::BeginFrameId current_id_2(1, 2);
  viz::BeginFrameArgs args_2 = SimulateBeginFrameArgs(current_id_2);

  viz::BeginFrameId current_id_3(1, 3);
  viz::BeginFrameArgs args_3 = SimulateBeginFrameArgs(current_id_3);

  // BF
  reporting_controller_.WillBeginImplFrame(args_1);
  EXPECT_EQ(1, reporting_controller_.ActiveReporters());

  // BF -> BF
  // Should replace previous reporter.
  reporting_controller_.WillBeginImplFrame(args_2);
  EXPECT_EQ(1, reporting_controller_.ActiveReporters());

  // BF -> BMF -> BF
  // Should add new reporter.
  reporting_controller_.WillBeginMainFrame(args_2);
  reporting_controller_.WillBeginImplFrame(args_3);
  EXPECT_EQ(2, reporting_controller_.ActiveReporters());

  // BF -> BMF -> BF -> Commit
  // Should stay same.
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  EXPECT_EQ(2, reporting_controller_.ActiveReporters());

  // BF -> BMF -> BF -> Commit -> BMF -> Activate -> Commit -> Activation
  // Having two reporters at Activate phase should delete the older one.
  reporting_controller_.WillBeginMainFrame(args_3);
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();

  // There is a reporters tracking frame_3 in BeginMain state and one reporter
  // for frame_2 in activate state.
  EXPECT_EQ(2, reporting_controller_.ActiveReporters());

  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  // Reporter in activate state for frame_2 is overwritten by the reporter for
  // frame_3.
  EXPECT_EQ(1, reporting_controller_.ActiveReporters());

  last_activated_id_ = current_id_3;
  reporting_controller_.DidSubmitCompositorFrame(0, current_id_3,
                                                 last_activated_id_, {});
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
      "CompositorLatency.DroppedFrame.BeginImplFrameToSendBeginMainFrame", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.SendBeginMainFrameToCommit", 0);
  histogram_tester.ExpectTotalCount("CompositorLatency.DroppedFrame.Commit", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.EndCommitToActivation", 0);
  histogram_tester.ExpectTotalCount("CompositorLatency.DroppedFrame.Activation",
                                    0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.EndActivateToSubmitCompositorFrame", 0);
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
      "CompositorLatency.DroppedFrame.BeginImplFrameToSendBeginMainFrame", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.SendBeginMainFrameToCommit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.DroppedFrame.Commit", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.EndCommitToActivation", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.DroppedFrame.Activation",
                                    0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.EndActivateToSubmitCompositorFrame", 0);
}

TEST_F(CompositorFrameReportingControllerTest, ImplFrameCausedNoDamage) {
  base::HistogramTester histogram_tester;

  SimulateBeginImplFrame();
  SimulateBeginImplFrame();
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.BeginImplFrameToSendBeginMainFrame", 0);
}

TEST_F(CompositorFrameReportingControllerTest, MainFrameCausedNoDamage) {
  base::HistogramTester histogram_tester;
  viz::BeginFrameId current_id_1(1, 1);
  viz::BeginFrameArgs args_1 = SimulateBeginFrameArgs(current_id_1);

  viz::BeginFrameId current_id_2(1, 2);
  viz::BeginFrameArgs args_2 = SimulateBeginFrameArgs(current_id_2);

  viz::BeginFrameId current_id_3(1, 3);
  viz::BeginFrameArgs args_3 = SimulateBeginFrameArgs(current_id_3);

  reporting_controller_.WillBeginImplFrame(args_1);
  reporting_controller_.WillBeginMainFrame(args_1);
  reporting_controller_.BeginMainFrameAborted(current_id_1);
  reporting_controller_.OnFinishImplFrame(current_id_1);
  reporting_controller_.DidNotProduceFrame(current_id_1);

  reporting_controller_.WillBeginImplFrame(args_2);
  reporting_controller_.WillBeginMainFrame(args_2);
  reporting_controller_.OnFinishImplFrame(current_id_2);
  reporting_controller_.BeginMainFrameAborted(current_id_2);
  reporting_controller_.DidNotProduceFrame(current_id_2);

  reporting_controller_.WillBeginImplFrame(args_3);
  reporting_controller_.WillBeginMainFrame(args_3);

  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.BeginImplFrameToSendBeginMainFrame", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.SendBeginMainFrameToCommit", 0);
}

TEST_F(CompositorFrameReportingControllerTest, DidNotProduceFrame) {
  base::HistogramTester histogram_tester;

  viz::BeginFrameId current_id_1(1, 1);
  viz::BeginFrameArgs args_1 = SimulateBeginFrameArgs(current_id_1);

  viz::BeginFrameId current_id_2(1, 2);
  viz::BeginFrameArgs args_2 = SimulateBeginFrameArgs(current_id_2);

  reporting_controller_.WillBeginImplFrame(args_1);
  reporting_controller_.WillBeginMainFrame(args_1);
  reporting_controller_.OnFinishImplFrame(current_id_1);
  reporting_controller_.DidNotProduceFrame(current_id_1);

  reporting_controller_.WillBeginImplFrame(args_2);
  reporting_controller_.OnFinishImplFrame(current_id_2);
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  reporting_controller_.DidSubmitCompositorFrame(1, current_id_2, current_id_1,
                                                 {});
  viz::FrameTimingDetails details = {};
  reporting_controller_.DidPresentCompositorFrame(1, details);

  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.BeginImplFrameToSendBeginMainFrame", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.SendBeginMainFrameToCommit", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 2);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Activation", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 2);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SubmitCompositorFrameToPresentationCompositorFrame",
      2);
}

TEST_F(CompositorFrameReportingControllerTest, MainFrameAborted) {
  base::HistogramTester histogram_tester;

  reporting_controller_.WillBeginImplFrame(args_);
  reporting_controller_.WillBeginMainFrame(args_);
  reporting_controller_.BeginMainFrameAborted(current_id_);
  reporting_controller_.OnFinishImplFrame(current_id_);
  reporting_controller_.DidSubmitCompositorFrame(1, current_id_,
                                                 last_activated_id_, {});

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
  viz::BeginFrameId current_id_1(1, 1);
  viz::BeginFrameArgs args_1 = SimulateBeginFrameArgs(current_id_1);

  viz::BeginFrameId current_id_2(1, 2);
  viz::BeginFrameArgs args_2 = SimulateBeginFrameArgs(current_id_2);

  viz::BeginFrameId current_id_3(1, 3);
  viz::BeginFrameArgs args_3 = SimulateBeginFrameArgs(current_id_3);

  reporting_controller_.WillBeginImplFrame(args_1);
  reporting_controller_.OnFinishImplFrame(current_id_1);
  reporting_controller_.WillBeginMainFrame(args_1);
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  reporting_controller_.WillBeginImplFrame(args_2);
  reporting_controller_.WillBeginMainFrame(args_2);
  reporting_controller_.OnFinishImplFrame(current_id_2);
  reporting_controller_.BeginMainFrameAborted(current_id_2);
  reporting_controller_.DidSubmitCompositorFrame(1, current_id_2, current_id_1,
                                                 {});
  viz::FrameTimingDetails details = {};
  reporting_controller_.DidPresentCompositorFrame(1, details);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.BeginImplFrameToSendBeginMainFrame", 0);
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
  reporting_controller_.DidSubmitCompositorFrame(2, current_id_2, current_id_1,
                                                 {});
  reporting_controller_.DidPresentCompositorFrame(2, details);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.BeginImplFrameToSendBeginMainFrame", 0);
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
  reporting_controller_.WillBeginImplFrame(args_3);
  reporting_controller_.OnFinishImplFrame(current_id_3);
  reporting_controller_.DidSubmitCompositorFrame(3, current_id_3, current_id_1,
                                                 {});
  reporting_controller_.DidPresentCompositorFrame(3, details);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.BeginImplFrameToSendBeginMainFrame", 0);
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

TEST_F(CompositorFrameReportingControllerTest, LongMainFrame) {
  base::HistogramTester histogram_tester;
  viz::BeginFrameId current_id_1(1, 1);
  viz::BeginFrameArgs args_1 = SimulateBeginFrameArgs(current_id_1);

  viz::BeginFrameId current_id_2(1, 2);
  viz::BeginFrameArgs args_2 = SimulateBeginFrameArgs(current_id_2);

  viz::BeginFrameId current_id_3(1, 3);
  viz::BeginFrameArgs args_3 = SimulateBeginFrameArgs(current_id_3);

  viz::FrameTimingDetails details = {};
  reporting_controller_.WillBeginImplFrame(args_1);
  reporting_controller_.OnFinishImplFrame(current_id_1);
  reporting_controller_.WillBeginMainFrame(args_1);
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  reporting_controller_.DidSubmitCompositorFrame(1, current_id_1, current_id_1,
                                                 {});
  reporting_controller_.DidPresentCompositorFrame(1, details);

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

  // Second frame will not have the main frame update ready and will only submit
  // the Impl update
  reporting_controller_.WillBeginImplFrame(args_2);
  reporting_controller_.WillBeginMainFrame(args_2);
  reporting_controller_.OnFinishImplFrame(current_id_2);
  reporting_controller_.DidSubmitCompositorFrame(2, current_id_2, current_id_1,
                                                 {});
  reporting_controller_.DidPresentCompositorFrame(2, details);

  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 2);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Activation", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 2);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SubmitCompositorFrameToPresentationCompositorFrame",
      2);

  reporting_controller_.WillBeginImplFrame(args_3);
  reporting_controller_.OnFinishImplFrame(current_id_3);
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  reporting_controller_.DidSubmitCompositorFrame(3, current_id_3, current_id_2,
                                                 {});
  reporting_controller_.DidPresentCompositorFrame(3, details);

  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 4);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 2);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 2);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    2);
  histogram_tester.ExpectTotalCount("CompositorLatency.Activation", 2);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 4);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SubmitCompositorFrameToPresentationCompositorFrame",
      4);
}

TEST_F(CompositorFrameReportingControllerTest, LongMainFrame2) {
  base::HistogramTester histogram_tester;
  viz::BeginFrameId current_id_1(1, 1);
  viz::BeginFrameArgs args_1 = SimulateBeginFrameArgs(current_id_1);

  viz::BeginFrameId current_id_2(1, 2);
  viz::BeginFrameArgs args_2 = SimulateBeginFrameArgs(current_id_2);

  viz::FrameTimingDetails details = {};
  reporting_controller_.WillBeginImplFrame(args_1);
  reporting_controller_.OnFinishImplFrame(current_id_1);
  reporting_controller_.WillBeginMainFrame(args_1);
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  reporting_controller_.DidSubmitCompositorFrame(1, current_id_1, current_id_1,
                                                 {});
  reporting_controller_.DidPresentCompositorFrame(1, details);

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

  // Second frame will not have the main frame update ready and will only submit
  // the Impl update
  reporting_controller_.WillBeginImplFrame(args_2);
  reporting_controller_.WillBeginMainFrame(args_2);
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.OnFinishImplFrame(current_id_2);
  reporting_controller_.DidSubmitCompositorFrame(2, current_id_2, current_id_1,
                                                 {});
  reporting_controller_.DidPresentCompositorFrame(2, details);

  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 2);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Activation", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 2);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SubmitCompositorFrameToPresentationCompositorFrame",
      2);
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

// If the presentation of the frame happens before deadline.
TEST_F(CompositorFrameReportingControllerTest, ReportingMissedDeadlineFrame1) {
  base::HistogramTester histogram_tester;

  reporting_controller_.WillBeginImplFrame(args_);
  reporting_controller_.OnFinishImplFrame(current_id_);
  reporting_controller_.WillBeginMainFrame(args_);
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  reporting_controller_.DidSubmitCompositorFrame(1, current_id_, current_id_,
                                                 {});
  viz::FrameTimingDetails details = {};
  details.presentation_feedback.timestamp =
      args_.frame_time + args_.interval * 1.5 -
      base::TimeDelta::FromMicroseconds(100);
  reporting_controller_.DidPresentCompositorFrame(1, details);

  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.TotalLatency", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedDeadlineFrame."
      "BeginImplFrameToSendBeginMainFrame",
      0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedDeadlineFrame.TotalLatency", 0);

  // Non-dropped cases.
  histogram_tester.ExpectBucketCount("CompositorLatency.Type", 0, 1);
  // Missed-deadline cases.
  histogram_tester.ExpectBucketCount("CompositorLatency.Type", 1, 0);
  // Dropped cases.
  histogram_tester.ExpectBucketCount("CompositorLatency.Type", 2, 0);
}

// If the presentation of the frame happens after deadline.
TEST_F(CompositorFrameReportingControllerTest, ReportingMissedDeadlineFrame2) {
  base::HistogramTester histogram_tester;

  reporting_controller_.WillBeginImplFrame(args_);
  reporting_controller_.OnFinishImplFrame(current_id_);
  reporting_controller_.WillBeginMainFrame(args_);
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  reporting_controller_.DidSubmitCompositorFrame(1, current_id_, current_id_,
                                                 {});
  viz::FrameTimingDetails details = {};
  details.presentation_feedback.timestamp =
      args_.frame_time + args_.interval * 1.5 +
      base::TimeDelta::FromMicroseconds(100);
  reporting_controller_.DidPresentCompositorFrame(1, details);

  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.TotalLatency", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedDeadlineFrame."
      "BeginImplFrameToSendBeginMainFrame",
      1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedDeadlineFrame.TotalLatency", 1);

  // Non-dropped cases.
  histogram_tester.ExpectBucketCount("CompositorLatency.Type", 0, 1);
  // Missed-deadline cases.
  histogram_tester.ExpectBucketCount("CompositorLatency.Type", 1, 1);
  // Dropped cases.
  histogram_tester.ExpectBucketCount("CompositorLatency.Type", 2, 0);
}

// Tests that EventLatency histograms are reported properly when a frame is
// presented to the user.
TEST_F(CompositorFrameReportingControllerTest,
       EventLatencyForPresentedFrameReported) {
  base::HistogramTester histogram_tester;

  const base::TimeTicks event_time = base::TimeTicks::Now();
  std::vector<EventMetrics> events_metrics = {
      {ui::ET_TOUCH_PRESSED, event_time, base::nullopt},
      {ui::ET_TOUCH_MOVED, event_time, base::nullopt},
      {ui::ET_TOUCH_MOVED, event_time, base::nullopt},
  };
  EXPECT_THAT(events_metrics, ::testing::Each(IsWhitelisted()));

  // Submit a compositor frame and notify CompositorFrameReporter of the events
  // affecting the frame.
  ++next_token_;
  SimulateSubmitCompositorFrame(*next_token_, {std::move(events_metrics), {}});

  // Present the submitted compositor frame to the user.
  const base::TimeTicks presentation_time = base::TimeTicks::Now();
  viz::FrameTimingDetails details;
  details.presentation_feedback.timestamp = presentation_time;
  reporting_controller_.DidPresentCompositorFrame(*next_token_, details);

  // Verify that EventLatency histograms are recorded.
  const int latency_ms = (presentation_time - event_time).InMicroseconds();
  histogram_tester.ExpectTotalCount("EventLatency.TouchPressed.TotalLatency",
                                    1);
  histogram_tester.ExpectTotalCount("EventLatency.TouchMoved.TotalLatency", 2);
  histogram_tester.ExpectBucketCount("EventLatency.TouchPressed.TotalLatency",
                                     latency_ms, 1);
  histogram_tester.ExpectBucketCount("EventLatency.TouchMoved.TotalLatency",
                                     latency_ms, 2);
}

// Tests that EventLatency histograms are reported properly for scroll events
// when a frame is presented to the user.
TEST_F(CompositorFrameReportingControllerTest,
       EventLatencyScrollForPresentedFrameReported) {
  base::HistogramTester histogram_tester;

  const base::TimeTicks event_time = base::TimeTicks::Now();
  std::vector<EventMetrics> events_metrics = {
      {ui::ET_GESTURE_SCROLL_BEGIN, event_time, ScrollInputType::kWheel},
      {ui::ET_GESTURE_SCROLL_UPDATE, event_time, ScrollInputType::kWheel},
      {ui::ET_GESTURE_SCROLL_UPDATE, event_time, ScrollInputType::kWheel},
  };
  EXPECT_THAT(events_metrics, ::testing::Each(IsWhitelisted()));

  // Submit a compositor frame and notify CompositorFrameReporter of the events
  // affecting the frame.
  ++next_token_;
  SimulateSubmitCompositorFrame(*next_token_, {std::move(events_metrics), {}});

  // Present the submitted compositor frame to the user.
  const base::TimeTicks presentation_time = base::TimeTicks::Now();
  viz::FrameTimingDetails details;
  details.presentation_feedback.timestamp = presentation_time;
  reporting_controller_.DidPresentCompositorFrame(*next_token_, details);

  // Verify that EventLatency histograms are recorded.
  const int latency_ms = (presentation_time - event_time).InMicroseconds();
  histogram_tester.ExpectTotalCount(
      "EventLatency.GestureScrollBegin.Wheel.TotalLatency", 1);
  histogram_tester.ExpectTotalCount(
      "EventLatency.GestureScrollUpdate.Wheel.TotalLatency", 2);
  histogram_tester.ExpectBucketCount(
      "EventLatency.GestureScrollBegin.Wheel.TotalLatency", latency_ms, 1);
  histogram_tester.ExpectBucketCount(
      "EventLatency.GestureScrollUpdate.Wheel.TotalLatency", latency_ms, 2);
}

// Tests that EventLatency histograms are not reported when the frame is dropped
// and not presented to the user.
TEST_F(CompositorFrameReportingControllerTest,
       EventLatencyForDidNotPresentFrameNotReported) {
  base::HistogramTester histogram_tester;

  const base::TimeTicks event_time = base::TimeTicks::Now();
  std::vector<EventMetrics> events_metrics = {
      {ui::ET_TOUCH_PRESSED, event_time, base::nullopt},
      {ui::ET_TOUCH_MOVED, event_time, base::nullopt},
      {ui::ET_TOUCH_MOVED, event_time, base::nullopt},
  };
  EXPECT_THAT(events_metrics, ::testing::Each(IsWhitelisted()));

  // Submit a compositor frame and notify CompositorFrameReporter of the events
  // affecting the frame.
  ++next_token_;
  SimulateSubmitCompositorFrame(*next_token_, {std::move(events_metrics), {}});

  // Submit another compositor frame.
  ++next_token_;
  IncrementCurrentId();
  SimulateSubmitCompositorFrame(*next_token_, {});

  // Present the second compositor frame to the uesr, dropping the first one.
  viz::FrameTimingDetails details;
  details.presentation_feedback.timestamp = base::TimeTicks::Now();
  reporting_controller_.DidPresentCompositorFrame(*next_token_, details);

  // Verify that no EventLatency histogram is recorded.
  histogram_tester.ExpectTotalCount("EventLatency.TouchPressed.TotalLatency",
                                    0);
  histogram_tester.ExpectTotalCount("EventLatency.TouchMoved.TotalLatency", 0);
}

}  // namespace
}  // namespace cc

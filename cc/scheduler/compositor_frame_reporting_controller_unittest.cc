// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/compositor_frame_reporting_controller.h"

#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
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
  CompositorFrameReportingControllerTest() : reporting_controller_(this) {}

  // The following functions simulate the actions that would
  // occur for each phase of the reporting controller.
  void SimulateBeginImplFrame() { reporting_controller_.WillBeginImplFrame(); }

  void SimulateBeginMainFrame() {
    if (!reporting_controller_.reporters()[CompositorFrameReportingController::
                                               PipelineStage::kBeginImplFrame])
      SimulateBeginImplFrame();
    CHECK(
        reporting_controller_.reporters()[CompositorFrameReportingController::
                                              PipelineStage::kBeginImplFrame]);
    reporting_controller_.WillBeginMainFrame();
  }

  void SimulateCommit() {
    if (!reporting_controller_.reporters()[CompositorFrameReportingController::
                                               PipelineStage::kBeginMainFrame])
      SimulateBeginMainFrame();
    CHECK(
        reporting_controller_.reporters()[CompositorFrameReportingController::
                                              PipelineStage::kBeginMainFrame]);
    reporting_controller_.WillCommit();
    reporting_controller_.DidCommit();
  }

  void SimulateActivate() {
    if (!reporting_controller_.reporters()
             [CompositorFrameReportingController::PipelineStage::kCommit])
      SimulateCommit();
    CHECK(reporting_controller_.reporters()
              [CompositorFrameReportingController::PipelineStage::kCommit]);
    reporting_controller_.WillActivate();
    reporting_controller_.DidActivate();
  }

  void SimulateSubmitCompositorFrame(uint32_t frame_token) {
    if (!reporting_controller_.reporters()
             [CompositorFrameReportingController::PipelineStage::kActivate])
      SimulateActivate();
    CHECK(reporting_controller_.reporters()
              [CompositorFrameReportingController::PipelineStage::kActivate]);
    reporting_controller_.DidSubmitCompositorFrame(frame_token);
  }

  void SimulatePresentCompositorFrame() {
    ++next_token_;
    SimulateSubmitCompositorFrame(*next_token_);
    reporting_controller_.DidPresentCompositorFrame(*next_token_,
                                                    base::TimeTicks::Now());
  }

 protected:
  TestCompositorFrameReportingController reporting_controller_;

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
  reporting_controller_.WillBeginImplFrame();
  EXPECT_EQ(1, reporting_controller_.ActiveReporters());

  // BF -> BF
  // Should replace previous reporter.
  reporting_controller_.WillBeginImplFrame();
  EXPECT_EQ(1, reporting_controller_.ActiveReporters());

  // BF -> BMF -> BF
  // Should add new reporter.
  reporting_controller_.WillBeginMainFrame();
  reporting_controller_.WillBeginImplFrame();
  EXPECT_EQ(2, reporting_controller_.ActiveReporters());

  // BF -> BMF -> BF -> Commit
  // Should stay same.
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  EXPECT_EQ(2, reporting_controller_.ActiveReporters());

  // BF -> BMF -> BF -> Commit -> BMF -> Activate -> Commit -> Activation
  // Having two reporters at Activate phase should delete the older one.
  reporting_controller_.WillBeginMainFrame();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  reporting_controller_.WillCommit();
  reporting_controller_.DidCommit();
  reporting_controller_.WillActivate();
  reporting_controller_.DidActivate();
  EXPECT_EQ(1, reporting_controller_.ActiveReporters());

  reporting_controller_.DidSubmitCompositorFrame(0);
  EXPECT_EQ(0, reporting_controller_.ActiveReporters());

  // 4 simultaneous reporters active.
  SimulateActivate();

  SimulateCommit();

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
  SimulateBeginImplFrame();

  // Submitting and Presenting the next reporter should be a missed.
  SimulatePresentCompositorFrame();

  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrame.BeginImplFrameToSendBeginMainFrame", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrame.SendBeginMainFrameToCommit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.MissedFrame.Commit", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrame.EndCommitToActivation", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.MissedFrame.Activation",
                                    1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrame.EndActivateToSubmitCompositorFrame", 1);

  // Other histograms should not be reported.
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 0);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 0);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    0);
  histogram_tester.ExpectTotalCount("CompositorLatency.Activation", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 0);

  // Submitting the next reporter will not be counted as missed.
  // In practice this submitted frame should be considered as missed because a
  // new BeginFrame would have been issued, which is the cause for this frame
  // submission.
  SimulatePresentCompositorFrame();
  // Other histograms should not be reported.
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

  // Missed frame histogram counts should not change.
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrame.BeginImplFrameToSendBeginMainFrame", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrame.SendBeginMainFrameToCommit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.MissedFrame.Commit", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrame.EndCommitToActivation", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.MissedFrame.Activation",
                                    1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.MissedFrame.EndActivateToSubmitCompositorFrame", 1);
}
}  // namespace
}  // namespace cc

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/frame_sequence_tracker.h"

#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

TEST(FrameSequenceMetricsTest, MergeMetrics) {
  // Create a metric with only a small number of frames. It shouldn't report any
  // metrics.
  FrameSequenceMetrics first(FrameSequenceTrackerType::kTouchScroll, nullptr,
                             nullptr);
  first.impl_throughput().frames_expected = 20;
  first.impl_throughput().frames_produced = 10;
  EXPECT_FALSE(first.HasEnoughDataForReporting());

  // Create a second metric with too few frames to report any metrics.
  auto second = std::make_unique<FrameSequenceMetrics>(
      FrameSequenceTrackerType::kTouchScroll, nullptr, nullptr);
  second->impl_throughput().frames_expected = 90;
  second->impl_throughput().frames_produced = 60;
  EXPECT_FALSE(second->HasEnoughDataForReporting());

  // Merge the two metrics. The result should have enough frames to report
  // metrics.
  first.Merge(std::move(second));
  EXPECT_TRUE(first.HasEnoughDataForReporting());
}

TEST(FrameSequenceMetricsTest, AllMetricsReported) {
  base::HistogramTester histograms;

  // Create a metric with enough frames on impl to be reported, but not enough
  // on main.
  FrameSequenceMetrics first(FrameSequenceTrackerType::kTouchScroll, nullptr,
                             nullptr);
  first.impl_throughput().frames_expected = 120;
  first.impl_throughput().frames_produced = 80;
  first.main_throughput().frames_expected = 20;
  first.main_throughput().frames_produced = 10;
  EXPECT_TRUE(first.HasEnoughDataForReporting());
  first.ReportMetrics();

  // The compositor-thread metric should be reported, but not the main-thread
  // metric.
  histograms.ExpectTotalCount(
      "Graphics.Smoothness.Throughput.CompositorThread.TouchScroll", 1u);
  histograms.ExpectTotalCount(
      "Graphics.Smoothness.Throughput.MainThread.TouchScroll", 0u);

  // There should still be data left over for the main-thread.
  EXPECT_TRUE(first.HasDataLeftForReporting());

  auto second = std::make_unique<FrameSequenceMetrics>(
      FrameSequenceTrackerType::kTouchScroll, nullptr, nullptr);
  second->impl_throughput().frames_expected = 110;
  second->impl_throughput().frames_produced = 100;
  second->main_throughput().frames_expected = 90;
  first.Merge(std::move(second));
  EXPECT_TRUE(first.HasEnoughDataForReporting());
  first.ReportMetrics();
  histograms.ExpectTotalCount(
      "Graphics.Smoothness.Throughput.CompositorThread.TouchScroll", 2u);
  histograms.ExpectTotalCount(
      "Graphics.Smoothness.Throughput.MainThread.TouchScroll", 1u);
  // All the metrics have now been reported. No data should be left over.
  EXPECT_FALSE(first.HasDataLeftForReporting());
}

}  // namespace cc

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/frame_sequence_metrics.h"

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/metrics/frame_sequence_tracker.h"
#include "cc/metrics/throughput_ukm_reporter.h"

namespace cc {

namespace {

// Avoid reporting any throughput metric for sequences that do not have a
// sufficient number of frames.
constexpr int kMinFramesForThroughputMetric = 100;

constexpr int kBuiltinSequenceNum =
    static_cast<int>(FrameSequenceTrackerType::kMaxType) + 1;
constexpr int kMaximumHistogramIndex = 3 * kBuiltinSequenceNum;

int GetIndexForMetric(FrameSequenceMetrics::ThreadType thread_type,
                      FrameSequenceTrackerType type) {
  if (thread_type == FrameSequenceMetrics::ThreadType::kMain)
    return static_cast<int>(type);
  if (thread_type == FrameSequenceMetrics::ThreadType::kCompositor)
    return static_cast<int>(type) + kBuiltinSequenceNum;
  return static_cast<int>(type) + 2 * kBuiltinSequenceNum;
}

std::string GetCheckerboardingHistogramName(FrameSequenceTrackerType type) {
  return base::StrCat(
      {"Graphics.Smoothness.Checkerboarding.",
       FrameSequenceTracker::GetFrameSequenceTrackerTypeName(type)});
}

std::string GetThroughputHistogramName(FrameSequenceTrackerType type,
                                       const char* thread_name) {
  return base::StrCat(
      {"Graphics.Smoothness.PercentDroppedFrames.", thread_name, ".",
       FrameSequenceTracker::GetFrameSequenceTrackerTypeName(type)});
}

std::string GetFrameSequenceLengthHistogramName(FrameSequenceTrackerType type) {
  return base::StrCat(
      {"Graphics.Smoothness.FrameSequenceLength.",
       FrameSequenceTracker::GetFrameSequenceTrackerTypeName(type)});
}

bool ShouldReportForAnimation(FrameSequenceTrackerType sequence_type,
                              FrameSequenceMetrics::ThreadType thread_type) {
  if (sequence_type == FrameSequenceTrackerType::kCompositorAnimation)
    return thread_type == FrameSequenceMetrics::ThreadType::kCompositor;

  if (sequence_type == FrameSequenceTrackerType::kMainThreadAnimation ||
      sequence_type == FrameSequenceTrackerType::kRAF)
    return thread_type == FrameSequenceMetrics::ThreadType::kMain;

  return false;
}

bool ShouldReportForInteraction(FrameSequenceMetrics* metrics,
                                FrameSequenceMetrics::ThreadType thread_type) {
  const auto sequence_type = metrics->type();

  // For touch/wheel scroll, the slower thread is the one we want to report. For
  // pinch-zoom, it's the compositor-thread.
  if (sequence_type == FrameSequenceTrackerType::kTouchScroll ||
      sequence_type == FrameSequenceTrackerType::kWheelScroll)
    return thread_type == metrics->GetEffectiveThread();

  if (sequence_type == FrameSequenceTrackerType::kPinchZoom)
    return thread_type == FrameSequenceMetrics::ThreadType::kCompositor;

  return false;
}

bool IsInteractionType(FrameSequenceTrackerType sequence_type) {
  return sequence_type == FrameSequenceTrackerType::kTouchScroll ||
         sequence_type == FrameSequenceTrackerType::kWheelScroll ||
         sequence_type == FrameSequenceTrackerType::kPinchZoom;
}

}  // namespace

FrameSequenceMetrics::FrameSequenceMetrics(FrameSequenceTrackerType type,
                                           ThroughputUkmReporter* ukm_reporter)
    : type_(type), throughput_ukm_reporter_(ukm_reporter) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "cc,benchmark", "FrameSequenceTracker", TRACE_ID_LOCAL(this), "name",
      FrameSequenceTracker::GetFrameSequenceTrackerTypeName(type_));
}

FrameSequenceMetrics::~FrameSequenceMetrics() {
  if (HasDataLeftForReporting()) {
    ReportMetrics();
  }
}

void FrameSequenceMetrics::SetScrollingThread(ThreadType scrolling_thread) {
  DCHECK(type_ == FrameSequenceTrackerType::kTouchScroll ||
         type_ == FrameSequenceTrackerType::kWheelScroll ||
         type_ == FrameSequenceTrackerType::kScrollbarScroll);
  DCHECK_EQ(scrolling_thread_, ThreadType::kUnknown);
  scrolling_thread_ = scrolling_thread;
}

void FrameSequenceMetrics::SetCustomReporter(CustomReporter custom_reporter) {
  DCHECK_EQ(FrameSequenceTrackerType::kCustom, type_);
  custom_reporter_ = std::move(custom_reporter);
}

FrameSequenceMetrics::ThreadType FrameSequenceMetrics::GetEffectiveThread()
    const {
  switch (type_) {
    case FrameSequenceTrackerType::kCompositorAnimation:
    case FrameSequenceTrackerType::kPinchZoom:
      return ThreadType::kCompositor;

    case FrameSequenceTrackerType::kMainThreadAnimation:
    case FrameSequenceTrackerType::kRAF:
    case FrameSequenceTrackerType::kVideo:
      return ThreadType::kMain;

    case FrameSequenceTrackerType::kTouchScroll:
    case FrameSequenceTrackerType::kScrollbarScroll:
    case FrameSequenceTrackerType::kWheelScroll:
      return scrolling_thread_;

    case FrameSequenceTrackerType::kUniversal:
      return ThreadType::kSlower;

    case FrameSequenceTrackerType::kCustom:
    case FrameSequenceTrackerType::kMaxType:
      NOTREACHED();
  }
  return ThreadType::kUnknown;
}

void FrameSequenceMetrics::Merge(
    std::unique_ptr<FrameSequenceMetrics> metrics) {
  DCHECK_EQ(type_, metrics->type_);
  DCHECK_EQ(GetEffectiveThread(), metrics->GetEffectiveThread());
  impl_throughput_.Merge(metrics->impl_throughput_);
  main_throughput_.Merge(metrics->main_throughput_);
  aggregated_throughput_.Merge(metrics->aggregated_throughput_);
  frames_checkerboarded_ += metrics->frames_checkerboarded_;

  // Reset the state of |metrics| before destroying it, so that it doesn't end
  // up reporting the metrics.
  metrics->impl_throughput_ = {};
  metrics->main_throughput_ = {};
  metrics->aggregated_throughput_ = {};
  metrics->frames_checkerboarded_ = 0;
}

bool FrameSequenceMetrics::HasEnoughDataForReporting() const {
  return impl_throughput_.frames_expected >= kMinFramesForThroughputMetric ||
         main_throughput_.frames_expected >= kMinFramesForThroughputMetric;
}

bool FrameSequenceMetrics::HasDataLeftForReporting() const {
  return impl_throughput_.frames_expected > 0 ||
         main_throughput_.frames_expected > 0;
}

void FrameSequenceMetrics::ComputeAggregatedThroughput() {
  // Whenever we are expecting and producing main frames, we are expecting and
  // producing impl frames as well. As an example, if we expect one main frame
  // to be produced, and when that main frame is presented, we are expecting 3
  // impl frames, then the number of expected frames is 3 for the aggregated
  // throughput.
  aggregated_throughput_.frames_expected = impl_throughput_.frames_expected;
  DCHECK_LE(aggregated_throughput_.frames_produced,
            aggregated_throughput_.frames_produced);
}

void FrameSequenceMetrics::ReportMetrics() {
  DCHECK_LE(impl_throughput_.frames_produced, impl_throughput_.frames_expected);
  DCHECK_LE(main_throughput_.frames_produced, main_throughput_.frames_expected);
  TRACE_EVENT_NESTABLE_ASYNC_END2(
      "cc,benchmark", "FrameSequenceTracker", TRACE_ID_LOCAL(this), "args",
      ThroughputData::ToTracedValue(impl_throughput_, main_throughput_),
      "checkerboard", frames_checkerboarded_);

  if (type_ == FrameSequenceTrackerType::kCustom) {
    DCHECK(!custom_reporter_.is_null());
    std::move(custom_reporter_).Run(std::move(main_throughput_));

    main_throughput_ = {};
    impl_throughput_ = {};
    aggregated_throughput_ = {};
    frames_checkerboarded_ = 0;
    return;
  }

  ComputeAggregatedThroughput();

  // Report the throughput metrics.
  base::Optional<int> impl_throughput_percent = ThroughputData::ReportHistogram(
      this, ThreadType::kCompositor,
      GetIndexForMetric(FrameSequenceMetrics::ThreadType::kCompositor, type_),
      impl_throughput_);
  base::Optional<int> main_throughput_percent = ThroughputData::ReportHistogram(
      this, ThreadType::kMain,
      GetIndexForMetric(FrameSequenceMetrics::ThreadType::kMain, type_),
      main_throughput_);

  // Report for the 'slower thread' for the metrics where it makes sense.
  bool should_report_slower_thread =
      IsInteractionType(type_) || type_ == FrameSequenceTrackerType::kUniversal;
  base::Optional<int> aggregated_throughput_percent;
  if (should_report_slower_thread) {
    aggregated_throughput_percent = ThroughputData::ReportHistogram(
        this, ThreadType::kSlower,
        GetIndexForMetric(FrameSequenceMetrics::ThreadType::kSlower, type_),
        aggregated_throughput_);
    if (aggregated_throughput_percent.has_value() && throughput_ukm_reporter_) {
      throughput_ukm_reporter_->ReportThroughputUkm(
          aggregated_throughput_percent, impl_throughput_percent,
          main_throughput_percent, type_);
    }
  }

  // Report for the 'scrolling thread' for the scrolling interactions.
  if (scrolling_thread_ != ThreadType::kUnknown) {
    base::Optional<int> scrolling_thread_throughput;
    switch (scrolling_thread_) {
      case ThreadType::kCompositor:
        scrolling_thread_throughput = impl_throughput_percent;
        break;
      case ThreadType::kMain:
        scrolling_thread_throughput = main_throughput_percent;
        break;
      case ThreadType::kSlower:
      case ThreadType::kUnknown:
        NOTREACHED();
        break;
    }
    if (scrolling_thread_throughput.has_value()) {
      // It's OK to use the UMA histogram in the following code while still
      // using |GetThroughputHistogramName()| to get the name of the metric,
      // since the input-params to the function never change at runtime.
      if (type_ == FrameSequenceTrackerType::kWheelScroll) {
        UMA_HISTOGRAM_PERCENTAGE(
            GetThroughputHistogramName(FrameSequenceTrackerType::kWheelScroll,
                                       "ScrollingThread"),
            scrolling_thread_throughput.value());
      } else if (type_ == FrameSequenceTrackerType::kTouchScroll) {
        UMA_HISTOGRAM_PERCENTAGE(
            GetThroughputHistogramName(FrameSequenceTrackerType::kTouchScroll,
                                       "ScrollingThread"),
            scrolling_thread_throughput.value());
      } else {
        DCHECK_EQ(type_, FrameSequenceTrackerType::kScrollbarScroll);
        UMA_HISTOGRAM_PERCENTAGE(
            GetThroughputHistogramName(
                FrameSequenceTrackerType::kScrollbarScroll, "ScrollingThread"),
            scrolling_thread_throughput.value());
      }
    }
  }

  // Report the checkerboarding metrics.
  if (impl_throughput_.frames_expected >= kMinFramesForThroughputMetric) {
    const int checkerboarding_percent = static_cast<int>(
        100 * frames_checkerboarded_ / impl_throughput_.frames_expected);
    STATIC_HISTOGRAM_POINTER_GROUP(
        GetCheckerboardingHistogramName(type_), static_cast<int>(type_),
        static_cast<int>(FrameSequenceTrackerType::kMaxType),
        Add(checkerboarding_percent),
        base::LinearHistogram::FactoryGet(
            GetCheckerboardingHistogramName(type_), 1, 100, 101,
            base::HistogramBase::kUmaTargetedHistogramFlag));
    frames_checkerboarded_ = 0;
  }

  // Reset the metrics that reach reporting threshold.
  if (impl_throughput_.frames_expected >= kMinFramesForThroughputMetric)
    impl_throughput_ = {};
  if (main_throughput_.frames_expected >= kMinFramesForThroughputMetric)
    main_throughput_ = {};
  if (aggregated_throughput_percent.has_value())
    aggregated_throughput_ = {};
}

base::Optional<int> FrameSequenceMetrics::ThroughputData::ReportHistogram(
    FrameSequenceMetrics* metrics,
    ThreadType thread_type,
    int metric_index,
    const ThroughputData& data) {
  const auto sequence_type = metrics->type();
  DCHECK_LT(sequence_type, FrameSequenceTrackerType::kMaxType);

  STATIC_HISTOGRAM_POINTER_GROUP(
      GetFrameSequenceLengthHistogramName(sequence_type),
      static_cast<int>(sequence_type),
      static_cast<int>(FrameSequenceTrackerType::kMaxType),
      Add(data.frames_expected),
      base::Histogram::FactoryGet(
          GetFrameSequenceLengthHistogramName(sequence_type), 1, 1000, 50,
          base::HistogramBase::kUmaTargetedHistogramFlag));

  if (data.frames_expected < kMinFramesForThroughputMetric)
    return base::nullopt;

  // Throughput means the percent of frames that was expected to show on the
  // screen but didn't. In other words, the lower the throughput is, the
  // smoother user experience.
  const int percent =
      std::ceil(100 * (data.frames_expected - data.frames_produced) /
                static_cast<double>(data.frames_expected));

  const bool is_animation =
      ShouldReportForAnimation(sequence_type, thread_type);
  const bool is_interaction = ShouldReportForInteraction(metrics, thread_type);

  ThroughputUkmReporter* const ukm_reporter = metrics->ukm_reporter();

  if (is_animation) {
    UMA_HISTOGRAM_PERCENTAGE(
        "Graphics.Smoothness.PercentDroppedFrames.AllAnimations", percent);
    if (ukm_reporter) {
      ukm_reporter->ReportAggregateThroughput(AggregationType::kAllAnimations,
                                              percent);
    }
  }

  if (is_interaction) {
    UMA_HISTOGRAM_PERCENTAGE(
        "Graphics.Smoothness.PercentDroppedFrames.AllInteractions", percent);
    if (ukm_reporter) {
      ukm_reporter->ReportAggregateThroughput(AggregationType::kAllInteractions,
                                              percent);
    }
  }

  if (is_animation || is_interaction) {
    UMA_HISTOGRAM_PERCENTAGE(
        "Graphics.Smoothness.PercentDroppedFrames.AllSequences", percent);
    if (ukm_reporter) {
      ukm_reporter->ReportAggregateThroughput(AggregationType::kAllSequences,
                                              percent);
    }
  }

  if (!is_animation && !IsInteractionType(sequence_type) &&
      sequence_type != FrameSequenceTrackerType::kUniversal &&
      sequence_type != FrameSequenceTrackerType::kVideo) {
    return base::nullopt;
  }

  const char* thread_name =
      thread_type == ThreadType::kCompositor
          ? "CompositorThread"
          : thread_type == ThreadType::kMain ? "MainThread" : "SlowerThread";
  STATIC_HISTOGRAM_POINTER_GROUP(
      GetThroughputHistogramName(sequence_type, thread_name), metric_index,
      kMaximumHistogramIndex, Add(percent),
      base::LinearHistogram::FactoryGet(
          GetThroughputHistogramName(sequence_type, thread_name), 1, 100, 101,
          base::HistogramBase::kUmaTargetedHistogramFlag));
  return percent;
}

}  // namespace cc

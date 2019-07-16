// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/text_fragment_anchor_metrics.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

TextFragmentAnchorMetrics::TextFragmentAnchorMetrics(Document* document)
    : document_(document) {}

void TextFragmentAnchorMetrics::DidCreateAnchor(int selector_count) {
  UseCounter::Count(document_, WebFeature::kTextFragmentAnchor);
  create_time_ = base::TimeTicks::Now();
  selector_count_ = selector_count;
}

void TextFragmentAnchorMetrics::DidFindMatch() {
  match_count_++;
}

void TextFragmentAnchorMetrics::ResetMatchCount() {
  match_count_ = 0;
}

void TextFragmentAnchorMetrics::DidFindAmbiguousMatch() {
  ambiguous_match_ = true;
}

void TextFragmentAnchorMetrics::ScrollCancelled() {
  scroll_cancelled_ = true;
}

void TextFragmentAnchorMetrics::DidScroll() {
  if (first_scroll_into_view_time_.is_null())
    first_scroll_into_view_time_ = base::TimeTicks::Now();
}

void TextFragmentAnchorMetrics::DidNonZeroScroll() {
  did_non_zero_scroll_ = true;
}

void TextFragmentAnchorMetrics::ReportMetrics() {
#ifndef NDEBUG
  DCHECK(!metrics_reported_);
#endif
  DCHECK(selector_count_);
  DCHECK(match_count_ <= selector_count_);

  if (match_count_ > 0) {
    UseCounter::Count(document_, WebFeature::kTextFragmentAnchorMatchFound);
  }

  UMA_HISTOGRAM_COUNTS_100("TextFragmentAnchor.SelectorCount", selector_count_);
  TRACE_EVENT_INSTANT1("blink", "TextFragmentAnchorMetrics::ReportMetrics",
                       TRACE_EVENT_SCOPE_THREAD, "selector_count",
                       selector_count_);

  const int match_rate_percent =
      static_cast<int>(100 * ((match_count_ + 0.0) / selector_count_));
  UMA_HISTOGRAM_PERCENTAGE("TextFragmentAnchor.MatchRate", match_rate_percent);
  TRACE_EVENT_INSTANT1("blink", "TextFragmentAnchorMetrics::ReportMetrics",
                       TRACE_EVENT_SCOPE_THREAD, "match_rate",
                       match_rate_percent);

  UMA_HISTOGRAM_BOOLEAN("TextFragmentAnchor.AmbiguousMatch", ambiguous_match_);
  TRACE_EVENT_INSTANT1("blink", "TextFragmentAnchorMetrics::ReportMetrics",
                       TRACE_EVENT_SCOPE_THREAD, "ambiguous_match",
                       ambiguous_match_);

  UMA_HISTOGRAM_BOOLEAN("TextFragmentAnchor.ScrollCancelled",
                        scroll_cancelled_);
  TRACE_EVENT_INSTANT1("blink", "TextFragmentAnchorMetrics::ReportMetrics",
                       TRACE_EVENT_SCOPE_THREAD, "scroll_cancelled",
                       scroll_cancelled_);

  if (first_scroll_into_view_time_ > create_time_) {
    UMA_HISTOGRAM_BOOLEAN("TextFragmentAnchor.DidScrollIntoView",
                          did_non_zero_scroll_);
    TRACE_EVENT_INSTANT1("blink", "TextFragmentAnchorMetrics::ReportMetrics",
                         TRACE_EVENT_SCOPE_THREAD, "did_scroll_into_view",
                         did_non_zero_scroll_);

    base::TimeDelta time_to_scroll_into_view(first_scroll_into_view_time_ -
                                             create_time_);
    UMA_HISTOGRAM_TIMES("TextFragmentAnchor.TimeToScrollIntoView",
                        time_to_scroll_into_view);
    TRACE_EVENT_INSTANT1("blink", "TextFragmentAnchorMetrics::ReportMetrics",
                         TRACE_EVENT_SCOPE_THREAD, "time_to_scroll_into_view",
                         time_to_scroll_into_view.InMilliseconds());
  }

#ifndef NDEBUG
  metrics_reported_ = true;
#endif
}

void TextFragmentAnchorMetrics::Trace(blink::Visitor* visitor) {
  visitor->Trace(document_);
}

}  // namespace blink

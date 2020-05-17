// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/text_fragment_anchor_metrics.h"

#include "base/check.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

namespace {

const size_t kMaxTraceEventStringLength = 1000;

}  // namespace

TextFragmentAnchorMetrics::TextFragmentAnchorMetrics(Document* document)
    : document_(document) {}

void TextFragmentAnchorMetrics::DidCreateAnchor(int selector_count,
                                                int directive_length) {
  UseCounter::Count(document_, WebFeature::kTextFragmentAnchor);
  create_time_ = base::TimeTicks::Now();
  selector_count_ = selector_count;
  directive_length_ = directive_length;
}

void TextFragmentAnchorMetrics::DidFindMatch(Match match) {
  matches_.push_back(match);
}

void TextFragmentAnchorMetrics::ResetMatchCount() {
  matches_.clear();
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
  DCHECK(matches_.size() <= selector_count_);

  if (matches_.size() > 0) {
    UseCounter::Count(document_, WebFeature::kTextFragmentAnchorMatchFound);
  }

  UMA_HISTOGRAM_COUNTS_100("TextFragmentAnchor.SelectorCount", selector_count_);
  TRACE_EVENT_INSTANT1("blink", "TextFragmentAnchorMetrics::ReportMetrics",
                       TRACE_EVENT_SCOPE_THREAD, "selector_count",
                       selector_count_);

  UMA_HISTOGRAM_COUNTS_1000("TextFragmentAnchor.DirectiveLength",
                            directive_length_);
  TRACE_EVENT_INSTANT1("blink", "TextFragmentAnchorMetrics::ReportMetrics",
                       TRACE_EVENT_SCOPE_THREAD, "directive_length",
                       directive_length_);

  const int match_rate_percent =
      static_cast<int>(100 * ((matches_.size() + 0.0) / selector_count_));
  UMA_HISTOGRAM_PERCENTAGE("TextFragmentAnchor.MatchRate", match_rate_percent);
  TRACE_EVENT_INSTANT1("blink", "TextFragmentAnchorMetrics::ReportMetrics",
                       TRACE_EVENT_SCOPE_THREAD, "match_rate",
                       match_rate_percent);

  for (const Match& match : matches_) {
    TRACE_EVENT_INSTANT2(
        "blink", "TextFragmentAnchorMetrics::ReportMetrics",
        TRACE_EVENT_SCOPE_THREAD, "match_found",
        match.text.Utf8().substr(0, kMaxTraceEventStringLength), "match_length",
        match.text.length());

    if (match.selector.Type() == TextFragmentSelector::kExact) {
      UMA_HISTOGRAM_COUNTS_1000("TextFragmentAnchor.ExactTextLength",
                                match.text.length());
      TRACE_EVENT_INSTANT1("blink", "TextFragmentAnchorMetrics::ReportMetrics",
                           TRACE_EVENT_SCOPE_THREAD, "exact_text_length",
                           match.text.length());
    } else if (match.selector.Type() == TextFragmentSelector::kRange) {
      UMA_HISTOGRAM_COUNTS_1000("TextFragmentAnchor.RangeMatchLength",
                                match.text.length());
      TRACE_EVENT_INSTANT1("blink", "TextFragmentAnchorMetrics::ReportMetrics",
                           TRACE_EVENT_SCOPE_THREAD, "range_match_length",
                           match.text.length());

      UMA_HISTOGRAM_COUNTS_1000("TextFragmentAnchor.StartTextLength",
                                match.selector.Start().length());
      TRACE_EVENT_INSTANT1("blink", "TextFragmentAnchorMetrics::ReportMetrics",
                           TRACE_EVENT_SCOPE_THREAD, "start_text_length",
                           match.selector.Start().length());

      UMA_HISTOGRAM_COUNTS_1000("TextFragmentAnchor.EndTextLength",
                                match.selector.End().length());
      TRACE_EVENT_INSTANT1("blink", "TextFragmentAnchorMetrics::ReportMetrics",
                           TRACE_EVENT_SCOPE_THREAD, "end_text_length",
                           match.selector.End().length());
    }

    UMA_HISTOGRAM_ENUMERATION("TextFragmentAnchor.Parameters",
                              GetParametersForMatch(match));
  }

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

void TextFragmentAnchorMetrics::Dismissed() {
  // We report Dismissed separately from ReportMetrics as it may or may not
  // get called in the lifetime of the TextFragmentAnchor.
  UseCounter::Count(document_, WebFeature::kTextFragmentAnchorTapToDismiss);
  TRACE_EVENT_INSTANT0("blink", "TextFragmentAnchorMetrics::Dismissed",
                       TRACE_EVENT_SCOPE_THREAD);
}

void TextFragmentAnchorMetrics::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
}

TextFragmentAnchorMetrics::TextFragmentAnchorParameters
TextFragmentAnchorMetrics::GetParametersForMatch(const Match& match) {
  TextFragmentAnchorParameters parameters =
      TextFragmentAnchorParameters::kUnknown;

  if (match.selector.Type() == TextFragmentSelector::SelectorType::kExact) {
    if (match.selector.Prefix().length() && match.selector.Suffix().length())
      parameters = TextFragmentAnchorParameters::kExactTextWithContext;
    else if (match.selector.Prefix().length())
      parameters = TextFragmentAnchorParameters::kExactTextWithPrefix;
    else if (match.selector.Suffix().length())
      parameters = TextFragmentAnchorParameters::kExactTextWithSuffix;
    else
      parameters = TextFragmentAnchorParameters::kExactText;
  } else if (match.selector.Type() ==
             TextFragmentSelector::SelectorType::kRange) {
    if (match.selector.Prefix().length() && match.selector.Suffix().length())
      parameters = TextFragmentAnchorParameters::kTextRangeWithContext;
    else if (match.selector.Prefix().length())
      parameters = TextFragmentAnchorParameters::kTextRangeWithPrefix;
    else if (match.selector.Suffix().length())
      parameters = TextFragmentAnchorParameters::kTextRangeWithSuffix;
    else
      parameters = TextFragmentAnchorParameters::kTextRange;
  }

  return parameters;
}

}  // namespace blink

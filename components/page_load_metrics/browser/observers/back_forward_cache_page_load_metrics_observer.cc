// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/back_forward_cache_page_load_metrics_observer.h"

#include "components/page_load_metrics/browser/page_load_metrics_util.h"

namespace internal {

const char kHistogramFirstPaintAfterBackForwardCacheRestore[] =
    "PageLoad.PaintTiming.NavigationToFirstPaint.AfterBackForwardCacheRestore";
const char kHistogramFirstInputDelayAfterBackForwardCacheRestore[] =
    "PageLoad.InteractiveTiming.FirstInputDelay.AfterBackForwardCacheRestore";
extern const char
    kHistogramCumulativeShiftScoreMainFrameAfterBackForwardCacheRestore[] =
        "PageLoad.LayoutInstability.CumulativeShiftScore.MainFrame."
        "AfterBackForwardCacheRestore";
extern const char kHistogramCumulativeShiftScoreAfterBackForwardCacheRestore[] =
    "PageLoad.LayoutInstability.CumulativeShiftScore."
    "AfterBackForwardCacheRestore";

}  // namespace internal

BackForwardCachePageLoadMetricsObserver::
    BackForwardCachePageLoadMetricsObserver() = default;

BackForwardCachePageLoadMetricsObserver::
    ~BackForwardCachePageLoadMetricsObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
BackForwardCachePageLoadMetricsObserver::OnEnterBackForwardCache(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  in_back_forward_cache_ = true;
  DumpLayoutShiftScoreAfterBackForwardCacheRestore(timing);
  return CONTINUE_OBSERVING;
}

void BackForwardCachePageLoadMetricsObserver::OnRestoreFromBackForwardCache(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  in_back_forward_cache_ = false;
}

void BackForwardCachePageLoadMetricsObserver::
    OnFirstPaintAfterBackForwardCacheRestoreInPage(
        const page_load_metrics::mojom::BackForwardCacheTiming& timing,
        size_t index) {
  auto first_paint = timing.first_paint_after_back_forward_cache_restore;
  DCHECK(!first_paint.is_zero());
  if (page_load_metrics::
          WasStartedInForegroundOptionalEventInForegroundAfterBackForwardCacheRestore(
              first_paint, GetDelegate(), index)) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramFirstPaintAfterBackForwardCacheRestore,
        first_paint);
  }
}

void BackForwardCachePageLoadMetricsObserver::
    OnFirstInputAfterBackForwardCacheRestoreInPage(
        const page_load_metrics::mojom::BackForwardCacheTiming& timing,
        size_t index) {
  auto first_input_delay =
      timing.first_input_delay_after_back_forward_cache_restore;
  DCHECK(first_input_delay.has_value());
  if (page_load_metrics::
          WasStartedInForegroundOptionalEventInForegroundAfterBackForwardCacheRestore(
              first_input_delay, GetDelegate(), index)) {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        internal::kHistogramFirstInputDelayAfterBackForwardCacheRestore,
        *first_input_delay, base::TimeDelta::FromMilliseconds(1),
        base::TimeDelta::FromSeconds(60), 50);
  }
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
BackForwardCachePageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  OnComplete(timing);
  return STOP_OBSERVING;
}

void BackForwardCachePageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // If the page is in the back-forward cache and OnComplete is called, the page
  // is evicted from the cache. Do not dump CLS here as we have already recorded
  // it in OnEnterBackForwardCache.
  if (in_back_forward_cache_)
    return;

  DumpLayoutShiftScoreAfterBackForwardCacheRestore(timing);
}

void BackForwardCachePageLoadMetricsObserver::
    DumpLayoutShiftScoreAfterBackForwardCacheRestore(
        const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!last_main_frame_layout_shift_score_.has_value()) {
    DCHECK(!last_layout_shift_score_.has_value());
    last_main_frame_layout_shift_score_ =
        GetDelegate().GetMainFrameRenderData().layout_shift_score;
    last_layout_shift_score_ =
        GetDelegate().GetPageRenderData().layout_shift_score;

    // When DumpLayoutShiftScore is called first time, the page has not been in
    // back-forward cache. The scores not after the page is restored from back-
    // forward cache are recorded in other observers like
    // UkmPageLoadMetricsObserver.
    return;
  }

  double layout_main_frame_shift_score =
      GetDelegate().GetMainFrameRenderData().layout_shift_score -
      last_main_frame_layout_shift_score_.value();
  DCHECK_GE(layout_main_frame_shift_score, 0);
  double layout_shift_score =
      GetDelegate().GetPageRenderData().layout_shift_score -
      last_layout_shift_score_.value();
  DCHECK_GE(layout_shift_score, 0);

  UMA_HISTOGRAM_COUNTS_100(
      internal::
          kHistogramCumulativeShiftScoreMainFrameAfterBackForwardCacheRestore,
      page_load_metrics::LayoutShiftUmaValue(layout_main_frame_shift_score));
  UMA_HISTOGRAM_COUNTS_100(
      internal::kHistogramCumulativeShiftScoreAfterBackForwardCacheRestore,
      page_load_metrics::LayoutShiftUmaValue(layout_shift_score));

  last_main_frame_layout_shift_score_ =
      GetDelegate().GetMainFrameRenderData().layout_shift_score;
  last_layout_shift_score_ =
      GetDelegate().GetPageRenderData().layout_shift_score;

  // TODO(hajimehoshi): Add UKM
}

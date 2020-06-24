// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_BACK_FORWARD_CACHE_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_BACK_FORWARD_CACHE_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace internal {

extern const char kHistogramFirstPaintAfterBackForwardCacheRestore[];
extern const char kHistogramFirstInputDelayAfterBackForwardCacheRestore[];

}  // namespace internal

class BackForwardCachePageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  BackForwardCachePageLoadMetricsObserver();
  ~BackForwardCachePageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  page_load_metrics::PageLoadMetricsObserver::ObservePolicy
  OnEnterBackForwardCache(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstPaintAfterBackForwardCacheRestoreInPage(
      const page_load_metrics::mojom::BackForwardCacheTiming& timing,
      size_t index) override;
  void OnFirstInputAfterBackForwardCacheRestoreInPage(
      const page_load_metrics::mojom::BackForwardCacheTiming& timing,
      size_t index) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BackForwardCachePageLoadMetricsObserver);
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_BACK_FORWARD_CACHE_PAGE_LOAD_METRICS_OBSERVER_H_

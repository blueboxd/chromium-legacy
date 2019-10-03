// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SUBRESOURCE_LOADING_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SUBRESOURCE_LOADING_PAGE_LOAD_METRICS_OBSERVER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

// Records metrics related to loading of subresources on a page.
class SubresourceLoadingPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  SubresourceLoadingPageLoadMetricsObserver();
  ~SubresourceLoadingPageLoadMetricsObserver() override;

 private:
  void RecordMetrics();

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy OnHidden(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnResourceDataUseObserved(
      content::RenderFrameHost* rfh,
      const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
          resources) override;

  size_t loaded_css_js_from_cache_before_fcp_ = 0;
  size_t loaded_css_js_from_network_before_fcp_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(SubresourceLoadingPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SUBRESOURCE_LOADING_PAGE_LOAD_METRICS_OBSERVER_H_

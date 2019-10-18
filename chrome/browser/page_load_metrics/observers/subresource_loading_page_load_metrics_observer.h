// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SUBRESOURCE_LOADING_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SUBRESOURCE_LOADING_PAGE_LOAD_METRICS_OBSERVER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "url/gurl.h"

// Records metrics related to loading of subresources on a page.
class SubresourceLoadingPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  SubresourceLoadingPageLoadMetricsObserver();
  ~SubresourceLoadingPageLoadMetricsObserver() override;

 protected:
  // Used as a callback for history service query results. Protected for
  // testing.
  void OnOriginLastVisitResult(history::HistoryLastVisitToHostResult result);

 private:
  void RecordMetrics();

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
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

  // The time that the navigation started. Used to timebox the history service
  // query on commit.
  base::Time navigation_start_;

  size_t loaded_css_js_from_cache_before_fcp_ = 0;
  size_t loaded_css_js_from_network_before_fcp_ = 0;

  // The minimum number of days since the last visit, as reported by
  // HistoryService, to any origin in the redirect chain. Set to -1 if there is
  // a response from the history service but was no previous visit.
  base::Optional<int> min_days_since_last_visit_to_origin_;

  // Task tracker for calls for the history service.
  base::CancelableTaskTracker task_tracker_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SubresourceLoadingPageLoadMetricsObserver> weak_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(SubresourceLoadingPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SUBRESOURCE_LOADING_PAGE_LOAD_METRICS_OBSERVER_H_

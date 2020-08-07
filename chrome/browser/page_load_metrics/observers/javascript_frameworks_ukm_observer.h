// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_JAVASCRIPT_FRAMEWORKS_UKM_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_JAVASCRIPT_FRAMEWORKS_UKM_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

// If URL-Keyed-Metrics (UKM) is enabled in the system, this is used to
// populate it with JavaScript framework-related page-load metrics.
class JavascriptFrameworksUkmObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  JavascriptFrameworksUkmObserver();
  ~JavascriptFrameworksUkmObserver() override;

  void OnLoadingBehaviorObserved(content::RenderFrameHost* rfh,
                                 int behavior_flag) override;

 private:
  bool nextjs_detected = false;
  void RecordNextJS();

  DISALLOW_COPY_AND_ASSIGN(JavascriptFrameworksUkmObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_JAVASCRIPT_FRAMEWORKS_UKM_OBSERVER_H_

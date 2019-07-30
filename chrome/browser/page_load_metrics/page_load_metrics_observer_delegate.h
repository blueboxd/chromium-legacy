// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_OBSERVER_DELEGATE_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_OBSERVER_DELEGATE_H_

#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/resource_tracker.h"
#include "chrome/browser/scoped_visibility_tracker.h"
#include "chrome/common/page_load_metrics/page_end_reason.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace content {
class WebContents;
}  // namespace content

namespace page_load_metrics {

namespace mojom {
class PageLoadMetadata;
}  // namespace mojom

struct UserInitiatedInfo;
struct PageRenderData;

// This class tracks global state for the page load that should be accessible
// from any PageLoadMetricsObserver.
class PageLoadMetricsObserverDelegate {
 public:
  virtual content::WebContents* GetWebContents() const = 0;
  virtual base::TimeTicks GetNavigationStart() const = 0;
  virtual const base::Optional<base::TimeDelta>& GetFirstBackgroundTime()
      const = 0;
  virtual const base::Optional<base::TimeDelta>& GetFirstForegroundTime()
      const = 0;
  virtual bool StartedInForeground() const = 0;
  virtual const UserInitiatedInfo& GetUserInitiatedInfo() const = 0;
  virtual const GURL& GetUrl() const = 0;
  virtual const GURL& GetStartUrl() const = 0;
  virtual bool DidCommit() const = 0;
  virtual PageEndReason GetPageEndReason() const = 0;
  virtual const UserInitiatedInfo& GetPageEndUserInitiatedInfo() const = 0;
  virtual base::Optional<base::TimeDelta> GetPageEndTime() const = 0;
  virtual const mojom::PageLoadMetadata& GetMainFrameMetadata() const = 0;
  virtual const mojom::PageLoadMetadata& GetSubframeMetadata() const = 0;
  virtual const PageRenderData& GetPageRenderData() const = 0;
  virtual const PageRenderData& GetMainFrameRenderData() const = 0;
  virtual const ScopedVisibilityTracker& GetVisibilityTracker() const = 0;
  virtual const ResourceTracker& GetResourceTracker() const = 0;
  virtual ukm::SourceId GetSourceId() const = 0;
};

}  // namespace page_load_metrics

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_OBSERVER_DELEGATE_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_THIRD_PARTY_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_THIRD_PARTY_METRICS_OBSERVER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "components/page_load_metrics/browser/observers/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "url/gurl.h"
#include "url/origin.h"

// Records metrics about third-party storage accesses to a page.
class ThirdPartyMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  enum class AccessType {
    kCookieRead,
    kCookieWrite,
    kLocalStorage,
    kSessionStorage,
    kFileSystem,
    kIndexedDb,
    kCacheStorage,
    kUnknown,
  };

  ThirdPartyMetricsObserver();
  ~ThirdPartyMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnLoadedResource(const page_load_metrics::ExtraRequestCompleteInfo&
                            extra_request_complete_info) override;
  void OnCookiesRead(const GURL& url,
                     const GURL& first_party_url,
                     const net::CookieList& cookie_list,
                     bool blocked_by_policy) override;
  void OnCookieChange(const GURL& url,
                      const GURL& first_party_url,
                      const net::CanonicalCookie& cookie,
                      bool blocked_by_policy) override;
  void OnStorageAccessed(const GURL& url,
                         const GURL& first_party_url,
                         bool blocked_by_policy,
                         page_load_metrics::StorageType storage_type) override;
  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void OnTimingUpdate(
      content::RenderFrameHost* subframe_rfh,
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 private:
  struct AccessedTypes {
    explicit AccessedTypes(AccessType access_type);
    bool cookie_read = false;
    bool cookie_write = false;
    bool local_storage = false;
    bool session_storage = false;
  };

  void OnCookieOrStorageAccess(const GURL& url,
                               const GURL& first_party_url,
                               bool blocked_by_policy,
                               AccessType access_type);
  void RecordMetrics(
      const page_load_metrics::mojom::PageLoadTiming& main_frame_timing);

  // Records feature usage for |access_type| with use counters.
  void RecordStorageUseCounter(AccessType accesse_type);

  AccessType StorageTypeToAccessType(
      page_load_metrics::StorageType storage_type);

  // A map of third parties that have read or written cookies, or have
  // accessed local storage or session storage on this page.
  //
  // A third party document.cookie / window.localStorage /
  // window.sessionStorage happens when the context's scheme://eTLD+1
  // differs from the main frame's. A third party resource request happens
  // when the URL request's scheme://eTLD+1 differs from the main frame's.
  // For URLs which have no registrable domain, the hostname is used
  // instead.
  std::map<GURL, AccessedTypes> third_party_accessed_types_;

  // A set of RenderFrameHosts that we've recorded timing data for. The
  // RenderFrameHosts are later removed when they navigate again or are deleted.
  std::set<content::RenderFrameHost*> recorded_frames_;

  // If the page has any blocked_by_policy cookie or DOM storage access (e.g.,
  // block third-party cookies is enabled) then we don't want to record any
  // metrics for the page.
  bool should_record_metrics_ = true;

  // True if this page loaded a third-party font.
  bool third_party_font_loaded_ = false;

  page_load_metrics::LargestContentfulPaintHandler
      largest_contentful_paint_handler_;

  DISALLOW_COPY_AND_ASSIGN(ThirdPartyMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_THIRD_PARTY_METRICS_OBSERVER_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_THIRD_PARTY_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_THIRD_PARTY_METRICS_OBSERVER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "net/cookies/canonical_cookie.h"
#include "url/gurl.h"
#include "url/origin.h"

// Records metrics about third-party storage accesses to a page.
class ThirdPartyMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  ThirdPartyMetricsObserver();
  ~ThirdPartyMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;
  void OnComplete(const page_load_metrics::mojom::PageLoadTiming& timing,
                  const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnCookiesRead(const GURL& url,
                     const GURL& first_party_url,
                     const net::CookieList& cookie_list,
                     bool blocked_by_policy) override;
  void OnCookieChange(const GURL& url,
                      const GURL& first_party_url,
                      const net::CanonicalCookie& cookie,
                      bool blocked_by_policy) override;

 private:
  enum class AccessType { kRead, kWrite };

  struct CookieAccessTypes {
    explicit CookieAccessTypes(AccessType access_type);
    bool read = false;
    bool write = false;
  };

  void OnCookieAccess(const GURL& url,
                      const GURL& first_party_url,
                      bool blocked_by_policy,
                      AccessType access_type);
  void RecordMetrics();

  // A map of third parties that have read or written cookies on this page. A
  // third party document.cookie access happens when the context's registrable
  // domain differs from the main frame's. A third party resource request
  // happens when the URL request's registrable domain differs from the main
  // frame's. URLs which have no registrable domain are not considered third
  // party.
  std::map<std::string, CookieAccessTypes> third_party_access_types_;

  // If the page has any blocked_by_policy cookie reads or writes (e.g., block
  // third-party cookies is enabled) then we don't want to record any cookie
  // metrics for the page.
  bool page_has_blocked_cookies_ = false;

  DISALLOW_COPY_AND_ASSIGN(ThirdPartyMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_THIRD_PARTY_METRICS_OBSERVER_H_

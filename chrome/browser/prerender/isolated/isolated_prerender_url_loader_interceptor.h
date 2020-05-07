// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_URL_LOADER_INTERCEPTOR_H_
#define CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_URL_LOADER_INTERCEPTOR_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/availability/availability_prober.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_tab_helper.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

class PrefetchedMainframeResponseContainer;

// Intercepts prerender navigations that are eligible to be isolated.
class IsolatedPrerenderURLLoaderInterceptor
    : public content::URLLoaderRequestInterceptor,
      public AvailabilityProber::Delegate {
 public:
  explicit IsolatedPrerenderURLLoaderInterceptor(int frame_tree_node_id);
  ~IsolatedPrerenderURLLoaderInterceptor() override;

  // content::URLLaoderRequestInterceptor:
  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      content::BrowserContext* browser_context,
      content::URLLoaderRequestInterceptor::LoaderCallback callback) override;

 protected:
  // Virtual for testing
  virtual std::unique_ptr<PrefetchedMainframeResponseContainer>
  GetPrefetchedResponse(const GURL& url);

 private:
  void InterceptPrefetchedNavigation(
      const network::ResourceRequest& tentative_resource_request,
      std::unique_ptr<PrefetchedMainframeResponseContainer>);
  void DoNotInterceptNavigation();

  // AvailabilityProber::Delegate:
  bool ShouldSendNextProbe() override;
  bool IsResponseSuccess(net::Error net_error,
                         const network::mojom::URLResponseHead* head,
                         std::unique_ptr<std::string> body) override;

  void StartProbe(const GURL& url, base::OnceClosure on_success_callback);

  // Called when the probe finishes with |success|.
  void OnProbeComplete(base::OnceClosure on_success_callback, bool success);

  // Notifies the Tab Helper about the usage of a prefetched resource.
  void NotifyPrefetchStatusUpdate(
      IsolatedPrerenderTabHelper::PrefetchStatus usage) const;

  // Used to get the current WebContents.
  const int frame_tree_node_id_;

  // The url that |MaybeCreateLoader| is called with.
  GURL url_;

  // Probes the origin to establish that it is reachable before
  // attempting to reuse a cached prefetch.
  std::unique_ptr<AvailabilityProber> origin_prober_;

  // The time when probing was started. Only set when |origin_prober_| is not
  // null. Used to calculate probe latency which is reported to the tab helper.
  base::Optional<base::TimeTicks> probe_start_time_;

  // Set in |MaybeCreateLoader| and used in |On[DoNot]InterceptRequest|.
  content::URLLoaderRequestInterceptor::LoaderCallback loader_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(IsolatedPrerenderURLLoaderInterceptor);
};

#endif  // CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_URL_LOADER_INTERCEPTOR_H_

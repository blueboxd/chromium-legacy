// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_URL_LOADER_INTERCEPTOR_H_
#define CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_URL_LOADER_INTERCEPTOR_H_

#include <memory>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "chrome/browser/availability/availability_prober.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

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

  void CallOnProbeCompleteForTesting(
      const network::ResourceRequest& tentative_resource_request,
      content::BrowserContext* browser_context,
      bool success);

  // TODO(crbug/1023485): Add logic to handle subresources.

 private:
  void OnInterceptRequest(
      const network::ResourceRequest& tentative_resource_request,
      content::BrowserContext* browser_context);
  void OnDoNotInterceptRequest();

  // AvailabilityProber::Delegate:
  bool ShouldSendNextProbe() override;
  bool IsResponseSuccess(net::Error net_error,
                         const network::mojom::URLResponseHead* head,
                         std::unique_ptr<std::string> body) override;

  // Starts a probe to the origin of |tentative_resource_request|'s url.
  void StartProbe(const network::ResourceRequest& tentative_resource_request,
                  content::BrowserContext* browser_context);

  // Called when the probe finishes with |success|.
  void OnProbeComplete(
      const network::ResourceRequest& tentative_resource_request,
      content::BrowserContext* browser_context,
      bool success);

  // Used to get the current WebContents.
  const int frame_tree_node_id_;

  // Probes the origin to establish that it is reachable before
  // attempting to reuse a cached prefetch.
  std::unique_ptr<AvailabilityProber> origin_prober_;

  // Set in |MaybeCreateLoader| and used in |On[DoNot]InterceptRequest|.
  content::URLLoaderRequestInterceptor::LoaderCallback loader_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(IsolatedPrerenderURLLoaderInterceptor);
};

#endif  // CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_URL_LOADER_INTERCEPTOR_H_

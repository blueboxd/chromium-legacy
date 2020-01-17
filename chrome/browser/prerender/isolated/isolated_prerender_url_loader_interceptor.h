// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_URL_LOADER_INTERCEPTOR_H_
#define CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_URL_LOADER_INTERCEPTOR_H_

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

// Intercepts prerender navigations that are eligible to be isolated.
class IsolatedPrerenderURLLoaderInterceptor
    : public content::URLLoaderRequestInterceptor {
 public:
  explicit IsolatedPrerenderURLLoaderInterceptor(int frame_tree_node_id);
  ~IsolatedPrerenderURLLoaderInterceptor() override;

  // content::URLLaoderRequestInterceptor:
  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      content::BrowserContext* browser_context,
      content::URLLoaderRequestInterceptor::LoaderCallback callback) override;

  // TODO(crbug/1023485): Add logic to handle subresources.

 private:
  // Used to get the current WebContents.
  const int frame_tree_node_id_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(IsolatedPrerenderURLLoaderInterceptor);
};

#endif  // CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_URL_LOADER_INTERCEPTOR_H_

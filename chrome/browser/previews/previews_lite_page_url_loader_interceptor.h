// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_URL_LOADER_INTERCEPTOR_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_URL_LOADER_INTERCEPTOR_H_

#include <stdint.h>
#include <memory>
#include <set>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/previews/previews_lite_page_redirect_url_loader.h"
#include "chrome/browser/previews/previews_lite_page_serving_url_loader.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace previews {

// A class that attempts to intercept requests and fetch the Lite Page version
// of the request. Its lifetime matches that of the content/ navigation loader
// code. Currently, not fully implemented.
class PreviewsLitePageURLLoaderInterceptor
    : public content::URLLoaderRequestInterceptor {
 public:
  PreviewsLitePageURLLoaderInterceptor(
      const scoped_refptr<network::SharedURLLoaderFactory>&
          network_loader_factory,
      uint64_t page_id,
      int frame_tree_node_id);
  ~PreviewsLitePageURLLoaderInterceptor() override;

  // content::URLLaoderRequestInterceptor:
  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      content::BrowserContext* browser_context,
      content::ResourceContext* resource_context,
      content::URLLoaderRequestInterceptor::LoaderCallback callback) override;

 private:
  // Begins an attempt at fetching the lite page version of the URL.
  void CreateRedirectLoader(
      const network::ResourceRequest& tentative_resource_request,
      content::BrowserContext* browser_context,
      content::ResourceContext* resource_context,
      content::URLLoaderRequestInterceptor::LoaderCallback callback);

  // Creates a redirect URL loader that immediately serves a redirect to
  // |original_url|.
  void CreateOriginalURLLoader(
      const network::ResourceRequest& tentative_resource_request,
      const GURL& original_url,
      content::URLLoaderRequestInterceptor::LoaderCallback callback);

  // Runs |callback| with |handler| and stores |serving_url_loader|.
  void HandleRedirectLoader(
      content::URLLoaderRequestInterceptor::LoaderCallback callback,
      std::unique_ptr<PreviewsLitePageServingURLLoader> serving_url_loader,
      RequestHandler handler);

  // All URLs already seen in this navigation. This prevents redirect loops,
  // etc.
  std::set<GURL> urls_processed_;

  // While attempting to fetch a lite page, this object manages communication
  // with the lite page server and serving redirects. Once, a decision has been
  // made regarding serving the preview, this object will be null.
  std::unique_ptr<PreviewsLitePageRedirectURLLoader> redirect_url_loader_;

  // Once a decision to serve the lite page has been made (based on server
  // response), this object will exist until a redirect to the lite page URL has
  // been handed off to the navigation stack and the next request is being
  // handled.
  std::unique_ptr<PreviewsLitePageServingURLLoader> serving_url_loader_;

  // Factory to create a network service URLLoader.
  scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory_;

  // Used in the chrome-proxy header if a preview is attempted.
  uint64_t page_id_;

  // Used to create the network service URLLoader.
  int frame_tree_node_id_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PreviewsLitePageURLLoaderInterceptor);
};

}  // namespace previews

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_URL_LOADER_INTERCEPTOR_H_

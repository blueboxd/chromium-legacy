// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_url_loader_interceptor.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_lite_page_redirect.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/resource_type.h"
#include "net/http/http_request_headers.h"
#include "net/nqe/effective_connection_type.h"

namespace previews {

namespace {

void RecordInterceptAttempt(bool attempted) {
  UMA_HISTOGRAM_BOOLEAN("Previews.ServerLitePage.URLLoader.Attempted",
                        attempted);
}

bool ShouldCreateLoader(const network::ResourceRequest& resource_request) {
  if (!(resource_request.previews_state & content::LITE_PAGE_REDIRECT_ON))
    return false;

  DCHECK_EQ(resource_request.resource_type,
            static_cast<int>(content::ResourceType::kMainFrame));
  DCHECK(resource_request.url.SchemeIsHTTPOrHTTPS());
  DCHECK_EQ(resource_request.method, "GET");

  return true;
}

net::HttpRequestHeaders GetChromeProxyHeaders(
    content::BrowserContext* browser_context,
    content::ResourceContext* resource_context,
    uint64_t page_id) {
  net::HttpRequestHeaders headers;
  // Return empty headers for unittests.
  if (!resource_context && !browser_context)
    return headers;

  DCHECK(!(resource_context && browser_context));
  if (resource_context) {
    auto* io_data = ProfileIOData::FromResourceContext(resource_context);
    if (io_data && io_data->data_reduction_proxy_io_data()) {
      DCHECK(data_reduction_proxy::params::IsEnabledWithNetworkService());
      data_reduction_proxy::DataReductionProxyRequestOptions* request_options =
          io_data->data_reduction_proxy_io_data()->request_options();
      request_options->AddRequestHeader(&headers, page_id != 0U ? page_id : 1);

      headers.SetHeader(data_reduction_proxy::chrome_proxy_ect_header(),
                        net::GetNameForEffectiveConnectionType(
                            io_data->data_reduction_proxy_io_data()
                                ->GetEffectiveConnectionType()));
    }
  } else {
    DCHECK(browser_context);
    auto* settings =
        DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
            browser_context);
    if (settings) {
      DCHECK(data_reduction_proxy::params::IsEnabledWithNetworkService());
      std::string header;
      if (settings->GetProxyRequestHeaders().GetHeader(
              data_reduction_proxy::chrome_proxy_header(), &header)) {
        data_reduction_proxy::DataReductionProxyRequestOptions::
            AddRequestHeader(&headers, page_id != 0U ? page_id : 1, header);
      }

      headers.SetHeader(data_reduction_proxy::chrome_proxy_ect_header(),
                        net::GetNameForEffectiveConnectionType(
                            settings->data_reduction_proxy_service()
                                ->GetEffectiveConnectionType()));
    }
  }

  return headers;
}

}  // namespace

PreviewsLitePageURLLoaderInterceptor::PreviewsLitePageURLLoaderInterceptor(
    const scoped_refptr<network::SharedURLLoaderFactory>&
        network_loader_factory,
    uint64_t page_id,
    int frame_tree_node_id)
    : network_loader_factory_(network_loader_factory),
      page_id_(page_id),
      frame_tree_node_id_(frame_tree_node_id) {}

PreviewsLitePageURLLoaderInterceptor::~PreviewsLitePageURLLoaderInterceptor() {}

void PreviewsLitePageURLLoaderInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    content::BrowserContext* browser_context,
    content::ResourceContext* resource_context,
    content::URLLoaderRequestInterceptor::LoaderCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (serving_url_loader_) {
    RequestHandler handler = serving_url_loader_->ServingResponseHandler();
    // The serving loader manages its own lifetime at this point.
    serving_url_loader_.release();
    std::move(callback).Run(std::move(handler));
    return;
  }

  // Do not attempt to serve the same URL multiple times.
  if (base::Contains(urls_processed_, tentative_resource_request.url)) {
    std::move(callback).Run({});
    return;
  }

  urls_processed_.insert(tentative_resource_request.url);

  std::string original_url;
  if (previews::ExtractOriginalURLFromLitePageRedirectURL(
          tentative_resource_request.url, &original_url)) {
    // Add the original URL to |urls_processed_| so that we will not retrigger
    // on this navigation. This is used to allow `location.reload()` JavaScript
    // code to load the original page when a preview has been committed.
    urls_processed_.insert(GURL(original_url));
    CreateOriginalURLLoader(tentative_resource_request, GURL(original_url),
                            std::move(callback));
    return;
  }

  if (ShouldCreateLoader(tentative_resource_request)) {
    CreateRedirectLoader(tentative_resource_request, browser_context,
                         resource_context, std::move(callback));
    return;
  }
  RecordInterceptAttempt(false);
  std::move(callback).Run({});
}

void PreviewsLitePageURLLoaderInterceptor::CreateRedirectLoader(
    const network::ResourceRequest& tentative_resource_request,
    content::BrowserContext* browser_context,
    content::ResourceContext* resource_context,
    content::URLLoaderRequestInterceptor::LoaderCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RecordInterceptAttempt(true);

  redirect_url_loader_ = std::make_unique<PreviewsLitePageRedirectURLLoader>(
      tentative_resource_request,
      base::BindOnce(
          &PreviewsLitePageURLLoaderInterceptor::HandleRedirectLoader,
          base::Unretained(this), std::move(callback)));

  // |redirect_url_loader_| can be null after this call.
  redirect_url_loader_->StartRedirectToPreview(
      GetChromeProxyHeaders(browser_context, resource_context, page_id_),
      network_loader_factory_, frame_tree_node_id_);
}

void PreviewsLitePageURLLoaderInterceptor::CreateOriginalURLLoader(
    const network::ResourceRequest& tentative_resource_request,
    const GURL& original_url,
    content::URLLoaderRequestInterceptor::LoaderCallback callback) {
  redirect_url_loader_ = std::make_unique<PreviewsLitePageRedirectURLLoader>(
      tentative_resource_request,
      base::BindOnce(
          &PreviewsLitePageURLLoaderInterceptor::HandleRedirectLoader,
          base::Unretained(this), std::move(callback)));

  // |redirect_url_loader_| can be null after this call.
  redirect_url_loader_->StartRedirectToOriginalURL(original_url);
}

void PreviewsLitePageURLLoaderInterceptor::HandleRedirectLoader(
    content::URLLoaderRequestInterceptor::LoaderCallback callback,
    std::unique_ptr<PreviewsLitePageServingURLLoader> serving_url_loader,
    RequestHandler handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Handle any failure by using default loader.
  if (handler.is_null()) {
    DCHECK(!serving_url_loader_);
    redirect_url_loader_.reset();
    std::move(callback).Run({});
    return;
  }

  // Save the serving loader to handle the next request. It can be null when
  // serving the original URL on a reload.
  serving_url_loader_ = std::move(serving_url_loader);

  // |redirect_url_loader_| now manages its own lifetime via a mojo channel.
  // |handler| is guaranteed to be called.
  redirect_url_loader_.release();
  std::move(callback).Run(std::move(handler));
}

}  // namespace previews

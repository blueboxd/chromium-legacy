// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/subresource_redirect_url_loader_throttle.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "content/public/common/resource_type.h"
#include "net/http/http_status_code.h"

namespace subresource_redirect {

SubresourceRedirectURLLoaderThrottle::SubresourceRedirectURLLoaderThrottle() =
    default;
SubresourceRedirectURLLoaderThrottle::~SubresourceRedirectURLLoaderThrottle() =
    default;

void SubresourceRedirectURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  DCHECK_EQ(request->resource_type,
            static_cast<int>(content::ResourceType::kImage));

  DCHECK(request->url.SchemeIs(url::kHttpsScheme));

  request->url = GetSubresourceURLForURL(request->url);
  *defer = false;
}

void SubresourceRedirectURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::ResourceResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* to_be_removed_request_headers,
    net::HttpRequestHeaders* modified_request_headers) {
  UMA_HISTOGRAM_ENUMERATION(
      "SubresourceRedirect.CompressionAttempt.Status",
      static_cast<net::HttpStatusCode>(response_head.headers->response_code()),
      net::HTTP_VERSION_NOT_SUPPORTED);
}

void SubresourceRedirectURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::ResourceResponseHead* response_head,
    bool* defer) {
  UMA_HISTOGRAM_ENUMERATION(
      "SubresourceRedirect.CompressionAttempt.Status",
      static_cast<net::HttpStatusCode>(response_head->headers->response_code()),
      net::HTTP_VERSION_NOT_SUPPORTED);

  // If compression was unsuccessful don't try and record compression percent.
  if (response_head->headers->response_code() != 200)
    return;

  float content_length =
      static_cast<float>(response_head->headers->GetContentLength());

  float ofcl =
      static_cast<float>(data_reduction_proxy::GetDataReductionProxyOFCL(
          response_head->headers.get()));

  // If either |content_length| or |ofcl| are missing don't compute compression
  // percent.
  if (content_length < 0.0 || ofcl <= 0.0)
    return;

  UMA_HISTOGRAM_PERCENTAGE(
      "SubresourceRedirect.DidCompress.CompressionPercent",
      static_cast<int>(100 - ((content_length / ofcl) * 100)));

  UMA_HISTOGRAM_COUNTS_1M("SubresourceRedirect.DidCompress.BytesSaved",
                          static_cast<int>(ofcl - content_length));
}

void SubresourceRedirectURLLoaderThrottle::DetachFromCurrentSequence() {}

}  // namespace subresource_redirect

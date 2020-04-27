// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/network_service/aw_url_loader_throttle.h"

#include "android_webview/browser/aw_resource_context.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "net/http/http_request_headers.h"

namespace android_webview {

namespace {

// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "WebViewExtraHeaders" in src/tools/metrics/histograms/enums.xml.
enum class ExtraHeaders {
  kAddedInStartRequest = 0,
  kKeptOnSameOriginRedirect = 1,
  kRemovedOnCrossOriginRedirect = 2,
  kMaxValue = kRemovedOnCrossOriginRedirect
};

void RecordExtraHeadersUMA(ExtraHeaders value) {
  UMA_HISTOGRAM_ENUMERATION("Android.WebView.ExtraHeaders", value);
}

}  // namespace

AwURLLoaderThrottle::AwURLLoaderThrottle(AwResourceContext* aw_resource_context)
    : aw_resource_context_(aw_resource_context) {}

AwURLLoaderThrottle::~AwURLLoaderThrottle() = default;

void AwURLLoaderThrottle::WillStartRequest(network::ResourceRequest* request,
                                           bool* defer) {
  AddExtraHeadersIfNeeded(request->url, &request->headers);
  if (!added_headers_.empty()) {
    original_origin_ = url::Origin::Create(request->url);
    RecordExtraHeadersUMA(ExtraHeaders::kAddedInStartRequest);
  }
}

void AwURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* to_be_removed_request_headers,
    net::HttpRequestHeaders* modified_request_headers,
    net::HttpRequestHeaders* modified_cors_exempt_request_headers) {
  if (!added_headers_.empty()) {
    if (original_origin_.CanBeDerivedFrom(redirect_info->new_url)) {
      RecordExtraHeadersUMA(ExtraHeaders::kKeptOnSameOriginRedirect);
    } else {
      // Cross-origin redirect. Remove the headers we added.
      to_be_removed_request_headers->insert(
          to_be_removed_request_headers->end(),
          std::make_move_iterator(added_headers_.begin()),
          std::make_move_iterator(added_headers_.end()));
      added_headers_.clear();
      RecordExtraHeadersUMA(ExtraHeaders::kRemovedOnCrossOriginRedirect);
    }
  }
}

void AwURLLoaderThrottle::AddExtraHeadersIfNeeded(
    const GURL& url,
    net::HttpRequestHeaders* headers) {
  std::string extra_headers = aw_resource_context_->GetExtraHeaders(url);
  if (extra_headers.empty())
    return;

  net::HttpRequestHeaders temp_headers;
  temp_headers.AddHeadersFromString(extra_headers);
  for (net::HttpRequestHeaders::Iterator it(temp_headers); it.GetNext();) {
    if (headers->HasHeader(it.name()))
      continue;

    headers->SetHeader(it.name(), it.value());
    added_headers_.push_back(it.name());
  }
}

}  // namespace android_webview

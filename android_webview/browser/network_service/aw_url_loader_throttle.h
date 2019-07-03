// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_URL_LOADER_THROTTLE_H_
#define ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_URL_LOADER_THROTTLE_H_

#include "base/macros.h"
#include "content/public/common/url_loader_throttle.h"

class GURL;

namespace net {
class HttpRequestHeaders;
}

namespace android_webview {
class AwResourceContext;

class AwURLLoaderThrottle : public content::URLLoaderThrottle {
 public:
  explicit AwURLLoaderThrottle(AwResourceContext* aw_resource_context);
  ~AwURLLoaderThrottle() override;

  // content::URLLoaderThrottle implementation:
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::ResourceResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* to_be_removed_request_headers,
      net::HttpRequestHeaders* modified_request_headers) override;

 private:
  void AddExtraHeadersIfNeeded(const GURL& url,
                               net::HttpRequestHeaders* headers);

  AwResourceContext* aw_resource_context_;

  DISALLOW_COPY_AND_ASSIGN(AwURLLoaderThrottle);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_URL_LOADER_THROTTLE_H_

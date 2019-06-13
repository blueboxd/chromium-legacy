// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_URL_LOADER_THROTTLE_H_
#define CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_URL_LOADER_THROTTLE_H_

#include "content/public/common/url_loader_throttle.h"

namespace subresource_redirect {

// This class handles internal redirects for subresouces on HTTPS sites to
// compressed versions of subresources.
class SubresourceRedirectURLLoaderThrottle : public content::URLLoaderThrottle {
 public:
  SubresourceRedirectURLLoaderThrottle();
  ~SubresourceRedirectURLLoaderThrottle() override;

  // content::URLLoaderThrottle:
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::ResourceResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* to_be_removed_request_headers,
      net::HttpRequestHeaders* modified_request_headers) override;
  void WillProcessResponse(const GURL& response_url,
                           network::ResourceResponseHead* response_head,
                           bool* defer) override;
  // Overridden to do nothing as the default implementation is NOT_REACHED()
  void DetachFromCurrentSequence() override;
};

}  // namespace subresource_redirect

#endif  // CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_URL_LOADER_THROTTLE_H_

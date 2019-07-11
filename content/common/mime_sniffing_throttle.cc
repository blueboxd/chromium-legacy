// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mime_sniffing_throttle.h"

#include "content/common/mime_sniffing_url_loader.h"
#include "net/base/mime_sniffer.h"

namespace content {

MimeSniffingThrottle::MimeSniffingThrottle(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

MimeSniffingThrottle::~MimeSniffingThrottle() = default;

void MimeSniffingThrottle::WillProcessResponse(
    const GURL& response_url,
    network::ResourceResponseHead* response_head,
    bool* defer) {
  // No need to do mime sniffing again.
  if (response_head->did_mime_sniff)
    return;

  bool blocked_sniffing_mime = false;
  std::string content_type_options;
  if (response_head->headers &&
      response_head->headers->GetNormalizedHeader("x-content-type-options",
                                                  &content_type_options)) {
    blocked_sniffing_mime =
        base::LowerCaseEqualsASCII(content_type_options, "nosniff");
  }

  if (!blocked_sniffing_mime &&
      net::ShouldSniffMimeType(response_url, response_head->mime_type)) {
    // Pause the response until the mime type becomes ready.
    *defer = true;

    network::mojom::URLLoaderPtr new_loader;
    network::mojom::URLLoaderClientRequest new_loader_request;
    network::mojom::URLLoaderPtr source_loader;
    network::mojom::URLLoaderClientRequest source_client_request;
    MimeSniffingURLLoader* mime_sniffing_loader;
    std::tie(new_loader, new_loader_request, mime_sniffing_loader) =
        MimeSniffingURLLoader::CreateLoader(weak_factory_.GetWeakPtr(),
                                            response_url, *response_head,
                                            task_runner_);
    delegate_->InterceptResponse(std::move(new_loader),
                                 std::move(new_loader_request), &source_loader,
                                 &source_client_request);
    mime_sniffing_loader->Start(std::move(source_loader),
                                std::move(source_client_request));
  }
}

void MimeSniffingThrottle::ResumeWithNewResponseHead(
    const network::ResourceResponseHead& new_response_head) {
  delegate_->UpdateDeferredResponseHead(new_response_head);
  delegate_->Resume();
}

}  // namespace content

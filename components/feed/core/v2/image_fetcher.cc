// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/image_fetcher.h"

#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace feed {

ImageFetcher::ImageFetcher(
    scoped_refptr<::network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {}

ImageFetcher::~ImageFetcher() = default;

void ImageFetcher::Fetch(const GURL& url, ImageCallback callback) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("interest_feedv2_image_send", R"(
        semantics {
          sender: "Feed Library"
          description: "Images for articles in the feed."
          trigger: "Triggered when viewing the feed on the NTP."
          data: "Request for an image associated with a feed article."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          setting: "This can be disabled from the New Tab Page by collapsing "
          "the articles section."
          chrome_policy {
            NTPContentSuggestionsEnabled {
              policy_options {mode: MANDATORY}
              NTPContentSuggestionsEnabled: false
            }
          }
        })");
  auto resource_request = std::make_unique<::network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = net::HttpRequestHeaders::kGetMethod;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  auto simple_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  simple_loader->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&ImageFetcher::OnFetchComplete, weak_factory_.GetWeakPtr(),
                     std::move(simple_loader), std::move(callback)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void ImageFetcher::OnFetchComplete(
    std::unique_ptr<network::SimpleURLLoader> simple_loader,
    ImageCallback callback,
    std::unique_ptr<std::string> response_data) {
  std::move(callback).Run(std::move(response_data));
}

}  // namespace feed
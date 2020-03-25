// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/internal/tile_info_fetcher.h"

#include <utility>

#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace upboarding {
namespace {

class TileInfoFetcherImpl : public TileInfoFetcher {
 public:
  TileInfoFetcherImpl(
      const GURL& url,
      const std::string& locale,
      const std::string& accept_languages,
      const std::string& api_key,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      FinishedCallback callback)
      : url_loader_factory_(url_loader_factory),
        callback_(std::move(callback)) {
    tile_info_request_status_ = TileInfoRequestStatus::kInit;
    // Start fetching.
    auto resource_request =
        BuildGetRequest(url, locale, accept_languages, api_key);
    url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                   traffic_annotation);

    url_loader_->SetOnResponseStartedCallback(
        base::BindRepeating(&TileInfoFetcherImpl::OnResponseStarted,
                            weak_ptr_factory_.GetWeakPtr()));
    // TODO(hesen): Estimate max size of response then replace to
    // DownloadToString method.
    url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory_.get(),
        base::BindOnce(&TileInfoFetcherImpl::OnDownloadComplete,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  // Build the request to get tile info.
  std::unique_ptr<network::ResourceRequest> BuildGetRequest(
      const GURL& url,
      const std::string& locale,
      const std::string& accept_languages,
      const std::string& api_key) {
    auto request = std::make_unique<network::ResourceRequest>();
    request->method = net::HttpRequestHeaders::kGetMethod;
    request->headers.SetHeader("x-goog-api-key", api_key);
    if (!accept_languages.empty())
      request->headers.SetHeader(net::HttpRequestHeaders::kAcceptLanguage,
                                 accept_languages);
    return request;
  }

  // Called when start receiving HTTP response.
  void OnResponseStarted(const GURL& final_url,
                         const network::mojom::URLResponseHead& response_head) {
    int response_code = -1;
    if (response_head.headers)
      response_code = response_head.headers->response_code();

    // TODO(hesen): Handle more possible status, and record status to UMA.
    if (response_code == -1 || (response_code < 200 || response_code > 299))
      tile_info_request_status_ = TileInfoRequestStatus::kFailure;
  }

  // Called after receiving HTTP response. Processes the response code.
  void OnDownloadComplete(std::unique_ptr<std::string> response_body) {
    std::move(callback_).Run(tile_info_request_status_,
                             std::move(response_body));
  }

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Simple URL loader to fetch proto from network.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // Callback to be executed after fetching is done.
  FinishedCallback callback_;

  // Status of the tile info request.
  TileInfoRequestStatus tile_info_request_status_;

  base::WeakPtrFactory<TileInfoFetcherImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
std::unique_ptr<TileInfoFetcher> TileInfoFetcher::CreateAndFetchForTileInfo(
    const GURL& url,
    const std::string& locale,
    const std::string& accept_languages,
    const std::string& api_key,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    FinishedCallback callback) {
  return std::make_unique<TileInfoFetcherImpl>(
      url, locale, accept_languages, api_key, traffic_annotation,
      url_loader_factory, std::move(callback));
}

TileInfoFetcher::TileInfoFetcher() = default;
TileInfoFetcher::~TileInfoFetcher() = default;

}  // namespace upboarding

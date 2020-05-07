// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/tile_fetcher.h"

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

const char kRequestContentType[] = "application/x-protobuf";

constexpr net::NetworkTrafficAnnotationTag kQueryTilesFetcherTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("query_tiles_fetcher", R"(
              semantics {
                sender: "Query Tiles Fetcher"
                description:
                  "Fetches RPC for query tiles on Android NTP and omnibox."
                trigger:
                  "A priodic TileBackgroundTask will always be scheduled to "
                  "fetch RPC from server, unless the feature is disabled "
                  "or suspended."
                data: "Country code and accepted languages will be sent via "
                  "the header. No user information is sent."
                destination: GOOGLE_OWNED_SERVICE
              }
              policy {
                cookies_allowed: NO
                setting: "Disabled if a non-Google search engine is used."
                chrome_policy {
                  DefaultSearchProviderEnabled {
                    DefaultSearchProviderEnabled: false
                  }
                }
              }
    )");

class TileFetcherImpl : public TileFetcher {
 public:
  TileFetcherImpl(
      const GURL& url,
      const std::string& country_code,
      const std::string& accept_languages,
      const std::string& api_key,
      const std::string& experiment_tag,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : url_loader_factory_(url_loader_factory) {
    tile_info_request_status_ = TileInfoRequestStatus::kInit;
    auto resource_request = BuildGetRequest(url, country_code, accept_languages,
                                            api_key, experiment_tag);
    url_loader_ = network::SimpleURLLoader::Create(
        std::move(resource_request), kQueryTilesFetcherTrafficAnnotation);
  }

 private:
  // TileFetcher implementation.
  void StartFetchForTiles(FinishedCallback callback) override {
    // TODO(hesen): Estimate max size of response then replace to
    // DownloadToString method.
    url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory_.get(),
        base::BindOnce(&TileFetcherImpl::OnDownloadComplete,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // Build the request to get tile info.
  std::unique_ptr<network::ResourceRequest> BuildGetRequest(
      const GURL& url,
      const std::string& country_code,
      const std::string& accept_languages,
      const std::string& api_key,
      const std::string& experiment_tag) {
    auto request = std::make_unique<network::ResourceRequest>();
    request->method = net::HttpRequestHeaders::kGetMethod;
    request->headers.SetHeader("x-goog-api-key", api_key);
    request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                               kRequestContentType);
    request->url =
        net::AppendOrReplaceQueryParameter(url, "country_code", country_code);
    if (!experiment_tag.empty()) {
      request->url = net::AppendOrReplaceQueryParameter(
          request->url, "experiment_tag", experiment_tag);
    }
    if (!accept_languages.empty()) {
      request->headers.SetHeader(net::HttpRequestHeaders::kAcceptLanguage,
                                 accept_languages);
    }
    return request;
  }

  bool ShouldSuspendDueToNetError() {
    auto error_code = url_loader_->NetError();
    switch (error_code) {
      case net::ERR_BLOCKED_BY_ADMINISTRATOR:
        return true;
      default:
        return false;
    }
  }

  bool ShouldSuspend(int response_code) {
    switch (response_code) {
      case net::HTTP_NOT_IMPLEMENTED:
      case net::HTTP_FORBIDDEN:
        return true;
      default:
        return ShouldSuspendDueToNetError();
    }
  }

  // Called after receiving HTTP response. Processes the response code and net
  // error.
  void OnDownloadComplete(FinishedCallback callback,
                          std::unique_ptr<std::string> response_body) {
    int response_code = -1;
    if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers)
      response_code = url_loader_->ResponseInfo()->headers->response_code();

    if (response_code >= 200 && response_code < 300 && response_body) {
      tile_info_request_status_ = TileInfoRequestStatus::kSuccess;
    } else {
      tile_info_request_status_ = ShouldSuspend(response_code)
                                      ? TileInfoRequestStatus::kShouldSuspend
                                      : TileInfoRequestStatus::kFailure;
    }
    // TODO(crbug.com/1068683): Record response code UMA.
    std::move(callback).Run(tile_info_request_status_,
                            std::move(response_body));
    tile_info_request_status_ = TileInfoRequestStatus::kInit;
  }

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Simple URL loader to fetch proto from network.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // Status of the tile info request.
  TileInfoRequestStatus tile_info_request_status_;

  base::WeakPtrFactory<TileFetcherImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
std::unique_ptr<TileFetcher> TileFetcher::Create(
    const GURL& url,
    const std::string& country_code,
    const std::string& accept_languages,
    const std::string& api_key,
    const std::string& experiment_tag,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return std::make_unique<TileFetcherImpl>(url, country_code, accept_languages,
                                           api_key, experiment_tag,
                                           url_loader_factory);
}

TileFetcher::TileFetcher() = default;
TileFetcher::~TileFetcher() = default;

}  // namespace upboarding

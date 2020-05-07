// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/feeds/media_feeds_fetcher.h"

#include "components/schema_org/common/metadata.mojom.h"
#include "components/schema_org/extractor.h"
#include "components/schema_org/schema_org_entity_names.h"
#include "components/schema_org/validator.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace media_feeds {

MediaFeedsFetcher::MediaFeedsFetcher(
    scoped_refptr<::network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory),
      extractor_({schema_org::entity::kCompleteDataFeed}) {}

MediaFeedsFetcher::~MediaFeedsFetcher() = default;

void MediaFeedsFetcher::FetchFeed(const GURL& url,
                                  const bool bypass_cache,
                                  MediaFeedCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("media_feeds", R"(
        semantics {
          sender: "Media Feeds Service"
          description:
            "Media Feeds service fetches a schema.org DataFeed object "
            "containing Media Feed items used to provide recommendations to "
            "the signed-in user. Feed data will be stored in the Media History "
            "database."
          trigger:
            "Having a discovered feed that has not been fetched recently. "
            "Feeds are discovered when the browser visits a page with a feed "
            "link element in the header."
          data: "User cookies."
          destination: OTHER
          destination_other: "Media providers which provide media feed data."
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
             "The feature is enabled by default. The user can disable "
             "individual media feeds. The feature does not operate in "
             "incognito mode."
          chrome_policy {
            SavingBrowserHistoryDisabled {
              policy_options {mode: MANDATORY}
              SavingBrowserHistoryDisabled: false
            }
          }
        })");
  auto resource_request = std::make_unique<::network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = net::HttpRequestHeaders::kGetMethod;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      "application/ld+json");
  resource_request->redirect_mode = ::network::mojom::RedirectMode::kError;
  url::Origin origin = url::Origin::Create(url);
  // Treat this request as same-site for the purposes of cookie inclusion.
  resource_request->site_for_cookies = net::SiteForCookies::FromOrigin(origin);
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RedirectMode::kUpdateNothing, origin, origin,
      net::SiteForCookies::FromOrigin(origin));

  if (bypass_cache)
    resource_request->load_flags |= net::LOAD_BYPASS_CACHE;

  DCHECK(!pending_request_);
  pending_request_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  pending_request_->SetAllowHttpErrorResults(true);

  auto fetcher_callback =
      base::BindOnce(&MediaFeedsFetcher::OnURLFetchComplete,
                     base::Unretained(this), url, std::move(callback));
  pending_request_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(), std::move(fetcher_callback));
}

void MediaFeedsFetcher::OnURLFetchComplete(
    const GURL& original_url,
    MediaFeedCallback callback,
    std::unique_ptr<std::string> feed_data) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // The SimpleURLLoader will be deleted when the request is handled.
  std::unique_ptr<const ::network::SimpleURLLoader> request =
      std::move(pending_request_);
  DCHECK(request);

  if (request->NetError() != net::OK) {
    std::move(callback).Run(nullptr, Status::kRequestFailed, false);
    return;
  }

  int response_code = 0;
  bool was_fetched_via_cache = false;

  if (request->ResponseInfo()) {
    was_fetched_via_cache = request->ResponseInfo()->was_fetched_via_cache;

    if (request->ResponseInfo()->headers)
      response_code = request->ResponseInfo()->headers->response_code();
  }

  if (response_code == net::HTTP_GONE) {
    std::move(callback).Run(nullptr, Status::kGone, was_fetched_via_cache);
    return;
  }

  if (response_code != net::HTTP_OK) {
    std::move(callback).Run(nullptr, Status::kRequestFailed,
                            was_fetched_via_cache);
    return;
  }

  if (!feed_data || feed_data->empty()) {
    std::move(callback).Run(nullptr, Status::kNotFound, was_fetched_via_cache);
    return;
  }

  // Parse the received data.
  extractor_.Extract(*feed_data,
                     base::BindOnce(&MediaFeedsFetcher::OnParseComplete,
                                    base::Unretained(this), std::move(callback),
                                    was_fetched_via_cache));
}

void MediaFeedsFetcher::OnParseComplete(
    MediaFeedCallback callback,
    bool was_fetched_via_cache,
    schema_org::improved::mojom::EntityPtr parsed_entity) {
  if (!schema_org::ValidateEntity(parsed_entity.get())) {
    std::move(callback).Run(nullptr, Status::kInvalidFeedData,
                            was_fetched_via_cache);
    return;
  }

  std::move(callback).Run(std::move(parsed_entity), Status::kOk,
                          was_fetched_via_cache);
}

}  // namespace media_feeds

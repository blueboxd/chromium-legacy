// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/feeds/media_feeds_fetcher.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/media/feeds/media_feeds_converter.h"
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

namespace {

// Path to the Media Feeds well-known JSON file.
const char kAssociatedOriginWellKnownPath[] = "/.well-known/media-feeds";

media_feeds::mojom::FetchResult GetFetchResult(
    MediaFeedsFetcher::Status status) {
  switch (status) {
    case MediaFeedsFetcher::Status::kOk:
      return media_feeds::mojom::FetchResult::kSuccess;
    case MediaFeedsFetcher::Status::kInvalidFeedData:
    case MediaFeedsFetcher::Status::kRequestFailed:
      return media_feeds::mojom::FetchResult::kFailedBackendError;
    case MediaFeedsFetcher::Status::kNotFound:
      return media_feeds::mojom::FetchResult::kFailedNetworkError;
    default:
      return media_feeds::mojom::FetchResult::kNone;
  }
}

std::unique_ptr<media_history::MediaHistoryKeyedService::MediaFeedFetchResult>
BuildResult(MediaFeedsFetcher::Status status, bool was_fetched_via_cache) {
  auto result = std::make_unique<
      media_history::MediaHistoryKeyedService::MediaFeedFetchResult>();
  result->status = GetFetchResult(status);
  result->was_fetched_from_cache = was_fetched_via_cache;
  result->gone = status == MediaFeedsFetcher::Status::kGone;
  return result;
}

// Extracts an origin from a base::Value and adds it to |out|.
void ExtractOriginFromValue(const base::Value& value,
                            std::set<url::Origin>* out) {
  if (!value.is_string())
    return;

  GURL url(value.GetString());
  if (!url.is_valid() || url.is_empty())
    return;

  out->insert(url::Origin::Create(url));
}

// The associated origin fetcher checks whether each associated origin is
// allowed by fetching a "well-known" JSON file on the associated origin.
class MediaFeedsAssociatedOriginFetcher {
 public:
  using MediaFeedsAssociatedOriginFetcherCallback =
      base::OnceCallback<void(std::set<url::Origin>)>;

  MediaFeedsAssociatedOriginFetcher(
      const url::Origin& origin,
      const bool bypass_cache,
      scoped_refptr<::network::SharedURLLoaderFactory> url_loader_factory,
      MediaFeedsAssociatedOriginFetcherCallback callback) {
    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("media_feeds_associated_origin", R"(
      semantics {
        sender: "Media Feeds Associated Origin Checker"
        description:
          "Media Feeds are feeds of personalized media recommendations "
          "that are fetched from media websites and displayed to the user. "
          "These feeds are discovered automatically on websites that embed "
          "them. Chrome will then periodically fetch the feeds in the "
          "background. This checker will check the media feed supplied valid "
          "associated websites. It does this by checking a file on the "
          "associated website has the media feed listed inside it."
        trigger:
          "Having a discovered feed that has not been fetched recently. "
          "Feeds are discovered when the browser visits a page with a feed "
          "link element in the header."
        data: "None."
        destination: OTHER
        destination_other: "Media providers which provide media feed data."
      }
      policy {
        cookies_allowed: NO
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
    resource_request->method = net::HttpRequestHeaders::kGetMethod;
    resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                        "application/json");
    resource_request->redirect_mode = ::network::mojom::RedirectMode::kError;
    resource_request->trusted_params =
        network::ResourceRequest::TrustedParams();
    resource_request->trusted_params->isolation_info =
        net::IsolationInfo::Create(
            net::IsolationInfo::RedirectMode::kUpdateNothing, origin, origin,
            net::SiteForCookies::FromOrigin(origin));

    // Disable cookies for this request.
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

    if (bypass_cache)
      resource_request->load_flags |= net::LOAD_BYPASS_CACHE;

    // Build the URL.
    GURL url(origin.GetURL());
    GURL::Replacements replacements;
    replacements.SetPathStr(kAssociatedOriginWellKnownPath);
    resource_request->url = url.ReplaceComponents(replacements);

    // Build the request.
    DCHECK(!pending_request_);
    pending_request_ = network::SimpleURLLoader::Create(
        std::move(resource_request), traffic_annotation);
    pending_request_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory.get(),
        base::BindOnce(&MediaFeedsAssociatedOriginFetcher::OnFetchComplete,
                       base::Unretained(this), std::move(callback)));
  }

  ~MediaFeedsAssociatedOriginFetcher() = default;
  MediaFeedsAssociatedOriginFetcher(
      const MediaFeedsAssociatedOriginFetcher& t) = delete;
  MediaFeedsAssociatedOriginFetcher& operator=(
      const MediaFeedsAssociatedOriginFetcher&) = delete;

 private:
  void OnFetchComplete(MediaFeedsAssociatedOriginFetcherCallback callback,
                       std::unique_ptr<std::string> feed_data) {
    if (!feed_data) {
      std::move(callback).Run(std::set<url::Origin>());
      return;
    }

    // Use isolated JSON decoder for parsing.
    data_decoder::DataDecoder::ParseJsonIsolated(
        *feed_data,
        base::BindOnce(&MediaFeedsAssociatedOriginFetcher::OnJsonParsed,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnJsonParsed(MediaFeedsAssociatedOriginFetcherCallback callback,
                    data_decoder::DataDecoder::ValueOrError result) {
    const base::DictionaryValue* dict = nullptr;

    if (!result.value || !result.value.value().GetAsDictionary(&dict)) {
      std::move(callback).Run(std::set<url::Origin>());
      return;
    }

    // The allowed associations contain the origins of Media Feeds that are
    // allowed to associated with this origin.
    auto* allowed = dict->FindKey("allowedAssociations");
    if (!allowed) {
      std::move(callback).Run(std::set<url::Origin>());
      return;
    }

    // Extract the origins from the JSON into a set.
    std::set<url::Origin> allowed_associations;
    if (allowed->is_list()) {
      for (auto& value : base::Value::AsListValue(*allowed))
        ExtractOriginFromValue(value, &allowed_associations);
    } else {
      ExtractOriginFromValue(*allowed, &allowed_associations);
    }

    std::move(callback).Run(allowed_associations);
  }

  std::unique_ptr<::network::SimpleURLLoader> pending_request_;
  base::WeakPtrFactory<MediaFeedsAssociatedOriginFetcher> weak_factory_{this};
};

}  // namespace

const char MediaFeedsFetcher::kFetchSizeKbHistogramName[] =
    "Media.Feeds.Fetch.Size";

MediaFeedsFetcher::MediaFeedsFetcher(
    scoped_refptr<::network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory),
      extractor_({schema_org::entity::kCompleteDataFeed}) {}

MediaFeedsFetcher::~MediaFeedsFetcher() = default;

void MediaFeedsFetcher::FetchFeed(const GURL& url,
                                  const bool bypass_cache,
                                  MediaFeedCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!pending_callback_.is_null()) {
    std::move(callback).Run(
        std::move(*BuildResult(Status::kRequestFailed,
                               /*was_fetched_via_cache=*/false)));
    return;
  }

  feed_origin_ = url::Origin::Create(url);
  bypass_cache_ = bypass_cache;
  pending_callback_ = std::move(callback);

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
  pending_request_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&MediaFeedsFetcher::OnURLFetchComplete,
                     base::Unretained(this), url));
}

void MediaFeedsFetcher::OnURLFetchComplete(
    const GURL& original_url,
    std::unique_ptr<std::string> feed_data) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // The SimpleURLLoader will be deleted when the request is handled.
  std::unique_ptr<const ::network::SimpleURLLoader> request =
      std::move(pending_request_);
  DCHECK(request);

  if (request->NetError() != net::OK) {
    std::move(pending_callback_)
        .Run(std::move(*BuildResult(Status::kRequestFailed,
                                    /*was_fetched_via_cache=*/false)));
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
    std::move(pending_callback_)
        .Run(std::move(*BuildResult(Status::kGone, was_fetched_via_cache)));
    return;
  }

  if (response_code != net::HTTP_OK) {
    std::move(pending_callback_)
        .Run(std::move(
            *BuildResult(Status::kRequestFailed, was_fetched_via_cache)));
    return;
  }

  if (!feed_data || feed_data->empty()) {
    std::move(pending_callback_)
        .Run(std::move(*BuildResult(Status::kNotFound, was_fetched_via_cache)));
    return;
  }

  // Record the fetch size in KB.
  if (!feed_data->empty()) {
    base::UmaHistogramMemoryKB(MediaFeedsFetcher::kFetchSizeKbHistogramName,
                               feed_data->size() / 1000);
  }

  // Parse the received data.
  extractor_.Extract(
      *feed_data,
      base::BindOnce(&MediaFeedsFetcher::OnParseComplete,
                     base::Unretained(this), was_fetched_via_cache));
}

void MediaFeedsFetcher::OnParseComplete(
    bool was_fetched_via_cache,
    schema_org::improved::mojom::EntityPtr parsed_entity) {
  if (!schema_org::ValidateEntity(parsed_entity.get())) {
    std::move(pending_callback_)
        .Run(std::move(
            *BuildResult(Status::kInvalidFeedData, was_fetched_via_cache)));
    return;
  }

  auto result = BuildResult(Status::kOk, was_fetched_via_cache);
  if (!media_feeds_converter_.ConvertMediaFeed(parsed_entity, result.get()))
    result->status = media_feeds::mojom::FetchResult::kInvalidFeed;

  // If we don't need to check any associated origins then we can return now.
  if (result->associated_origins.empty()) {
    std::move(pending_callback_).Run(std::move(*result));
    return;
  }

  pending_result_ = std::move(result);

  // For each associated origin we should create a fetcher to check that the
  // association is allowed.
  for (auto& origin : pending_result_->associated_origins) {
    pending_origin_checks_.emplace(
        origin,
        std::make_unique<MediaFeedsAssociatedOriginFetcher>(
            origin, bypass_cache_, url_loader_factory_,
            base::BindOnce(&MediaFeedsFetcher::OnAssociatedOriginCheckComplete,
                           base::Unretained(this), origin)));
  }
}

void MediaFeedsFetcher::OnAssociatedOriginCheckComplete(
    const url::Origin& associated_origin,
    std::set<url::Origin> allowed_associations) {
  // If the feed origin is not in the associated origins "allowed associations"
  // set then remove it from the list of associated origins.
  if (!base::Contains(allowed_associations, feed_origin_))
    pending_result_->associated_origins.erase(associated_origin);

  pending_origin_checks_.erase(associated_origin);

  // If we have finished checking associated origins then call the callback.
  if (pending_origin_checks_.empty())
    std::move(pending_callback_).Run(std::move(*pending_result_));
}

}  // namespace media_feeds

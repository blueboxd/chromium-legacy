// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feed_network_impl.h"

#include <memory>
#include <utility>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/proto/v2/wire/action_request.pb.h"
#include "components/feed/core/proto/v2/wire/feed_action_response.pb.h"
#include "components/feed/core/proto/v2/wire/feed_query.pb.h"
#include "components/feed/core/proto/v2/wire/request.pb.h"
#include "components/feed/core/proto/v2/wire/response.pb.h"
#include "components/feed/core/v2/metrics_reporter.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/zlib/google/compression_utils.h"

namespace feed {
namespace {
constexpr char kAuthenticationScope[] =
    "https://www.googleapis.com/auth/googlenow";
constexpr char kApplicationOctetStream[] = "application/octet-stream";
constexpr base::TimeDelta kNetworkTimeout = base::TimeDelta::FromSeconds(30);

// Add URLs for Bling when it is supported.
constexpr char kFeedQueryUrl[] =
    "https://www.google.com/httpservice/retry/TrellisClankService/FeedQuery";
constexpr char kNextPageQueryUrl[] =
    "https://www.google.com/httpservice/retry/TrellisClankService/"
    "NextPageQuery";
constexpr char kBackgroundQueryUrl[] =
    "https://www.google.com/httpservice/noretry/TrellisClankService/"
    "FeedQuery";

GURL GetUrlWithoutQuery(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearQuery();
  return url.ReplaceComponents(replacements);
}

using RawResponse = FeedNetworkImpl::RawResponse;
}  // namespace

struct FeedNetworkImpl::RawResponse {
  // HTTP response body.
  std::string response_bytes;
  NetworkResponseInfo response_info;
};

namespace {
template <typename RESULT, NetworkRequestType REQUEST_TYPE>
void ParseAndForwardResponse(base::OnceCallback<void(RESULT)> result_callback,
                             RawResponse raw_response) {
  MetricsReporter::NetworkRequestComplete(
      REQUEST_TYPE, raw_response.response_info.status_code);
  RESULT result;
  result.response_info = raw_response.response_info;
  if (result.response_info.status_code == 200) {
    auto response_message = std::make_unique<typename decltype(
        result.response_body)::element_type>();

    ::google::protobuf::io::CodedInputStream input_stream(
        reinterpret_cast<const uint8_t*>(raw_response.response_bytes.data()),
        raw_response.response_bytes.size());

    // The first few bytes of the body are a varint containing the size of the
    // message. We need to skip over them.
    int message_size;
    input_stream.ReadVarintSizeAsInt(&message_size);

    if (response_message->ParseFromCodedStream(&input_stream)) {
      result.response_body = std::move(response_message);
    }
  }
  std::move(result_callback).Run(std::move(result));
}

void AddMothershipPayloadQueryParams(bool is_post,
                                     const std::string& payload,
                                     const std::string& language_tag,
                                     GURL* url) {
  if (!is_post)
    *url = net::AppendQueryParameter(*url, "reqpld", payload);
  *url = net::AppendQueryParameter(*url, "fmt", "bin");
  if (!language_tag.empty())
    *url = net::AppendQueryParameter(*url, "hl", language_tag);
}

}  // namespace

// Each NetworkFetch instance represents a single "logical" fetch that ends by
// calling the associated callback. Network fetches will actually attempt two
// fetches if there is a signed in user; the first to retrieve an access token,
// and the second to the specified url.
class FeedNetworkImpl::NetworkFetch {
 public:
  NetworkFetch(const GURL& url,
               const std::string& request_type,
               std::string request_body,
               signin::IdentityManager* identity_manager,
               network::SharedURLLoaderFactory* loader_factory,
               const std::string& api_key,
               const base::TickClock* tick_clock,
               PrefService* pref_service)
      : url_(url),
        request_type_(request_type),
        request_body_(std::move(request_body)),
        identity_manager_(identity_manager),
        loader_factory_(loader_factory),
        api_key_(api_key),
        tick_clock_(tick_clock),
        entire_send_start_ticks_(tick_clock_->NowTicks()),
        pref_service_(pref_service) {
    // Apply the host override (from snippets-internals).
    std::string host_override =
        pref_service_->GetString(feed::prefs::kHostOverrideHost);
    if (!host_override.empty()) {
      GURL override_host_url(host_override);
      if (override_host_url.is_valid()) {
        GURL::Replacements replacements;
        replacements.SetSchemeStr(override_host_url.scheme_piece());
        replacements.SetHostStr(override_host_url.host_piece());
        replacements.SetPortStr(override_host_url.port_piece());
        url_ = url_.ReplaceComponents(replacements);
        host_overridden_ = true;
      }
    }
  }
  ~NetworkFetch() = default;
  NetworkFetch(const NetworkFetch&) = delete;
  NetworkFetch& operator=(const NetworkFetch&) = delete;

  void Start(base::OnceCallback<void(RawResponse)> done_callback) {
    done_callback_ = std::move(done_callback);

    if (!identity_manager_->HasPrimaryAccount()) {
      StartLoader();
      return;
    }

    StartAccessTokenFetch();
  }

 private:
  void StartAccessTokenFetch() {
    signin::ScopeSet scopes{kAuthenticationScope};
    // It's safe to pass base::Unretained(this) since deleting the token fetcher
    // will prevent the callback from being completed.
    token_fetcher_ = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
        "feed", identity_manager_, scopes,
        base::BindOnce(&NetworkFetch::AccessTokenFetchFinished,
                       base::Unretained(this), tick_clock_->NowTicks()),
        signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);
  }

  void AccessTokenFetchFinished(base::TimeTicks token_start_ticks,
                                GoogleServiceAuthError error,
                                signin::AccessTokenInfo access_token_info) {
    UMA_HISTOGRAM_ENUMERATION(
        "ContentSuggestions.Feed.Network.TokenFetchStatus", error.state(),
        GoogleServiceAuthError::NUM_STATES);

    base::TimeDelta token_duration =
        tick_clock_->NowTicks() - token_start_ticks;
    UMA_HISTOGRAM_MEDIUM_TIMES("ContentSuggestions.Feed.Network.TokenDuration",
                               token_duration);

    access_token_ = access_token_info.token;
    StartLoader();
  }

  void StartLoader() {
    loader_only_start_ticks_ = tick_clock_->NowTicks();
    simple_loader_ = MakeLoader();
    simple_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        loader_factory_, base::BindOnce(&NetworkFetch::OnSimpleLoaderComplete,
                                        base::Unretained(this)));
  }

  std::unique_ptr<network::SimpleURLLoader> MakeLoader() {
    // TODO(pnoland): Add data use measurement once it's supported for simple
    // url loader.
    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("interest_feedv2_send", R"(
        semantics {
          sender: "Feed Library"
          description: "Chrome can show content suggestions (e.g. articles) "
            "in the form of a feed. For signed-in users, these may be "
            "personalized based on interest signals in the user's account."
          trigger: "Triggered periodically in the background, or upon "
            "explicit user request."
          data: "The locale of the device and data describing the suggested "
            "content that the user interacted with. For signed-in users "
            "the request is authenticated. "
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "This can be disabled from the New Tab Page by collapsing "
          "the articles section."
          chrome_policy {
            NTPContentSuggestionsEnabled {
              policy_options {mode: MANDATORY}
              NTPContentSuggestionsEnabled: false
            }
          }
        })");
    GURL url(url_);
    if (access_token_.empty() && !api_key_.empty())
      url = net::AppendQueryParameter(url_, "key", api_key_);

    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = url;

    resource_request->load_flags = net::LOAD_BYPASS_CACHE;
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    resource_request->method = request_type_;

    // Include credentials ONLY if the user has overridden the feed host through
    // the internals page. This allows for some authentication workflows we need
    // for testing.
    if (host_overridden_) {
      resource_request->credentials_mode =
          network::mojom::CredentialsMode::kInclude;
      resource_request->site_for_cookies = net::SiteForCookies::FromUrl(url);
    }

    SetRequestHeaders(!request_body_.empty(), resource_request.get());

    auto simple_loader = network::SimpleURLLoader::Create(
        std::move(resource_request), traffic_annotation);
    simple_loader->SetAllowHttpErrorResults(true);
    simple_loader->SetTimeoutDuration(kNetworkTimeout);
    PopulateRequestBody(simple_loader.get());
    return simple_loader;
  }

  void SetRequestHeaders(bool has_request_body,
                         network::ResourceRequest* request) const {
    if (has_request_body) {
      request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                 kApplicationOctetStream);
      request->headers.SetHeader("Content-Encoding", "gzip");
    }

    variations::SignedIn signed_in_status = variations::SignedIn::kNo;
    if (!access_token_.empty()) {
      request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                 "Bearer " + access_token_);
      signed_in_status = variations::SignedIn::kYes;
    }

    // Add X-Client-Data header with experiment IDs from field trials.
    variations::AppendVariationsHeader(url_, variations::InIncognito::kNo,
                                       signed_in_status, request);
  }

  void PopulateRequestBody(network::SimpleURLLoader* loader) {
    std::string compressed_request_body;
    if (!request_body_.empty()) {
      std::string uncompressed_request_body(
          reinterpret_cast<const char*>(request_body_.data()),
          request_body_.size());

      compression::GzipCompress(uncompressed_request_body,
                                &compressed_request_body);

      loader->AttachStringForUpload(compressed_request_body,
                                    kApplicationOctetStream);
    }

    UMA_HISTOGRAM_COUNTS_1M(
        "ContentSuggestions.Feed.Network.RequestSizeKB.Compressed",
        static_cast<int>(compressed_request_body.size() / 1024));
  }

  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response) {
    NetworkResponseInfo response_info;
    response_info.status_code = simple_loader_->NetError();
    response_info.fetch_duration =
        tick_clock_->NowTicks() - entire_send_start_ticks_;
    response_info.fetch_time = base::Time::Now();
    response_info.base_request_url = GetUrlWithoutQuery(url_);

    // If overriding the feed host, try to grab the Bless nonce. This is
    // strictly informational, and only displayed in snippets-internals.
    if (host_overridden_ && simple_loader_->ResponseInfo()) {
      size_t iter = 0;
      std::string value;
      while (simple_loader_->ResponseInfo()->headers->EnumerateHeader(
          &iter, "www-authenticate", &value)) {
        size_t pos = value.find("nonce=\"");
        if (pos != std::string::npos) {
          std::string nonce = value.substr(pos + 7, 16);
          if (nonce.size() == 16) {
            response_info.bless_nonce = nonce;
            break;
          }
        }
      }
    }

    std::string response_body;
    if (response) {
      response_info.status_code =
          simple_loader_->ResponseInfo()->headers->response_code();
      response_body = std::move(*response);

      if (response_info.status_code == net::HTTP_UNAUTHORIZED) {
        signin::ScopeSet scopes{kAuthenticationScope};
        CoreAccountId account_id = identity_manager_->GetPrimaryAccountId();
        if (!account_id.empty()) {
          identity_manager_->RemoveAccessTokenFromCache(account_id, scopes,
                                                        access_token_);
        }
      }
    }

    UMA_HISTOGRAM_MEDIUM_TIMES("ContentSuggestions.Feed.Network.Duration",
                               response_info.fetch_duration);

    base::TimeDelta loader_only_duration =
        tick_clock_->NowTicks() - loader_only_start_ticks_;
    // This histogram purposefully matches name and bucket size used in
    // RemoteSuggestionsFetcherImpl.
    UMA_HISTOGRAM_TIMES("NewTabPage.Snippets.FetchTime", loader_only_duration);

    // The below is true even if there is a protocol error, so this will
    // record response size as long as the request completed.
    if (response_info.status_code >= 200) {
      UMA_HISTOGRAM_COUNTS_1M("ContentSuggestions.Feed.Network.ResponseSizeKB",
                              static_cast<int>(response_body.size() / 1024));
    }

    RawResponse raw_response;
    raw_response.response_info = std::move(response_info);
    raw_response.response_bytes = std::move(response_body);
    std::move(done_callback_).Run(std::move(raw_response));
  }

 private:
  GURL url_;
  const std::string request_type_;
  std::string access_token_;
  const std::string request_body_;
  signin::IdentityManager* const identity_manager_;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> token_fetcher_;
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;
  base::OnceCallback<void(RawResponse)> done_callback_;
  network::SharedURLLoaderFactory* loader_factory_;
  const std::string api_key_;
  const base::TickClock* tick_clock_;

  // Set when the NetworkFetch is constructed, before token and article fetch.
  const base::TimeTicks entire_send_start_ticks_;

  // Should be set right before the article fetch, and after the token fetch if
  // there is one.
  base::TimeTicks loader_only_start_ticks_;
  PrefService* pref_service_;
  bool host_overridden_ = false;
};

FeedNetworkImpl::FeedNetworkImpl(
    Delegate* delegate,
    signin::IdentityManager* identity_manager,
    const std::string& api_key,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    const base::TickClock* tick_clock,
    PrefService* pref_service)
    : delegate_(delegate),
      identity_manager_(identity_manager),
      api_key_(api_key),
      loader_factory_(loader_factory),
      tick_clock_(tick_clock),
      pref_service_(pref_service) {}

FeedNetworkImpl::~FeedNetworkImpl() = default;

void FeedNetworkImpl::SendQueryRequest(
    const feedwire::Request& request,
    base::OnceCallback<void(QueryRequestResult)> callback) {
  std::string binary_proto;
  request.SerializeToString(&binary_proto);
  std::string base64proto;
  base::Base64UrlEncode(
      binary_proto, base::Base64UrlEncodePolicy::INCLUDE_PADDING, &base64proto);

  // TODO(harringtond): Decide how we want to override these URLs for testing.
  // Should probably add a command-line flag.
  GURL url;
  switch (request.feed_request().feed_query().reason()) {
    case feedwire::FeedQuery::SCHEDULED_REFRESH:
    case feedwire::FeedQuery::IN_PLACE_UPDATE:
      url = GURL(kBackgroundQueryUrl);
      break;
    case feedwire::FeedQuery::NEXT_PAGE_SCROLL:
      url = GURL(kNextPageQueryUrl);
      break;
    case feedwire::FeedQuery::MANUAL_REFRESH:
      url = GURL(kFeedQueryUrl);
      break;
    default:
      std::move(callback).Run({});
      return;
  }

  AddMothershipPayloadQueryParams(/*is_post=*/false, base64proto,
                                  delegate_->GetLanguageTag(), &url);
  Send(url, "GET", /*request_body=*/std::string(),
       base::BindOnce(&ParseAndForwardResponse<QueryRequestResult,
                                               NetworkRequestType::kFeedQuery>,
                      std::move(callback)));
}

void FeedNetworkImpl::SendActionRequest(
    const feedwire::ActionRequest& request,
    base::OnceCallback<void(ActionRequestResult)> callback) {
  std::string binary_proto;
  request.SerializeToString(&binary_proto);

  GURL url(
      "https://www.google.com/httpservice/retry/ClankActionUploadService/"
      "ClankActionUpload");
  AddMothershipPayloadQueryParams(/*is_post=*/true, /*payload=*/std::string(),
                                  delegate_->GetLanguageTag(), &url);
  Send(url, "POST", std::move(binary_proto),
       base::BindOnce(
           &ParseAndForwardResponse<ActionRequestResult,
                                    NetworkRequestType::kUploadActions>,
           std::move(callback)));
}

void FeedNetworkImpl::CancelRequests() {
  pending_requests_.clear();
}

void FeedNetworkImpl::Send(const GURL& url,
                           const std::string& request_type,
                           std::string request_body,
                           base::OnceCallback<void(RawResponse)> callback) {
  auto fetch = std::make_unique<NetworkFetch>(
      url, request_type, std::move(request_body), identity_manager_,
      loader_factory_.get(), api_key_, tick_clock_, pref_service_);
  NetworkFetch* fetch_unowned = fetch.get();
  pending_requests_.emplace(std::move(fetch));

  // It's safe to pass base::Unretained(this) since deleting the network fetch
  // will prevent the callback from being completed.
  fetch_unowned->Start(base::BindOnce(&FeedNetworkImpl::SendComplete,
                                      base::Unretained(this), fetch_unowned,
                                      std::move(callback)));
}

void FeedNetworkImpl::SendComplete(
    NetworkFetch* fetch,
    base::OnceCallback<void(RawResponse)> callback,
    RawResponse raw_response) {
  DCHECK_EQ(1UL, pending_requests_.count(fetch));
  pending_requests_.erase(fetch);

  std::move(callback).Run(std::move(raw_response));
}

}  // namespace feed

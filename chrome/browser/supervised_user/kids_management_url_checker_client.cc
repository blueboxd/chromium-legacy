// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/kids_management_url_checker_client.h"

#include <utility>

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "components/google/core/common/google_util.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_status.h"
#include "services/identity/public/cpp/scope_set.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/url_constants.h"

enum class KidsManagementClassification { kAllowed, kRestricted, kUnknown };

enum class KidsManagementResponseStatus {
  kSuccess = 0,
  kNetworkError = 1,
  kUnexpectedClassificationValue = 2,
  kParsingError = 3,
  kTokenError = 4,
  kHttpError = 5,
  kMaxValue = kHttpError,
};

struct KidsManagementURLCheckerResponse {
 public:
  KidsManagementClassification classification;
  KidsManagementResponseStatus status;

  static KidsManagementURLCheckerResponse ForSuccess(
      KidsManagementClassification classification) {
    return KidsManagementURLCheckerResponse(
        KidsManagementResponseStatus::kSuccess, classification);
  }

  static KidsManagementURLCheckerResponse ForFailure(
      KidsManagementResponseStatus status) {
    return KidsManagementURLCheckerResponse(
        status, KidsManagementClassification::kUnknown);
  }

 private:
  KidsManagementURLCheckerResponse(
      const KidsManagementResponseStatus& status,
      const KidsManagementClassification& classification)
      : classification(classification), status(status) {}
};

namespace {

const char kClassifyUrlApiPath[] =
    "https://kidsmanagement-pa.googleapis.com/kidsmanagement/v1/people/"
    "me:classifyUrl";

const char kKidPermissionScope[] =
    "https://www.googleapis.com/auth/kid.permission";
const char kDataContentType[] = "application/x-www-form-urlencoded";
const char kDataFormat[] = "url=%s&region_code=%s";
// Possible responses from KidsManagement ClassifyUrl RPC allowed and restricted
const char kAllowed[] = "allowed";
const char kRestricted[] = "restricted";

// Builds the POST data for KidsManagement ClassifyURL requests.
std::string BuildRequestData(const GURL& url, const std::string& region_code) {
  std::string query = net::EscapeQueryParamValue(url.spec(), true);
  return base::StringPrintf(kDataFormat, query.c_str(), region_code.c_str());
}

// Parses a KidsManagement API |response| and returns the classification.
// On failure returns classification kUnknown and status with the reason for
// failure.
KidsManagementURLCheckerResponse ParseResponse(const std::string& response) {
  base::Optional<base::Value> optional_value = base::JSONReader::Read(response);
  const base::DictionaryValue* dict = nullptr;
  if (!optional_value || !optional_value.value().GetAsDictionary(&dict)) {
    DLOG(WARNING) << "ParseResponse failed to parse global dictionary";
    return KidsManagementURLCheckerResponse::ForFailure(
        KidsManagementResponseStatus::kParsingError);
  }

  const base::Value* classification_value =
      dict->FindKey("displayClassification");
  if (!classification_value) {
    DLOG(WARNING) << "ParseResponse failed to parse displayClassification";
    return KidsManagementURLCheckerResponse::ForFailure(
        KidsManagementResponseStatus::kParsingError);
  }

  const std::string classification_string = classification_value->GetString();
  if (classification_string == kAllowed) {
    return KidsManagementURLCheckerResponse::ForSuccess(
        KidsManagementClassification::kAllowed);
  } else if (classification_string == kRestricted) {
    return KidsManagementURLCheckerResponse::ForSuccess(
        KidsManagementClassification::kRestricted);
  }
  DLOG(WARNING) << "ParseResponse expected a valid displayClassification";
  return KidsManagementURLCheckerResponse::ForFailure(
      KidsManagementResponseStatus::kUnexpectedClassificationValue);
}

safe_search_api::ClientClassification ToClientClassification(
    const KidsManagementClassification& classification) {
  switch (classification) {
    case KidsManagementClassification::kAllowed:
      return safe_search_api::ClientClassification::kAllowed;
    case KidsManagementClassification::kRestricted:
      return safe_search_api::ClientClassification::kRestricted;
    case KidsManagementClassification::kUnknown:
      return safe_search_api::ClientClassification::kUnknown;
  }
}

}  // namespace

struct KidsManagementURLCheckerClient::Check {
  Check(const GURL& url, ClientCheckCallback callback);
  ~Check();
  Check(Check&&) = default;

  GURL url;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher;
  bool access_token_expired;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader;
  ClientCheckCallback callback;
  base::TimeTicks start_time;
};

KidsManagementURLCheckerClient::Check::Check(const GURL& url,
                                             ClientCheckCallback callback)
    : url(url),
      access_token_expired(false),
      callback(std::move(callback)),
      start_time(base::TimeTicks::Now()) {}

KidsManagementURLCheckerClient::Check::~Check() {
  DCHECK(!callback);
}

KidsManagementURLCheckerClient::KidsManagementURLCheckerClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& country,
    signin::IdentityManager* identity_manager)
    : url_loader_factory_(std::move(url_loader_factory)),
      traffic_annotation_(
          net::DefineNetworkTrafficAnnotation("kids_management_url_checker", R"(
        semantics {
          sender: "Supervised Users"
          description:
            "Checks whether a given URL (or set of URLs) is considered safe by "
            "Google KidsManagement."
          trigger:
            "If the parent enabled this feature for the child account, this is "
            "sent for every navigation."
          data:
            "The request is authenticated with an OAuth2 access token "
            "identifying the Google account. The URL to be checked."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature is only used in child accounts and cannot be "
            "disabled by settings. Parent accounts can disable it in the "
            "family dashboard."
          chrome_policy {
            RestrictSigninToPattern {
              policy_options {mode: MANDATORY}
              RestrictSigninToPattern: "*@manageddomain.com"
            }
          }
        })")),
      country_(country),
      identity_manager_(identity_manager) {}

KidsManagementURLCheckerClient::~KidsManagementURLCheckerClient() = default;

void KidsManagementURLCheckerClient::CheckURL(const GURL& url,
                                              ClientCheckCallback callback) {
  checks_in_progress_.push_front(
      std::make_unique<Check>(url, std::move(callback)));

  StartFetching(checks_in_progress_.begin());
}

void KidsManagementURLCheckerClient::StartFetching(CheckList::iterator it) {
  identity::ScopeSet scopes{kKidPermissionScope};
  // It is safe to use Unretained(this) here given that the callback
  // will not be invoked if this object is deleted. Likewise, |it|
  // only comes from |checks_in_progress_|, which are owned by this object
  // too.
  it->get()->access_token_fetcher =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          "kids_url_classifier", identity_manager_, scopes,
          base::BindOnce(
              &KidsManagementURLCheckerClient::OnAccessTokenFetchComplete,
              base::Unretained(this), it),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate);
}

void KidsManagementURLCheckerClient::OnAccessTokenFetchComplete(
    CheckList::iterator it,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo token_info) {
  if (error.state() != GoogleServiceAuthError::NONE) {
    DLOG(WARNING) << "Token error: " << error.ToString();

    DispatchResult(it, KidsManagementURLCheckerResponse::ForFailure(
                           KidsManagementResponseStatus::kTokenError));
    return;
  }
  Check* check = it->get();

  DVLOG(1) << "Checking URL " << check->url;
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kClassifyUrlApiPath);
  resource_request->method = "POST";
  resource_request->load_flags =
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES;
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StringPrintf(supervised_users::kAuthorizationHeaderFormat,
                         token_info.token.c_str()));
  check->simple_url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation_);
  check->simple_url_loader->AttachStringForUpload(
      BuildRequestData(check->url, country_), kDataContentType);

  check->simple_url_loader->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&KidsManagementURLCheckerClient::OnSimpleLoaderComplete,
                     base::Unretained(this), it, token_info),
      /*max_body_size*/ 128);
}

void KidsManagementURLCheckerClient::OnSimpleLoaderComplete(
    CheckList::iterator it,
    signin::AccessTokenInfo token_info,
    std::unique_ptr<std::string> response_body) {
  Check* check = it->get();
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      std::move(check->simple_url_loader);
  int response_code = -1;
  if (simple_url_loader->ResponseInfo() &&
      simple_url_loader->ResponseInfo()->headers) {
    response_code = simple_url_loader->ResponseInfo()->headers->response_code();
  }

  // Handle first HTTP_UNAUTHORIZED response by removing access token and
  // restarting the request from the beginning (fetching access token).
  if (response_code == net::HTTP_UNAUTHORIZED && !check->access_token_expired) {
    DLOG(WARNING) << "Access token expired:\n" << token_info.token;
    check->access_token_expired = true;
    identity::ScopeSet scopes{kKidPermissionScope};
    identity_manager_->RemoveAccessTokenFromCache(
        identity_manager_->GetPrimaryAccountId(), scopes, token_info.token);
    StartFetching(it);
    return;
  }

  int net_error = simple_url_loader->NetError();
  if (net_error != net::OK) {
    DLOG(WARNING) << "Network error " << net_error;
    DispatchResult(it, KidsManagementURLCheckerResponse::ForFailure(
                           KidsManagementResponseStatus::kNetworkError));
    return;
  }

  if (response_code != net::HTTP_OK) {
    DLOG(WARNING) << "HTTP error " << response_code;
    DLOG(WARNING) << "Response: " << response_body.get();
    DispatchResult(it, KidsManagementURLCheckerResponse::ForFailure(
                           KidsManagementResponseStatus::kHttpError));
    return;
  }

  // |response_body| is nullptr only in case of failure
  if (!response_body) {
    DLOG(WARNING) << "URL request failed! Letting through...";
    DispatchResult(it, KidsManagementURLCheckerResponse::ForFailure(
                           KidsManagementResponseStatus::kNetworkError));
    return;
  }

  DispatchResult(it, ParseResponse(*response_body));
}

void KidsManagementURLCheckerClient::DispatchResult(
    CheckList::iterator it,
    KidsManagementURLCheckerResponse response) {
  if (response.status == KidsManagementResponseStatus::kSuccess) {
    UMA_HISTOGRAM_TIMES("ManagedUsers.KidsManagementClassifyUrlSuccessDelay",
                        base::TimeTicks::Now() - it->get()->start_time);
  } else {
    UMA_HISTOGRAM_TIMES("ManagedUsers.KidsManagementClassifyUrlFailureDelay",
                        base::TimeTicks::Now() - it->get()->start_time);
  }

  UMA_HISTOGRAM_ENUMERATION(
      "ManagedUsers.KidsManagementUrlCheckerResponseStatus", response.status);

  safe_search_api::ClientClassification api_classification =
      ToClientClassification(response.classification);
  std::move(it->get()->callback).Run(it->get()->url, api_classification);

  checks_in_progress_.erase(it);
}

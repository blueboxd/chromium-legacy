// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/federated_learning/floc_remote_permission_service.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/sync/driver/sync_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace federated_learning {

namespace {

const char kOAuthScope[] = "https://www.googleapis.com/auth/chromesync";

const char kQueryFlocPermissionUrl[] =
    "https://adservice.google.com/settings/do_ad_settings_allow_floc_poc";

// The maximum number of retries for the SimpleURLLoader requests.
const size_t kMaxRetries = 1;

// TODO(yaoxia): Move this and WebHistoryService::Request to a common place.
class RequestImpl : public FlocRemotePermissionService::Request {
 public:
  RequestImpl(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& url,
      FlocRemotePermissionService::CompletionCallback callback,
      const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation)
      : identity_manager_(identity_manager),
        url_loader_factory_(std::move(url_loader_factory)),
        url_(url),
        response_code_(0),
        auth_retry_count_(0),
        callback_(std::move(callback)),
        partial_traffic_annotation_(partial_traffic_annotation) {
    DCHECK(identity_manager_);
    DCHECK(url_loader_factory_);
  }

  ~RequestImpl() override = default;

  // Returns the response code received from the server, which will only be
  // valid if the request succeeded.
  int GetResponseCode() override { return response_code_; }

  // Returns the contents of the response body received from the server.
  const std::string& GetResponseBody() override { return response_body_; }

 private:
  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  signin::AccessTokenInfo access_token_info) {
    access_token_fetcher_.reset();

    if (error.state() != GoogleServiceAuthError::NONE) {
      std::move(callback_).Run(this, false);

      // It is valid for the callback to delete |this|, so do not access any
      // members below here.
      return;
    }

    DCHECK(!access_token_info.token.empty());
    access_token_ = access_token_info.token;

    // Got an access token -- start the actual API request.
    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = url_;
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    resource_request->method = "GET";
    resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                        "Bearer " + access_token_info.token);
    resource_request->headers.SetHeader(
        "X-Developer-Key", GaiaUrls::GetInstance()->oauth2_chrome_client_id());
    if (!user_agent_.empty()) {
      resource_request->headers.SetHeader(net::HttpRequestHeaders::kUserAgent,
                                          user_agent_);
    }

    // TODO(yaoxia): Once we have a client, incorporate
    // |partial_traffic_annotation_| to make an actual
    // CompleteNetworkTrafficAnnotation.
    simple_url_loader_ = network::SimpleURLLoader::Create(
        std::move(resource_request), MISSING_TRAFFIC_ANNOTATION);
    simple_url_loader_->SetRetryOptions(kMaxRetries,
                                        network::SimpleURLLoader::RETRY_ON_5XX);
    simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory_.get(),
        base::BindOnce(&RequestImpl::OnSimpleLoaderComplete,
                       base::Unretained(this)));
  }

  void Start() override {
    signin::ScopeSet oauth_scopes;
    oauth_scopes.insert(kOAuthScope);

    access_token_fetcher_ =
        std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
            "floc_remote_permission", identity_manager_, oauth_scopes,
            base::BindOnce(&RequestImpl::OnAccessTokenFetchComplete,
                           base::Unretained(this)),
            signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate);
  }

  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body) {
    response_code_ = -1;
    if (simple_url_loader_->ResponseInfo() &&
        simple_url_loader_->ResponseInfo()->headers) {
      response_code_ =
          simple_url_loader_->ResponseInfo()->headers->response_code();
    }
    simple_url_loader_.reset();

    // If the response code indicates that the token might not be valid,
    // invalidate the token and try again.
    if (response_code_ == net::HTTP_UNAUTHORIZED && ++auth_retry_count_ <= 1) {
      signin::ScopeSet oauth_scopes;
      oauth_scopes.insert(kOAuthScope);
      identity_manager_->RemoveAccessTokenFromCache(
          identity_manager_->GetPrimaryAccountId(), oauth_scopes,
          access_token_);

      access_token_.clear();
      Start();
      return;
    }
    if (response_body) {
      response_body_ = std::move(*response_body);
    } else {
      response_body_.clear();
    }
    std::move(callback_).Run(this, true);
    // It is valid for the callback to delete |this|, so do not access any
    // members below here.
  }

  signin::IdentityManager* identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The URL of the API endpoint.
  const GURL url_;

  // The user agent header used with this request.
  std::string user_agent_;

  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  // The current OAuth2 access token.
  std::string access_token_;

  // Handles the actual API requests after the OAuth token is acquired.
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  // Holds the response code received from the server.
  int response_code_;

  // Holds the response body received from the server.
  std::string response_body_;

  // The number of times this request has already been retried due to
  // authorization problems.
  int auth_retry_count_;

  // The callback to execute when the query is complete.
  FlocRemotePermissionService::CompletionCallback callback_;

  // Partial Network traffic annotation used to create SimpleURLLoader for this
  // request.
  const net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation_;
};

std::unique_ptr<base::ListValue> ReadResponseAsList(
    FlocRemotePermissionService::Request* request) {
  std::unique_ptr<base::ListValue> result;
  if (request->GetResponseCode() == net::HTTP_OK) {
    std::unique_ptr<base::Value> value =
        base::JSONReader::ReadDeprecated(request->GetResponseBody());
    if (value && value->is_list())
      result.reset(static_cast<base::ListValue*>(value.release()));
    else
      DLOG(WARNING) << "Non-JSON-Array response received from the server.";
  }
  return result;
}

}  // namespace

FlocRemotePermissionService::Request::Request() = default;

FlocRemotePermissionService::Request::~Request() = default;

FlocRemotePermissionService::FlocRemotePermissionService(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)) {}

FlocRemotePermissionService::~FlocRemotePermissionService() = default;

std::unique_ptr<FlocRemotePermissionService::Request>
FlocRemotePermissionService::CreateRequest(
    const GURL& url,
    CompletionCallback callback,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  return std::make_unique<RequestImpl>(identity_manager_, url_loader_factory_,
                                       url, std::move(callback),
                                       partial_traffic_annotation);
}

void FlocRemotePermissionService::QueryFlocPermission(
    QueryFlocPermissionCallback callback,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  // Wrap the original callback into a generic completion callback.
  CompletionCallback completion_callback = base::BindOnce(
      &FlocRemotePermissionService::QueryFlocPermissionCompletionCallback,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  GURL url(kQueryFlocPermissionUrl);
  std::unique_ptr<Request> request = CreateRequest(
      url, std::move(completion_callback), partial_traffic_annotation);
  Request* request_raw_ptr = request.get();
  pending_floc_permission_requests_[request_raw_ptr] = std::move(request);
  request_raw_ptr->Start();
}

void FlocRemotePermissionService::QueryFlocPermissionCompletionCallback(
    FlocRemotePermissionService::QueryFlocPermissionCallback callback,
    FlocRemotePermissionService::Request* request,
    bool success) {
  std::unique_ptr<Request> request_ptr =
      std::move(pending_floc_permission_requests_[request]);
  pending_floc_permission_requests_.erase(request);

  std::unique_ptr<base::ListValue> response_value;
  bool swaa = false;
  bool nac = false;
  bool account_type = false;

  if (success) {
    response_value = ReadResponseAsList(request);
    if (response_value) {
      base::Value::ListView l = response_value->GetList();
      if (l.size() == 3) {
        if (l[0].is_bool())
          swaa = l[0].GetBool();
        if (l[1].is_bool())
          nac = l[1].GetBool();
        if (l[2].is_bool())
          account_type = l[2].GetBool();
      }
    }
  }

  std::move(callback).Run(swaa && nac && account_type);
}

}  // namespace federated_learning

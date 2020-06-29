// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/client/nearby_share_client_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/nearby_sharing/client/nearby_share_api_call_flow_impl.h"
#include "chrome/browser/nearby_sharing/client/nearby_share_switches.h"
#include "chrome/browser/nearby_sharing/proto/certificate_rpc.pb.h"
#include "chrome/browser/nearby_sharing/proto/contact_rpc.pb.h"
#include "chrome/browser/nearby_sharing/proto/device_rpc.pb.h"
#include "chrome/browser/nearby_sharing/proto/rpc_resources.pb.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

// -------------------- Nearby Share Service v1 Endpoints --------------------

const char kDefaultNearbyShareV1HTTPHost[] =
    "https://www.nearbysharing-pa.googleapis.com";

const char kNearbyShareV1Path[] = "v1/";

const char kUpdateDevicePath[] = "users/me/devices/";
const char kCheckContactsReachabilityPath[] = "contactsReachability:check";
const char kListContactPeoplePathSeg1[] = "users/me/devices/";
const char kListContactPeoplePathSeg2[] = "/contactRecords";
const char kListPublicCertificatesPathSeg1[] = "users/me/devices/";
const char kListPublicCertificatesPathSeg2[] = "/publicCertificates";

const char kPageSize[] = "page_size";
const char kPageToken[] = "page_token";
const char kSecretIds[] = "secret_ids";

// TODO(cclem) figure out scope
const char kNearbyShareOAuth2Scope[] = "";

// Creates the full Nearby Share v1 URL for endpoint to the API with
// |request_path|.
GURL CreateV1RequestUrl(const std::string& request_path) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  GURL google_apis_url = command_line->HasSwitch(switches::kNearbyShareHTTPHost)
                             ? GURL(command_line->GetSwitchValueASCII(
                                   switches::kNearbyShareHTTPHost))
                             : GURL(kDefaultNearbyShareV1HTTPHost);
  return google_apis_url.Resolve(kNearbyShareV1Path + request_path);
}

NearbyShareApiCallFlow::QueryParameters
ListContactPeopleRequestToQueryParameters(
    const nearbyshare::proto::ListContactPeopleRequest& request) {
  NearbyShareApiCallFlow::QueryParameters query_parameters;
  if (request.page_size() > 0) {
    query_parameters.emplace_back(kPageSize,
                                  base::NumberToString(request.page_size()));
  }
  if (!request.page_token().empty()) {
    query_parameters.emplace_back(kPageToken, request.page_token());
  }
  return query_parameters;
}

NearbyShareApiCallFlow::QueryParameters
ListPublicCertificatesRequestToQueryParameters(
    const nearbyshare::proto::ListPublicCertificatesRequest& request) {
  NearbyShareApiCallFlow::QueryParameters query_parameters;
  if (request.page_size() > 0) {
    query_parameters.emplace_back(kPageSize,
                                  base::NumberToString(request.page_size()));
  }
  if (!request.page_token().empty()) {
    query_parameters.emplace_back(kPageToken, request.page_token());
  }
  for (int i = 0; i < request.secret_ids_size(); ++i) {
    query_parameters.emplace_back(kSecretIds, request.secret_ids(i));
  }
  return query_parameters;
}

}  // namespace

NearbyShareClientImpl::NearbyShareClientImpl(
    std::unique_ptr<NearbyShareApiCallFlow> api_call_flow,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : api_call_flow_(std::move(api_call_flow)),
      identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)),
      has_call_started_(false) {}

NearbyShareClientImpl::~NearbyShareClientImpl() = default;

void NearbyShareClientImpl::UpdateDevice(
    const nearbyshare::proto::UpdateDeviceRequest& request,
    UpdateDeviceCallback&& callback,
    ErrorCallback&& error_callback,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  // TODO(cclem): Use correct device identifier
  MakeApiCall(CreateV1RequestUrl(kUpdateDevicePath + request.device().name()),
              RequestType::kPatch, request.SerializeAsString(),
              base::nullopt /* request_as_query_parameters */,
              std::move(callback), std::move(error_callback),
              partial_traffic_annotation);
}

void NearbyShareClientImpl::CheckContactsReachability(
    const nearbyshare::proto::CheckContactsReachabilityRequest& request,
    CheckContactsReachabilityCallback&& callback,
    ErrorCallback&& error_callback,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  MakeApiCall(CreateV1RequestUrl(kCheckContactsReachabilityPath),
              RequestType::kPost, request.SerializeAsString(),
              base::nullopt /* request_as_query_parameters */,
              std::move(callback), std::move(error_callback),
              partial_traffic_annotation);
}

void NearbyShareClientImpl::ListContactPeople(
    const nearbyshare::proto::ListContactPeopleRequest& request,
    ListContactPeopleCallback&& callback,
    ErrorCallback&& error_callback,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  // TODO(cclem): Use correct identifier in URL
  MakeApiCall(CreateV1RequestUrl(kListContactPeoplePathSeg1 + request.parent() +
                                 kListContactPeoplePathSeg2),
              RequestType::kGet, base::nullopt /* serialized_request */,
              ListContactPeopleRequestToQueryParameters(request),
              std::move(callback), std::move(error_callback),
              partial_traffic_annotation);
}

void NearbyShareClientImpl::ListPublicCertificates(
    const nearbyshare::proto::ListPublicCertificatesRequest& request,
    ListPublicCertificatesCallback&& callback,
    ErrorCallback&& error_callback,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  // TODO(cclem): Use correct identifier in URL
  MakeApiCall(
      CreateV1RequestUrl(kListPublicCertificatesPathSeg1 + request.parent() +
                         kListPublicCertificatesPathSeg2),
      RequestType::kGet, base::nullopt /* serialized_request */,
      ListPublicCertificatesRequestToQueryParameters(request),
      std::move(callback), std::move(error_callback),
      partial_traffic_annotation);
}

std::string NearbyShareClientImpl::GetAccessTokenUsed() {
  return access_token_used_;
}

template <class ResponseProto>
void NearbyShareClientImpl::MakeApiCall(
    const GURL& request_url,
    RequestType request_type,
    const base::Optional<std::string>& serialized_request,
    const base::Optional<NearbyShareApiCallFlow::QueryParameters>&
        request_as_query_parameters,
    base::OnceCallback<void(const ResponseProto&)>&& response_callback,
    ErrorCallback&& error_callback,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  DCHECK(!has_call_started_)
      << "NearbyShareClientImpl::MakeApiCall(): Tried to make an API "
      << "call, but the client had already been used.";
  has_call_started_ = true;

  api_call_flow_->SetPartialNetworkTrafficAnnotation(
      partial_traffic_annotation);

  request_url_ = request_url;
  error_callback_ = std::move(error_callback);

  OAuth2AccessTokenManager::ScopeSet scopes;
  scopes.insert(kNearbyShareOAuth2Scope);

  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          "nearby_share_client", identity_manager_, scopes,
          base::BindOnce(
              &NearbyShareClientImpl::OnAccessTokenFetched<ResponseProto>,
              weak_ptr_factory_.GetWeakPtr(), request_type, serialized_request,
              request_as_query_parameters, std::move(response_callback)),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
          signin::ConsentLevel::kNotRequired);
}

template <class ResponseProto>
void NearbyShareClientImpl::OnAccessTokenFetched(
    RequestType request_type,
    const base::Optional<std::string>& serialized_request,
    const base::Optional<NearbyShareApiCallFlow::QueryParameters>&
        request_as_query_parameters,
    base::OnceCallback<void(const ResponseProto&)>&& response_callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    OnApiCallFailed(NearbyShareRequestError::kAuthenticationError);
    return;
  }
  access_token_used_ = access_token_info.token;

  switch (request_type) {
    case RequestType::kGet:
      DCHECK(request_as_query_parameters && !serialized_request);
      api_call_flow_->StartGetRequest(
          request_url_, *request_as_query_parameters, url_loader_factory_,
          access_token_used_,
          base::BindOnce(&NearbyShareClientImpl::OnFlowSuccess<ResponseProto>,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(response_callback)),
          base::BindOnce(&NearbyShareClientImpl::OnApiCallFailed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    case RequestType::kPost:
      DCHECK(serialized_request && !request_as_query_parameters);
      api_call_flow_->StartPostRequest(
          request_url_, *serialized_request, url_loader_factory_,
          access_token_used_,
          base::BindOnce(&NearbyShareClientImpl::OnFlowSuccess<ResponseProto>,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(response_callback)),
          base::BindOnce(&NearbyShareClientImpl::OnApiCallFailed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    case RequestType::kPatch:
      DCHECK(serialized_request && !request_as_query_parameters);
      api_call_flow_->StartPatchRequest(
          request_url_, *serialized_request, url_loader_factory_,
          access_token_used_,
          base::BindOnce(&NearbyShareClientImpl::OnFlowSuccess<ResponseProto>,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(response_callback)),
          base::BindOnce(&NearbyShareClientImpl::OnApiCallFailed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
  }
}

template <class ResponseProto>
void NearbyShareClientImpl::OnFlowSuccess(
    base::OnceCallback<void(const ResponseProto&)>&& result_callback,
    const std::string& serialized_response) {
  ResponseProto response;
  if (!response.ParseFromString(serialized_response)) {
    OnApiCallFailed(NearbyShareRequestError::kResponseMalformed);
    return;
  }
  std::move(result_callback).Run(response);
}

void NearbyShareClientImpl::OnApiCallFailed(NearbyShareRequestError error) {
  std::move(error_callback_).Run(error);
}

NearbyShareClientFactoryImpl::NearbyShareClientFactoryImpl(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)) {}

NearbyShareClientFactoryImpl::~NearbyShareClientFactoryImpl() = default;

std::unique_ptr<NearbyShareClient>
NearbyShareClientFactoryImpl::CreateInstance() {
  return std::make_unique<NearbyShareClientImpl>(
      std::make_unique<NearbyShareApiCallFlowImpl>(), identity_manager_,
      url_loader_factory_);
}

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/cors_url_loader.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "net/base/load_flags.h"
#include "services/network/cors/preflight_controller.h"
#include "services/network/loader_util.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/cpp/header_util.h"
#include "url/url_util.h"

namespace network {

namespace cors {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CompletionStatusMetric {
  kPassedWhenCorsFlagUnset = 0,
  kFailedWhenCorsFlagUnset = 1,
  kPassedWhenCorsFlagSet = 2,
  kFailedWhenCorsFlagSet = 3,
  kBlockedByCors = 4,

  kMaxValue = kBlockedByCors,
};

bool NeedsPreflight(const ResourceRequest& request) {
  if (!IsCorsEnabledRequestMode(request.mode))
    return false;

  if (request.is_external_request)
    return true;

  if (request.mode == mojom::RequestMode::kCorsWithForcedPreflight) {
    return true;
  }

  if (request.cors_preflight_policy ==
      mojom::CorsPreflightPolicy::kPreventPreflight) {
    return false;
  }

  if (!IsCorsSafelistedMethod(request.method))
    return true;

  return !CorsUnsafeNotForbiddenRequestHeaderNames(
              request.headers.GetHeaderVector(), request.is_revalidating)
              .empty();
}

void ReportCompletionStatusMetric(bool fetch_cors_flag,
                                  const URLLoaderCompletionStatus& status) {
  CompletionStatusMetric metric;
  if (status.error_code == net::OK) {
    metric = fetch_cors_flag ? CompletionStatusMetric::kPassedWhenCorsFlagSet
                             : CompletionStatusMetric::kPassedWhenCorsFlagUnset;
  } else if (status.cors_error_status) {
    metric = CompletionStatusMetric::kBlockedByCors;
  } else {
    metric = fetch_cors_flag ? CompletionStatusMetric::kFailedWhenCorsFlagSet
                             : CompletionStatusMetric::kFailedWhenCorsFlagUnset;
  }
  UMA_HISTOGRAM_ENUMERATION("Net.Cors.CompletionStatus", metric);
}

}  // namespace

CorsURLLoader::CorsURLLoader(
    mojom::URLLoaderRequest loader_request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    DeleteCallback delete_callback,
    const ResourceRequest& resource_request,
    mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojom::URLLoaderFactory* network_loader_factory,
    const base::Optional<url::Origin>& factory_bound_origin,
    const OriginAccessList* origin_access_list,
    const OriginAccessList* factory_bound_origin_access_list,
    PreflightController* preflight_controller)
    : binding_(this, std::move(loader_request)),
      routing_id_(routing_id),
      request_id_(request_id),
      options_(options),
      delete_callback_(std::move(delete_callback)),
      network_loader_factory_(network_loader_factory),
      network_client_binding_(this),
      request_(resource_request),
      forwarding_client_(std::move(client)),
      traffic_annotation_(traffic_annotation),
      factory_bound_origin_(factory_bound_origin),
      origin_access_list_(origin_access_list),
      factory_bound_origin_access_list_(factory_bound_origin_access_list),
      preflight_controller_(preflight_controller) {
  binding_.set_connection_error_handler(base::BindOnce(
      &CorsURLLoader::OnConnectionError, base::Unretained(this)));
  DCHECK(network_loader_factory_);
  DCHECK(origin_access_list_);
  DCHECK(preflight_controller_);
  SetCorsFlagIfNeeded();
}

CorsURLLoader::~CorsURLLoader() {
  // Close pipes first to ignore possible subsequent callback invocations
  // cased by |network_loader_|
  network_client_binding_.Close();
}

void CorsURLLoader::Start() {
  if (fetch_cors_flag_ && IsCorsEnabledRequestMode(request_.mode)) {
    // Username and password should be stripped in a CORS-enabled request.
    if (request_.url.has_username() || request_.url.has_password()) {
      GURL::Replacements replacements;
      replacements.SetUsernameStr("");
      replacements.SetPasswordStr("");
      request_.url = request_.url.ReplaceComponents(replacements);
    }
  }
  StartRequest();
}

void CorsURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const base::Optional<GURL>& new_url) {
  if (!network_loader_ || !deferred_redirect_url_) {
    HandleComplete(URLLoaderCompletionStatus(net::ERR_FAILED));
    return;
  }

  if (new_url &&
      (new_url->GetOrigin() != deferred_redirect_url_->GetOrigin())) {
    NOTREACHED() << "Can only change the URL within the same origin.";
    HandleComplete(URLLoaderCompletionStatus(net::ERR_FAILED));
    return;
  }

  deferred_redirect_url_.reset();

  // When the redirect mode is "error", the client is not expected to
  // call this function. Let's abort the request.
  if (request_.redirect_mode == mojom::RedirectMode::kError) {
    HandleComplete(URLLoaderCompletionStatus(net::ERR_FAILED));
    return;
  }

  // Does not allow modifying headers that are stored in |cors_exempt_headers|.
  for (const auto& header : modified_headers.GetHeaderVector()) {
    if (request_.cors_exempt_headers.HasHeader(header.key)) {
      LOG(WARNING) << "A client is trying to modify header value for '"
                   << header.key << "', but it is not permitted.";
      HandleComplete(URLLoaderCompletionStatus(net::ERR_INVALID_ARGUMENT));
      return;
    }
  }

  LogConcerningRequestHeaders(modified_headers,
                              true /* added_during_redirect */);

  for (const auto& name : removed_headers) {
    request_.headers.RemoveHeader(name);
    request_.cors_exempt_headers.RemoveHeader(name);
  }
  request_.headers.MergeFrom(modified_headers);

  if (!AreRequestHeadersSafe(request_.headers)) {
    HandleComplete(URLLoaderCompletionStatus(net::ERR_INVALID_ARGUMENT));
    return;
  }

  const std::string original_method = std::move(request_.method);
  request_.url = redirect_info_.new_url;
  request_.method = redirect_info_.new_method;
  request_.referrer = GURL(redirect_info_.new_referrer);
  request_.referrer_policy = redirect_info_.new_referrer_policy;

  // The request method can be changed to "GET". In this case we need to
  // reset the request body manually.
  if (request_.method == net::HttpRequestHeaders::kGetMethod)
    request_.request_body = nullptr;

  const bool original_fetch_cors_flag = fetch_cors_flag_;
  SetCorsFlagIfNeeded();

  // We cannot use FollowRedirect for a request with preflight (i.e., when both
  // |fetch_cors_flag_| and |NeedsPreflight(request_)| are true).
  //
  // When |original_fetch_cors_flag| is false, |fetch_cors_flag_| is true and
  // |NeedsPreflight(request)| is false, the net/ implementation won't attach an
  // "origin" header on redirect, as the original request didn't have one.
  //
  // When the request method is changed (due to 302 status code, for example),
  // the net/ implementation removes the origin header.
  //
  // In such cases we need to re-issue a request manually in order to attach the
  // correct origin header. For "no-cors" requests we rely on redirect logic in
  // net/ (specifically in net/url_request/redirect_util.cc).
  //
  // After both OOR-CORS and network service are fully shipped, we may be able
  // to remove the logic in net/.
  if ((fetch_cors_flag_ && NeedsPreflight(request_)) ||
      (!original_fetch_cors_flag && fetch_cors_flag_) ||
      (fetch_cors_flag_ && original_method != request_.method)) {
    DCHECK_NE(request_.mode, mojom::RequestMode::kNoCors);
    network_client_binding_.Unbind();
    StartRequest();
    return;
  }

  response_tainting_ = CalculateResponseTainting(
      request_.url, request_.mode, request_.request_initiator, fetch_cors_flag_,
      tainted_, origin_access_list_);
  network_loader_->FollowRedirect(removed_headers, modified_headers, new_url);
}

void CorsURLLoader::ProceedWithResponse() {
  NOTREACHED();
}

void CorsURLLoader::SetPriority(net::RequestPriority priority,
                                int32_t intra_priority_value) {
  if (network_loader_)
    network_loader_->SetPriority(priority, intra_priority_value);
}

void CorsURLLoader::PauseReadingBodyFromNet() {
  if (network_loader_)
    network_loader_->PauseReadingBodyFromNet();
}

void CorsURLLoader::ResumeReadingBodyFromNet() {
  if (network_loader_)
    network_loader_->ResumeReadingBodyFromNet();
}

void CorsURLLoader::OnReceiveResponse(
    const ResourceResponseHead& response_head) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!deferred_redirect_url_);

  int response_status_code =
      response_head.headers ? response_head.headers->response_code() : 0;

  const bool is_304_for_revalidation =
      request_.is_revalidating && response_status_code == 304;
  if (fetch_cors_flag_ && !is_304_for_revalidation) {
    const auto error_status = CheckAccess(
        request_.url, response_status_code,
        GetHeaderString(response_head, header_names::kAccessControlAllowOrigin),
        GetHeaderString(response_head,
                        header_names::kAccessControlAllowCredentials),
        request_.credentials_mode,
        tainted_ ? url::Origin() : *request_.request_initiator);
    if (error_status) {
      HandleComplete(URLLoaderCompletionStatus(*error_status));
      return;
    }
  }

  ResourceResponseHead response_head_to_pass = response_head;
  response_head_to_pass.response_type = response_tainting_;
  forwarding_client_->OnReceiveResponse(response_head_to_pass);
}

void CorsURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const ResourceResponseHead& response_head) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!deferred_redirect_url_);

  if (request_.redirect_mode == mojom::RedirectMode::kManual) {
    deferred_redirect_url_ = std::make_unique<GURL>(redirect_info.new_url);
    forwarding_client_->OnReceiveRedirect(redirect_info, response_head);
    return;
  }

  // If |CORS flag| is set and a CORS check for |request| and |response| returns
  // failure, then return a network error.
  if (fetch_cors_flag_ && IsCorsEnabledRequestMode(request_.mode)) {
    const auto error_status = CheckAccess(
        request_.url, response_head.headers->response_code(),
        GetHeaderString(response_head, header_names::kAccessControlAllowOrigin),
        GetHeaderString(response_head,
                        header_names::kAccessControlAllowCredentials),
        request_.credentials_mode,
        tainted_ ? url::Origin() : *request_.request_initiator);
    if (error_status) {
      HandleComplete(URLLoaderCompletionStatus(*error_status));
      return;
    }
  }

  // Because we initiate a new request on redirect in some cases, we cannot
  // rely on the redirect logic in the network stack. Hence we need to
  // implement some logic in
  // https://fetch.spec.whatwg.org/#http-redirect-fetch here.

  // If |request|’s redirect count is twenty, return a network error.
  // Increase |request|’s redirect count by one.
  if (redirect_count_++ == 20) {
    HandleComplete(URLLoaderCompletionStatus(net::ERR_TOO_MANY_REDIRECTS));
    return;
  }

  const auto error_status = CheckRedirectLocation(
      redirect_info.new_url, request_.mode, request_.request_initiator,
      fetch_cors_flag_, tainted_);
  if (error_status) {
    HandleComplete(URLLoaderCompletionStatus(*error_status));
    return;
  }

  // TODO(yhirano): Implement the following (Note: this is needed when upload
  // streaming is implemented):
  // If |actualResponse|’s status is not 303, |request|’s body is non-null, and
  // |request|’s body’s source is null, then return a network error.

  // If |actualResponse|’s location URL’s origin is not same origin with
  // |request|’s current url’s origin and |request|’s origin is not same origin
  // with |request|’s current url’s origin, then set |request|’s tainted origin
  // flag.
  if (request_.request_initiator &&
      (!url::Origin::Create(redirect_info.new_url)
            .IsSameOriginWith(url::Origin::Create(request_.url)) &&
       !request_.request_initiator->IsSameOriginWith(
           url::Origin::Create(request_.url)))) {
    tainted_ = true;
  }

  // TODO(yhirano): Implement the following:
  // If either |actualResponse|’s status is 301 or 302 and |request|’s method is
  // `POST`, or |actualResponse|’s status is 303, set |request|’s method to
  // `GET` and request’s body to null.

  // TODO(yhirano): Implement the following:
  // Invoke |set request’s referrer policy on redirect| on |request| and
  // |actualResponse|.

  redirect_info_ = redirect_info;

  deferred_redirect_url_ = std::make_unique<GURL>(redirect_info.new_url);

  auto response_head_to_pass = response_head;
  if (request_.redirect_mode == mojom::RedirectMode::kManual) {
    response_head_to_pass.response_type =
        mojom::FetchResponseType::kOpaqueRedirect;
  } else {
    response_head_to_pass.response_type = response_tainting_;
  }
  forwarding_client_->OnReceiveRedirect(redirect_info, response_head_to_pass);
}

void CorsURLLoader::OnUploadProgress(int64_t current_position,
                                     int64_t total_size,
                                     OnUploadProgressCallback ack_callback) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!deferred_redirect_url_);
  forwarding_client_->OnUploadProgress(current_position, total_size,
                                       std::move(ack_callback));
}

void CorsURLLoader::OnReceiveCachedMetadata(mojo_base::BigBuffer data) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!deferred_redirect_url_);
  forwarding_client_->OnReceiveCachedMetadata(std::move(data));
}

void CorsURLLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!deferred_redirect_url_);
  forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void CorsURLLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!deferred_redirect_url_);
  forwarding_client_->OnStartLoadingResponseBody(std::move(body));
}

void CorsURLLoader::OnComplete(const URLLoaderCompletionStatus& status) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);

  // |network_loader_| will call OnComplete at anytime when a problem happens
  // inside the URLLoader, e.g. on URLLoader::OnConnectionError call. We need
  // to expect it also happens even during redirect handling.
  DCHECK(!deferred_redirect_url_ || status.error_code != net::OK);

  URLLoaderCompletionStatus modified_status(status);
  if (status.error_code == net::OK)
    modified_status.cors_preflight_timing_info.swap(preflight_timing_info_);
  HandleComplete(modified_status);
}

void CorsURLLoader::StartRequest() {
  if (fetch_cors_flag_ &&
      !base::Contains(url::GetCorsEnabledSchemes(), request_.url.scheme())) {
    HandleComplete(URLLoaderCompletionStatus(
        CorsErrorStatus(mojom::CorsError::kCorsDisabledScheme)));
    return;
  }

  // If the |CORS flag| is set, |httpRequest|’s method is neither `GET` nor
  // `HEAD`, or |httpRequest|’s mode is "websocket", then append
  // `Origin`/the result of serializing a request origin with |httpRequest|, to
  // |httpRequest|’s header list.
  //
  // We exclude navigation requests to keep the existing behavior.
  // TODO(yhirano): Reconsider this.
  if (request_.mode != mojom::RequestMode::kNavigate &&
      request_.request_initiator &&
      (fetch_cors_flag_ ||
       (request_.method != "GET" && request_.method != "HEAD"))) {
    if (!fetch_cors_flag_ &&
        request_.headers.HasHeader(net::HttpRequestHeaders::kOrigin) &&
        request_.request_initiator->scheme() == "chrome-extension") {
      // We need to attach an origin header when the request's method is neither
      // GET nor HEAD. For requests made by an extension content scripts, we
      // want to attach page's origin, whereas the request's origin is the
      // content script's origin. See https://crbug.com/944704 for details.
      // TODO(crbug.com/940068) Remove this condition.
    } else {
      request_.headers.SetHeader(
          net::HttpRequestHeaders::kOrigin,
          (tainted_ ? url::Origin() : *request_.request_initiator).Serialize());
    }
  }

  if (fetch_cors_flag_ && request_.mode == mojom::RequestMode::kSameOrigin) {
    DCHECK(request_.request_initiator);
    HandleComplete(URLLoaderCompletionStatus(
        CorsErrorStatus(mojom::CorsError::kDisallowedByMode)));
    return;
  }

  response_tainting_ = CalculateResponseTainting(
      request_.url, request_.mode, request_.request_initiator, fetch_cors_flag_,
      tainted_, origin_access_list_);

  if (!CalculateCredentialsFlag(request_.credentials_mode,
                                response_tainting_)) {
    request_.allow_credentials = false;
  }

  // Note that even when |NeedsPreflight(request_)| holds we don't make a
  // preflight request when |fetch_cors_flag_| is false (e.g., when the origin
  // of the url is equal to the origin of the request.
  if (!fetch_cors_flag_ || !NeedsPreflight(request_)) {
    StartNetworkRequest(net::OK, base::nullopt, base::nullopt);
    return;
  }

  preflight_controller_->PerformPreflightCheck(
      base::BindOnce(&CorsURLLoader::StartNetworkRequest,
                     weak_factory_.GetWeakPtr()),
      request_, tainted_, net::NetworkTrafficAnnotationTag(traffic_annotation_),
      network_loader_factory_);
}

void CorsURLLoader::StartNetworkRequest(
    int error_code,
    base::Optional<CorsErrorStatus> status,
    base::Optional<PreflightTimingInfo> preflight_timing_info) {
  if (error_code != net::OK) {
    HandleComplete(status ? URLLoaderCompletionStatus(*status)
                          : URLLoaderCompletionStatus(error_code));
    return;
  }
  DCHECK(!status);

  if (preflight_timing_info)
    preflight_timing_info_.push_back(*preflight_timing_info);

  mojom::URLLoaderClientPtr network_client;
  network_client_binding_.Bind(mojo::MakeRequest(&network_client));
  // Binding |this| as an unretained pointer is safe because
  // |network_client_binding_| shares this object's lifetime.
  network_client_binding_.set_connection_error_handler(base::BindOnce(
      &CorsURLLoader::OnConnectionError, base::Unretained(this)));
  network_loader_factory_->CreateLoaderAndStart(
      mojo::MakeRequest(&network_loader_), routing_id_, request_id_, options_,
      request_, std::move(network_client), traffic_annotation_);
}

void CorsURLLoader::HandleComplete(const URLLoaderCompletionStatus& status) {
  ReportCompletionStatusMetric(fetch_cors_flag_, status);
  forwarding_client_->OnComplete(status);
  std::move(delete_callback_).Run(this);
  // |this| is deleted here.
}

void CorsURLLoader::OnConnectionError() {
  HandleComplete(URLLoaderCompletionStatus(net::ERR_ABORTED));
}

// This should be identical to CalculateCorsFlag defined in
// //third_party/blink/renderer/platform/loader/cors/cors.cc.
void CorsURLLoader::SetCorsFlagIfNeeded() {
  if (fetch_cors_flag_)
    return;

  if (!network::cors::ShouldCheckCors(request_.url, request_.request_initiator,
                                      request_.mode)) {
    return;
  }

  // In some cases we want to use the origin attached to the URLLoaderFactory
  // to check same-originness.
  // TODO(lukasza): https://crbug.com/940068: Revert https://crrev.com/c/1642752
  // once request_initiator is always set to the webpage (and never to the
  // isolated world).
  if (request_.should_also_use_factory_bound_origin_for_cors &&
      factory_bound_origin_ &&
      factory_bound_origin_->IsSameOriginWith(
          url::Origin::Create(request_.url))) {
    return;
  }

  // The source origin and destination URL pair may be in the allow list.
  switch (origin_access_list_->CheckAccessState(*request_.request_initiator,
                                                request_.url)) {
    case OriginAccessList::AccessState::kAllowed:
      return;
    case OriginAccessList::AccessState::kBlocked:
      break;
    case OriginAccessList::AccessState::kNotListed:
      if (factory_bound_origin_access_list_->CheckAccessState(
              *request_.request_initiator, request_.url) ==
          OriginAccessList::AccessState::kAllowed) {
        return;
      }
      break;
  }

  // When a request is initiated in a unique opaque origin (e.g., in a sandboxed
  // iframe) and the blob is also created in the context, |request_initiator|
  // is a unique opaque origin and url::Origin::Create(request_.url) is another
  // unique opaque origin. url::Origin::IsSameOriginWith(p, q) returns false
  // when both |p| and |q| are opaque, but in this case we want to say that the
  // request is a same-origin request. Hence we don't set |fetch_cors_flag_|,
  // assuming the request comes from a renderer and the origin is checked there
  // (in BaseFetchContext::CanRequest).
  // In the future blob URLs will not come here because there will be a
  // separate URLLoaderFactory for blobs.
  // TODO(yhirano): Remove this logic at the time.
  if (request_.url.SchemeIsBlob() && request_.request_initiator->opaque() &&
      url::Origin::Create(request_.url).opaque()) {
    return;
  }

  fetch_cors_flag_ = true;
}

// Keep this in sync with the identical function
// blink::cors::CalculateResponseTainting.
//
// static
mojom::FetchResponseType CorsURLLoader::CalculateResponseTainting(
    const GURL& url,
    mojom::RequestMode request_mode,
    const base::Optional<url::Origin>& origin,
    bool cors_flag,
    bool tainted_origin,
    const OriginAccessList* origin_access_list) {
  if (url.SchemeIs(url::kDataScheme))
    return mojom::FetchResponseType::kBasic;

  if (cors_flag) {
    DCHECK(IsCorsEnabledRequestMode(request_mode));
    return mojom::FetchResponseType::kCors;
  }

  if (!origin) {
    // This is actually not defined in the fetch spec, but in this case CORS
    // is disabled so no one should care this value.
    return mojom::FetchResponseType::kBasic;
  }

  if (request_mode == mojom::RequestMode::kNoCors) {
    if (tainted_origin ||
        (!origin->IsSameOriginWith(url::Origin::Create(url)) &&
         origin_access_list->CheckAccessState(*origin, url) !=
             OriginAccessList::AccessState::kAllowed)) {
      return mojom::FetchResponseType::kOpaque;
    }
  }
  return mojom::FetchResponseType::kBasic;
}

base::Optional<std::string> CorsURLLoader::GetHeaderString(
    const ResourceResponseHead& response,
    const std::string& header_name) {
  if (!response.headers)
    return base::nullopt;
  std::string header_value;
  if (!response.headers->GetNormalizedHeader(header_name, &header_value))
    return base::nullopt;
  return header_value;
}

}  // namespace cors

}  // namespace network

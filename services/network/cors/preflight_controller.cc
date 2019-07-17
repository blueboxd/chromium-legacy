// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/preflight_controller.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "url/gurl.h"

namespace network {

namespace cors {

namespace {

int RetrieveCacheFlags(int load_flags) {
  return load_flags & (net::LOAD_VALIDATE_CACHE | net::LOAD_BYPASS_CACHE |
                       net::LOAD_DISABLE_CACHE);
}

base::Optional<std::string> GetHeaderString(
    const scoped_refptr<net::HttpResponseHeaders>& headers,
    const std::string& header_name) {
  std::string header_value;
  if (!headers || !headers->GetNormalizedHeader(header_name, &header_value))
    return base::nullopt;
  return header_value;
}

// Algorithm step 3 of the CORS-preflight fetch,
// https://fetch.spec.whatwg.org/#cors-preflight-fetch-0, that requires
//  - CORS-safelisted request-headers excluded
//  - duplicates excluded
//  - sorted lexicographically
//  - byte-lowercased
std::string CreateAccessControlRequestHeadersHeader(
    const net::HttpRequestHeaders& headers,
    bool is_revalidating) {
  // Exclude the forbidden headers because they may be added by the user
  // agent. They must be checked separately and rejected for
  // JavaScript-initiated requests.
  std::vector<std::string> filtered_headers =
      CorsUnsafeNotForbiddenRequestHeaderNames(headers.GetHeaderVector(),
                                               is_revalidating);
  if (filtered_headers.empty())
    return std::string();

  // Sort header names lexicographically.
  std::sort(filtered_headers.begin(), filtered_headers.end());

  return base::JoinString(filtered_headers, ",");
}

std::unique_ptr<ResourceRequest> CreatePreflightRequest(
    const ResourceRequest& request,
    bool tainted) {
  DCHECK(!request.url.has_username());
  DCHECK(!request.url.has_password());

  std::unique_ptr<ResourceRequest> preflight_request =
      std::make_unique<ResourceRequest>();

  // Algorithm step 1 through 4 of the CORS-preflight fetch,
  // https://fetch.spec.whatwg.org/#cors-preflight-fetch-0.
  preflight_request->url = request.url;
  preflight_request->method = "OPTIONS";
  preflight_request->priority = request.priority;
  preflight_request->fetch_request_context_type =
      request.fetch_request_context_type;
  preflight_request->referrer = request.referrer;
  preflight_request->referrer_policy = request.referrer_policy;

  preflight_request->credentials_mode = mojom::CredentialsMode::kOmit;
  preflight_request->allow_credentials = false;
  preflight_request->load_flags = RetrieveCacheFlags(request.load_flags);
  preflight_request->fetch_window_id = request.fetch_window_id;
  preflight_request->render_frame_id = request.render_frame_id;

  preflight_request->headers.SetHeader(
      header_names::kAccessControlRequestMethod, request.method);
  preflight_request->headers.SetHeader("Sec-Fetch-Mode", "cors");

  std::string request_headers = CreateAccessControlRequestHeadersHeader(
      request.headers, request.is_revalidating);
  if (!request_headers.empty()) {
    preflight_request->headers.SetHeader(
        header_names::kAccessControlRequestHeaders, request_headers);
  }

  if (request.is_external_request) {
    preflight_request->headers.SetHeader(
        header_names::kAccessControlRequestExternal, "true");
  }

  DCHECK(request.request_initiator);
  preflight_request->request_initiator = request.request_initiator;
  preflight_request->headers.SetHeader(
      net::HttpRequestHeaders::kOrigin,
      (tainted ? url::Origin() : *request.request_initiator).Serialize());

  // TODO(toyoshim): Should not matter, but at this moment, it hits a sanity
  // check in ResourceDispatcherHostImpl if |resource_type| isn't set.
  preflight_request->resource_type = request.resource_type;

  return preflight_request;
}

std::unique_ptr<PreflightResult> CreatePreflightResult(
    const GURL& final_url,
    const ResourceResponseHead& head,
    const ResourceRequest& original_request,
    bool tainted,
    base::Optional<CorsErrorStatus>* detected_error_status) {
  DCHECK(detected_error_status);

  const int response_code = head.headers ? head.headers->response_code() : 0;
  *detected_error_status = CheckPreflightAccess(
      final_url, response_code,
      GetHeaderString(head.headers, header_names::kAccessControlAllowOrigin),
      GetHeaderString(head.headers,
                      header_names::kAccessControlAllowCredentials),
      original_request.credentials_mode,
      tainted ? url::Origin() : *original_request.request_initiator);
  if (*detected_error_status)
    return nullptr;

  base::Optional<mojom::CorsError> error;
  error = CheckPreflight(response_code);
  if (error) {
    *detected_error_status = CorsErrorStatus(*error);
    return nullptr;
  }

  if (original_request.is_external_request) {
    *detected_error_status = CheckExternalPreflight(GetHeaderString(
        head.headers, header_names::kAccessControlAllowExternal));
    if (*detected_error_status)
      return nullptr;
  }

  auto result = PreflightResult::Create(
      original_request.credentials_mode,
      GetHeaderString(head.headers, header_names::kAccessControlAllowMethods),
      GetHeaderString(head.headers, header_names::kAccessControlAllowHeaders),
      GetHeaderString(head.headers, header_names::kAccessControlMaxAge),
      &error);

  if (error)
    *detected_error_status = CorsErrorStatus(*error);
  return result;
}

base::Optional<CorsErrorStatus> CheckPreflightResult(
    PreflightResult* result,
    const ResourceRequest& original_request) {
  base::Optional<CorsErrorStatus> status =
      result->EnsureAllowedCrossOriginMethod(original_request.method);
  if (status)
    return status;

  return result->EnsureAllowedCrossOriginHeaders(
      original_request.headers, original_request.is_revalidating);
}

}  // namespace

class PreflightController::PreflightLoader final {
 public:
  PreflightLoader(PreflightController* controller,
                  CompletionCallback completion_callback,
                  const ResourceRequest& request,
                  bool tainted,
                  const net::NetworkTrafficAnnotationTag& annotation_tag)
      : controller_(controller),
        completion_callback_(std::move(completion_callback)),
        original_request_(request),
        tainted_(tainted) {
    loader_ = SimpleURLLoader::Create(CreatePreflightRequest(request, tainted),
                                      annotation_tag);
  }

  void Request(mojom::URLLoaderFactory* loader_factory) {
    DCHECK(loader_);

    loader_->SetOnRedirectCallback(base::BindRepeating(
        &PreflightLoader::HandleRedirect, base::Unretained(this)));
    loader_->SetOnResponseStartedCallback(base::BindRepeating(
        &PreflightLoader::HandleResponseHeader, base::Unretained(this)));

    loader_->DownloadToString(
        loader_factory,
        base::BindOnce(&PreflightLoader::HandleResponseBody,
                       base::Unretained(this)),
        0);
  }

 private:
  void HandleRedirect(const net::RedirectInfo& redirect_info,
                      const network::ResourceResponseHead& response_head,
                      std::vector<std::string>* to_be_removed_headers) {
    // Preflight should not allow any redirect.
    FinalizeLoader();

    std::move(completion_callback_)
        .Run(net::ERR_FAILED,
             CorsErrorStatus(mojom::CorsError::kPreflightDisallowedRedirect),
             base::nullopt);

    RemoveFromController();
    // |this| is deleted here.
  }

  void HandleResponseHeader(const GURL& final_url,
                            const ResourceResponseHead& head) {
    FinalizeLoader();

    timing_info_.start_time = head.request_start;
    timing_info_.response_end = base::TimeTicks::Now();
    timing_info_.alpn_negotiated_protocol = head.alpn_negotiated_protocol;
    timing_info_.connection_info = head.connection_info;
    auto timing_allow_origin =
        GetHeaderString(head.headers, "Timing-Allow-Origin");
    if (timing_allow_origin)
      timing_info_.timing_allow_origin = *timing_allow_origin;
    timing_info_.transfer_size = head.encoded_data_length;

    base::Optional<CorsErrorStatus> detected_error_status;
    std::unique_ptr<PreflightResult> result = CreatePreflightResult(
        final_url, head, original_request_, tainted_, &detected_error_status);

    if (result) {
      // Preflight succeeded. Check |original_request_| with |result|.
      DCHECK(!detected_error_status);
      detected_error_status =
          CheckPreflightResult(result.get(), original_request_);
    }

    if (!(original_request_.load_flags & net::LOAD_DISABLE_CACHE) &&
        !detected_error_status) {
      controller_->AppendToCache(*original_request_.request_initiator,
                                 original_request_.url, std::move(result));
    }

    base::Optional<PreflightTimingInfo> timing_info;
    if (!detected_error_status)
      timing_info = std::move(timing_info_);
    std::move(completion_callback_)
        .Run(detected_error_status ? net::ERR_FAILED : net::OK,
             detected_error_status, std::move(timing_info));

    RemoveFromController();
    // |this| is deleted here.
  }

  void HandleResponseBody(std::unique_ptr<std::string> response_body) {
    // Reached only when the request fails without receiving headers, e.g.
    // unknown hosts, unreachable remote, reset by peer, and so on.
    // See https://crbug.com/826868 for related discussion.
    DCHECK(!response_body);
    const int error = loader_->NetError();
    DCHECK_NE(error, net::OK);
    FinalizeLoader();
    std::move(completion_callback_).Run(error, base::nullopt, base::nullopt);
    RemoveFromController();
    // |this| is deleted here.
  }

  void FinalizeLoader() {
    DCHECK(loader_);
    loader_.reset();
  }

  // Removes |this| instance from |controller_|. Once the method returns, |this|
  // is already removed.
  void RemoveFromController() { controller_->RemoveLoader(this); }

  // PreflightController owns all PreflightLoader instances, and should outlive.
  PreflightController* const controller_;

  // Holds SimpleURLLoader instance for the CORS-preflight request.
  std::unique_ptr<SimpleURLLoader> loader_;

  PreflightTimingInfo timing_info_;

  // Holds caller's information.
  PreflightController::CompletionCallback completion_callback_;
  const ResourceRequest original_request_;

  const bool tainted_;

  DISALLOW_COPY_AND_ASSIGN(PreflightLoader);
};

// static
std::unique_ptr<ResourceRequest>
PreflightController::CreatePreflightRequestForTesting(
    const ResourceRequest& request,
    bool tainted) {
  return CreatePreflightRequest(request, tainted);
}

// static
std::unique_ptr<PreflightResult>
PreflightController::CreatePreflightResultForTesting(
    const GURL& final_url,
    const ResourceResponseHead& head,
    const ResourceRequest& original_request,
    bool tainted,
    base::Optional<CorsErrorStatus>* detected_error_status) {
  return CreatePreflightResult(final_url, head, original_request, tainted,
                               detected_error_status);
}

PreflightController::PreflightController() = default;

PreflightController::~PreflightController() = default;

void PreflightController::PerformPreflightCheck(
    CompletionCallback callback,
    const ResourceRequest& request,
    bool tainted,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    mojom::URLLoaderFactory* loader_factory) {
  DCHECK(request.request_initiator);

  if (!RetrieveCacheFlags(request.load_flags) && !request.is_external_request &&
      cache_.CheckIfRequestCanSkipPreflight(
          request.request_initiator->Serialize(), request.url,
          request.credentials_mode, request.method, request.headers,
          request.is_revalidating)) {
    std::move(callback).Run(net::OK, base::nullopt, base::nullopt);
    return;
  }

  auto emplaced_pair = loaders_.emplace(std::make_unique<PreflightLoader>(
      this, std::move(callback), request, tainted, annotation_tag));
  (*emplaced_pair.first)->Request(loader_factory);
}

void PreflightController::RemoveLoader(PreflightLoader* loader) {
  auto it = loaders_.find(loader);
  DCHECK(it != loaders_.end());
  loaders_.erase(it);
}

void PreflightController::AppendToCache(
    const url::Origin& origin,
    const GURL& url,
    std::unique_ptr<PreflightResult> result) {
  cache_.AppendEntry(origin.Serialize(), url, std::move(result));
}

}  // namespace cors

}  // namespace network

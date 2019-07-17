// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/cors_url_loader_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "mojo/public/cpp/bindings/message.h"
#include "net/base/load_flags.h"
#include "services/network/cors/cors_url_loader.h"
#include "services/network/cors/preflight_controller.h"
#include "services/network/initiator_lock_compatibility.h"
#include "services/network/loader_util.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/resource_scheduler/resource_scheduler_client.h"
#include "services/network/url_loader_factory.h"

namespace network {

namespace cors {

CorsURLLoaderFactory::CorsURLLoaderFactory(
    NetworkContext* context,
    mojom::URLLoaderFactoryParamsPtr params,
    scoped_refptr<ResourceSchedulerClient> resource_scheduler_client,
    mojom::URLLoaderFactoryRequest request,
    const OriginAccessList* origin_access_list,
    std::unique_ptr<mojom::URLLoaderFactory> network_loader_factory_for_testing)
    : context_(context),
      disable_web_security_(params->disable_web_security),
      process_id_(params->process_id),
      request_initiator_site_lock_(params->request_initiator_site_lock),
      origin_access_list_(origin_access_list) {
  DCHECK(context_);
  DCHECK(origin_access_list_);
  DCHECK_NE(mojom::kInvalidProcessId, process_id_);
  factory_bound_origin_access_list_ = std::make_unique<OriginAccessList>();
  if (params->factory_bound_allow_patterns.size()) {
    DCHECK(params->request_initiator_site_lock);
    factory_bound_origin_access_list_->SetAllowListForOrigin(
        *params->request_initiator_site_lock,
        params->factory_bound_allow_patterns);
  }
  network_loader_factory_ =
      network_loader_factory_for_testing
          ? std::move(network_loader_factory_for_testing)
          : std::make_unique<network::URLLoaderFactory>(
                context, std::move(params),
                std::move(resource_scheduler_client), this);

  bindings_.AddBinding(this, std::move(request));
  bindings_.set_connection_error_handler(base::BindRepeating(
      &CorsURLLoaderFactory::DeleteIfNeeded, base::Unretained(this)));
}

CorsURLLoaderFactory::~CorsURLLoaderFactory() = default;

void CorsURLLoaderFactory::OnLoaderCreated(
    std::unique_ptr<mojom::URLLoader> loader) {
  if (context_)
    context_->LoaderCreated(process_id_);
  loaders_.insert(std::move(loader));
}

void CorsURLLoaderFactory::DestroyURLLoader(mojom::URLLoader* loader) {
  if (context_)
    context_->LoaderDestroyed(process_id_);
  auto it = loaders_.find(loader);
  DCHECK(it != loaders_.end());
  loaders_.erase(it);

  DeleteIfNeeded();
}

void CorsURLLoaderFactory::CreateLoaderAndStart(
    mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& resource_request,
    mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (!IsSane(context_, resource_request)) {
    client->OnComplete(URLLoaderCompletionStatus(net::ERR_INVALID_ARGUMENT));
    return;
  }

  if (features::ShouldEnableOutOfBlinkCors() && !disable_web_security_) {
    auto loader = std::make_unique<CorsURLLoader>(
        std::move(request), routing_id, request_id, options,
        base::BindOnce(&CorsURLLoaderFactory::DestroyURLLoader,
                       base::Unretained(this)),
        resource_request, std::move(client), traffic_annotation,
        network_loader_factory_.get(), request_initiator_site_lock_,
        origin_access_list_, factory_bound_origin_access_list_.get(),
        context_->cors_preflight_controller());
    auto* raw_loader = loader.get();
    OnLoaderCreated(std::move(loader));
    raw_loader->Start();
  } else {
    network_loader_factory_->CreateLoaderAndStart(
        std::move(request), routing_id, request_id, options, resource_request,
        std::move(client), traffic_annotation);
  }
}

void CorsURLLoaderFactory::Clone(mojom::URLLoaderFactoryRequest request) {
  // The cloned factories stop working when this factory is destructed.
  bindings_.AddBinding(this, std::move(request));
}

void CorsURLLoaderFactory::ClearBindings() {
  bindings_.CloseAllBindings();
}

void CorsURLLoaderFactory::DeleteIfNeeded() {
  if (!context_)
    return;
  if (bindings_.empty() && loaders_.empty())
    context_->DestroyURLLoaderFactory(this);
}

bool CorsURLLoaderFactory::IsSane(const NetworkContext* context,
                                  const ResourceRequest& request) {
  // CORS needs a proper origin (including a unique opaque origin). If the
  // request doesn't have one, CORS cannot work.
  if (!request.request_initiator &&
      request.mode != mojom::RequestMode::kNavigate &&
      request.mode != mojom::RequestMode::kNoCors) {
    LOG(WARNING) << "|mode| is " << request.mode
                 << ", but |request_initiator| is not set.";
    mojo::ReportBadMessage("CorsURLLoaderFactory: cors without initiator");
    return false;
  }

  const auto load_flags_pattern = net::LOAD_DO_NOT_SAVE_COOKIES |
                                  net::LOAD_DO_NOT_SEND_COOKIES |
                                  net::LOAD_DO_NOT_SEND_AUTH_DATA;
  // The Fetch credential mode and lower-level options should match. If the
  // Fetch mode is kOmit, then either |allow_credentials| must be false or
  // all three load flags must be set. https://crbug.com/799935 tracks
  // unifying |LOAD_DO_NOT_*| into |allow_credentials|.
  if (request.credentials_mode == mojom::CredentialsMode::kOmit &&
      request.allow_credentials &&
      (request.load_flags & load_flags_pattern) != load_flags_pattern) {
    LOG(WARNING) << "|credentials_mode| and |allow_credentials| or "
                    "|load_flags| contradict each "
                    "other.";
    mojo::ReportBadMessage(
        "CorsURLLoaderFactory: omit-credentials vs load_flags");
    return false;
  }

  // Ensure that renderer requests are covered either by CORS or CORB.
  if (process_id_ != mojom::kBrowserProcessId) {
    switch (request.mode) {
      case mojom::RequestMode::kNavigate:
        // Only the browser process can initiate navigations.  This helps ensure
        // that a malicious/compromised renderer cannot bypass CORB by issuing
        // kNavigate, rather than kNoCors requests.  (CORB should apply only to
        // no-cors requests as tracked in https://crbug.com/953315 and as
        // captured in https://fetch.spec.whatwg.org/#main-fetch).
        mojo::ReportBadMessage(
            "CorsURLLoaderFactory: navigate from non-browser-process");
        return false;

      case mojom::RequestMode::kSameOrigin:
      case mojom::RequestMode::kCors:
      case mojom::RequestMode::kCorsWithForcedPreflight:
        // SOP enforced by CORS.
        break;

      case mojom::RequestMode::kNoCors:
        // SOP enforced by CORB.
        break;
    }
  }

  InitiatorLockCompatibility initiator_lock_compatibility =
      process_id_ == mojom::kBrowserProcessId
          ? InitiatorLockCompatibility::kBrowserProcess
          : VerifyRequestInitiatorLock(request_initiator_site_lock_,
                                       request.request_initiator);
  UMA_HISTOGRAM_ENUMERATION(
      "NetworkService.URLLoader.RequestInitiatorOriginLockCompatibility",
      initiator_lock_compatibility);
  // TODO(lukasza): Enforce the origin lock.
  // - https://crbug.com/766694: In the long-term kIncorrectLock should trigger
  //   a renderer kill, but this can't be done until HTML Imports are gone.
  // - https://crbug.com/515309: The lock should apply to Origin header (and
  //   SameSite cookies) in addition to CORB (which was taken care of in
  //   https://crbug.com/871827).  Here enforcement most likely would mean
  //   setting |url_request_|'s initiator to something other than
  //   |request.request_initiator| (opaque origin?  lock origin?).

  if (context) {
    net::HttpRequestHeaders::Iterator header_iterator(
        request.cors_exempt_headers);
    const auto& allowed_exempt_headers = context->cors_exempt_header_list();
    while (header_iterator.GetNext()) {
      if (allowed_exempt_headers.find(header_iterator.name()) !=
          allowed_exempt_headers.end()) {
        continue;
      }
      LOG(WARNING) << "|cors_exempt_headers| contains unexpected key: "
                   << header_iterator.name();
      return false;
    }
  }

  if (!AreRequestHeadersSafe(request.headers) ||
      !AreRequestHeadersSafe(request.cors_exempt_headers)) {
    return false;
  }

  LogConcerningRequestHeaders(request.headers,
                              false /* added_during_redirect */);

  // TODO(yhirano): If the request mode is "no-cors", the redirect mode should
  // be "follow".
  return true;
}

}  // namespace cors

}  // namespace network

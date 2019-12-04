// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/cors_url_loader_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/load_flags.h"
#include "services/network/cors/cors_url_loader.h"
#include "services/network/cors/preflight_controller.h"
#include "services/network/crash_keys.h"
#include "services/network/cross_origin_read_blocking.h"
#include "services/network/loader_util.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/cpp/initiator_lock_compatibility.h"
#include "services/network/public/cpp/request_mode.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/resource_scheduler/resource_scheduler_client.h"
#include "services/network/url_loader_factory.h"

namespace network {

namespace cors {

namespace {

InitiatorLockCompatibility VerifyRequestInitiatorLockWithPluginCheck(
    uint32_t process_id,
    const base::Optional<url::Origin>& request_initiator_site_lock,
    const base::Optional<url::Origin>& request_initiator) {
  if (process_id == mojom::kBrowserProcessId)
    return InitiatorLockCompatibility::kBrowserProcess;

  InitiatorLockCompatibility result = VerifyRequestInitiatorLock(
      request_initiator_site_lock, request_initiator);

  if (result == InitiatorLockCompatibility::kIncorrectLock &&
      CrossOriginReadBlocking::ShouldAllowForPlugin(process_id)) {
    result = InitiatorLockCompatibility::kExcludedUniversalAccessPlugin;
  }

  return result;
}

}  // namespace

class CorsURLLoaderFactory::FactoryOverride final {
 public:
  class ExposedNetworkLoaderFactory final : public mojom::URLLoaderFactory {
   public:
    ExposedNetworkLoaderFactory(
        std::unique_ptr<URLLoaderFactory> network_loader_factory,
        mojo::PendingReceiver<mojom::URLLoaderFactory> receiver)
        : network_loader_factory_(std::move(network_loader_factory)) {
      if (receiver) {
        receivers_.Add(this, std::move(receiver));
      }
    }
    ~ExposedNetworkLoaderFactory() override = default;

    ExposedNetworkLoaderFactory(const ExposedNetworkLoaderFactory&) = delete;
    ExposedNetworkLoaderFactory& operator=(const ExposedNetworkLoaderFactory&) =
        delete;

    // mojom::URLLoaderFactory implementation
    void CreateLoaderAndStart(
        mojo::PendingReceiver<mojom::URLLoader> receiver,
        int32_t routing_id,
        int32_t request_id,
        uint32_t options,
        const ResourceRequest& request,
        mojo::PendingRemote<mojom::URLLoaderClient> client,
        const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
        override {
      return network_loader_factory_->CreateLoaderAndStart(
          std::move(receiver), routing_id, request_id, options, request,
          std::move(client), traffic_annotation);
    }
    void Clone(
        mojo::PendingReceiver<mojom::URLLoaderFactory> receiver) override {
      receivers_.Add(this, std::move(receiver));
    }

   private:
    std::unique_ptr<URLLoaderFactory> network_loader_factory_;
    mojo::ReceiverSet<mojom::URLLoaderFactory> receivers_;
  };

  FactoryOverride(mojom::URLLoaderFactoryOverridePtr params,
                  std::unique_ptr<URLLoaderFactory> network_loader_factory)
      : network_loader_factory_(std::move(network_loader_factory),
                                std::move(params->overridden_factory_receiver)),
        overriding_factory_(std::move(params->overriding_factory)) {}

  FactoryOverride(const FactoryOverride&) = delete;
  FactoryOverride& operator=(const FactoryOverride&) = delete;

  mojom::URLLoaderFactory* get() { return overriding_factory_.get(); }

 private:
  ExposedNetworkLoaderFactory network_loader_factory_;
  mojo::Remote<mojom::URLLoaderFactory> overriding_factory_;
};

bool CorsURLLoaderFactory::allow_external_preflights_for_testing_ = false;

CorsURLLoaderFactory::CorsURLLoaderFactory(
    NetworkContext* context,
    mojom::URLLoaderFactoryParamsPtr params,
    scoped_refptr<ResourceSchedulerClient> resource_scheduler_client,
    mojo::PendingReceiver<mojom::URLLoaderFactory> receiver,
    const OriginAccessList* origin_access_list)
    : context_(context),
      is_trusted_(params->is_trusted),
      disable_web_security_(params->disable_web_security),
      process_id_(params->process_id),
      request_initiator_site_lock_(params->request_initiator_site_lock),
      origin_access_list_(origin_access_list) {
  DCHECK(context_);
  DCHECK(origin_access_list_);
  DCHECK_NE(mojom::kInvalidProcessId, process_id_);
  factory_bound_origin_access_list_ = std::make_unique<OriginAccessList>();
  if (params->factory_bound_access_patterns) {
    factory_bound_origin_access_list_->SetAllowListForOrigin(
        params->factory_bound_access_patterns->source_origin,
        params->factory_bound_access_patterns->allow_patterns);
    factory_bound_origin_access_list_->SetBlockListForOrigin(
        params->factory_bound_access_patterns->source_origin,
        params->factory_bound_access_patterns->block_patterns);
  }

  auto factory_override = std::move(params->factory_override);
  auto network_loader_factory = std::make_unique<network::URLLoaderFactory>(
      context, std::move(params), std::move(resource_scheduler_client), this);

  if (factory_override) {
    DCHECK(factory_override->overriding_factory);
    factory_override_ = std::make_unique<FactoryOverride>(
        std::move(factory_override), std::move(network_loader_factory));
  } else {
    network_loader_factory_ = std::move(network_loader_factory);
  }

  receivers_.Add(this, std::move(receiver));
  receivers_.set_disconnect_handler(base::BindRepeating(
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
    mojo::PendingReceiver<mojom::URLLoader> receiver,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& resource_request,
    mojo::PendingRemote<mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  debug::ScopedRequestCrashKeys request_crash_keys(resource_request);

  if (!IsSane(context_, resource_request, options)) {
    mojo::Remote<mojom::URLLoaderClient>(std::move(client))
        ->OnComplete(URLLoaderCompletionStatus(net::ERR_INVALID_ARGUMENT));
    return;
  }

  mojom::URLLoaderFactory* const inner_url_loader_factory =
      factory_override_ ? factory_override_->get()
                        : network_loader_factory_.get();
  DCHECK(inner_url_loader_factory);
  if (context_->IsCorsEnabled() && !disable_web_security_) {
    auto loader = std::make_unique<CorsURLLoader>(
        std::move(receiver), routing_id, request_id, options,
        base::BindOnce(&CorsURLLoaderFactory::DestroyURLLoader,
                       base::Unretained(this)),
        resource_request, std::move(client), traffic_annotation,
        inner_url_loader_factory, origin_access_list_,
        factory_bound_origin_access_list_.get(),
        context_->cors_preflight_controller());
    auto* raw_loader = loader.get();
    OnLoaderCreated(std::move(loader));
    raw_loader->Start();
  } else {
    inner_url_loader_factory->CreateLoaderAndStart(
        std::move(receiver), routing_id, request_id, options, resource_request,
        std::move(client), traffic_annotation);
  }
}

void CorsURLLoaderFactory::Clone(
    mojo::PendingReceiver<mojom::URLLoaderFactory> receiver) {
  // The cloned factories stop working when this factory is destructed.
  receivers_.Add(this, std::move(receiver));
}

void CorsURLLoaderFactory::ClearBindings() {
  receivers_.Clear();
}

void CorsURLLoaderFactory::DeleteIfNeeded() {
  if (!context_)
    return;
  if (receivers_.empty() && loaders_.empty())
    context_->DestroyURLLoaderFactory(this);
}

bool CorsURLLoaderFactory::IsSane(const NetworkContext* context,
                                  const ResourceRequest& request,
                                  uint32_t options) {
  // CORS needs a proper origin (including a unique opaque origin). If the
  // request doesn't have one, CORS cannot work.
  if (!request.request_initiator && !IsNavigationRequestMode(request.mode) &&
      request.mode != mojom::RequestMode::kNoCors) {
    LOG(WARNING) << "|mode| is " << request.mode
                 << ", but |request_initiator| is not set.";
    mojo::ReportBadMessage("CorsURLLoaderFactory: cors without initiator");
    return false;
  }

  // Reject request with trusted params if factory is not for a trusted
  // consumer.
  if (request.trusted_params && !is_trusted_) {
    mojo::ReportBadMessage(
        "CorsURLLoaderFactory: Untrusted caller making trusted request");
    return false;
  }

  // Reject request if the restricted prefetch load flag is set but the
  // request's NetworkIsolationKey is not present. This is because the
  // restricted prefetch flag is only used when the browser sets the request's
  // NetworkIsolationKey to correctly cache-partition the resource.
  bool request_network_isolation_key_present =
      request.trusted_params &&
      !request.trusted_params->network_isolation_key.IsEmpty();
  if (request.load_flags & net::LOAD_RESTRICTED_PREFETCH &&
      !request_network_isolation_key_present) {
    mojo::ReportBadMessage(
        "CorsURLLoaderFactory: Request with LOAD_RESTRICTED_PREFETCH flag is "
        "not trusted");
    return false;
  }

  // Ensure that renderer requests are covered either by CORS or CORB.
  if (process_id_ != mojom::kBrowserProcessId) {
    switch (request.mode) {
      case mojom::RequestMode::kNavigate:
      case mojom::RequestMode::kNavigateNestedFrame:
      case mojom::RequestMode::kNavigateNestedObject:
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

  // Compare |request_initiator| and |request_initiator_site_lock_|.
  InitiatorLockCompatibility initiator_lock_compatibility =
      VerifyRequestInitiatorLockWithPluginCheck(
          process_id_, request_initiator_site_lock_, request.request_initiator);
  UMA_HISTOGRAM_ENUMERATION(
      "NetworkService.URLLoader.RequestInitiatorOriginLockCompatibility",
      initiator_lock_compatibility);
  switch (initiator_lock_compatibility) {
    case InitiatorLockCompatibility::kCompatibleLock:
    case InitiatorLockCompatibility::kBrowserProcess:
    case InitiatorLockCompatibility::kExcludedUniversalAccessPlugin:
      break;

    case InitiatorLockCompatibility::kNoLock:
      // TODO(lukasza): https://crbug.com/891872: Browser process should always
      // specify the request_initiator_site_lock in URLLoaderFactories given to
      // a renderer process.  Once https://crbug.com/891872 is fixed, the case
      // below should return |false| (i.e. = bad message).
      break;

    case InitiatorLockCompatibility::kNoInitiator:
      // Requests from the renderer need to always specify an initiator.
      NOTREACHED();
      mojo::ReportBadMessage(
          "CorsURLLoaderFactory: no initiator in a renderer request");
      return false;

    case InitiatorLockCompatibility::kIncorrectLock:
      // Requests from the renderer need to always specify a correct initiator.
      NOTREACHED();
      // TODO(lukasza): https://crbug.com/920634: Report bad message and return
      // false below.
      break;
  }

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

  // Specifying CredentialsMode::kSameOrigin without an initiator origin doesn't
  // make sense.
  if (request.credentials_mode == mojom::CredentialsMode::kSameOrigin &&
      !request.request_initiator) {
    LOG(WARNING) << "same-origin credentials mode without initiator";
    mojo::ReportBadMessage(
        "CorsURLLoaderFactory: same-origin credentials mode without initiator");
    return false;
  }

  // We only support |kInclude| credentials mode with navigations. See also:
  // a note at https://fetch.spec.whatwg.org/#concept-request-credentials-mode.
  if (request.credentials_mode != mojom::CredentialsMode::kInclude &&
      IsNavigationRequestMode(request.mode)) {
    LOG(WARNING) << "unsupported credentials mode on a navigation request";
    mojo::ReportBadMessage(
        "CorsURLLoaderFactory: unsupported credentials mode on navigation");
    return false;
  }

  if (!allow_external_preflights_for_testing_) {
    // kURLLoadOptionAsCorsPreflight should be set only by the network service.
    // Otherwise the network service will be confused.
    if (options & mojom::kURLLoadOptionAsCorsPreflight) {
      mojo::ReportBadMessage(
          "CorsURLLoaderFactory: kURLLoadOptionAsCorsPreflight is set");
      return false;
    }
  }

  // TODO(yhirano): If the request mode is "no-cors", the redirect mode should
  // be "follow".
  return true;
}

}  // namespace cors

}  // namespace network

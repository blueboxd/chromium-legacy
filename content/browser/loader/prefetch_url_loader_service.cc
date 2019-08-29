// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/prefetch_url_loader_service.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/time/default_tick_clock.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/loader/prefetch_url_loader.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/resource_type.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/features.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"

namespace content {

struct PrefetchURLLoaderService::BindContext {
  BindContext(int frame_tree_node_id,
              scoped_refptr<network::SharedURLLoaderFactory> factory,
              base::WeakPtr<RenderFrameHostImpl> render_frame_host,
              scoped_refptr<PrefetchedSignedExchangeCache>
                  prefetched_signed_exchange_cache)
      : frame_tree_node_id(frame_tree_node_id),
        factory(factory),
        render_frame_host(std::move(render_frame_host)),
        cross_origin_factory(nullptr),
        prefetched_signed_exchange_cache(
            std::move(prefetched_signed_exchange_cache)) {}

  explicit BindContext(const std::unique_ptr<BindContext>& other)
      : frame_tree_node_id(other->frame_tree_node_id),
        factory(other->factory),
        render_frame_host(other->render_frame_host),
        cross_origin_factory(other->cross_origin_factory),
        prefetched_signed_exchange_cache(
            other->prefetched_signed_exchange_cache) {}

  ~BindContext() = default;

  const int frame_tree_node_id;
  scoped_refptr<network::SharedURLLoaderFactory> factory;
  base::WeakPtr<RenderFrameHostImpl> render_frame_host;

  // This member is lazily initialized by EnsureCrossOriginFactory().
  scoped_refptr<network::SharedURLLoaderFactory> cross_origin_factory;
  scoped_refptr<PrefetchedSignedExchangeCache> prefetched_signed_exchange_cache;
};

PrefetchURLLoaderService::PrefetchURLLoaderService(
    BrowserContext* browser_context)
    : browser_context_(browser_context),
      signed_exchange_prefetch_metric_recorder_(
          base::MakeRefCounted<SignedExchangePrefetchMetricRecorder>(
              base::DefaultTickClock::GetInstance())) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  accept_langs_ =
      GetContentClient()->browser()->GetAcceptLangs(browser_context);

  // Create a RendererPreferenceWatcher to observe updates in the preferences.
  GetContentClient()->browser()->RegisterRendererPreferenceWatcher(
      browser_context, preference_watcher_receiver_.BindNewPipeAndPassRemote());
}

void PrefetchURLLoaderService::GetFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    int frame_tree_node_id,
    std::unique_ptr<network::SharedURLLoaderFactoryInfo> factories,
    base::WeakPtr<RenderFrameHostImpl> render_frame_host,
    scoped_refptr<PrefetchedSignedExchangeCache>
        prefetched_signed_exchange_cache) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto factory_bundle =
      network::SharedURLLoaderFactory::Create(std::move(factories));
  loader_factory_receivers_.Add(
      this, std::move(receiver),
      std::make_unique<BindContext>(
          frame_tree_node_id, factory_bundle, std::move(render_frame_host),
          std::move(prefetched_signed_exchange_cache)));
}

void PrefetchURLLoaderService::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request_in,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Make a copy of |resource_request_in|, because we may need to modify the
  // request.
  network::ResourceRequest resource_request = resource_request_in;

  DCHECK_EQ(static_cast<int>(ResourceType::kPrefetch),
            resource_request.resource_type);
  const auto& current_context = *loader_factory_receivers_.current_context();

  if (!current_context.render_frame_host) {
    client->OnComplete(network::URLLoaderCompletionStatus(net::ERR_ABORTED));
    return;
  }

  scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory_to_use =
      current_context.factory;

  if (resource_request.load_flags & net::LOAD_RESTRICTED_PREFETCH) {
    // The renderer has marked this prefetch as restricted, meaning it is a
    // cross-origin prefetch intended for top-leve navigation reuse. We must
    // verify that the request meets the necessary security requirements, and
    // populate |resource_request|'s NetworkIsolationKey appropriately.
    EnsureCrossOriginFactory();
    DCHECK(current_context.cross_origin_factory);

    // An invalid request could indicate a compromised renderer inappropriately
    // modifying the request, so we immediately complete it with an error.
    if (!IsValidCrossOriginPrefetch(resource_request)) {
      client->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_INVALID_ARGUMENT));
      return;
    }

    // Use the trusted cross-origin prefetch loader factory, and set the
    // request's NetworkIsolationKey suitable for the cross-origin prefetch.
    network_loader_factory_to_use = current_context.cross_origin_factory;
    url::Origin destination_origin = url::Origin::Create(resource_request.url);
    resource_request.trusted_params = network::ResourceRequest::TrustedParams();
    resource_request.trusted_params->network_isolation_key =
        net::NetworkIsolationKey(destination_origin, destination_origin);
  }

  if (prefetch_load_callback_for_testing_)
    prefetch_load_callback_for_testing_.Run();

  scoped_refptr<PrefetchedSignedExchangeCache> prefetched_signed_exchange_cache;
  if (loader_factory_receivers_.current_context()) {
    prefetched_signed_exchange_cache =
        loader_factory_receivers_.current_context()
            ->prefetched_signed_exchange_cache;
  }

  auto frame_tree_node_id_getter = base::BindRepeating(
      [](int id) { return id; }, current_context.frame_tree_node_id);

  // For now we strongly bind the loader to the request, while we can
  // also possibly make the new loader owned by the factory so that
  // they can live longer than the client (i.e. run in detached mode).
  // TODO(kinuko): Revisit this.
  mojo::MakeStrongBinding(
      std::make_unique<PrefetchURLLoader>(
          routing_id, request_id, options, frame_tree_node_id_getter,
          resource_request, std::move(client), traffic_annotation,
          std::move(network_loader_factory_to_use),
          base::BindRepeating(
              &PrefetchURLLoaderService::CreateURLLoaderThrottles, this,
              resource_request, frame_tree_node_id_getter),
          browser_context_, signed_exchange_prefetch_metric_recorder_,
          std::move(prefetched_signed_exchange_cache), blob_storage_context_,
          accept_langs_),
      std::move(request));
}

PrefetchURLLoaderService::~PrefetchURLLoaderService() = default;

// This method is used to determine whether it is safe to set the
// NetworkIsolationKey of a cross-origin prefetch request coming from the
// renderer, so that it can be cached correctly.
bool PrefetchURLLoaderService::IsValidCrossOriginPrefetch(
    const network::ResourceRequest& resource_request) {
  // The request is expected to be cross-origin. Same-origin prefetches do not
  // need a special NetworkIsolationKey, and therefore must not be marked for
  // restricted use.
  url::Origin destination_origin = url::Origin::Create(resource_request.url);
  // TODO(domfarolino): We want to check that the request's initiator is
  // equivalent to the frame's last committed origin. If they are not equal, we
  // must cancel the request.
  if (!resource_request.request_initiator ||
      resource_request.request_initiator->IsSameOriginWith(
          destination_origin)) {
    return false;
  }

  // If the PrefetchRedirectError feature is enabled, the request's redirect
  // mode must be |kError|.
  if (base::FeatureList::IsEnabled(blink::features::kPrefetchRedirectError) &&
      resource_request.redirect_mode != network::mojom::RedirectMode::kError) {
    return false;
  }

  // This prefetch request must not be able to reuse restricted prefetches from
  // the prefetch cache. This is because it is possible that another origin
  // prefetched the same resource, which should only be reused for top-level
  // navigations.
  if (resource_request.load_flags & net::LOAD_CAN_USE_RESTRICTED_PREFETCH)
    return false;

  // The request must not already have its |trusted_params| initialized.
  if (resource_request.trusted_params)
    return false;

  return true;
}

void PrefetchURLLoaderService::EnsureCrossOriginFactory() {
  DCHECK(base::FeatureList::IsEnabled(
      net::features::kSplitCacheByNetworkIsolationKey));
  auto& current_context = *loader_factory_receivers_.current_context();
  // If the factory has already been created, don't re-create it.
  if (current_context.cross_origin_factory)
    return;

  DCHECK(current_context.render_frame_host);
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> factories =
      current_context.render_frame_host
          ->CreateCrossOriginPrefetchLoaderFactoryBundle();
  current_context.cross_origin_factory =
      network::SharedURLLoaderFactory::Create(std::move(factories));
}

void PrefetchURLLoaderService::Clone(
    network::mojom::URLLoaderFactoryRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  loader_factory_receivers_.Add(
      this, std::move(request),
      std::make_unique<BindContext>(
          loader_factory_receivers_.current_context()));
}

void PrefetchURLLoaderService::NotifyUpdate(
    blink::mojom::RendererPreferencesPtr new_prefs) {
  SetAcceptLanguages(new_prefs->accept_languages);
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
PrefetchURLLoaderService::CreateURLLoaderThrottles(
    const network::ResourceRequest& request,
    base::RepeatingCallback<int(void)> frame_tree_node_id_getter) {
  int frame_tree_node_id = frame_tree_node_id_getter.Run();
  return GetContentClient()->browser()->CreateURLLoaderThrottles(
      request, browser_context_,
      base::BindRepeating(&WebContents::FromFrameTreeNodeId,
                          frame_tree_node_id),
      nullptr /* navigation_ui_data */, frame_tree_node_id);
}

}  // namespace content

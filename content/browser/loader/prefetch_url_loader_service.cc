// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/prefetch_url_loader_service.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/time/default_tick_clock.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/loader/prefetch_url_loader.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/url_loader_throttle.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"

namespace content {

struct PrefetchURLLoaderService::BindContext {
  BindContext(int frame_tree_node_id,
              scoped_refptr<network::SharedURLLoaderFactory> factory,
              scoped_refptr<PrefetchedSignedExchangeCache>
                  prefetched_signed_exchange_cache)
      : frame_tree_node_id(frame_tree_node_id),
        factory(factory),
        prefetched_signed_exchange_cache(
            std::move(prefetched_signed_exchange_cache)) {}

  explicit BindContext(const std::unique_ptr<BindContext>& other)
      : frame_tree_node_id(other->frame_tree_node_id),
        factory(other->factory),
        prefetched_signed_exchange_cache(
            other->prefetched_signed_exchange_cache) {}

  ~BindContext() = default;

  const int frame_tree_node_id;
  scoped_refptr<network::SharedURLLoaderFactory> factory;
  scoped_refptr<PrefetchedSignedExchangeCache> prefetched_signed_exchange_cache;
};

PrefetchURLLoaderService::PrefetchURLLoaderService(
    BrowserContext* browser_context)
    : browser_context_(browser_context),
      preference_watcher_binding_(this),
      signed_exchange_prefetch_metric_recorder_(
          base::MakeRefCounted<SignedExchangePrefetchMetricRecorder>(
              base::DefaultTickClock::GetInstance())) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  accept_langs_ =
      GetContentClient()->browser()->GetAcceptLangs(browser_context);

  // Create a RendererPreferenceWatcher to observe updates in the preferences.
  blink::mojom::RendererPreferenceWatcherPtr watcher_ptr;
  preference_watcher_request_ = mojo::MakeRequest(&watcher_ptr);
  if (NavigationURLLoaderImpl::IsNavigationLoaderOnUIEnabled())
    preference_watcher_binding_.Bind(std::move(preference_watcher_request_));
  GetContentClient()->browser()->RegisterRendererPreferenceWatcher(
      browser_context, std::move(watcher_ptr));
}

void PrefetchURLLoaderService::InitializeResourceContext(
    ResourceContext* resource_context,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    ChromeBlobStorageContext* blob_storage_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!NavigationURLLoaderImpl::IsNavigationLoaderOnUIEnabled());
  DCHECK(!resource_context_);
  DCHECK(!request_context_getter_);
  resource_context_ = resource_context;
  request_context_getter_ = request_context_getter;
  blob_storage_context_ = blob_storage_context->context()->AsWeakPtr();
  preference_watcher_binding_.Bind(std::move(preference_watcher_request_));
}

void PrefetchURLLoaderService::GetFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    int frame_tree_node_id,
    std::unique_ptr<network::SharedURLLoaderFactoryInfo> factories,
    scoped_refptr<PrefetchedSignedExchangeCache>
        prefetched_signed_exchange_cache) {
  DCHECK_CURRENTLY_ON(
      NavigationURLLoaderImpl::GetLoaderRequestControllerThreadID());
  auto factory_bundle =
      network::SharedURLLoaderFactory::Create(std::move(factories));
  loader_factory_receivers_.Add(
      this, std::move(receiver),
      std::make_unique<BindContext>(
          frame_tree_node_id, factory_bundle,
          std::move(prefetched_signed_exchange_cache)));
}

void PrefetchURLLoaderService::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory,
    base::RepeatingCallback<int(void)> frame_tree_node_id_getter) {
  DCHECK_CURRENTLY_ON(
      NavigationURLLoaderImpl::GetLoaderRequestControllerThreadID());
  DCHECK_EQ(static_cast<int>(ResourceType::kPrefetch),
            resource_request.resource_type);
  DCHECK(NavigationURLLoaderImpl::IsNavigationLoaderOnUIEnabled() ||
         resource_context_);

  if (prefetch_load_callback_for_testing_)
    prefetch_load_callback_for_testing_.Run();

  scoped_refptr<PrefetchedSignedExchangeCache> prefetched_signed_exchange_cache;
  if (loader_factory_receivers_.current_context()) {
    prefetched_signed_exchange_cache =
        loader_factory_receivers_.current_context()
            ->prefetched_signed_exchange_cache;
  }

  // For now we strongly bind the loader to the request, while we can
  // also possibly make the new loader owned by the factory so that
  // they can live longer than the client (i.e. run in detached mode).
  // TODO(kinuko): Revisit this.
  mojo::MakeStrongBinding(
      std::make_unique<PrefetchURLLoader>(
          routing_id, request_id, options, frame_tree_node_id_getter,
          resource_request, std::move(client), traffic_annotation,
          std::move(network_loader_factory),
          base::BindRepeating(
              &PrefetchURLLoaderService::CreateURLLoaderThrottles, this,
              resource_request, frame_tree_node_id_getter),
          browser_context_, resource_context_, request_context_getter_,
          signed_exchange_prefetch_metric_recorder_,
          std::move(prefetched_signed_exchange_cache), blob_storage_context_,
          accept_langs_),
      std::move(request));
}

PrefetchURLLoaderService::~PrefetchURLLoaderService() = default;

void PrefetchURLLoaderService::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CURRENTLY_ON(
      NavigationURLLoaderImpl::GetLoaderRequestControllerThreadID());
  const auto& current_context = *loader_factory_receivers_.current_context();
  int frame_tree_node_id = current_context.frame_tree_node_id;
  CreateLoaderAndStart(
      std::move(request), routing_id, request_id, options, resource_request,
      std::move(client), traffic_annotation, current_context.factory,
      base::BindRepeating([](int id) { return id; }, frame_tree_node_id));
}

void PrefetchURLLoaderService::Clone(
    network::mojom::URLLoaderFactoryRequest request) {
  DCHECK_CURRENTLY_ON(
      NavigationURLLoaderImpl::GetLoaderRequestControllerThreadID());
  loader_factory_receivers_.Add(
      this, std::move(request),
      std::make_unique<BindContext>(
          loader_factory_receivers_.current_context()));
}

void PrefetchURLLoaderService::NotifyUpdate(
    blink::mojom::RendererPreferencesPtr new_prefs) {
  SetAcceptLanguages(new_prefs->accept_languages);
}

std::vector<std::unique_ptr<content::URLLoaderThrottle>>
PrefetchURLLoaderService::CreateURLLoaderThrottles(
    const network::ResourceRequest& request,
    base::RepeatingCallback<int(void)> frame_tree_node_id_getter) {
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService) ||
      !request_context_getter_ ||
      !request_context_getter_->GetURLRequestContext())
    return std::vector<std::unique_ptr<content::URLLoaderThrottle>>();
  int frame_tree_node_id = frame_tree_node_id_getter.Run();
  if (NavigationURLLoaderImpl::IsNavigationLoaderOnUIEnabled()) {
    return GetContentClient()->browser()->CreateURLLoaderThrottles(
        request, browser_context_,
        base::BindRepeating(&WebContents::FromFrameTreeNodeId,
                            frame_tree_node_id),
        nullptr /* navigation_ui_data */, frame_tree_node_id);
  }
  return GetContentClient()->browser()->CreateURLLoaderThrottlesOnIO(
      request, resource_context_,
      base::BindRepeating(&WebContents::FromFrameTreeNodeId,
                          frame_tree_node_id),
      nullptr /* navigation_ui_data */, frame_tree_node_id);
}

}  // namespace content

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_redirect/subresource_redirect_observer.h"

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/optimization_guide/optimization_guide_decider.h"
#include "components/optimization_guide/proto/performance_hints_metadata.pb.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/previews_resource_loading_hints.mojom.h"
#include "url/gurl.h"

namespace subresource_redirect {

namespace {

// Returns the OptimizationGuideDecider when LiteMode and the subresource
// redirect feature are enabled.
optimization_guide::OptimizationGuideDecider*
GetOptimizationGuideDeciderFromWebContents(content::WebContents* web_contents) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kSubresourceRedirect));
  if (!web_contents)
    return nullptr;

  if (Profile* profile =
          Profile::FromBrowserContext(web_contents->GetBrowserContext())) {
    if (data_reduction_proxy::DataReductionProxySettings::
            IsDataSaverEnabledByUser(profile->IsOffTheRecord(),
                                     profile->GetPrefs())) {
      return OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
    }
  }
  return nullptr;
}

// Pass down the |images_hints| to |render_frame_host|.
void SetResourceLoadingImageHints(
    content::RenderFrameHost* render_frame_host,
    blink::mojom::CompressPublicImagesHintsPtr images_hints) {
  mojo::AssociatedRemote<blink::mojom::PreviewsResourceLoadingHintsReceiver>
      loading_hints_agent;

  if (render_frame_host->GetRemoteAssociatedInterfaces()) {
    render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
        &loading_hints_agent);
    loading_hints_agent->SetCompressPublicImagesHints(std::move(images_hints));
  }
}

// Invoked when the OptimizationGuideKeyedService has sufficient information
// to make a decision for whether we can send resource loading image hints.
// If |decision| is true, public image URLs contained in |metadata| will be
// sent to the render frame host as specified by
// |render_frame_host_routing_id| to later be compressed.
void OnReadyToSendResourceLoadingImageHints(
    content::GlobalFrameRoutingId render_frame_host_routing_id,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& optimization_metadata) {
  content::RenderFrameHost* current_render_frame_host =
      content::RenderFrameHost::FromID(render_frame_host_routing_id);
  // Check if the same render frame host is still valid.
  if (!current_render_frame_host)
    return;

  if (decision != optimization_guide::OptimizationGuideDecision::kTrue)
    return;
  if (!optimization_metadata.public_image_metadata())
    return;

  std::vector<std::string> public_image_urls;
  const optimization_guide::proto::PublicImageMetadata public_image_metadata =
      optimization_metadata.public_image_metadata().value();
  public_image_urls.reserve(public_image_metadata.url_size());
  for (const auto& url : public_image_metadata.url())
    public_image_urls.push_back(url);
  // Pass down the image URLs to renderer even if it could be empty. This acts
  // as a signal that the image hint fetch has finished, for coverage metrics
  // purposes.
  SetResourceLoadingImageHints(
      current_render_frame_host,
      blink::mojom::CompressPublicImagesHints::New(public_image_urls));
}

}  // namespace

// static
void SubresourceRedirectObserver::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  if (base::FeatureList::IsEnabled(blink::features::kSubresourceRedirect)) {
    SubresourceRedirectObserver::CreateForWebContents(web_contents);
  }
}

SubresourceRedirectObserver::SubresourceRedirectObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  auto* optimization_guide_decider =
      GetOptimizationGuideDeciderFromWebContents(web_contents);
  if (optimization_guide_decider) {
    optimization_guide_decider->RegisterOptimizationTypesAndTargets(
        {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES}, {});
  }
}

SubresourceRedirectObserver::~SubresourceRedirectObserver() = default;

void SubresourceRedirectObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK(navigation_handle);
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->GetURL().SchemeIsHTTPOrHTTPS()) {
    return;
  }
  auto* optimization_guide_decider = GetOptimizationGuideDeciderFromWebContents(
      navigation_handle->GetWebContents());
  if (!optimization_guide_decider)
    return;

  content::RenderFrameHost* render_frame_host =
      navigation_handle->GetRenderFrameHost();
  if (!render_frame_host || !render_frame_host->GetProcess())
    return;

  optimization_guide_decider->CanApplyOptimizationAsync(
      navigation_handle, optimization_guide::proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce(&OnReadyToSendResourceLoadingImageHints,
                     content::GlobalFrameRoutingId(
                         render_frame_host->GetProcess()->GetID(),
                         render_frame_host->GetRoutingID())));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SubresourceRedirectObserver)

}  // namespace subresource_redirect

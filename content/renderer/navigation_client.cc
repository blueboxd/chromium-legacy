// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/navigation_client.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/renderer/render_frame_impl.h"
#include "third_party/blink/public/platform/task_type.h"

namespace content {

NavigationClient::NavigationClient(RenderFrameImpl* render_frame)
    : navigation_client_binding_(this), render_frame_(render_frame) {}

NavigationClient::~NavigationClient() {}

void NavigationClient::CommitNavigation(
    const CommonNavigationParams& common_params,
    const CommitNavigationParams& commit_params,
    const network::ResourceResponseHead& response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    std::unique_ptr<blink::URLLoaderFactoryBundleInfo> subresource_loaders,
    base::Optional<std::vector<::content::mojom::TransferrableURLLoaderPtr>>
        subresource_overrides,
    blink::mojom::ControllerServiceWorkerInfoPtr controller_service_worker_info,
    blink::mojom::ServiceWorkerProviderInfoForClientPtr provider_info,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        prefetch_loader_factory,
    const base::UnguessableToken& devtools_navigation_token,
    CommitNavigationCallback callback) {
  // TODO(ahemery): The reset should be done when the navigation did commit
  // (meaning at a later stage). This is not currently possible because of
  // race conditions leading to the early deletion of NavigationRequest would
  // unexpectedly abort the ongoing navigation. Remove when the races are fixed.
  ResetDisconnectionHandler();
  render_frame_->CommitPerNavigationMojoInterfaceNavigation(
      common_params, commit_params, response_head, std::move(response_body),
      std::move(url_loader_client_endpoints), std::move(subresource_loaders),
      std::move(subresource_overrides),
      std::move(controller_service_worker_info), std::move(provider_info),
      std::move(prefetch_loader_factory), devtools_navigation_token,
      std::move(callback));
}

void NavigationClient::CommitFailedNavigation(
    const CommonNavigationParams& common_params,
    const CommitNavigationParams& commit_params,
    bool has_stale_copy_in_cache,
    int error_code,
    const base::Optional<std::string>& error_page_content,
    std::unique_ptr<blink::URLLoaderFactoryBundleInfo> subresource_loaders,
    CommitFailedNavigationCallback callback) {
  ResetDisconnectionHandler();
  render_frame_->CommitFailedPerNavigationMojoInterfaceNavigation(
      common_params, commit_params, has_stale_copy_in_cache, error_code,
      error_page_content, std::move(subresource_loaders), std::move(callback));
}

void NavigationClient::Bind(mojom::NavigationClientAssociatedRequest request) {
  navigation_client_binding_.Bind(
      std::move(request), render_frame_->GetTaskRunner(
                              blink::TaskType::kInternalNavigationAssociated));
  SetDisconnectionHandler();
}

void NavigationClient::MarkWasInitiatedInThisFrame() {
  DCHECK(!was_initiated_in_this_frame_);
  was_initiated_in_this_frame_ = true;
}

void NavigationClient::SetDisconnectionHandler() {
  navigation_client_binding_.set_connection_error_handler(base::BindOnce(
      &NavigationClient::OnDroppedNavigation, base::Unretained(this)));
}

void NavigationClient::ResetDisconnectionHandler() {
  navigation_client_binding_.set_connection_error_handler(base::DoNothing());
}

void NavigationClient::OnDroppedNavigation() {
  render_frame_->OnDroppedNavigation();
}

}  // namespace content

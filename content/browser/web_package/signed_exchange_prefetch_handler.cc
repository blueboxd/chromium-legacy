// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_prefetch_handler.h"

#include "base/callback.h"
#include "base/feature_list.h"
#include "content/browser/web_package/signed_exchange_devtools_proxy.h"
#include "content/browser/web_package/signed_exchange_loader.h"
#include "content/browser/web_package/signed_exchange_prefetch_metric_recorder.h"
#include "content/browser/web_package/signed_exchange_reporter.h"
#include "content/browser/web_package/signed_exchange_url_loader_factory_for_non_network_service.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

SignedExchangePrefetchHandler::SignedExchangePrefetchHandler(
    base::RepeatingCallback<int(void)> frame_tree_node_id_getter,
    const network::ResourceRequest& resource_request,
    const network::ResourceResponseHead& response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    network::mojom::URLLoaderPtr network_loader,
    network::mojom::URLLoaderClientRequest network_client_request,
    scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory,
    URLLoaderThrottlesGetter loader_throttles_getter,
    ResourceContext* resource_context,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    network::mojom::URLLoaderClient* forwarding_client,
    scoped_refptr<SignedExchangePrefetchMetricRecorder> metric_recorder,
    const std::string& accept_langs)
    : loader_client_binding_(this), forwarding_client_(forwarding_client) {
  network::mojom::URLLoaderClientEndpointsPtr endpoints =
      network::mojom::URLLoaderClientEndpoints::New(
          std::move(network_loader).PassInterface(),
          std::move(network_client_request));
  network::mojom::URLLoaderClientPtr client;
  loader_client_binding_.Bind(mojo::MakeRequest(&client));
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    url_loader_factory = base::MakeRefCounted<
        SignedExchangeURLLoaderFactoryForNonNetworkService>(
        resource_context, request_context_getter.get());
  } else {
    url_loader_factory = std::move(network_loader_factory);
  }
  signed_exchange_loader_ = std::make_unique<SignedExchangeLoader>(
      resource_request, response_head, std::move(response_body),
      std::move(client), std::move(endpoints),
      network::mojom::kURLLoadOptionNone,
      false /* should_redirect_to_fallback */,
      std::make_unique<SignedExchangeDevToolsProxy>(
          resource_request.url, response_head, frame_tree_node_id_getter,
          base::nullopt /* devtools_navigation_token */,
          resource_request.report_raw_headers),
      SignedExchangeReporter::MaybeCreate(
          resource_request.url, resource_request.referrer.spec(), response_head,
          frame_tree_node_id_getter),
      std::move(url_loader_factory), loader_throttles_getter,
      frame_tree_node_id_getter, std::move(metric_recorder), accept_langs);
}

SignedExchangePrefetchHandler::~SignedExchangePrefetchHandler() = default;

network::mojom::URLLoaderClientRequest
SignedExchangePrefetchHandler::FollowRedirect(
    network::mojom::URLLoaderRequest loader_request) {
  DCHECK(signed_exchange_loader_);
  network::mojom::URLLoaderClientPtr client;
  auto pending_request = mojo::MakeRequest(&client);
  signed_exchange_loader_->ConnectToClient(std::move(client));
  mojo::MakeStrongBinding(std::move(signed_exchange_loader_),
                          std::move(loader_request));
  return pending_request;
}

base::Optional<net::SHA256HashValue>
SignedExchangePrefetchHandler::ComputeHeaderIntegrity() const {
  if (!signed_exchange_loader_)
    return base::nullopt;
  return signed_exchange_loader_->ComputeHeaderIntegrity();
}

base::Time SignedExchangePrefetchHandler::GetSignatureExpireTime() const {
  if (!signed_exchange_loader_)
    return base::Time();
  return signed_exchange_loader_->GetSignatureExpireTime();
}

void SignedExchangePrefetchHandler::OnReceiveResponse(
    const network::ResourceResponseHead& head) {
  NOTREACHED();
}

void SignedExchangePrefetchHandler::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& head) {
  forwarding_client_->OnReceiveRedirect(redirect_info, head);
}

void SignedExchangePrefetchHandler::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    base::OnceCallback<void()> callback) {
  NOTREACHED();
}

void SignedExchangePrefetchHandler::OnReceiveCachedMetadata(
    mojo_base::BigBuffer data) {
  NOTREACHED();
}

void SignedExchangePrefetchHandler::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  NOTREACHED();
}

void SignedExchangePrefetchHandler::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  NOTREACHED();
}

void SignedExchangePrefetchHandler::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  // We only reach here on error, since successful completion of the
  // outer sxg load should trigger redirect and land on ::OnReceiveRedirect.
  DCHECK_NE(net::OK, status.error_code);

  forwarding_client_->OnComplete(status);
}

}  // namespace content

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_proxying_url_loader_factory.h"

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "mojo/public/cpp/bindings/receiver.h"

IsolatedPrerenderProxyingURLLoaderFactory::InProgressRequest::InProgressRequest(
    IsolatedPrerenderProxyingURLLoaderFactory* factory,
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
    : factory_(factory),
      target_client_(std::move(client)),
      loader_receiver_(this, std::move(loader_receiver)) {
  mojo::PendingRemote<network::mojom::URLLoaderClient> proxy_client =
      client_receiver_.BindNewPipeAndPassRemote();

  factory_->target_factory_->CreateLoaderAndStart(
      target_loader_.BindNewPipeAndPassReceiver(), routing_id, request_id,
      options, request, std::move(proxy_client), traffic_annotation);

  // Calls |OnBindingsClosed| only after both disconnect handlers have been run.
  base::RepeatingClosure closure = base::BarrierClosure(
      2, base::BindOnce(&InProgressRequest::OnBindingsClosed,
                        base::Unretained(this)));
  loader_receiver_.set_disconnect_handler(closure);
  client_receiver_.set_disconnect_handler(closure);
}

IsolatedPrerenderProxyingURLLoaderFactory::InProgressRequest::
    ~InProgressRequest() {
  if (destruction_callback_) {
    std::move(destruction_callback_).Run();
  }
}

void IsolatedPrerenderProxyingURLLoaderFactory::InProgressRequest::
    FollowRedirect(const std::vector<std::string>& removed_headers,
                   const net::HttpRequestHeaders& modified_headers,
                   const net::HttpRequestHeaders& modified_cors_exempt_headers,
                   const base::Optional<GURL>& new_url) {
  target_loader_->FollowRedirect(removed_headers, modified_headers,
                                 modified_cors_exempt_headers, new_url);
}

void IsolatedPrerenderProxyingURLLoaderFactory::InProgressRequest::SetPriority(
    net::RequestPriority priority,
    int32_t intra_priority_value) {
  target_loader_->SetPriority(priority, intra_priority_value);
}

void IsolatedPrerenderProxyingURLLoaderFactory::InProgressRequest::
    PauseReadingBodyFromNet() {
  target_loader_->PauseReadingBodyFromNet();
}

void IsolatedPrerenderProxyingURLLoaderFactory::InProgressRequest::
    ResumeReadingBodyFromNet() {
  target_loader_->ResumeReadingBodyFromNet();
}

void IsolatedPrerenderProxyingURLLoaderFactory::InProgressRequest::
    OnReceiveResponse(network::mojom::URLResponseHeadPtr head) {
  target_client_->OnReceiveResponse(std::move(head));
}

void IsolatedPrerenderProxyingURLLoaderFactory::InProgressRequest::
    OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                      network::mojom::URLResponseHeadPtr head) {
  target_client_->OnReceiveRedirect(redirect_info, std::move(head));
}

void IsolatedPrerenderProxyingURLLoaderFactory::InProgressRequest::
    OnUploadProgress(int64_t current_position,
                     int64_t total_size,
                     OnUploadProgressCallback callback) {
  target_client_->OnUploadProgress(current_position, total_size,
                                   std::move(callback));
}

void IsolatedPrerenderProxyingURLLoaderFactory::InProgressRequest::
    OnReceiveCachedMetadata(mojo_base::BigBuffer data) {
  target_client_->OnReceiveCachedMetadata(std::move(data));
}

void IsolatedPrerenderProxyingURLLoaderFactory::InProgressRequest::
    OnTransferSizeUpdated(int32_t transfer_size_diff) {
  target_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void IsolatedPrerenderProxyingURLLoaderFactory::InProgressRequest::
    OnStartLoadingResponseBody(mojo::ScopedDataPipeConsumerHandle body) {
  target_client_->OnStartLoadingResponseBody(std::move(body));
}

void IsolatedPrerenderProxyingURLLoaderFactory::InProgressRequest::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  target_client_->OnComplete(status);
}

void IsolatedPrerenderProxyingURLLoaderFactory::InProgressRequest::
    OnBindingsClosed() {
  // Destroys |this|.
  factory_->RemoveRequest(this);
}

IsolatedPrerenderProxyingURLLoaderFactory::
    IsolatedPrerenderProxyingURLLoaderFactory(
        int frame_tree_node_id,
        mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
        mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory,
        DisconnectCallback on_disconnect)
    : on_disconnect_(std::move(on_disconnect)) {
  target_factory_.Bind(std::move(target_factory));
  target_factory_.set_disconnect_handler(base::BindOnce(
      &IsolatedPrerenderProxyingURLLoaderFactory::OnTargetFactoryError,
      base::Unretained(this)));

  proxy_receivers_.Add(this, std::move(loader_receiver));
  proxy_receivers_.set_disconnect_handler(base::BindRepeating(
      &IsolatedPrerenderProxyingURLLoaderFactory::OnProxyBindingError,
      base::Unretained(this)));
}

IsolatedPrerenderProxyingURLLoaderFactory::
    ~IsolatedPrerenderProxyingURLLoaderFactory() = default;

void IsolatedPrerenderProxyingURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  requests_.insert(std::make_unique<InProgressRequest>(
      this, std::move(loader_receiver), routing_id, request_id, options,
      request, std::move(client), traffic_annotation));
}

void IsolatedPrerenderProxyingURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver) {
  proxy_receivers_.Add(this, std::move(loader_receiver));
}

void IsolatedPrerenderProxyingURLLoaderFactory::OnTargetFactoryError() {
  // Stop calls to CreateLoaderAndStart() when |target_factory_| is invalid.
  target_factory_.reset();
  proxy_receivers_.Clear();

  MaybeDestroySelf();
}

void IsolatedPrerenderProxyingURLLoaderFactory::OnProxyBindingError() {
  if (proxy_receivers_.empty())
    target_factory_.reset();

  MaybeDestroySelf();
}

void IsolatedPrerenderProxyingURLLoaderFactory::RemoveRequest(
    InProgressRequest* request) {
  auto it = requests_.find(request);
  DCHECK(it != requests_.end());
  requests_.erase(it);

  MaybeDestroySelf();
}

void IsolatedPrerenderProxyingURLLoaderFactory::MaybeDestroySelf() {
  // Even if all URLLoaderFactory pipes connected to this object have been
  // closed it has to stay alive until all active requests have completed.
  if (target_factory_.is_bound() || !requests_.empty())
    return;

  // Deletes |this|.
  std::move(on_disconnect_).Run(this);
}

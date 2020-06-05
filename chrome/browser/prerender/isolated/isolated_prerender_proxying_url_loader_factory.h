// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_PROXYING_URL_LOADER_FACTORY_H_
#define CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_PROXYING_URL_LOADER_FACTORY_H_

#include <memory>
#include <set>

#include "base/callback.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

// This class is an intermediary URLLoaderFactory between the renderer and
// network process, AKA proxy which should not be confused with a proxy server.
//
// Currently, this class doesn't do anything but forward all messages directly
// to the normal network process and is only boilerplate for future changes.
class IsolatedPrerenderProxyingURLLoaderFactory
    : public network::mojom::URLLoaderFactory {
 public:
  using DisconnectCallback =
      base::OnceCallback<void(IsolatedPrerenderProxyingURLLoaderFactory*)>;

  IsolatedPrerenderProxyingURLLoaderFactory(
      int frame_tree_node_id,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory,
      DisconnectCallback on_disconnect);
  ~IsolatedPrerenderProxyingURLLoaderFactory() override;

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory>
                 loader_receiver) override;

 private:
  class InProgressRequest : public network::mojom::URLLoader,
                            public network::mojom::URLLoaderClient {
   public:
    InProgressRequest(
        IsolatedPrerenderProxyingURLLoaderFactory* factory,
        mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
        int32_t routing_id,
        int32_t request_id,
        uint32_t options,
        const network::ResourceRequest& request,
        mojo::PendingRemote<network::mojom::URLLoaderClient> client,
        const net::MutableNetworkTrafficAnnotationTag& traffic_annotation);
    ~InProgressRequest() override;

    // network::mojom::URLLoader:
    void FollowRedirect(
        const std::vector<std::string>& removed_headers,
        const net::HttpRequestHeaders& modified_headers,
        const net::HttpRequestHeaders& modified_cors_exempt_headers,
        const base::Optional<GURL>& new_url) override;
    void SetPriority(net::RequestPriority priority,
                     int32_t intra_priority_value) override;
    void PauseReadingBodyFromNet() override;
    void ResumeReadingBodyFromNet() override;

    // network::mojom::URLLoaderClient:
    void OnReceiveResponse(network::mojom::URLResponseHeadPtr head) override;
    void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                           network::mojom::URLResponseHeadPtr head) override;
    void OnUploadProgress(int64_t current_position,
                          int64_t total_size,
                          OnUploadProgressCallback callback) override;
    void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override;
    void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
    void OnStartLoadingResponseBody(
        mojo::ScopedDataPipeConsumerHandle body) override;
    void OnComplete(const network::URLLoaderCompletionStatus& status) override;

   private:
    void OnBindingsClosed();

    // Back pointer to the factory which owns this class.
    IsolatedPrerenderProxyingURLLoaderFactory* const factory_;

    // This should be run on destruction of |this|.
    base::OnceClosure destruction_callback_;

    // There are the mojo pipe endpoints between this proxy and the renderer.
    // Messages received by |client_receiver_| are forwarded to
    // |target_client_|.
    mojo::Receiver<network::mojom::URLLoaderClient> client_receiver_{this};
    mojo::Remote<network::mojom::URLLoaderClient> target_client_;

    // These are the mojo pipe endpoints between this proxy and the network
    // process. Messages received by |loader_receiver_| are forwarded to
    // |target_loader_|.
    mojo::Receiver<network::mojom::URLLoader> loader_receiver_;
    mojo::Remote<network::mojom::URLLoader> target_loader_;

    DISALLOW_COPY_AND_ASSIGN(InProgressRequest);
  };

  void OnTargetFactoryError();
  void OnProxyBindingError();
  void RemoveRequest(InProgressRequest* request);
  void MaybeDestroySelf();

  mojo::ReceiverSet<network::mojom::URLLoaderFactory> proxy_receivers_;

  // All active network requests handled by this factory.
  std::set<std::unique_ptr<InProgressRequest>, base::UniquePtrComparator>
      requests_;

  // The network process URLLoaderFactory.
  mojo::Remote<network::mojom::URLLoaderFactory> target_factory_;

  // Deletes |this| when run.
  DisconnectCallback on_disconnect_;

  DISALLOW_COPY_AND_ASSIGN(IsolatedPrerenderProxyingURLLoaderFactory);
};

#endif  // CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_PROXYING_URL_LOADER_FACTORY_H_

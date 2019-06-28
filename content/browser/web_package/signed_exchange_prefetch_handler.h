// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_PREFETCH_HANDLER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_PREFETCH_HANDLER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace net {
struct SHA256HashValue;
class URLRequestContextGetter;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace content {

class ResourceContext;
class URLLoaderThrottle;
class SignedExchangeLoader;
class SignedExchangePrefetchMetricRecorder;

// Attached to each PrefetchURLLoader if the prefetch is for a signed exchange.
class SignedExchangePrefetchHandler final
    : public network::mojom::URLLoaderClient {
 public:
  using URLLoaderThrottlesGetter = base::RepeatingCallback<
      std::vector<std::unique_ptr<content::URLLoaderThrottle>>()>;

  // This takes |network_loader| and |network_client| to set up the
  // SignedExchangeLoader (so that the loader can load data from the network).
  // |forwarding_client| is a pointer to the downstream client (typically who
  // creates this handler).
  SignedExchangePrefetchHandler(
      base::RepeatingCallback<int(void)> frame_tree_node_id_getter,
      const network::ResourceRequest& resource_request,
      const network::ResourceResponseHead& response,
      mojo::ScopedDataPipeConsumerHandle response_body,
      network::mojom::URLLoaderPtr network_loader,
      network::mojom::URLLoaderClientRequest network_client_request,
      scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory,
      URLLoaderThrottlesGetter loader_throttles_getter,
      ResourceContext* resource_context,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      network::mojom::URLLoaderClient* forwarding_client,
      scoped_refptr<SignedExchangePrefetchMetricRecorder> metric_recorder,
      const std::string& accept_langs);

  ~SignedExchangePrefetchHandler() override;

  // This connects |loader_request| to the SignedExchangeLoader, and returns the
  // pending client request to the loader.
  // The returned client request can be bound to the downstream client so that
  // they can start directly receiving upcalls from the SignedExchangeLoader.
  // After this point |this| can be destructed.
  network::mojom::URLLoaderClientRequest FollowRedirect(
      network::mojom::URLLoaderRequest loader_request);

  // Returns the header integrity value of the loaded signed exchange if
  // available. This is available after OnReceiveRedirect() of
  // |forwarding_client| is called and before FollowRedirect() of |this| is
  // called. Otherwise returns nullopt.
  base::Optional<net::SHA256HashValue> ComputeHeaderIntegrity() const;

  // Returns the signature expire time of the loaded signed exchange if
  // available. This is available after OnReceiveRedirect() of
  // |forwarding_client| is called and before FollowRedirect() of |this| is
  // called. Otherwise returns a null Time.
  base::Time GetSignatureExpireTime() const;

 private:
  // network::mojom::URLLoaderClient overrides:
  void OnReceiveResponse(const network::ResourceResponseHead& head) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const network::ResourceResponseHead& head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        base::OnceCallback<void()> callback) override;
  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  mojo::Binding<network::mojom::URLLoaderClient> loader_client_binding_;

  std::unique_ptr<SignedExchangeLoader> signed_exchange_loader_;

  network::mojom::URLLoaderClient* forwarding_client_;

  DISALLOW_COPY_AND_ASSIGN(SignedExchangePrefetchHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_PREFETCH_HANDLER_H_

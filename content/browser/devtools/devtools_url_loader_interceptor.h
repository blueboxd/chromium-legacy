// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_LOADER_INTERCEPTOR_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_LOADER_INTERCEPTOR_H_

#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/devtools_network_interceptor.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

class RenderProcessHost;

class DevToolsURLLoaderInterceptor {
 public:
  class Impl;

  using HandleAuthRequestCallback =
      base::OnceCallback<void(bool use_fallback,
                              const base::Optional<net::AuthCredentials>&)>;
  // Can only be called on the IO thread.
  static void HandleAuthRequest(int32_t process_id,
                                int32_t routing_id,
                                int32_t request_id,
                                const net::AuthChallengeInfo& auth_info,
                                HandleAuthRequestCallback callback);

  explicit DevToolsURLLoaderInterceptor(
      DevToolsNetworkInterceptor::RequestInterceptedCallback callback);
  ~DevToolsURLLoaderInterceptor();

  void SetPatterns(std::vector<DevToolsNetworkInterceptor::Pattern> patterns,
                   bool handle_auth);

  void GetResponseBody(
      const std::string& interception_id,
      std::unique_ptr<
          DevToolsNetworkInterceptor::GetResponseBodyForInterceptionCallback>
          callback);
  void TakeResponseBodyPipe(
      const std::string& interception_id,
      DevToolsNetworkInterceptor::TakeResponseBodyPipeCallback callback);
  void ContinueInterceptedRequest(
      const std::string& interception_id,
      std::unique_ptr<DevToolsNetworkInterceptor::Modifications> modifications,
      std::unique_ptr<
          DevToolsNetworkInterceptor::ContinueInterceptedRequestCallback>
          callback);

  bool CreateProxyForInterception(
      RenderProcessHost* rph,
      const base::UnguessableToken& frame_token,
      bool is_navigation,
      bool is_download,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>*
          target_factory_receiver) const;

 private:
  bool enabled_;
  std::unique_ptr<Impl, base::OnTaskRunnerDeleter> impl_;
  base::WeakPtr<Impl> weak_impl_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsURLLoaderInterceptor);
};

// The purpose of this class is to have a thin wrapper around
// InterfacePtr<URLLoaderFactory> that is held by the client as
// unique_ptr<network::mojom::URLLoaderFactory>, since this is the
// way some clients pass the factory. We prefer wrapping a mojo proxy
// rather than exposing original DevToolsURLLoaderFactoryProxy because
// this takes care of thread hopping when necessary.
class DevToolsURLLoaderFactoryAdapter
    : public network::mojom::URLLoaderFactory {
 public:
  DevToolsURLLoaderFactoryAdapter() = delete;
  explicit DevToolsURLLoaderFactoryAdapter(
      network::mojom::URLLoaderFactoryPtr factory);
  ~DevToolsURLLoaderFactoryAdapter() override;

 private:
  // network::mojom::URLLoaderFactory implementation
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest loader,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(network::mojom::URLLoaderFactoryRequest request) override;

  network::mojom::URLLoaderFactoryPtr factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_LOADER_INTERCEPTOR_H_

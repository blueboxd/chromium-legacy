// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_mojo_proxy_resolver_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/system_connector.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

ChromeMojoProxyResolverFactory::ChromeMojoProxyResolverFactory() = default;

ChromeMojoProxyResolverFactory::~ChromeMojoProxyResolverFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

mojo::PendingRemote<proxy_resolver::mojom::ProxyResolverFactory>
ChromeMojoProxyResolverFactory::CreateWithSelfOwnedReceiver() {
  mojo::PendingRemote<proxy_resolver::mojom::ProxyResolverFactory> remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ChromeMojoProxyResolverFactory>(),
      remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void ChromeMojoProxyResolverFactory::CreateResolver(
    const std::string& pac_script,
    mojo::PendingReceiver<proxy_resolver::mojom::ProxyResolver> receiver,
    mojo::PendingRemote<
        proxy_resolver::mojom::ProxyResolverFactoryRequestClient> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Bind a ProxyResolverFactory backed by the proxy resolver service, have it
  // create a ProxyResolverFactory and then destroy the factory, to avoid
  // keeping the service alive after all resolvers have been destroyed.
  mojo::Remote<proxy_resolver::mojom::ProxyResolverFactory> resolver_factory;
  content::GetSystemConnector()->Connect(
      proxy_resolver::mojom::kProxyResolverServiceName,
      resolver_factory.BindNewPipeAndPassReceiver());
  resolver_factory->CreateResolver(pac_script, std::move(receiver),
                                   std::move(client));
}

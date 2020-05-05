// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/cert_verifier/cert_verifier_service_factory.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_verifier.h"
#include "services/cert_verifier/cert_net_url_loader/cert_net_fetcher_url_loader.h"
#include "services/cert_verifier/cert_verifier_service.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/cpp/cert_verifier/cert_verifier_creation.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace cert_verifier {

namespace {
void ReconnectCertNetFetcher(
    mojo::Remote<mojom::URLLoaderFactoryConnector>* connector,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>
        url_loader_factory) {
  (*connector)->CreateURLLoaderFactory(std::move(url_loader_factory));
}
}  // namespace

CertVerifierServiceFactoryImpl::CertVerifierServiceFactoryImpl(
    mojo::PendingReceiver<mojom::CertVerifierServiceFactory> receiver)
    : receiver_(this, std::move(receiver)) {}

CertVerifierServiceFactoryImpl::~CertVerifierServiceFactoryImpl() = default;

// static
scoped_refptr<CertNetFetcherURLLoader>
CertVerifierServiceFactoryImpl::CreateCertNetFetcher(
    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
    mojo::PendingRemote<mojom::URLLoaderFactoryConnector>
        cert_net_fetcher_url_loader_factory_connector) {
  auto connector =
      std::make_unique<mojo::Remote<mojom::URLLoaderFactoryConnector>>(
          std::move(cert_net_fetcher_url_loader_factory_connector));
  // The callback will own the CertNetFetcherReconnector, and the callback
  // will be owned by the CertNetFetcherURLLoader. Only the
  // CertNetFetcherURLLoader uses the CertNetFetcherReconnector.
  auto reconnector_cb = base::BindRepeating(&ReconnectCertNetFetcher,
                                            base::Owned(std::move(connector)));
  return base::MakeRefCounted<CertNetFetcherURLLoader>(
      std::move(url_loader_factory), std::move(reconnector_cb));
}

void CertVerifierServiceFactoryImpl::GetNewCertVerifier(
    mojo::PendingReceiver<mojom::CertVerifierService> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
    mojo::PendingRemote<mojom::URLLoaderFactoryConnector>
        cert_net_fetcher_url_loader_factory_connector,
    network::mojom::CertVerifierCreationParamsPtr creation_params) {
  scoped_refptr<CertNetFetcherURLLoader> cert_net_fetcher;

  // Sometimes the cert_net_fetcher isn't used by CreateCertVerifier.
  // But losing the last ref without calling Shutdown() will cause a CHECK
  // failure, so keep a ref.
  if (network::IsUsingCertNetFetcher()) {
    DCHECK(url_loader_factory.is_valid());
    DCHECK(cert_net_fetcher_url_loader_factory_connector.is_valid());
    cert_net_fetcher = CreateCertNetFetcher(
        std::move(url_loader_factory),
        std::move(cert_net_fetcher_url_loader_factory_connector));
  }

  // Create a new CertVerifier to back our service. This will be instantiated
  // without the coalescing or caching layers, because those layers will work
  // better in the network process, and will give us NetLog visibility if
  // running in the network process.
  std::unique_ptr<net::CertVerifier> cert_verifier =
      network::CreateCertVerifier(creation_params.get(), cert_net_fetcher);

  // As an optimization, if the CertNetFetcher isn't used by the CertVerifier,
  // shut it down immediately.
  if (cert_net_fetcher && cert_net_fetcher->HasOneRef()) {
    cert_net_fetcher->Shutdown();
    cert_net_fetcher.reset();
  }

  // The service will delete itself upon disconnection.
  new internal::CertVerifierServiceImpl(std::move(cert_verifier),
                                        std::move(receiver),
                                        std::move(cert_net_fetcher));
}

}  // namespace cert_verifier

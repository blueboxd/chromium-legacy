// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CERT_VERIFIER_CERT_VERIFIER_SERVICE_FACTORY_H_
#define SERVICES_CERT_VERIFIER_CERT_VERIFIER_SERVICE_FACTORY_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/cert_verifier/cert_net_url_loader/cert_net_fetcher_url_loader.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/mojom/cert_verifier_service.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace cert_verifier {

// Implements mojom::CertVerifierServiceFactory, and calls
// network::CreateCertVerifier to instantiate the concrete net::CertVerifier
// used to service requests.
class CertVerifierServiceFactoryImpl
    : public mojom::CertVerifierServiceFactory {
 public:
  explicit CertVerifierServiceFactoryImpl(
      mojo::PendingReceiver<mojom::CertVerifierServiceFactory> receiver);
  ~CertVerifierServiceFactoryImpl() override;

  // Creates a CertNetFetcherURLLoader using the given URLLoaderFactory, that
  // will try to reconnect its URLLoaderFactory using the
  // URLLoaderFactoryConnector in case the original URLLoaderFactory
  // disconnects.
  static scoped_refptr<CertNetFetcherURLLoader> CreateCertNetFetcher(
      mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
      mojo::PendingRemote<mojom::URLLoaderFactoryConnector>
          cert_net_fetcher_url_loader_factory_connector);

  // mojom::CertVerifierServiceFactory implementation:
  void GetNewCertVerifier(
      mojo::PendingReceiver<mojom::CertVerifierService> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
      mojo::PendingRemote<mojom::URLLoaderFactoryConnector>
          cert_net_fetcher_url_loader_factory_connector,
      network::mojom::CertVerifierCreationParamsPtr creation_params) override;

 private:
  mojo::Receiver<mojom::CertVerifierServiceFactory> receiver_;
};

}  // namespace cert_verifier

#endif  // SERVICES_CERT_VERIFIER_CERT_VERIFIER_SERVICE_FACTORY_H_

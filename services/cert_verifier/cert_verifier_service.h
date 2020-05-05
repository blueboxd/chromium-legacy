// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CERT_VERIFIER_CERT_VERIFIER_SERVICE_H_
#define SERVICES_CERT_VERIFIER_CERT_VERIFIER_SERVICE_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/log/net_log_with_source.h"
#include "services/network/public/mojom/cert_verifier_service.mojom.h"

// Defines an implementation of a Cert Verifier Service to be queried by network
// service or others.
namespace cert_verifier {
namespace internal {

// This class will delete itself upon disconnection of its Mojo receiver.
class CertVerifierServiceImpl : public mojom::CertVerifierService {
 public:
  explicit CertVerifierServiceImpl(
      std::unique_ptr<net::CertVerifier> verifier,
      mojo::PendingReceiver<mojom::CertVerifierService> receiver,
      scoped_refptr<net::CertNetFetcher> cert_net_fetcher);

  // mojom::CertVerifierService implementation:
  void Verify(const net::CertVerifier::RequestParams& params,
              mojo::PendingRemote<mojom::CertVerifierRequest>
                  cert_verifier_request) override;
  void SetConfig(const net::CertVerifier::Config& config) override;

 private:
  ~CertVerifierServiceImpl() override;

  void OnDisconnectFromService();

  std::unique_ptr<net::CertVerifier> verifier_;
  mojo::Receiver<mojom::CertVerifierService> receiver_;
  scoped_refptr<net::CertNetFetcher> cert_net_fetcher_;

  // A null NetLog for |verifier_->Verify()|. Initialized with a non-capturing
  // NetLog.
  const net::NetLogWithSource null_net_log_;
};

}  // namespace internal
}  // namespace cert_verifier

#endif  // SERVICES_CERT_VERIFIER_CERT_VERIFIER_SERVICE_H_

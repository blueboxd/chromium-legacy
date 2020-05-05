// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CERT_VERIFIER_MOJO_CERT_VERIFIER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CERT_VERIFIER_MOJO_CERT_VERIFIER_H_

#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/log/net_log_with_source.h"
#include "services/network/public/mojom/cert_verifier_service.mojom.h"

namespace cert_verifier {

// Implementation of net::CertVerifier that proxies across a Mojo interface to
// verify certificates.
class MojoCertVerifier : public net::CertVerifier {
 public:
  explicit MojoCertVerifier(
      mojo::PendingRemote<mojom::CertVerifierService> mojo_cert_verifier);
  ~MojoCertVerifier() override;

  // net::CertVerifier implementation:
  int Verify(const net::CertVerifier::RequestParams& params,
             net::CertVerifyResult* verify_result,
             net::CompletionOnceCallback callback,
             std::unique_ptr<net::CertVerifier::Request>* out_req,
             const net::NetLogWithSource& net_log) override;
  void SetConfig(const net::CertVerifier::Config& config) override;

 private:
  mojo::Remote<mojom::CertVerifierService> mojo_cert_verifier_;
};

}  // namespace cert_verifier

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CERT_VERIFIER_MOJO_CERT_VERIFIER_H_

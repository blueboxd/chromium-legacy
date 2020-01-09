// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/enterprise/cert_store/security_token_operation_bridge.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service_factory.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace arc {
namespace keymaster {

namespace {

base::Optional<uint16_t> ConvertMojoAlgorithmToTls(mojom::Algorithm algorithm,
                                                   mojom::Digest digest) {
  // Currently, only RSA algorithm is supported.
  if (algorithm != mojom::Algorithm::kRsaPkcs1)
    return base::nullopt;
  switch (digest) {
    case mojom::Digest::kSha1:
      return SSL_SIGN_RSA_PKCS1_SHA1;
    case mojom::Digest::kSha256:
      return SSL_SIGN_RSA_PKCS1_SHA256;
    case mojom::Digest::kSha384:
      return SSL_SIGN_RSA_PKCS1_SHA384;
    case mojom::Digest::kSha512:
      return SSL_SIGN_RSA_PKCS1_SHA512;
  }
  return base::nullopt;
}

mojom::SignatureResult ConvertNetToMojomError(net::Error error) {
  switch (error) {
    case net::OK:
      return mojom::SignatureResult::kOk;
    case net::ERR_FAILED:
      LOG(ERROR) << "Signature operation failed due to generic reason";
      return mojom::SignatureResult::kFailed;
    default:
      LOG(ERROR) << "Signature operation failed with error=" << error;
      return mojom::SignatureResult::kFailed;
  }
}

}  // namespace

SecurityTokenOperationBridge::SecurityTokenOperationBridge(
    content::BrowserContext* context,
    mojo::PendingReceiver<mojom::SecurityTokenOperation> receiver)
    : receiver_(this, std::move(receiver)),
      certificate_provider_service_(
          chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
              context)),
      weak_ptr_factory_(this) {
  VLOG(2) << "SecurityTokenOperationBridge::SecurityTokenOperationBridge";
  DCHECK(certificate_provider_service_);
}

SecurityTokenOperationBridge::SecurityTokenOperationBridge(
    chromeos::CertificateProviderService* certificate_provider_service)
    : receiver_(this),
      certificate_provider_service_(certificate_provider_service),
      weak_ptr_factory_(this) {
  VLOG(2) << "SecurityTokenOperationBridge::SecurityTokenOperationBridge";
  DCHECK(certificate_provider_service_);
}

SecurityTokenOperationBridge::~SecurityTokenOperationBridge() {
  VLOG(2) << "SecurityTokenOperationBridge::~SecurityTokenOperationBridge";
}

void SecurityTokenOperationBridge::SignDigest(
    const std::string& subject_public_key_info,
    mojom::Algorithm algorithm,
    mojom::Digest digest,
    const std::vector<uint8_t>& data,
    SignDigestCallback callback) {
  VLOG(2) << "SecurityTokenOperationBridge::SignDigest";
  base::Optional<uint16_t> crypto_algorithm =
      ConvertMojoAlgorithmToTls(algorithm, digest);
  if (!crypto_algorithm.has_value()) {
    LOG(ERROR) << "Unsupported security token signature algorithm: "
               << algorithm << " digest: " << digest;
    std::move(callback).Run(mojom::SignatureResult::kUnsupportedAlgorithm,
                            base::nullopt);
    return;
  }
  certificate_provider_service_->RequestSignatureBySpki(
      subject_public_key_info, crypto_algorithm.value(), data, base::nullopt,
      base::BindOnce(&SecurityTokenOperationBridge::OnSignCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SecurityTokenOperationBridge::OnSignCompleted(
    SignDigestCallback callback,
    net::Error error,
    const std::vector<uint8_t>& signature) {
  VLOG(2) << "SecurityTokenOperationBridge::OnSignCompleted";
  base::Optional<std::vector<uint8_t>> opt_signature;
  if (error == net::OK)
    opt_signature = signature;
  std::move(callback).Run(ConvertNetToMojomError(error),
                          std::move(opt_signature));
}

}  // namespace keymaster
}  // namespace arc

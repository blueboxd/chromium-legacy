// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_client_cert_usage_observer.h"

#include <cstdint>

#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace chromeos {

namespace {

CertificateProviderService* GetCertificateProviderService() {
  Profile* signin_profile = ProfileHelper::GetSigninProfile();
  return CertificateProviderServiceFactory::GetForBrowserContext(
      signin_profile);
}

bool ObtainSignatureAlgorithms(
    const net::X509Certificate& cert,
    std::vector<ChallengeResponseKey::SignatureAlgorithm>*
        signature_algorithms) {
  auto* certificate_provider_service = GetCertificateProviderService();
  base::StringPiece spki;
  if (!net::asn1::ExtractSPKIFromDERCert(
          net::x509_util::CryptoBufferAsStringPiece(cert.cert_buffer()),
          &spki)) {
    return false;
  }
  std::vector<uint16_t> ssl_algorithms;
  if (!certificate_provider_service->GetSupportedAlgorithmsBySpki(
          spki.as_string(), &ssl_algorithms)) {
    return false;
  }
  signature_algorithms->clear();
  for (auto ssl_algorithm : ssl_algorithms) {
    switch (ssl_algorithm) {
      case SSL_SIGN_RSA_PKCS1_SHA1:
        signature_algorithms->push_back(
            ChallengeResponseKey::SignatureAlgorithm::kRsassaPkcs1V15Sha1);
        break;
      case SSL_SIGN_RSA_PKCS1_SHA256:
        signature_algorithms->push_back(
            ChallengeResponseKey::SignatureAlgorithm::kRsassaPkcs1V15Sha256);
        break;
      case SSL_SIGN_RSA_PKCS1_SHA384:
        signature_algorithms->push_back(
            ChallengeResponseKey::SignatureAlgorithm::kRsassaPkcs1V15Sha384);
        break;
      case SSL_SIGN_RSA_PKCS1_SHA512:
        signature_algorithms->push_back(
            ChallengeResponseKey::SignatureAlgorithm::kRsassaPkcs1V15Sha512);
        break;
    }
  }
  return !signature_algorithms->empty();
}

}  // namespace

LoginClientCertUsageObserver::LoginClientCertUsageObserver() {
  GetCertificateProviderService()->AddObserver(this);
}

LoginClientCertUsageObserver::~LoginClientCertUsageObserver() {
  GetCertificateProviderService()->RemoveObserver(this);
}

bool LoginClientCertUsageObserver::ClientCertsWereUsed() const {
  return used_cert_count_ > 0;
}

bool LoginClientCertUsageObserver::GetOnlyUsedClientCert(
    scoped_refptr<net::X509Certificate>* cert,
    std::vector<ChallengeResponseKey::SignatureAlgorithm>* signature_algorithms)
    const {
  if (!used_cert_count_)
    return false;
  if (used_cert_count_ > 1) {
    LOG(ERROR)
        << "Failed to choose the client certificate for offline user "
           "authentication, since more than one client certificate was used";
    return false;
  }
  DCHECK(used_cert_);
  if (!ObtainSignatureAlgorithms(*used_cert_, signature_algorithms))
    return false;
  *cert = used_cert_;
  return true;
}

void LoginClientCertUsageObserver::OnSignCompleted(
    const scoped_refptr<net::X509Certificate>& certificate) {
  if (!used_cert_ || !used_cert_->EqualsExcludingChain(certificate.get()))
    ++used_cert_count_;
  used_cert_ = certificate;
}

}  // namespace chromeos

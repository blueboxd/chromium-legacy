// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/path_builder.h"

#include "net/base/net_errors.h"
#include "net/cert/internal/cert_issuer_source_static.h"
#include "net/cert/internal/common_cert_errors.h"
#include "net/cert/internal/crl.h"
#include "net/cert/internal/parse_certificate.h"
#include "net/cert/internal/parsed_certificate.h"
#include "net/cert/internal/simple_path_builder_delegate.h"
#include "net/cert/internal/trust_store_in_memory.h"
#include "net/cert/internal/verify_certificate_chain.h"
#include "net/der/encode_values.h"
#include "net/der/input.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

#include "net/cert/internal/nist_pkits_unittest.h"

namespace net {

namespace {

class CrlCheckingPathBuilderDelegate : public SimplePathBuilderDelegate {
 public:
  CrlCheckingPathBuilderDelegate(const std::vector<std::string>& der_crls,
                                 const base::Time& verify_time,
                                 const base::TimeDelta& max_age,
                                 size_t min_rsa_modulus_length_bits,
                                 DigestPolicy digest_policy)
      : SimplePathBuilderDelegate(min_rsa_modulus_length_bits, digest_policy),
        der_crls_(der_crls),
        verify_time_(verify_time),
        max_age_(max_age) {}

  void CheckPathAfterVerification(CertPathBuilderResultPath* path) override {
    SimplePathBuilderDelegate::CheckPathAfterVerification(path);

    if (!path->IsValid())
      return;

    // It would be preferable if this test could use
    // CheckValidatedChainRevocation somehow, but that only supports getting
    // CRLs by http distributionPoints. So this just settles for writing a
    // little bit of wrapper code to test CheckCRL directly.
    const ParsedCertificateList& certs = path->certs;
    for (size_t reverse_i = 0; reverse_i < certs.size(); ++reverse_i) {
      size_t i = certs.size() - reverse_i - 1;
      const ParsedCertificate* cert = certs[i].get();
      const ParsedCertificate* issuer_cert =
          i + 1 < certs.size() ? certs[i + 1].get() : nullptr;

      // Trust anchors bypass OCSP/CRL revocation checks. (The only way to
      // revoke trust anchors is via CRLSet or the built-in SPKI blacklist).
      if (reverse_i == 0 && path->last_cert_trust.IsTrustAnchor())
        continue;

      bool cert_good = false;

      CHECK(issuer_cert);
      CHECK(cert->normalized_issuer() == issuer_cert->normalized_subject());
      for (const auto& der_crl : der_crls_) {
        CRLRevocationStatus crl_status =
            CheckCRL(der_crl, cert, /*cert_dp=*/nullptr, issuer_cert,
                     verify_time_, max_age_);
        if (crl_status == CRLRevocationStatus::REVOKED) {
          path->errors.GetErrorsForCert(i)->AddError(
              cert_errors::kCertificateRevoked);
          return;
        }
        if (crl_status == CRLRevocationStatus::GOOD) {
          cert_good = true;
          break;
        }
      }
      if (!cert_good) {
        // PKITS tests assume hard-fail revocation checking.
        // From PKITS 4.4: "When running the tests in this section, the
        // application should be configured in such a way that the
        // certification path is not accepted unless valid, up-to-date
        // revocation data is available for every certificate in the path."
        path->errors.GetErrorsForCert(i)->AddError(
            cert_errors::kUnableToCheckRevocation);
      }
    }
  }

 private:
  std::vector<std::string> der_crls_;
  const base::Time verify_time_;
  const base::TimeDelta max_age_;
};

class PathBuilderPkitsTestDelegate {
 public:
  static void RunTest(std::vector<std::string> cert_ders,
                      std::vector<std::string> crl_ders,
                      const PkitsTestInfo& info) {
    ASSERT_FALSE(cert_ders.empty());
    ParsedCertificateList certs;
    for (const std::string& der : cert_ders) {
      CertErrors errors;
      ASSERT_TRUE(ParsedCertificate::CreateAndAddToVector(
          bssl::UniquePtr<CRYPTO_BUFFER>(
              CRYPTO_BUFFER_new(reinterpret_cast<const uint8_t*>(der.data()),
                                der.size(), nullptr)),
          {}, &certs, &errors))
          << errors.ToDebugString();
    }
    // First entry in the PKITS chain is the trust anchor.
    // TODO(mattm): test with all possible trust anchors in the trust store?
    TrustStoreInMemory trust_store;

    trust_store.AddTrustAnchor(certs[0]);

    // TODO(mattm): test with other irrelevant certs in cert_issuer_sources?
    CertIssuerSourceStatic cert_issuer_source;
    for (size_t i = 1; i < cert_ders.size() - 1; ++i)
      cert_issuer_source.AddCert(certs[i]);

    scoped_refptr<ParsedCertificate> target_cert(certs.back());

    base::StringPiece test_number = info.test_number;
    std::unique_ptr<CertPathBuilderDelegate> path_builder_delegate;
    if (test_number == "4.4.19" || test_number == "4.6.15" ||
        test_number == "4.6.17" || test_number == "4.9.6" ||
        test_number == "4.11.7" || test_number == "4.12.7" ||
        test_number == "4.12.9" || test_number == "4.13.19") {
      // TODO(https://crbug.com/749276): extend CRL support: These tests all
      // require better CRL issuer cert selection / discovery. Disable CRL
      // checking for them for now.
      path_builder_delegate = std::make_unique<SimplePathBuilderDelegate>(
          1024, SimplePathBuilderDelegate::DigestPolicy::kWeakAllowSha1);
    } else {
      base::Time verify_time;
      ASSERT_TRUE(der::GeneralizedTimeToTime(info.time, &verify_time));
      path_builder_delegate = std::make_unique<CrlCheckingPathBuilderDelegate>(
          crl_ders, verify_time, /*max_age=*/base::TimeDelta::FromDays(365 * 2),
          1024, SimplePathBuilderDelegate::DigestPolicy::kWeakAllowSha1);
    }

    CertPathBuilder::Result result;
    CertPathBuilder path_builder(
        std::move(target_cert), &trust_store, path_builder_delegate.get(),
        info.time, KeyPurpose::ANY_EKU, info.initial_explicit_policy,
        info.initial_policy_set, info.initial_policy_mapping_inhibit,
        info.initial_inhibit_any_policy, &result);
    path_builder.AddCertIssuerSource(&cert_issuer_source);

    path_builder.Run();

    if (info.should_validate != result.HasValidPath()) {
      for (size_t i = 0; i < result.paths.size(); ++i) {
        const net::CertPathBuilderResultPath* result_path =
            result.paths[i].get();
        LOG(ERROR) << "path " << i << " errors:\n"
                   << result_path->errors.ToDebugString(result_path->certs);
      }
    }

    ASSERT_EQ(info.should_validate, result.HasValidPath());

    if (result.HasValidPath()) {
      EXPECT_EQ(info.user_constrained_policy_set,
                result.GetBestValidPath()->user_constrained_policy_set);
    }
  }
};

}  // namespace

INSTANTIATE_TYPED_TEST_SUITE_P(PathBuilder,
                               PkitsTest01SignatureVerification,
                               PathBuilderPkitsTestDelegate);
INSTANTIATE_TYPED_TEST_SUITE_P(PathBuilder,
                               PkitsTest02ValidityPeriods,
                               PathBuilderPkitsTestDelegate);
INSTANTIATE_TYPED_TEST_SUITE_P(PathBuilder,
                               PkitsTest03VerifyingNameChaining,
                               PathBuilderPkitsTestDelegate);
INSTANTIATE_TYPED_TEST_SUITE_P(PathBuilder,
                               PkitsTest04BasicCertificateRevocationTests,
                               PathBuilderPkitsTestDelegate);
INSTANTIATE_TYPED_TEST_SUITE_P(PathBuilder,
                               PkitsTest06VerifyingBasicConstraints,
                               PathBuilderPkitsTestDelegate);
INSTANTIATE_TYPED_TEST_SUITE_P(PathBuilder,
                               PkitsTest07KeyUsage,
                               PathBuilderPkitsTestDelegate);
INSTANTIATE_TYPED_TEST_SUITE_P(PathBuilder,
                               PkitsTest08CertificatePolicies,
                               PathBuilderPkitsTestDelegate);
INSTANTIATE_TYPED_TEST_SUITE_P(PathBuilder,
                               PkitsTest09RequireExplicitPolicy,
                               PathBuilderPkitsTestDelegate);
INSTANTIATE_TYPED_TEST_SUITE_P(PathBuilder,
                               PkitsTest10PolicyMappings,
                               PathBuilderPkitsTestDelegate);
INSTANTIATE_TYPED_TEST_SUITE_P(PathBuilder,
                               PkitsTest11InhibitPolicyMapping,
                               PathBuilderPkitsTestDelegate);
INSTANTIATE_TYPED_TEST_SUITE_P(PathBuilder,
                               PkitsTest12InhibitAnyPolicy,
                               PathBuilderPkitsTestDelegate);
INSTANTIATE_TYPED_TEST_SUITE_P(PathBuilder,
                               PkitsTest13NameConstraints,
                               PathBuilderPkitsTestDelegate);
INSTANTIATE_TYPED_TEST_SUITE_P(PathBuilder,
                               PkitsTest16PrivateCertificateExtensions,
                               PathBuilderPkitsTestDelegate);

// TODO(https://crbug.com/749276): extend CRL support?:
// PkitsTest05VerifyingPathswithSelfIssuedCertificates: requires
// issuingDistributionPoint CRL extension handling and better CRL issuer cert
// selection.
// PkitsTest14DistributionPoints: indirect CRLs and reason codes are not
// supported.
// PkitsTest15DeltaCRLs: Delta CRLs are not supported.

}  // namespace net

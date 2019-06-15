// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/crl.h"

#include "base/stl_util.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/internal/revocation_util.h"
#include "net/cert/internal/signature_algorithm.h"
#include "net/cert/internal/verify_name_match.h"
#include "net/cert/internal/verify_signed_data.h"
#include "net/der/input.h"
#include "net/der/parse_values.h"
#include "net/der/parser.h"
#include "net/der/tag.h"

namespace net {

namespace {

WARN_UNUSED_RESULT bool NormalizeNameTLV(const der::Input& name_tlv,
                                         std::string* out_normalized_name) {
  der::Parser parser(name_tlv);
  der::Input name_rdn;
  net::CertErrors unused_errors;
  return parser.ReadTag(der::kSequence, &name_rdn) &&
         NormalizeName(name_rdn, out_normalized_name, &unused_errors) &&
         !parser.HasMore();
}

}  // namespace

bool ParseCrlCertificateList(const der::Input& crl_tlv,
                             der::Input* out_tbs_cert_list_tlv,
                             der::Input* out_signature_algorithm_tlv,
                             der::BitString* out_signature_value) {
  der::Parser parser(crl_tlv);

  //   CertificateList  ::=  SEQUENCE  {
  der::Parser certificate_list_parser;
  if (!parser.ReadSequence(&certificate_list_parser))
    return false;

  //        tbsCertList          TBSCertList
  if (!certificate_list_parser.ReadRawTLV(out_tbs_cert_list_tlv))
    return false;

  //        signatureAlgorithm   AlgorithmIdentifier,
  if (!certificate_list_parser.ReadRawTLV(out_signature_algorithm_tlv))
    return false;

  //        signatureValue       BIT STRING  }
  if (!certificate_list_parser.ReadBitString(out_signature_value))
    return false;

  // There isn't an extension point at the end of CertificateList.
  if (certificate_list_parser.HasMore())
    return false;

  // By definition the input was a single CertificateList, so there shouldn't be
  // unconsumed data.
  if (parser.HasMore())
    return false;

  return true;
}

bool ParseCrlTbsCertList(const der::Input& tbs_tlv, ParsedCrlTbsCertList* out) {
  der::Parser parser(tbs_tlv);

  //   TBSCertList  ::=  SEQUENCE  {
  der::Parser tbs_parser;
  if (!parser.ReadSequence(&tbs_parser))
    return false;

  //         version                 Version OPTIONAL,
  //                                      -- if present, MUST be v2
  base::Optional<der::Input> version_der;
  if (!tbs_parser.ReadOptionalTag(der::kInteger, &version_der))
    return false;
  if (version_der.has_value()) {
    uint64_t version64;
    if (!der::ParseUint64(*version_der, &version64))
      return false;
    // If version is present, it MUST be v2(1).
    if (version64 != 1)
      return false;
    out->version = CrlVersion::V2;
  } else {
    // Uh, RFC 5280 doesn't actually say it anywhere, but presumably if version
    // is not specified, it is V1.
    out->version = CrlVersion::V1;
  }

  //         signature               AlgorithmIdentifier,
  if (!tbs_parser.ReadRawTLV(&out->signature_algorithm_tlv))
    return false;

  //         issuer                  Name,
  if (!tbs_parser.ReadRawTLV(&out->issuer_tlv))
    return false;

  //         thisUpdate              Time,
  if (!ReadUTCOrGeneralizedTime(&tbs_parser, &out->this_update))
    return false;

  //         nextUpdate              Time OPTIONAL,
  der::Tag maybe_next_update_tag;
  der::Input unused_next_update_input;
  if (tbs_parser.PeekTagAndValue(&maybe_next_update_tag,
                                 &unused_next_update_input) &&
      (maybe_next_update_tag == der::kUtcTime ||
       maybe_next_update_tag == der::kGeneralizedTime)) {
    der::GeneralizedTime next_update_time;
    if (!ReadUTCOrGeneralizedTime(&tbs_parser, &next_update_time))
      return false;
    out->next_update = next_update_time;
  } else {
    out->next_update = base::nullopt;
  }

  //         revokedCertificates     SEQUENCE OF SEQUENCE  { ... } OPTIONAL,
  der::Input unused_revoked_certificates;
  der::Tag maybe_revoked_certifigates_tag;
  if (tbs_parser.PeekTagAndValue(&maybe_revoked_certifigates_tag,
                                 &unused_revoked_certificates) &&
      maybe_revoked_certifigates_tag == der::kSequence) {
    der::Input revoked_certificates_tlv;
    if (!tbs_parser.ReadRawTLV(&revoked_certificates_tlv))
      return false;
    out->revoked_certificates_tlv = revoked_certificates_tlv;
  } else {
    out->revoked_certificates_tlv = base::nullopt;
  }

  //         crlExtensions           [0]  EXPLICIT Extensions OPTIONAL
  //                                       -- if present, version MUST be v2
  if (!tbs_parser.ReadOptionalTag(der::ContextSpecificConstructed(0),
                                  &out->crl_extensions_tlv)) {
    return false;
  }
  if (out->crl_extensions_tlv.has_value()) {
    if (out->version != CrlVersion::V2)
      return false;
  }

  if (tbs_parser.HasMore()) {
    // Invalid or extraneous elements.
    return false;
  }

  // By definition the input was a single sequence, so there shouldn't be
  // unconsumed data.
  if (parser.HasMore())
    return false;

  return true;
}

CRLRevocationStatus GetCRLStatusForCert(
    const der::Input& cert_serial,
    CrlVersion crl_version,
    const base::Optional<der::Input>& revoked_certificates_tlv) {
  if (!revoked_certificates_tlv.has_value()) {
    // RFC 5280 Section 5.1.2.6: "When there are no revoked certificates, the
    // revoked certificates list MUST be absent."
    // No covered certificates are revoked, therefore the cert is good.
    return CRLRevocationStatus::GOOD;
  }

  der::Parser parser(*revoked_certificates_tlv);

  //         revokedCertificates     SEQUENCE OF SEQUENCE  {
  der::Parser revoked_certificates_parser;
  if (!parser.ReadSequence(&revoked_certificates_parser))
    return CRLRevocationStatus::UNKNOWN;

  // RFC 5280 Section 5.1.2.6: "When there are no revoked certificates, the
  // revoked certificates list MUST be absent."
  if (!revoked_certificates_parser.HasMore())
    return CRLRevocationStatus::UNKNOWN;

  // By definition the input was a single Extensions sequence, so there
  // shouldn't be unconsumed data.
  if (parser.HasMore())
    return CRLRevocationStatus::UNKNOWN;

  bool found_matching_serial = false;

  while (revoked_certificates_parser.HasMore()) {
    //         revokedCertificates     SEQUENCE OF SEQUENCE  {
    der::Parser crl_entry_parser;
    if (!revoked_certificates_parser.ReadSequence(&crl_entry_parser))
      return CRLRevocationStatus::UNKNOWN;

    der::Input revoked_cert_serial_number;
    //              userCertificate         CertificateSerialNumber,
    if (!crl_entry_parser.ReadTag(der::kInteger, &revoked_cert_serial_number))
      return CRLRevocationStatus::UNKNOWN;

    //              revocationDate          Time,
    der::GeneralizedTime unused_revocation_date;
    if (!ReadUTCOrGeneralizedTime(&crl_entry_parser, &unused_revocation_date))
      return CRLRevocationStatus::UNKNOWN;

    //              crlEntryExtensions      Extensions OPTIONAL
    if (crl_entry_parser.HasMore()) {
      //                                       -- if present, version MUST be v2
      if (crl_version != CrlVersion::V2)
        return CRLRevocationStatus::UNKNOWN;

      der::Input crl_entry_extensions_tlv;
      if (!crl_entry_parser.ReadRawTLV(&crl_entry_extensions_tlv))
        return CRLRevocationStatus::UNKNOWN;

      std::map<der::Input, ParsedExtension> extensions;
      if (!ParseExtensions(crl_entry_extensions_tlv, &extensions))
        return CRLRevocationStatus::UNKNOWN;

      // RFC 5280 Section 5.3: "If a CRL contains a critical CRL entry
      // extension that the application cannot process, then the application
      // MUST NOT use that CRL to determine the status of any certificates."
      for (const auto& ext : extensions) {
        if (ext.second.critical)
          return CRLRevocationStatus::UNKNOWN;
      }
    }

    if (crl_entry_parser.HasMore())
      return CRLRevocationStatus::UNKNOWN;

    if (revoked_cert_serial_number == cert_serial) {
      // Cert is revoked, but can't return yet since there might be critical
      // extensions on later entries that would prevent use of this CRL.
      found_matching_serial = true;
    }
  }

  if (found_matching_serial)
    return CRLRevocationStatus::REVOKED;

  // |cert| is not present in the revokedCertificates list.
  return CRLRevocationStatus::GOOD;
}

ParsedCrlTbsCertList::ParsedCrlTbsCertList() = default;
ParsedCrlTbsCertList::~ParsedCrlTbsCertList() = default;

CRLRevocationStatus CheckCRL(base::StringPiece raw_crl,
                             const ParsedCertificateList& valid_chain,
                             size_t target_cert_index,
                             const ParsedDistributionPoint* cert_dp,
                             const base::Time& verify_time,
                             const base::TimeDelta& max_age) {
  DCHECK_LT(target_cert_index, valid_chain.size());
  const ParsedCertificate* target_cert = valid_chain[target_cert_index].get();

  // 6.3.3 (a) Update the local CRL cache by obtaining a complete CRL, a
  //           delta CRL, or both, as required.
  //
  // This implementation only supports complete CRLs and takes the CRL as
  // input, it is up to the caller to provide an up to date CRL.

  der::Input tbs_cert_list_tlv;
  der::Input signature_algorithm_tlv;
  der::BitString signature_value;
  if (!ParseCrlCertificateList(der::Input(raw_crl), &tbs_cert_list_tlv,
                               &signature_algorithm_tlv, &signature_value)) {
    return CRLRevocationStatus::UNKNOWN;
  }

  ParsedCrlTbsCertList tbs_cert_list;
  if (!ParseCrlTbsCertList(tbs_cert_list_tlv, &tbs_cert_list))
    return CRLRevocationStatus::UNKNOWN;

  // 5.1.1.2  signatureAlgorithm
  //    This field MUST contain the same algorithm identifier as the
  //    signature field in the sequence tbsCertList (Section 5.1.2.2).
  if (!SignatureAlgorithm::IsEquivalent(
          signature_algorithm_tlv, tbs_cert_list.signature_algorithm_tlv)) {
    return CRLRevocationStatus::UNKNOWN;
  }
  // TODO(https://crbug.com/749276): Check the signature algorithm against
  // policy.
  std::unique_ptr<SignatureAlgorithm> signature_algorithm =
      SignatureAlgorithm::Create(signature_algorithm_tlv, /*errors=*/nullptr);
  if (!signature_algorithm)
    return CRLRevocationStatus::UNKNOWN;

  // Check CRL dates. Roughly corresponds to 6.3.3 (a) (1) but does not attempt
  // to update the CRL if it is out of date.
  if (!CheckRevocationDateValid(
          tbs_cert_list.this_update,
          base::OptionalOrNullptr(tbs_cert_list.next_update), verify_time,
          max_age)) {
    return CRLRevocationStatus::UNKNOWN;
  }

  // 6.3.3 (a) (2) is skipped: This implementation does not support delta CRLs.

  // 6.3.3 (b) Verify the issuer and scope of the complete CRL as follows:
  // 6.3.3 (b) (1) If the DP includes cRLIssuer, then verify that the issuer
  //               field in the complete CRL matches cRLIssuer in the DP and
  //               that the complete CRL contains an issuing distribution
  //               point extension with the indirectCRL boolean asserted.
  if (cert_dp && cert_dp->has_crl_issuer) {
    // Indirect CRLs are not supported.
    return CRLRevocationStatus::UNKNOWN;
  }
  // 6.3.3 (b) (1) Otherwise, verify that the CRL issuer matches the
  //               certificate issuer.
  //
  // Normalization for the name comparison is used although the RFC is not
  // clear on this. There are several places that explicitly are called out as
  // requiring identical encodings:
  //
  // 4.2.1.13.  CRL Distribution Points (cert extension) says the DP cRLIssuer
  //    field MUST be exactly the same as the encoding in issuer field of the
  //    CRL.
  //
  // 5.2.5.  Issuing Distribution Point (crl extension)
  //    The identical encoding MUST be used in the distributionPoint fields
  //    of the certificate and the CRL.
  //
  // 5.3.3.  Certificate Issuer (crl entry extension) also says "The encoding of
  //    the DN MUST be identical to the encoding used in the certificate"
  //
  // But 6.3.3 (b) (1) just says "matches". Also NIST PKITS includes at least
  // one test that requires normalization here.
  // TODO(https://crbug.com/749276): could do exact comparison first and only
  // fall back to normalizing if that fails.
  std::string normalized_crl_issuer;
  if (!NormalizeNameTLV(tbs_cert_list.issuer_tlv, &normalized_crl_issuer))
    return CRLRevocationStatus::UNKNOWN;
  if (der::Input(&normalized_crl_issuer) != target_cert->normalized_issuer())
    return CRLRevocationStatus::UNKNOWN;

  // 6.3.3 (b) (2) If the complete CRL includes an issuing distribution point
  //               (IDP) CRL extension ...
  // This implementation does not support CRL extensions, fail if any critical
  // CRL extensions are present.
  if (tbs_cert_list.crl_extensions_tlv.has_value()) {
    std::map<der::Input, ParsedExtension> extensions;
    if (!ParseExtensions(*tbs_cert_list.crl_extensions_tlv, &extensions))
      return CRLRevocationStatus::UNKNOWN;

    for (const auto& ext : extensions) {
      if (ext.second.critical)
        return CRLRevocationStatus::UNKNOWN;
    }
  }

  // 6.3.3 (c-e) skipped: delta CRLs and reason codes are not supported.

  // This implementation only supports direct CRLs where the CRL was signed by
  // one of the certs in its validated issuer chain. This allows handling some
  // cases of key rollover without requiring additional CRL issuer cert
  // discovery & path building.
  // TODO(https://crbug.com/749276): should this loop start at
  // |target_cert_index|? There doesn't seem to be anything in the specs that
  // precludes a CRL signed by a self-issued cert from covering itself. On the
  // other hand it seems like a pretty weird thing to allow and causes NIST
  // PKITS 4.5.3 to pass when it seems like it would not be intended to (since
  // issuingDistributionPoint CRL extension is not handled).
  for (size_t i = target_cert_index + 1; i < valid_chain.size(); ++i) {
    const ParsedCertificate* issuer_cert = valid_chain[i].get();

    // 6.3.3 (f) Obtain and validate the certification path for the issuer of
    //           the complete CRL.  The trust anchor for the certification
    //           path MUST be the same as the trust anchor used to validate
    //           the target certificate.
    //
    // As the |issuer_cert| is from the already validated chain, it is already
    // known to chain to the same trust anchor as the target certificate.
    if (der::Input(&normalized_crl_issuer) != issuer_cert->normalized_subject())
      continue;

    // 6.3.3 (f) If a key usage extension is present in the CRL issuer's
    //           certificate, verify that the cRLSign bit is set.
    if (issuer_cert->has_key_usage() &&
        !issuer_cert->key_usage().AssertsBit(KEY_USAGE_BIT_CRL_SIGN)) {
      continue;
    }

    // 6.3.3 (g) Validate the signature on the complete CRL using the public
    //           key validated in step (f).
    if (!VerifySignedData(*signature_algorithm, tbs_cert_list_tlv,
                          signature_value, issuer_cert->tbs().spki_tlv)) {
      continue;
    }

    // 6.3.3 (h,i) skipped. This implementation does not support delta CRLs.

    // 6.3.3 (j) If (cert_status is UNREVOKED), then search for the
    //           certificate on the complete CRL.  If an entry is found that
    //           matches the certificate issuer and serial number as described
    //           in Section 5.3.3, then set the cert_status variable to the
    //           indicated reason as described in step (i).
    //
    // CRL is valid and covers |target_cert|, check if |target_cert| is present
    // in the revokedCertificates sequence.
    return GetCRLStatusForCert(target_cert->tbs().serial_number,
                               tbs_cert_list.version,
                               tbs_cert_list.revoked_certificates_tlv);

    // 6.3.3 (k,l) skipped. This implementation does not support reason codes.
  }

  // Did not find the issuer & signer of |raw_crl| in |valid_chain|.
  return CRLRevocationStatus::UNKNOWN;
}

}  // namespace net

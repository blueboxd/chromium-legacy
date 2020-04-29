// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/test/trust_token_request_handler.h"

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/json/json_string_value_serializer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/bind_test_util.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "crypto/sha2.h"
#include "net/http/http_request_headers.h"
#include "net/http/structured_headers.h"
#include "services/network/trust_tokens/ed25519_trust_token_request_signer.h"
#include "services/network/trust_tokens/scoped_boringssl_bytes.h"
#include "services/network/trust_tokens/test/signed_request_verification_util.h"
#include "services/network/trust_tokens/trust_token_http_headers.h"
#include "services/network/trust_tokens/trust_token_request_canonicalizer.h"
#include "services/network/trust_tokens/trust_token_request_signing_helper.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/trust_token.h"

namespace network {
namespace test {
namespace {

struct IssuanceKeyPair {
  // Token signing and verification keys:
  std::vector<uint8_t> signing;
  std::vector<uint8_t> verification;

  // Default to a very long expiry time, but allow this to be overridden when
  // specific tests want to do so.
  base::Time expiry = base::Time::Max();
};

IssuanceKeyPair GenerateIssuanceKeyPair(int id) {
  IssuanceKeyPair keys;
  keys.signing.resize(TRUST_TOKEN_MAX_PRIVATE_KEY_SIZE);
  keys.verification.resize(TRUST_TOKEN_MAX_PUBLIC_KEY_SIZE);
  size_t signing_key_len, verification_key_len;
  TRUST_TOKEN_generate_key(keys.signing.data(), &signing_key_len,
                           keys.signing.size(), keys.verification.data(),
                           &verification_key_len, keys.verification.size(), id);
  keys.signing.resize(signing_key_len);
  keys.verification.resize(verification_key_len);

  return keys;
}

// This convenience helper prevents forgetting whether the inequality is weak or
// strict.
bool HasKeyPairExpired(const IssuanceKeyPair& p) {
  return p.expiry <= base::Time::Now();
}

}  // namespace

struct TrustTokenRequestHandler::Rep {
  // Issue at most this many tokens per issuance.
  int batch_size;

  // Signed redemption record (SRR) signing and verification keys:
  std::vector<uint8_t> srr_signing;
  std::vector<uint8_t> srr_verification;
  std::vector<IssuanceKeyPair> issuance_keys;

  // Creates a BoringSSL token issuer context suitable for issuance or
  // redemption, using only the unexpired key pairs from |issuance_keys|.
  bssl::UniquePtr<TRUST_TOKEN_ISSUER> CreateIssuerContextFromUnexpiredKeys()
      const;

  // Verifies the redemption request's client datal is a valid CBOR
  // encoding of a structure matching the format specified in the design doc.
  //
  // If this is the case, returns true and stores the contained
  // browser-generated public key hash in |hashes_of_redemption_bound_key_pairs|
  // for comparison against subsequent signed requests. Otherwise, returns false
  // and, if |error| is not null, sets |error| to a human-readable explanation
  // of why the input was not valid.
  bool ConfirmClientDataIntegrityAndStoreKeyHash(
      base::span<const uint8_t> client_data,
      std::string* error = nullptr);

  // Maintains all key pairs bound to successful redemptions.
  // TODO(davidvc): This can be expanded to map per top-frame origin for
  // tests across multiple origins.
  // TODO(davidvc): We can also expand the verification logic here to confirm
  // the private metadata field decodes appropriately.
  std::set<std::string> hashes_of_redemption_bound_key_pairs;

  // Contains a human-readable string explaining why the most recent signed
  // request verification to fail failed, or nullopt if no verification has
  // failed.
  base::Optional<std::string> last_verification_error;
};

bssl::UniquePtr<TRUST_TOKEN_ISSUER>
TrustTokenRequestHandler::Rep::CreateIssuerContextFromUnexpiredKeys() const {
  bssl::UniquePtr<TRUST_TOKEN_ISSUER> ret(TRUST_TOKEN_ISSUER_new(batch_size));
  if (!ret)
    return nullptr;

  for (const IssuanceKeyPair& key_pair : issuance_keys) {
    if (HasKeyPairExpired(key_pair))
      continue;

    if (!TRUST_TOKEN_ISSUER_add_key(ret.get(), key_pair.signing.data(),
                                    key_pair.signing.size())) {
      return nullptr;
    }
  }

  // Copying the comment from evp.h:
  // The [Ed25519] RFC 8032 private key format is the 32-byte prefix of
  // |ED25519_sign|'s 64-byte private key.
  bssl::UniquePtr<EVP_PKEY> issuer_srr_key(EVP_PKEY_new_raw_private_key(
      EVP_PKEY_ED25519, /*unused=*/nullptr, srr_signing.data(),
      /*len=*/32));

  if (!issuer_srr_key)
    return nullptr;

  if (!TRUST_TOKEN_ISSUER_set_srr_key(ret.get(), issuer_srr_key.get()))
    return nullptr;

  return ret;
}

bool TrustTokenRequestHandler::Rep::ConfirmClientDataIntegrityAndStoreKeyHash(
    base::span<const uint8_t> client_data,
    std::string* error) {
  std::string dummy_error;
  if (!error)
    error = &dummy_error;

  base::Optional<cbor::Value> maybe_value = cbor::Reader::Read(client_data);
  if (!maybe_value) {
    *error = "client data was invalid CBOR";
    return false;
  }

  if (!maybe_value->is_map()) {
    *error = "client data was valid CBOR but not a map";
    return false;
  }
  const cbor::Value::MapValue& map = maybe_value->GetMap();

  if (map.size() != 3u) {
    *error = "Unexpected number of fields in client data";
    return false;
  }

  auto it = map.find(cbor::Value("key-hash", cbor::Value::Type::STRING));
  if (it == map.end()) {
    *error = "client data was missing a 'key-hash' field";
    return false;
  }
  if (!it->second.is_bytestring()) {
    *error = "client data 'key-hash' field was not a bytestring";
    return false;
  }
  base::StringPiece key_hash = it->second.GetBytestringAsString();

  // Even though we don't yet examine the remaining fields in detail, perform
  // some structural integrity checks to make sure all's generally well:
  cbor::Value redeeming_origin_key("redeeming-origin",
                                   cbor::Value::Type::STRING);
  if (!map.contains(redeeming_origin_key) ||
      !map.at(redeeming_origin_key).is_string()) {
    *error = "Missing or type-unsafe redeeming-origin field in client data";
    return false;
  }
  cbor::Value redemption_timestamp_key("redemption-timestamp",
                                       cbor::Value::Type::STRING);
  if (!map.contains(redemption_timestamp_key) ||
      !map.at(redemption_timestamp_key).is_unsigned()) {
    *error = "Missing or type-unsafe redemption-timestamp field in client data";
    return false;
  }

  hashes_of_redemption_bound_key_pairs.insert(std::string(key_hash));

  return true;
}

TrustTokenRequestHandler::TrustTokenRequestHandler(int num_keys, int batch_size)
    : rep_(std::make_unique<Rep>()) {
  rep_->batch_size = batch_size;

  rep_->srr_signing.resize(ED25519_PRIVATE_KEY_LEN);
  rep_->srr_verification.resize(ED25519_PUBLIC_KEY_LEN);
  ED25519_keypair(rep_->srr_verification.data(), rep_->srr_signing.data());

  for (int i = 0; i < num_keys; ++i) {
    rep_->issuance_keys.push_back(GenerateIssuanceKeyPair(i));
  }
}

TrustTokenRequestHandler::~TrustTokenRequestHandler() = default;

std::string TrustTokenRequestHandler::GetKeyCommitmentRecord() const {
  base::AutoLock lock(mutex_);

  std::string ret;
  JSONStringValueSerializer serializer(&ret);

  base::Value value(base::Value::Type::DICTIONARY);
  value.SetStringKey(
      "srrkey", base::Base64Encode(base::make_span(rep_->srr_verification)));
  value.SetIntKey("batchsize", rep_->batch_size);

  for (size_t i = 0; i < rep_->issuance_keys.size(); ++i) {
    value.SetStringPath(base::NumberToString(i) + ".Y",
                        base::Base64Encode(base::make_span(
                            rep_->issuance_keys[i].verification)));
    value.SetStringPath(base::NumberToString(i) + ".expiry",
                        base::NumberToString((rep_->issuance_keys[i].expiry -
                                              base::Time::UnixEpoch())
                                                 .InMicroseconds()));
  }

  // It's OK to be a bit crashy in exceptional failure cases because it
  // indicates a serious coding error in this test-only code; we'd like to find
  // this out sooner rather than later.
  CHECK(serializer.Serialize(value));
  return ret;
}

base::Optional<std::string> TrustTokenRequestHandler::Issue(
    base::StringPiece issuance_request) {
  base::AutoLock lock(mutex_);

  bssl::UniquePtr<TRUST_TOKEN_ISSUER> issuer_ctx =
      rep_->CreateIssuerContextFromUnexpiredKeys();

  std::string decoded_issuance_request;
  if (!base::Base64Decode(issuance_request, &decoded_issuance_request))
    return base::nullopt;

  // TODO(davidvc): Perhaps make this configurable? Not a high priority, though.
  constexpr uint8_t kPrivateMetadata = 0;

  ScopedBoringsslBytes decoded_issuance_response;
  uint8_t num_tokens_issued = 0;
  bool ok = false;

  for (size_t i = 0; i < rep_->issuance_keys.size(); ++i) {
    if (HasKeyPairExpired(rep_->issuance_keys[i]))
      continue;

    if (TRUST_TOKEN_ISSUER_issue(
            issuer_ctx.get(), decoded_issuance_response.mutable_ptr(),
            decoded_issuance_response.mutable_len(), &num_tokens_issued,
            base::as_bytes(base::make_span(decoded_issuance_request)).data(),
            decoded_issuance_request.size(),
            /*public_metadata=*/static_cast<uint32_t>(i), kPrivateMetadata,
            rep_->batch_size)) {
      ok = true;
      break;
    }
  }

  if (!ok)
    return base::nullopt;

  return base::Base64Encode(decoded_issuance_response.as_span());
}

constexpr base::TimeDelta TrustTokenRequestHandler::kSrrLifetime =
    base::TimeDelta::FromDays(100);
base::Optional<std::string> TrustTokenRequestHandler::Redeem(
    base::StringPiece redemption_request) {
  base::AutoLock lock(mutex_);

  bssl::UniquePtr<TRUST_TOKEN_ISSUER> issuer_ctx =
      rep_->CreateIssuerContextFromUnexpiredKeys();

  std::string decoded_redemption_request;
  if (!base::Base64Decode(redemption_request, &decoded_redemption_request))
    return base::nullopt;

  ScopedBoringsslBytes decoded_redemption_response;
  TRUST_TOKEN* redeemed_token;
  ScopedBoringsslBytes redeemed_client_data;
  uint64_t received_redemption_timestamp;
  if (!TRUST_TOKEN_ISSUER_redeem(
          issuer_ctx.get(), decoded_redemption_response.mutable_ptr(),
          decoded_redemption_response.mutable_len(), &redeemed_token,
          redeemed_client_data.mutable_ptr(),
          redeemed_client_data.mutable_len(), &received_redemption_timestamp,
          base::as_bytes(base::make_span(decoded_redemption_request)).data(),
          decoded_redemption_request.size(), kSrrLifetime.InSeconds())) {
    return base::nullopt;
  }

  rep_->ConfirmClientDataIntegrityAndStoreKeyHash(
      redeemed_client_data.as_span());

  // Put the issuer-receied token in a smart pointer so it will get deleted on
  // leaving scope.
  bssl::UniquePtr<TRUST_TOKEN> redeemed_token_scoper(redeemed_token);

  return base::Base64Encode(decoded_redemption_response.as_span());
}

bool TrustTokenRequestHandler::VerifySignedRequest(
    const GURL& destination,
    const net::HttpRequestHeaders& headers,
    std::string* error_out) {
  std::string dummy_error;
  if (!error_out)
    error_out = &dummy_error;

  // In order to avoid deadlock, this must be before VerifySignedRequest's
  // |lock|'s definition. This is so that |set_last_error_on_return|'s
  // destructor (and associated callback) are run after the function-scoped
  // AutoLock is destroyed (and releases the mutex).
  base::ScopedClosureRunner set_last_error_on_return(
      base::BindLambdaForTesting([error_out, this]() {
        base::AutoLock lock(mutex_);
        if (!error_out->empty())
          rep_->last_verification_error = *error_out;
      }));

  base::AutoLock lock(mutex_);

  std::string verification_key;

  if (!ReconstructSigningDataAndVerifySignature(destination, headers,
                                                /*verifier=*/{}, error_out,
                                                &verification_key)) {
    return false;
  }

  if (!base::Contains(rep_->hashes_of_redemption_bound_key_pairs,
                      crypto::SHA256HashString(verification_key))) {
    if (error_out) {
      *error_out =
          "Got a request signed with a verification key whose hash was not "
          "previously bound to a redemption request.";
    }
    return false;
  }

  std::string sec_signed_redemption_record_header;
  if (!headers.GetHeader(kTrustTokensRequestHeaderSecSignedRedemptionRecord,
                         &sec_signed_redemption_record_header)) {
    if (error_out)
      *error_out = "Request missing its SRR header";

    return false;
  }

  std::string srr_body;
  switch (VerifyTrustTokenSignedRedemptionRecord(
      sec_signed_redemption_record_header,
      base::StringPiece(
          reinterpret_cast<const char*>(rep_->srr_verification.data()),
          rep_->srr_verification.size()),
      &srr_body)) {
    case SrrVerificationStatus::kSignatureVerificationError:
      if (error_out) {
        *error_out = "Request SRR signature failed to verify";
      }
      return false;
    case SrrVerificationStatus::kParseError:
      if (error_out) {
        *error_out = "Request SRR header failed to parse";
      }
      return false;
    case SrrVerificationStatus::kSuccess:
      break;
  }

  if (!ConfirmSrrBodyIntegrity(srr_body, error_out))
    return false;  // On failure, |ConfirmSrrBodyIntegrity| has set the error.

  return true;
}

base::Optional<std::string> TrustTokenRequestHandler::LastVerificationError() {
  base::AutoLock lock(mutex_);
  return rep_->last_verification_error;
}

}  // namespace test
}  // namespace network

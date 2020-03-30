// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_key_commitment_parser.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"

namespace network {

namespace {

// Parses a single key label. If |in| is the string representation of an integer
// in in the representable range of uint32_t, sets |*out| to that integer value
// and returns true. Otherwise, returns false.
bool ParseSingleKeyLabel(base::StringPiece in, uint32_t* out) {
  uint64_t key_label_in_uint64;
  if (!base::StringToUint64(in, &key_label_in_uint64))
    return false;
  if (!base::IsValueInRangeForNumericType<uint32_t>(key_label_in_uint64))
    return false;
  *out = base::checked_cast<uint32_t>(key_label_in_uint64);
  return true;
}

enum class ParseKeyResult {
  // Continue as if the key didn't exist.
  kIgnore,
  // Fail parsing totally.
  kFail,
  // Parsing the key succeeded.
  kSucceed
};

// Parses a single key, consisting of a body (the key material) and an expiry
// timestamp. Fails the parse if either field is missing or malformed. If the
// key has expired but is otherwise valid, ignores the key rather than failing
// the prase.
ParseKeyResult ParseSingleKeyExceptLabel(
    const base::Value& in,
    mojom::TrustTokenVerificationKey* out) {
  CHECK(in.is_dict());

  const std::string* expiry =
      in.FindStringKey(kTrustTokenKeyCommitmentExpiryField);
  const std::string* key_body =
      in.FindStringKey(kTrustTokenKeyCommitmentKeyField);
  if (!expiry || !key_body)
    return ParseKeyResult::kFail;

  uint64_t expiry_microseconds_since_unix_epoch;
  if (!base::StringToUint64(*expiry, &expiry_microseconds_since_unix_epoch))
    return ParseKeyResult::kFail;

  if (!base::Base64Decode(*key_body, &out->body))
    return ParseKeyResult::kFail;

  out->expiry =
      base::Time::UnixEpoch() +
      base::TimeDelta::FromMicroseconds(expiry_microseconds_since_unix_epoch);
  if (out->expiry <= base::Time::Now())
    return ParseKeyResult::kIgnore;

  return ParseKeyResult::kSucceed;
}

}  // namespace

const char kTrustTokenKeyCommitmentBatchsizeField[] = "batchsize";
const char kTrustTokenKeyCommitmentSrrkeyField[] = "srrkey";
const char kTrustTokenKeyCommitmentExpiryField[] = "expiry";
const char kTrustTokenKeyCommitmentKeyField[] = "Y";

// https://docs.google.com/document/d/1TNnya6B8pyomDK2F1R9CL3dY10OAmqWlnCxsWyOBDVQ/edit#bookmark=id.6wh9crbxdizi
// {
//   "batchsize" : ..., // Optional batch size; value of type int.
//   "srrkey" : ...,    // Required Signed Redemption Record (SRR)
//                      // verification key, in base64.
//
//   "1" : {            // Key label, a number in uint32_t range.
//     "Y" : ...,       // Required token issuance verification key, in
//                      // base64.
//     "expiry" : ...,  // Required token issuance key expiry time, in
//                      // microseconds since the Unix epoch.
//   },
//   "17" : {           // No guarantee that key labels (1, 17) are dense.
//     "Y" : ...,
//     "expiry" : ...,
//   }
// }
mojom::TrustTokenKeyCommitmentResultPtr TrustTokenKeyCommitmentParser::Parse(
    base::StringPiece response_body) {
  base::Optional<base::Value> maybe_value =
      base::JSONReader::Read(response_body);
  if (!maybe_value)
    return nullptr;

  if (!maybe_value->is_dict())
    return nullptr;

  auto result = mojom::TrustTokenKeyCommitmentResult::New();

  // Confirm that the batchsize field is type-safe, if it's present.
  if (maybe_value->FindKey(kTrustTokenKeyCommitmentBatchsizeField) &&
      !maybe_value->FindIntKey(kTrustTokenKeyCommitmentBatchsizeField)) {
    return nullptr;
  }
  if (base::Optional<int> maybe_batch_size =
          maybe_value->FindIntKey(kTrustTokenKeyCommitmentBatchsizeField)) {
    result->batch_size =
        mojom::TrustTokenKeyCommitmentBatchSize::New(*maybe_batch_size);
  }

  // Confirm that the srrkey field is present and base64-encoded.
  const std::string* maybe_srrkey =
      maybe_value->FindStringKey(kTrustTokenKeyCommitmentSrrkeyField);
  if (!maybe_srrkey)
    return nullptr;
  if (!base::Base64Decode(*maybe_srrkey,
                          &result->signed_redemption_record_verification_key)) {
    return nullptr;
  }

  // Parse the key commitments in the result (these are exactly the
  // key-value pairs in the dictionary with dictionary-typed values).
  for (const auto& kv : maybe_value->DictItems()) {
    const base::Value& item = kv.second;
    if (!item.is_dict())
      continue;

    auto key = mojom::TrustTokenVerificationKey::New();

    if (!ParseSingleKeyLabel(kv.first, &key->label))
      return nullptr;

    switch (ParseSingleKeyExceptLabel(item, key.get())) {
      case ParseKeyResult::kFail:
        return nullptr;
      case ParseKeyResult::kIgnore:
        continue;
      case ParseKeyResult::kSucceed:
        result->keys.push_back(std::move(key));
    }
  }

  return result;
}

}  // namespace network

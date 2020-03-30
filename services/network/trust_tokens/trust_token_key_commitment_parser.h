// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_KEY_COMMITMENT_PARSER_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_KEY_COMMITMENT_PARSER_H_

#include <memory>

#include "base/strings/string_piece_forward.h"
#include "services/network/public/mojom/trust_tokens.mojom-forward.h"
#include "services/network/trust_tokens/trust_token_key_commitment_controller.h"

namespace network {

// Field names from the key commitment JSON format specified in the Trust Tokens
// design doc
// (https://docs.google.com/document/d/1TNnya6B8pyomDK2F1R9CL3dY10OAmqWlnCxsWyOBDVQ/edit#bookmark=id.6wh9crbxdizi):
// - "batch size" (number of blinded tokens to provide per issuance request)
extern const char kTrustTokenKeyCommitmentBatchsizeField[];
// - verification key for the signatures the issuer provides over its Signed
// Redemption Records (SRRs)
extern const char kTrustTokenKeyCommitmentSrrkeyField[];
// - each issuance key's expiry timestamp
extern const char kTrustTokenKeyCommitmentExpiryField[];
// - each issuance key's key material
extern const char kTrustTokenKeyCommitmentKeyField[];

class TrustTokenKeyCommitmentParser
    : public TrustTokenKeyCommitmentController::Parser {
 public:
  TrustTokenKeyCommitmentParser() = default;
  ~TrustTokenKeyCommitmentParser() override = default;

  // Parses a JSON key commitment response.
  //
  // This method returns nullptr unless:
  // - the input is valid JSON; and
  // - the JSON represents a nonempty dictionary; and
  // - within this inner dictionary (which stores metadata like batch size, as
  // well as more dictionaries denoting keys' information):
  //   - every dictionary-type value has an expiry field
  //   (|kTrustTokenKeyCommitmentExpiryField| above) and a key body field
  //   (|kTrustTokenKeyCommitmentKeyField|), and
  //   - the expiry field is a positive integer (microseconds since the Unix
  //   epoch) storing a time in the future.
  mojom::TrustTokenKeyCommitmentResultPtr Parse(
      base::StringPiece response_body) override;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_KEY_COMMITMENT_PARSER_H_

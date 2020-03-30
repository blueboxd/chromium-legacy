// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_REQUEST_REDEMPTION_HELPER_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_REQUEST_REDEMPTION_HELPER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string_piece_forward.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/proto/public.pb.h"
#include "services/network/trust_tokens/trust_token_key_commitment_getter.h"
#include "services/network/trust_tokens/trust_token_request_helper.h"
#include "url/origin.h"

namespace net {
class URLRequest;
}  // namespace net

namespace network {
class TrustTokenStore;

namespace mojom {
class URLResponseHead;
}  // namespace mojom

// Class TrustTokenRequestRedemptionHelper performs a single trust token
// redemption operation (https://github.com/wicg/trust-token-api): it attaches a
// single signed, unblinded token to an outgoing request, hands it to the
// token's issuer, and expects a signed redemption record (SRR) in response. The
// normal case involves a total of two network requests: one to get an
// up-to-date view of a key set the issuer provides for verifying that it's safe
// to perform the redemption, and another to send the token to the issuer.
class TrustTokenRequestRedemptionHelper : public TrustTokenRequestHelper {
 public:
  // Class KeyPairGenerator generates a signing and verification key pair.
  // These are not used for any cryptographic operations during redemption
  // itself. Instead, a digest of the verification key goes into the redemption
  // request and, on redemption success, we store the key pair alongside the
  // Signed Redemption Record obtained from the server; the key pair can
  // subsequently be used to sign outgoing requests as part of the Trust Tokens
  // "request signing" operation.
  class KeyPairGenerator {
   public:
    virtual ~KeyPairGenerator() = default;
    // Generates a key pair, returning true on success and false on failure (for
    // instance, the underlying cryptographic code could fail unexpectedly).
    virtual bool Generate(std::string* signing_key_out,
                          std::string* verification_key_out) = 0;
  };

  // Class Cryptographer executes the underlying cryptographic operations
  // required for redemption. The API is intended to correspond closely to the
  // BoringSSL API.
  class Cryptographer {
   public:
    virtual ~Cryptographer() = default;

    // Given a trust token to redeem and parameters to encode in the redemption
    // request, returns an ASCII string suitable for attachment in the
    // Sec-Trust-Token header, or nullopt on error.
    virtual base::Optional<std::string> BeginRedemption(
        TrustToken token,
        base::StringPiece verification_key,
        const url::Origin& top_level_origin) = 0;

    // Given a base64-encoded redemption response header, validates and extracts
    // the signed redemption record (SRR) contained in the header. If
    // successful, returns the SRR. Otherwise, returns nullptr.
    virtual base::Optional<std::string> ConfirmRedemption(
        base::StringPiece response_header) = 0;
  };

  // Creates a new redemption helper.
  //
  // - |top_level_origin| is the top-level origin
  // of the request subsequently passed to Begin; |top_level_origin|'s scheme
  // must be both (1) HTTP or HTTPS and (2) "potentially trustworthy". This
  // precondition is slightly involved because there are two needs:
  //   1. HTTP or HTTPS so that the scheme serializes in a sensible manner in
  //   order to serve as a key for persisting state.
  //   2. potentially trustworthy origin to satisfy Web security requirements.
  //
  // - |refresh_policy| controls whether to attempt to overwrite the cached
  // SRR stored for the request's (issuer, top-level) origin pair. This is
  // permitted to have value |kRefresh| only when the redemption
  // request's initiator equals its issuer origin.
  //
  // - |token_store| will be responsible for storing underlying Trust Tokens
  // state. It must outlive this object.
  //
  // - |key_commitment_getter|, |key_pair_generator|, and
  // |cryptographer| are delegates that help execute the protocol; see
  // their class comments.
  TrustTokenRequestRedemptionHelper(
      const url::Origin& top_level_origin,
      mojom::TrustTokenRefreshPolicy refresh_policy,
      TrustTokenStore* token_store,
      std::unique_ptr<TrustTokenKeyCommitmentGetter> key_commitment_getter,
      std::unique_ptr<KeyPairGenerator> key_pair_generator,
      std::unique_ptr<Cryptographer> cryptographer);
  ~TrustTokenRequestRedemptionHelper() override;

  // Executes the outbound part of a Trust Tokens redemption operation,
  // interpreting |request|'s URL's origin as the token issuance origin;
  // 1. Checks preconditions (see "Returns" below); if unsuccessful, fails.
  // 2. Executes a Trust Tokens key commitment request against the issuer; if
  //    unsuccessful, fails.
  // 3. In a request header, adds a signed, unblinded token along with
  //    associated metadata provided by |cryptographer_|.
  //
  // Returns:
  // * kOk on success
  // * kResourceExhausted if the top-level origin provided to this
  //   object's constructor has already reached its number-of-issuers limit,
  //   or if the (issuer, top-level) pair has no tokens to redeem
  // * kAlreadyExists if the (issuer, top-level) pair already has a current
  //   SRR and this helper was not parameterized with |kRefresh|.
  // * kFailedPrecondition if preconditions fail, including receiving a
  //   malformed or otherwise invalid key commitment record from the issuer,
  //   or if |kRefresh| was provided and the request was not initiated
  //   from an issuer context.
  //
  // |request|'s initiator, and its destination URL's origin, must be both (1)
  // HTTP or HTTPS and (2) "potentially trustworthy" in the sense of
  // network::IsOriginPotentiallyTrustworthy. (See the justification in the
  // constructor's comment.)
  void Begin(
      net::URLRequest* request,
      base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done) override;

  // Performs the second half of Trust Token issuance's client side:
  // 1. Checks |response| for an issuance response header.
  // 2. If the header is present, strips it from the response and passes its
  // value to an underlying cryptographic library, which parses and validates
  // the response and splits it into a number of signed, unblinded tokens.
  //
  // If both of these steps are successful, stores the tokens in |token_store_|
  // and returns kOk. Otherwise, returns kBadResponse.
  mojom::TrustTokenOperationStatus Finalize(
      mojom::URLResponseHead* response) override;

 private:
  // Continuation of |Begin| after asynchronous key commitment fetching
  // concludes.
  void OnGotKeyCommitment(
      net::URLRequest* request,
      base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done,
      mojom::TrustTokenKeyCommitmentResultPtr commitment_result);

  // Helper method: searches |token_store_| for a single trust token and returns
  // it, returning nullopt if the store contains no tokens for |issuer_|.
  //
  // Warning: This does NOT remove the token from the store.
  base::Optional<TrustToken> RetrieveSingleToken();

  // |issuer_|, |top_level_origin_|, and |refresh_policy_| are parameters
  // determining the scope and control flow of the redemption operation.
  url::Origin issuer_;
  const url::Origin top_level_origin_;
  const mojom::TrustTokenRefreshPolicy refresh_policy_;

  // |signing_key_| and |verification_key_| are generated speculatively near the
  // beginning of redemption and committed to storage if the operation succeeds.
  std::string signing_key_;
  std::string verification_key_;

  TrustTokenStore* const token_store_;
  const std::unique_ptr<TrustTokenKeyCommitmentGetter> key_commitment_getter_;
  const std::unique_ptr<KeyPairGenerator> key_pair_generator_;
  const std::unique_ptr<Cryptographer> cryptographer_;

  base::WeakPtrFactory<TrustTokenRequestRedemptionHelper> weak_factory_{this};
};
}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_REQUEST_REDEMPTION_HELPER_H_

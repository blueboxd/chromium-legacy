// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_REQUEST_SIGNING_HELPER_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_REQUEST_SIGNING_HELPER_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/optional.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "services/network/trust_tokens/trust_token_request_helper.h"
#include "url/origin.h"

namespace network {
class TrustTokenRequestCanonicalizer;

namespace internal {

// Given a string representation of a Trust Tokens Signed-Headers header,
// returns the list of header names given in the header, or nullopt on parsing
// error.
base::Optional<std::vector<std::string>> ParseTrustTokenSignedHeadersHeader(
    base::StringPiece header);

}  // namespace internal

class TrustTokenStore;
class SignedTrustTokenRedemptionRecord;

// Class TrustTokenRequestSigningHelper executes a single trust token signing
// operation (https://github.com/wicg/trust-token-api): it searches storage for
// a Signed Redemption Record (SRR), attaches the SRR to the request, and,
// depending on how the operation is parameterized, potentially also computes
// and attaches a signature over the SRR, a canonical representation of some
// of the request's data (for instance, a collection of the request's headers),
// and some additional metadata.
// To compute this signature, it uses a signing key associated with the SRR
// and generated during the previous Trust Tokens redemption operation that
// yielded the SRR.
class TrustTokenRequestSigningHelper : public TrustTokenRequestHelper {
 public:
  // The list of headers that callers are allowed to specify
  // for signing. This allowlist exists in part because some headers are added
  // much later in request construction. For the Trust Tokens MVP ("v0"), this
  // is limited to the signed redemption record and added timestamp
  // (see Params::should_add_timestamp) headers.
  static const char* const kSignableRequestHeaders[];

  // These are magic strings used in request signing. The canonicalized request
  // data keys are used when constructing a CBOR dictionary; they are the keys
  // to the values of request URL, POST body, and signing public key
  // (if any).
  static constexpr char kCanonicalizedRequestDataUrlKey[] = "url";
  static constexpr char kCanonicalizedRequestDataPublicKeyKey[] = "public-key";

  // |kRequestSigningDomainSeparator| is a static (fixed major per protocol
  // version) string included in the signing data immediately prior to the
  // request's canonical representation. This allows rendering otherwise valid
  // signatures forwards-incompatible, which is useful in case the signing
  // data's semantics change across protocol versions but its syntax does not.
  static constexpr uint8_t kRequestSigningDomainSeparator[] = {
      'T', 'r', 'u', 's', 't', ' ', 'T', 'o', 'k', 'e', 'n', ' ', 'v', '0'};

  struct Params {
    Params();
    ~Params();

    Params(const Params&);
    Params& operator=(const Params&);

    // |issuer| is the Trust Tokens issuer origin for which to retrieve a Signed
    // Redemption Record and matching signing key. This must be both (1) HTTP or
    // HTTPS and (2) "potentially trustworthy". This precondition is slightly
    // involved because there are two needs:
    //   1. HTTP or HTTPS so that the scheme serializes in a sensible manner in
    //   order to serve as a key for persisting state.
    //   2. potentially trustworthy origin to satisfy Web security requirements.
    url::Origin issuer;

    // |toplevel| is the top-level origin of the initiating request. This must
    // satisfy the same preconditions as |issuer|.
    url::Origin toplevel;

    // |additional_headers_to_sign| is a list of headers to sign, in addition to
    // those specified by the request's Signed-Headers header. If these are not
    // case-insensitive versions of headers in the |kSignableRequestHeaders|
    // allowlist, signing will fail with error kInvalidArgument.
    std::vector<std::string> additional_headers_to_sign;

    // If |should_add_timestamp| is true, successful signing operations will add
    // a Sec-Time header to the request bearing a current timestamp. "Sec-Time"
    // may be specified in kRequest
    bool should_add_timestamp;

    // If |sign_request_data| is kInclude, the request's URL will be
    // included in the canonical request data used for signing. If it is
    // kHeadersOnly, the request's headers will be the only request data used.
    // If it is kOmit, no signature will be attached.
    mojom::TrustTokenSignRequestData sign_request_data;
  };

  // Class Signer is responsible for the actual generation of signatures over
  // request data.
  class Signer {
   public:
    virtual ~Signer() = default;

    // Initializes signer state with the given key. Must be called at least once
    // before the first call to |Sign|.
    virtual void Init(base::span<const uint8_t> key) = 0;

    // Returns a one-shot signature over the given data, or an error. |Init|
    // must have been called before the first call to |Sign|.
    virtual base::Optional<std::vector<uint8_t>> Sign(
        base::span<const uint8_t> data) = 0;

    // Verifies the given signature. Does not depend on the current state of the
    // signer (in particular, |Init| need not have been called).
    virtual bool Verify(base::span<const uint8_t> data,
                        base::span<const uint8_t> signature,
                        base::span<const uint8_t> verification_key) = 0;
  };

  // Creates a request signing helper with behavior determined by |params|,
  // relying on |token_store| to provide protocol state; |canonicalizer| to
  // generate the request's canonical request data; and |signer| to generate a
  // signature over the request's signing data once it has been constructed from
  // the canonical request data.
  //
  // |token_store| must outlive this object.
  TrustTokenRequestSigningHelper(
      TrustTokenStore* token_store,
      Params params,
      std::unique_ptr<Signer> signer,
      std::unique_ptr<TrustTokenRequestCanonicalizer> canonicalizer);

  ~TrustTokenRequestSigningHelper() override;

  TrustTokenRequestSigningHelper(const TrustTokenRequestSigningHelper&) =
      delete;
  TrustTokenRequestSigningHelper& operator=(
      const TrustTokenRequestSigningHelper&) = delete;

  // Attempts to attach a Signed Redemption Record (SRR) corresponding
  // to |request|'s initiating top-level origin and the provided issuer origin.
  //
  // PRECONDITIONS:
  // (0. |request|'s destination's origin and its initiator must satisfy the
  // same conditions as the issuer origin in |params_|. This is DCHECKed, since
  // it is not a protocol-level precondition.)
  //
  // 1. If the request already contains a Sec-Signed-Redemption-Record,
  // Sec-Time, or Sec-Signature header, returns kInvalidArgument without
  // touching the request.
  // 2. If the caller specified headers for signing other than those in
  // kSignableRequestHeaders (or if the request has a malformed or otherwise
  // invalid signed issuers list in its Signed-Headers header), returns
  // kInvalidArgument and attaches an empty Sec-Signed-Redemption-Record header
  // to the request.
  // 3. If |token_store_| contains no SRR for this issuer-toplevel pair,
  // returns kResourceExhausted and attaches an empty
  // Sec-Signed-Redemption-Record header.
  //
  // ATTACHING THE REDEMPTION RECORD:
  // In the case that an SRR is found and the requested headers to sign are
  // well-formed, attaches a Sec-Signed-Redemption-Record header
  // bearing the SRR and:
  // 1. if the request is configured for adding a Trust Tokens timestamp,
  // adds a timestamp header;
  // 2. if the request is configured for signing, computes the request's
  // canonical request data and adds a signature header, following the algorithm
  // in the explainer:
  // https://github.com/WICG/trust-token-api#extension-trust-bound-keypair-and-request-signing
  //
  // RETURNS:
  // - On success, returns kOk.
  // - On internal error during signing, returns kInternalError and attaches an
  // empty SRR header, no signature header, and no timestamp header.
  // - On precondition failure, returns an error code and possibly attaches an
  // empty SRR header; see PRECONDITIONS section above.
  void Begin(
      net::URLRequest* request,
      base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done) override;

  // Immediately returns kOk with no other effect. (Signing is an operation that
  // only needs to process requests, not their corresponding responses.)
  mojom::TrustTokenOperationStatus Finalize(
      mojom::URLResponseHead* response) override;

 private:
  // Given (unencoded) bytestrings |public_key| and |signature|, returns the
  // Trust Tokens signature header, a serialized Structured Headers Draft 13
  // dictionary looking roughly like (order not guaranteed):
  //   public-key=<pk>,
  //   sig=<signature>,
  //   sign-request-data=include | headers-only
  //
  // Returns nullopt on serialization error.
  base::Optional<std::string> BuildSignatureHeader(base::StringPiece public_key,
                                                   base::StringPiece signature);

  // Returns a signature over |request|'s pertinent data (public key,
  // user-specified headers and, possibly, destination URL), or nullopt in case
  // of internal error.
  base::Optional<std::vector<uint8_t>> GetSignature(
      net::URLRequest* request,
      const SignedTrustTokenRedemptionRecord& record,
      const std::vector<std::string>& headers_to_sign);

  TrustTokenStore* token_store_;

  // Temporary representation of the signing-related Fetch parameters until
  // they're implemented.
  // TODO(crbug.com/1043118): When integrating this with URLLoader/the signing
  // helper factory, update Params's fields, or perhaps remove the struct.
  Params params_;

  std::unique_ptr<Signer> signer_;
  std::unique_ptr<TrustTokenRequestCanonicalizer> canonicalizer_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_REQUEST_SIGNING_HELPER_H_

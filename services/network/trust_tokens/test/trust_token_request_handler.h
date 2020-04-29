// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TEST_TRUST_TOKEN_REQUEST_HANDLER_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TEST_TRUST_TOKEN_REQUEST_HANDLER_H_

#include <string>

#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"

namespace network {
namespace test {

// TrustTokenRequestHandler encapsulates server-side Trust Tokens issuance and
// redemption logic and implements some integrity and correctness checks for
// requests subsequently signed with keys bound to token redemptions.
//
// It's thread-safe so that the methods can be called by test code directly and
// by net::EmbeddedTestServer handlers.
class TrustTokenRequestHandler {
 public:
  // Initializes server-side Trust Tokens logic by generating |num_keys| many
  // issuance key pairs and a Signed Redemption Record (SRR)
  // signing-and-verification key pair.
  //
  // If |batch_size| is provided, the issuer will be willing to issue at most
  // that many tokens per issuance operation.
  static constexpr int kDefaultIssuerBatchSize = 10;
  explicit TrustTokenRequestHandler(int num_keys,
                                    int batch_size = kDefaultIssuerBatchSize);

  ~TrustTokenRequestHandler();

  // TODO(davidvc): Provide a way to specify when keys expire.

  // Returns a key commitment record suitable for inserting into a {issuer:
  // commitment} dictionary passed to the network service via
  // NetworkService::SetTrustTokenKeyCommitments. This comprises |num_keys|
  // token verification keys and a batch size of |batch_size| (or none if
  // |batch_size| is nullopt).
  std::string GetKeyCommitmentRecord() const;

  // Given a base64-encoded issuance request, processes the
  // request and returns either nullopt (on error) or a base64-encoded response.
  base::Optional<std::string> Issue(base::StringPiece issuance_request);

  // Given a base64-encoded redemption request, processes the
  // request and returns either nullopt (on error) or a base64-encoded response.
  // On success, the response's signed redemption record will have a lifetime of
  // |kSrrLifetime|. We use a ludicrously long lifetime because there's no way
  // to mock time in browser tests, and we don't want the SRR expiring
  // unexpectedly.
  //
  // TODO(davidvc): This needs to be expanded to be able to provide
  // SRRs that have already expired. (This seems like the easiest way of
  // exercising client-side SRR expiry logic in end-to-end tests, because
  // there's no way to fast-forward a clock past an expiry time.)
  static const base::TimeDelta kSrrLifetime;
  base::Optional<std::string> Redeem(base::StringPiece redemption_request);

  // Inspects |request| and returns true exactly when:
  // - the request bears a well-formed Sec-Signature header with a valid
  // signature over the request's canonical signing data;
  // - the signature's public key's hash was bound to a previous redemption
  // request; and
  // - the request contains a well-formed signed redemption record whose
  // signature verifies against the issuer's published SRR key.
  //
  // Otherwise, returns false and, if |error_out| is not null, sets |error_out|
  // to a helpful error message.
  //
  // TODO(davidvc): This currently doesn't support signRequestData: 'omit'.
  bool VerifySignedRequest(const GURL& destination,
                           const net::HttpRequestHeaders& headers,
                           std::string* error_out = nullptr);

  // Returns the verification error from the most recent unsuccessful
  // VerifySignedRequest call, if any.
  base::Optional<std::string> LastVerificationError();

 private:
  struct Rep;  // Contains state internal to this class's implementation.

  // Guards this class's internal state.
  mutable base::Lock mutex_;
  std::unique_ptr<Rep> rep_ GUARDED_BY(mutex_);
};

}  // namespace test
}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TEST_TRUST_TOKEN_REQUEST_HANDLER_H_

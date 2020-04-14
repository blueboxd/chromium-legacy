// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/trust_token_to_mojom.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"

namespace blink {

bool ConvertTrustTokenToMojom(const TrustToken& in,
                              ExceptionState* exception_state,
                              network::mojom::blink::TrustTokenParams* out) {
  DCHECK(in.hasType());  // field is required in IDL
  if (in.type() == "token-request") {
    out->type = network::mojom::blink::TrustTokenOperationType::kIssuance;
    return true;
  }

  if (in.type() == "srr-token-redemption") {
    out->type = network::mojom::blink::TrustTokenOperationType::kRedemption;

    DCHECK(in.hasRefreshPolicy());  // default is defined
    out->include_timestamp_header = in.includeTimestampHeader();

    if (in.refreshPolicy() == "none") {
      out->refresh_policy =
          network::mojom::blink::TrustTokenRefreshPolicy::kUseCached;
    } else if (in.refreshPolicy() == "refresh") {
      out->refresh_policy =
          network::mojom::blink::TrustTokenRefreshPolicy::kRefresh;
    }
    return true;
  }

  DCHECK_EQ(in.type(), "send-srr");  // final possible value of the input enum
  out->type = network::mojom::blink::TrustTokenOperationType::kSigning;

  if (in.hasSignRequestData()) {
    if (in.signRequestData() == "omit") {
      out->sign_request_data =
          network::mojom::blink::TrustTokenSignRequestData::kOmit;
    } else if (in.signRequestData() == "include") {
      out->sign_request_data =
          network::mojom::blink::TrustTokenSignRequestData::kInclude;
    } else if (in.signRequestData() == "headers-only") {
      out->sign_request_data =
          network::mojom::blink::TrustTokenSignRequestData::kHeadersOnly;
    }
  }

  if (in.hasAdditionalSignedHeaders()) {
    out->additional_signed_headers = in.additionalSignedHeaders();
  }

  DCHECK(in.hasIncludeTimestampHeader());  // default is defined
  out->include_timestamp_header = in.includeTimestampHeader();

  if (in.hasIssuer()) {
    // Two conditions on the issuer:
    // 1. HTTP or HTTPS (because much Trust Tokens protocol state is
    // stored keyed by issuer origin, requiring HTTP or HTTPS is a way to ensure
    // these origins serialize to unique values);
    // 2. potentially trustworthy (a security requirement).
    KURL parsed_url = KURL(in.issuer());
    if (!parsed_url.ProtocolIsInHTTPFamily()) {
      exception_state->ThrowTypeError(
          "trustToken: operation type 'send-srr' requires that the 'issuer' "
          "field parse to a HTTP(S) origin, but it did not: " +
          in.issuer());
      return false;
    }
    out->issuer = blink::SecurityOrigin::Create(parsed_url);
    if (!out->issuer->IsPotentiallyTrustworthy()) {
      exception_state->ThrowTypeError(
          "trustToken: operation type 'send-srr' requires that the 'issuer' "
          "field parse to a secure origin, but it did not: " +
          in.issuer());
      return false;
    }
  } else {
    exception_state->ThrowTypeError(
        "trustToken: operation type 'send-srr' requires that the 'issuer' "
        "field "
        "be present and parse to a secure HTTP(S) URL, but it was missing.");
    return false;
  }

  return true;
}

DOMException* TrustTokenErrorToDOMException(
    network::mojom::blink::TrustTokenOperationStatus error) {
  // This should only be called on failure.
  DCHECK_NE(error, network::mojom::blink::TrustTokenOperationStatus::kOk);

  switch (error) {
    case network::mojom::blink::TrustTokenOperationStatus::kAlreadyExists:
      return DOMException::Create(
          "Redemption operation aborted due to Signed Redemption Record "
          "cache hit",
          DOMException::GetErrorName(
              DOMExceptionCode::kNoModificationAllowedError));
    case network::mojom::blink::TrustTokenOperationStatus::kFailedPrecondition:
      return DOMException::Create(
          "Precondition failed during Trust Tokens operation",
          DOMException::GetErrorName(DOMExceptionCode::kInvalidStateError));
    default:
      return DOMException::Create(
          "Error executing Trust Tokens operation",
          DOMException::GetErrorName(DOMExceptionCode::kOperationError));
  }
}

}  // namespace blink

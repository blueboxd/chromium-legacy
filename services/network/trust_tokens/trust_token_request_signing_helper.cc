// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_request_signing_helper.h"

#include <iterator>
#include <memory>
#include <string>

#include "base/base64.h"
#include "base/containers/flat_set.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time_to_iso8601.h"
#include "base/values.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "net/http/structured_headers.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "services/network/trust_tokens/proto/public.pb.h"
#include "services/network/trust_tokens/trust_token_http_headers.h"
#include "services/network/trust_tokens/trust_token_request_canonicalizer.h"
#include "services/network/trust_tokens/trust_token_store.h"
#include "url/url_constants.h"

namespace network {

namespace {

void LogOutcome(const net::NetLogWithSource& log, base::StringPiece outcome) {
  log.EndEvent(net::NetLogEventType::TRUST_TOKEN_OPERATION_BEGIN_SIGNING,
               [outcome]() {
                 base::Value ret(base::Value::Type::DICTIONARY);
                 ret.SetStringKey("outcome", outcome);
                 return ret;
               });
}

}  // namespace
namespace internal {

// Parse the Signed-Headers input header as a Structured Headers Draft 15 list
// of "tokens" (unquoted strings with a constrained alphabet).
base::Optional<std::vector<std::string>> ParseTrustTokenSignedHeadersHeader(
    base::StringPiece header) {
  base::Optional<net::structured_headers::List> maybe_list =
      net::structured_headers::ParseList(header);
  if (!maybe_list)
    return base::nullopt;
  std::vector<std::string> ret;
  for (const net::structured_headers::ParameterizedMember&
           parameterized_member : *maybe_list) {
    if (!parameterized_member.params.empty() ||
        parameterized_member.member.size() != 1) {
      return base::nullopt;
    }
    const net::structured_headers::ParameterizedItem& parameterized_item =
        parameterized_member.member.front();
    if (!parameterized_item.params.empty())
      return base::nullopt;
    if (!parameterized_item.item.is_token())
      return base::nullopt;
    ret.push_back(parameterized_item.item.GetString());
  }
  return ret;
}

}  // namespace internal

const char* const TrustTokenRequestSigningHelper::kSignableRequestHeaders[]{
    kTrustTokensRequestHeaderSecSignedRedemptionRecord,
    kTrustTokensRequestHeaderSecTime,
};

constexpr char
    TrustTokenRequestSigningHelper::kCanonicalizedRequestDataUrlKey[];
constexpr char
    TrustTokenRequestSigningHelper::kCanonicalizedRequestDataPublicKeyKey[];
constexpr uint8_t
    TrustTokenRequestSigningHelper::kRequestSigningDomainSeparator[];

namespace {

using Params = TrustTokenRequestSigningHelper::Params;

// Constants for keys and values in the Sec-Signature header:
const char kSignatureHeaderSignRequestDataIncludeValue[] = "include";
const char kSignatureHeaderSignRequestDataHeadersOnlyValue[] = "headers-only";
const char kSignatureHeaderSignRequestDataKey[] = "sign-request-data";
const char kSignatureHeaderPublicKeyKey[] = "public-key";
const char kSignatureHeaderSignatureKey[] = "sig";

std::vector<std::string> Lowercase(std::vector<std::string> in) {
  for (std::string& str : in) {
    for (auto& ch : str) {
      ch = base::ToLowerASCII(ch);
    }
  }

  return in;
}

// In order to check whether all of the header names given by the client are
// signable, perform a single initial computation of the lower-cased versions
// of |kSignableRequestHeaders|.
const base::flat_set<std::string>& LowercaseSignableHeaders() {
  static base::NoDestructor<base::flat_set<std::string>>
      kLowercaseSignableHeaders{Lowercase(
          {std::begin(TrustTokenRequestSigningHelper::kSignableRequestHeaders),
           std::end(TrustTokenRequestSigningHelper::kSignableRequestHeaders)})};
  return *kLowercaseSignableHeaders;
}

// Attempts to combine the (comma-delimited) header names in |request|'s
// Signed-Headers header, if any, and the members of |additional_headers|.
//
// Returns nullopt, and removes |request|'s Signed-Headers header, if any
// provided header name is not present in the signable headers allowlist
// TrustTokenRequestSigningHelper::kSignableRequestHeaders.
//
// Otherwise:
// - updates |request|'s Signed-Headers header to contain the union of the
// lower-cased members of |additional_headers| and the lower-cased elements of
// |request|'s previous header value; and
// - returns the list of these header names.
base::Optional<std::vector<std::string>>
GetHeadersToSignAndUpdateSignedHeadersHeader(
    net::URLRequest* request,
    const std::vector<std::string>& additional_headers) {
  std::string signed_headers_header;
  ignore_result(request->extra_request_headers().GetHeader(
      kTrustTokensRequestHeaderSignedHeaders, &signed_headers_header));

  // Because of the characteristics of the protocol, there are expected to be
  // roughly 2-5 total headers to sign.
  base::flat_set<std::string> deduped_lowercase_headers_to_sign(
      Lowercase(additional_headers));

  base::Optional<std::vector<std::string>> maybe_parsed_header_names =
      internal::ParseTrustTokenSignedHeadersHeader(signed_headers_header);

  // Remove the Signed-Headers header:
  // - On failure, or on success with no headers to sign, this will stay removed
  // in order to denote that no headers are being signed.
  // - On success, it will be added back to the request.
  request->RemoveRequestHeaderByName(kTrustTokensRequestHeaderSignedHeaders);

  // Fail if the request's Signed-Headers header existed but failed to parse.
  if (!maybe_parsed_header_names)
    return base::nullopt;

  for (const std::string& header_name : Lowercase(*maybe_parsed_header_names))
    deduped_lowercase_headers_to_sign.insert(header_name);

  // If there are no headers to sign, don't bother readding the Signed-Headers
  // header.
  if (deduped_lowercase_headers_to_sign.empty())
    return std::vector<std::string>();

  if (!base::STLIncludes(LowercaseSignableHeaders(),
                         deduped_lowercase_headers_to_sign)) {
    return base::nullopt;
  }

  std::vector<std::string> out(
      std::make_move_iterator(deduped_lowercase_headers_to_sign.begin()),
      std::make_move_iterator(deduped_lowercase_headers_to_sign.end()));

  request->SetExtraRequestHeaderByName(kTrustTokensRequestHeaderSignedHeaders,
                                       base::JoinString(out, ","),
                                       /*overwrite=*/true);
  return out;
}

void AttachSignedRedemptionRecordHeader(net::URLRequest* request,
                                        const std::string& value) {
  request->SetExtraRequestHeaderByName(
      kTrustTokensRequestHeaderSecSignedRedemptionRecord, value,
      /*overwrite=*/true);
}

}  // namespace

TrustTokenRequestSigningHelper::TrustTokenRequestSigningHelper(
    TrustTokenStore* token_store,
    Params params,
    std::unique_ptr<Signer> signer,
    std::unique_ptr<TrustTokenRequestCanonicalizer> canonicalizer,
    net::NetLogWithSource net_log)
    : token_store_(token_store),
      params_(std::move(params)),
      signer_(std::move(signer)),
      canonicalizer_(std::move(canonicalizer)),
      net_log_(std::move(net_log)) {}

TrustTokenRequestSigningHelper::~TrustTokenRequestSigningHelper() = default;

Params::Params(SuitableTrustTokenOrigin issuer,
               SuitableTrustTokenOrigin toplevel,
               std::vector<std::string> additional_headers_to_sign,
               bool should_add_timestamp,
               mojom::TrustTokenSignRequestData sign_request_data)
    : issuer(std::move(issuer)),
      toplevel(std::move(toplevel)),
      additional_headers_to_sign(std::move(additional_headers_to_sign)),
      should_add_timestamp(should_add_timestamp),
      sign_request_data(sign_request_data) {}

Params::Params(SuitableTrustTokenOrigin issuer,
               SuitableTrustTokenOrigin toplevel)
    : issuer(std::move(issuer)), toplevel(std::move(toplevel)) {}
Params::~Params() = default;
Params::Params(const Params&) = default;
// The type alias causes a linter false positive.
// NOLINTNEXTLINE(misc-unconventional-assign-operator)
Params& Params::operator=(const Params&) = default;
Params::Params(Params&&) = default;
// NOLINTNEXTLINE(misc-unconventional-assign-operator)
Params& Params::operator=(Params&&) = default;

void TrustTokenRequestSigningHelper::Begin(
    net::URLRequest* request,
    base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done) {
  DCHECK(request);
  DCHECK(!request->initiator() ||
         IsOriginPotentiallyTrustworthy(*request->initiator()))
      << *request->initiator();

  // This class is responsible for adding these headers; callers should not add
  // them.
  DCHECK(!request->extra_request_headers().HasHeader(
      kTrustTokensRequestHeaderSecSignedRedemptionRecord));
  DCHECK(!request->extra_request_headers().HasHeader(
      kTrustTokensRequestHeaderSecTime));
  DCHECK(!request->extra_request_headers().HasHeader(
      kTrustTokensRequestHeaderSecSignature));

  net_log_.BeginEvent(
      net::NetLogEventType::TRUST_TOKEN_OPERATION_BEGIN_SIGNING);

  base::Optional<SignedTrustTokenRedemptionRecord> maybe_redemption_record =
      token_store_->RetrieveNonstaleRedemptionRecord(params_.issuer,
                                                     params_.toplevel);

  if (!maybe_redemption_record) {
    AttachSignedRedemptionRecordHeader(request, std::string());

    LogOutcome(net_log_, "No SRR for this (issuer, top-level context) pair");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kResourceExhausted);
    return;
  }

  base::Optional<std::vector<std::string>> maybe_headers_to_sign =
      GetHeadersToSignAndUpdateSignedHeadersHeader(
          request, params_.additional_headers_to_sign);

  if (!maybe_headers_to_sign) {
    AttachSignedRedemptionRecordHeader(request, std::string());

    LogOutcome(net_log_,
               "Unsignable header specified in Signed-Headers "
               "header or additionalSignedHeaders arg");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kInvalidArgument);
    return;
  }

  AttachSignedRedemptionRecordHeader(request, maybe_redemption_record->body());

  if (params_.should_add_timestamp) {
    request->SetExtraRequestHeaderByName(kTrustTokensRequestHeaderSecTime,
                                         base::TimeToISO8601(base::Time::Now()),
                                         /*overwrite=*/true);
  }

  if (params_.sign_request_data == mojom::TrustTokenSignRequestData::kOmit) {
    LogOutcome(net_log_, "Success (sign-request-data='omit')");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kOk);
    return;
  }

  base::Optional<std::vector<uint8_t>> maybe_signature =
      GetSignature(request, *maybe_redemption_record, *maybe_headers_to_sign);

  if (!maybe_signature) {
    AttachSignedRedemptionRecordHeader(request, std::string());
    request->RemoveRequestHeaderByName(kTrustTokensRequestHeaderSecTime);
    request->RemoveRequestHeaderByName(kTrustTokensRequestHeaderSignedHeaders);

    LogOutcome(net_log_, "Internal error generating signature");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kInternalError);
    return;
  }

  base::Optional<std::string> maybe_signature_header = BuildSignatureHeader(
      maybe_redemption_record->public_key(),
      base::StringPiece(reinterpret_cast<const char*>(maybe_signature->data()),
                        maybe_signature->size()));

  // Error serializing the header. Not expected.
  if (!maybe_signature_header) {
    LogOutcome(net_log_, "Internal error serializing signature header");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kInternalError);
    return;
  }

  request->SetExtraRequestHeaderByName(kTrustTokensRequestHeaderSecSignature,
                                       *maybe_signature_header,
                                       /*overwrite=*/true);

  LogOutcome(net_log_, "Success");
  std::move(done).Run(mojom::TrustTokenOperationStatus::kOk);
}

void TrustTokenRequestSigningHelper::Finalize(
    mojom::URLResponseHead* response,
    base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done) {
  return std::move(done).Run(mojom::TrustTokenOperationStatus::kOk);
}

base::Optional<std::string>
TrustTokenRequestSigningHelper::BuildSignatureHeader(
    base::StringPiece public_key,
    base::StringPiece signature) {
  net::structured_headers::Dictionary header_items;

  header_items[kSignatureHeaderPublicKeyKey] =
      net::structured_headers::ParameterizedMember(
          net::structured_headers::Item(
              std::string(public_key),
              net::structured_headers::Item::ItemType::kByteSequenceType),
          {});
  header_items[kSignatureHeaderSignatureKey] =
      net::structured_headers::ParameterizedMember(
          net::structured_headers::Item(
              std::string(signature),
              net::structured_headers::Item::ItemType::kByteSequenceType),
          {});

  // A value of kOmit denotes not wanting the request signed at all, so it'd be
  // a caller error if we were trying to sign the request with it set.
  DCHECK_NE(params_.sign_request_data, mojom::TrustTokenSignRequestData::kOmit);

  const char* sign_request_data_value =
      params_.sign_request_data == mojom::TrustTokenSignRequestData::kInclude
          ? kSignatureHeaderSignRequestDataIncludeValue
          : kSignatureHeaderSignRequestDataHeadersOnlyValue;

  header_items[kSignatureHeaderSignRequestDataKey] =
      net::structured_headers::ParameterizedMember(
          net::structured_headers::Item(
              sign_request_data_value,
              net::structured_headers::Item::ItemType::kTokenType),
          {});

  return net::structured_headers::SerializeDictionary(header_items);
}

base::Optional<std::vector<uint8_t>>
TrustTokenRequestSigningHelper::GetSignature(
    net::URLRequest* request,
    const SignedTrustTokenRedemptionRecord& redemption_record,
    const std::vector<std::string>& headers_to_sign) {
  // (This follows the normative pseudocode, labeled "signature
  // generation," in the Trust Tokens design doc.)
  //
  // 1. Generate a CBOR-encoded dictionary, the canonical request data.
  // 2. Sign the concatenation of “Trust Token v0” and the CBOR-encoded
  // dictionary. (The domain separator string “Trust Token v0” allows versioning
  // otherwise-forward-compatible protocol structures, which is useful in case
  // the semantics change across versions.)

  base::Optional<std::vector<uint8_t>> maybe_request_in_cbor =
      canonicalizer_->Canonicalize(
          request->url(), request->extra_request_headers(),
          redemption_record.public_key(), params_.sign_request_data);

  if (!maybe_request_in_cbor)
    return base::nullopt;

  // kRequestSigningDomainSeparator is an explicitly-specified char array, not
  // a string literal, so this will, as intended, not include a null terminator.
  std::vector<uint8_t> signing_data(std::begin(kRequestSigningDomainSeparator),
                                    std::end(kRequestSigningDomainSeparator));
  signing_data.insert(signing_data.end(), maybe_request_in_cbor->begin(),
                      maybe_request_in_cbor->end());

  base::span<const uint8_t> key_bytes =
      base::as_bytes(base::make_span(redemption_record.signing_key()));
  return signer_->Sign(key_bytes, base::make_span(signing_data));
}

}  // namespace network

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/trust_token_attribute_parsing.h"
#include "services/network/public/mojom/trust_tokens.mojom-blink.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_trust_token.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace internal {

namespace {
bool ParseType(const String& in, network::mojom::TrustTokenOperationType* out) {
  if (in == "token-request") {
    *out = network::mojom::TrustTokenOperationType::kIssuance;
    return true;
  } else if (in == "srr-token-redemption") {
    *out = network::mojom::TrustTokenOperationType::kRedemption;
    return true;
  } else if (in == "send-srr") {
    *out = network::mojom::TrustTokenOperationType::kSigning;
    return true;
  } else {
    return false;
  }
}
bool ParseRefreshPolicy(const String& in,
                        network::mojom::TrustTokenRefreshPolicy* out) {
  if (in == "none") {
    *out = network::mojom::TrustTokenRefreshPolicy::kUseCached;
    return true;
  } else if (in == "refresh") {
    *out = network::mojom::TrustTokenRefreshPolicy::kRefresh;
    return true;
  }
  return false;
}
bool ParseSignRequestData(const String& in,
                          network::mojom::TrustTokenSignRequestData* out) {
  if (in == "omit") {
    *out = network::mojom::TrustTokenSignRequestData::kOmit;
    return true;
  } else if (in == "headers-only") {
    *out = network::mojom::TrustTokenSignRequestData::kHeadersOnly;
    return true;
  } else if (in == "include") {
    *out = network::mojom::TrustTokenSignRequestData::kInclude;
    return true;
  }
  return false;
}
}  // namespace

// Given a JSON representation of a Trust Token parameters struct, constructs
// and returns the represented struct if the JSON representation is valid;
// returns nullopt otherwise.
network::mojom::blink::TrustTokenParamsPtr TrustTokenParamsFromJson(
    std::unique_ptr<JSONValue> in) {
  JSONObject* object = JSONObject::Cast(in.get());

  if (!object)
    return nullptr;

  auto ret = network::mojom::blink::TrustTokenParams::New();

  // |type| is required.
  String type;
  if (!object->GetString("type", &type))
    return nullptr;
  if (!ParseType(type, &ret->type))
    return nullptr;

  // |refreshPolicy| is optional.
  if (JSONValue* refresh_policy = object->Get("refreshPolicy")) {
    String str_policy;
    if (!refresh_policy->AsString(&str_policy))
      return nullptr;
    if (!ParseRefreshPolicy(str_policy, &ret->refresh_policy))
      return nullptr;
  }

  // |signRequestData| is optional.
  if (JSONValue* sign_request_data = object->Get("signRequestData")) {
    String str_sign_request_data;
    if (!sign_request_data->AsString(&str_sign_request_data))
      return nullptr;
    if (!ParseSignRequestData(str_sign_request_data, &ret->sign_request_data)) {
      return nullptr;
    }
  }

  // |includeTimestampHeader| is optional.
  if (JSONValue* include_timestamp_header =
          object->Get("includeTimestampHeader")) {
    if (!include_timestamp_header->AsBoolean(&ret->include_timestamp_header))
      return nullptr;
  }

  // |issuer| is optional, but, if it's present, it must be a valid origin,
  // potentially trustworthy, and HTTP or HTTPS.
  if (JSONValue* issuer = object->Get("issuer")) {
    String str_issuer;
    if (!issuer->AsString(&str_issuer))
      return nullptr;
    ret->issuer = SecurityOrigin::CreateFromString(str_issuer);
    if (!ret->issuer)
      return nullptr;
    if (!ret->issuer->IsPotentiallyTrustworthy())
      return nullptr;
    if (ret->issuer->Protocol() != "http" && ret->issuer->Protocol() != "https")
      return nullptr;
  }

  // |additionalSignedHeaders| is optional.
  if (JSONValue* additional_signed_headers =
          object->Get("additionalSignedHeaders")) {
    JSONArray* signed_headers_array =
        JSONArray::Cast(additional_signed_headers);
    if (!signed_headers_array)
      return nullptr;

    // Because of the characteristics of the Trust Tokens protocol, we expect
    // roughly 2-5 elements in this array.
    for (size_t i = 0; i < signed_headers_array->size(); ++i) {
      String next;
      if (!signed_headers_array->at(i)->AsString(&next))
        return nullptr;
      ret->additional_signed_headers.push_back(std::move(next));
    }
  }

  return ret;
}

}  // namespace internal
}  // namespace blink

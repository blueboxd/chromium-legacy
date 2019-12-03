// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/web_push_common.h"

#include "base/metrics/histogram_functions.h"

namespace {

// Response body when 403: Forbidden is received when sending web push message.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep in sync with
// ForbiddenResponseBody in enums.xml.
enum class ForbiddenResponseBody {
  kBodyMissing = 0,
  kUnknownBody = 1,
  kSenderNotAuthenticated = 2,
  kInvalidPublicKeyEncoding = 3,
  kInvalidPublicKeyLength = 4,
  kInvalidPublicKeyFormat = 5,
  kInvalidPublicKeyCurve = 6,
  kInvalidJWT = 7,
  kInvalidAudClaim = 8,
  kAudClaimMissingOrigin = 9,
  kExpClaimPeriodTooLong = 10,
  kExpClaimExpired = 11,
  kExpClaimMissing = 12,
  kNoP256ECDSA = 13,
  kNoK = 14,
  kBase64P256ECDSA = 15,
  kBase64K = 16,
  kEmptyP256ECDSA = 17,
  kEmptyK = 18,
  kInvalidP256ECDSA = 19,
  kInvalidK = 20,
  kMaxValue = kInvalidK,
};

ForbiddenResponseBody GetResposneBodyEnum(const std::string* response_body) {
  if (!response_body)
    return ForbiddenResponseBody::kBodyMissing;

  if (*response_body == "sender is not authenticated.\n")
    return ForbiddenResponseBody::kSenderNotAuthenticated;

  if (*response_body ==
      "make sure the public key is base64url-encoded [RFC7515]\n") {
    return ForbiddenResponseBody::kInvalidPublicKeyEncoding;
  }

  if (*response_body == "public key must be 65 bytes long\n")
    return ForbiddenResponseBody::kInvalidPublicKeyLength;

  if (*response_body ==
      "public key must be in uncompressed form. (First byte needs to be "
      "0x04)\n") {
    return ForbiddenResponseBody::kInvalidPublicKeyFormat;
  }

  if (*response_body == "VAPID public key must be on the P-256 curve\n")
    return ForbiddenResponseBody::kInvalidPublicKeyCurve;

  if (*response_body == "invalid JWT provided\n")
    return ForbiddenResponseBody::kInvalidJWT;

  if (*response_body == "invalid aud claim\n")
    return ForbiddenResponseBody::kInvalidAudClaim;

  if (*response_body ==
      "aud claim in the token MUST include the origin of the push resource "
      "URL\n") {
    return ForbiddenResponseBody::kAudClaimMissingOrigin;
  }

  if (*response_body ==
      "exp claim MUST NOT be more than 24 hours from the time of the "
      "request\n") {
    return ForbiddenResponseBody::kExpClaimPeriodTooLong;
  }

  if (*response_body == "token has expired\n")
    return ForbiddenResponseBody::kExpClaimExpired;

  if (*response_body == "exp claim is missing\n")
    return ForbiddenResponseBody::kExpClaimMissing;

  if (*response_body == "exp claim is missing\n")
    return ForbiddenResponseBody::kExpClaimMissing;

  if (*response_body ==
      "crypto-key header had no public application server key specified. "
      "crypto-key header should have the following format: "
      "p256ecdsa=base64(publicApplicationServerKey)\n") {
    return ForbiddenResponseBody::kNoP256ECDSA;
  }

  if (*response_body ==
      "authorization header had no public application server key specified. "
      "Authorization header should have the following format: vapid "
      "t=jwtToken; k=base64(publicApplicationServerKey)\n") {
    return ForbiddenResponseBody::kNoK;
  }

  if (*response_body ==
      "public application server key in crypto-key header must be base64 "
      "encoded in URL and filename safe format.\n") {
    return ForbiddenResponseBody::kBase64P256ECDSA;
  }

  if (*response_body ==
      "public application server key in authorization header must be base64 "
      "encoded in URL and filename safe format.\n") {
    return ForbiddenResponseBody::kBase64K;
  }

  if (*response_body ==
      "crypto-key header had empty public application server key. crypto-key "
      "header should have the following format: "
      "p256ecdsa=base64(publicApplicationServerKey)\n") {
    return ForbiddenResponseBody::kEmptyP256ECDSA;
  }

  if (*response_body ==
      "authorization header had empty public application server key. "
      "Authorization header should have the following format: vapid "
      "t=jwtToken; k=base64(publicApplicationServerKey)\n") {
    return ForbiddenResponseBody::kEmptyK;
  }

  if (*response_body ==
      "crypto-key header had invalid format. crypto-key header should have the "
      "following format: p256ecdsa=base64(publicApplicationServerKey)\n") {
    return ForbiddenResponseBody::kInvalidP256ECDSA;
  }

  if (*response_body ==
      "authorization header had invalid format. authorization header should "
      "have the following format: t=jwtToken; "
      "k=base64(publicApplicationServerKey)\n") {
    return ForbiddenResponseBody::kInvalidK;
  }

  return ForbiddenResponseBody::kUnknownBody;
}

}  // namespace

namespace gcm {

void InvokeWebPushCallback(WebPushCallback callback,
                           SendWebPushMessageResult result,
                           base::Optional<std::string> message_id) {
  DCHECK(message_id || result != SendWebPushMessageResult::kSuccessful);
  base::UmaHistogramEnumeration("GCM.SendWebPushMessageResult", result);
  std::move(callback).Run(result, std::move(message_id));
}

void LogSendWebPushMessagePayloadSize(int size) {
  // Note: The maximum size accepted by FCM is 4096.
  base::UmaHistogramCounts10000("GCM.SendWebPushMessagePayloadSize", size);
}

void LogSendWebPushMessageStatusCode(int status_code) {
  base::UmaHistogramSparse("GCM.SendWebPushMessageStatusCode", status_code);
}

void LogSendWebPushMessageForbiddenBody(const std::string* response_body) {
  if (response_body)
    DLOG(ERROR) << *response_body;

  base::UmaHistogramEnumeration("GCM.SendWebPushMessageForbiddenBody",
                                GetResposneBodyEnum(response_body));
}

}  // namespace gcm

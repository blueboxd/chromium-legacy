// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/web_push_sender.h"

#include <limits.h>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/gcm_driver/common/gcm_message.h"
#include "components/gcm_driver/crypto/json_web_token_util.h"
#include "components/gcm_driver/crypto/p256_key_util.h"
#include "components/gcm_driver/web_push_metrics.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace gcm {

namespace {

// VAPID header constants.
const char kClaimsKeyAudience[] = "aud";
const char kFCMServerAudience[] = "https://fcm.googleapis.com";

const char kClaimsKeyExpirationTime[] = "exp";

const char kAuthorizationRequestHeaderFormat[] = "vapid t=%s, k=%s";

// Endpoint constants.
const char kFCMServerUrlFormat[] = "https://fcm.googleapis.com/fcm/send/%s";

// HTTP header constants.
const char kTTL[] = "TTL";
const char kUrgency[] = "Urgency";

const char kContentEncodingProperty[] = "content-encoding";
const char kContentCodingAes128Gcm[] = "aes128gcm";

// Other constants.
const char kContentEncodingOctetStream[] = "application/octet-stream";
const int kMaximumBodySize = 4096;

base::Optional<std::string> GetAuthHeader(crypto::ECPrivateKey* vapid_key,
                                          int time_to_live) {
  base::Value claims(base::Value::Type::DICTIONARY);
  claims.SetKey(kClaimsKeyAudience, base::Value(kFCMServerAudience));

  int64_t exp =
      (base::Time::Now() - base::Time::UnixEpoch()).InSeconds() + time_to_live;
  // TODO: Year 2038 problem, base::Value does not support int64_t.
  if (exp > INT_MAX)
    return base::nullopt;

  claims.SetKey(kClaimsKeyExpirationTime,
                base::Value(static_cast<int32_t>(exp)));

  base::Optional<std::string> jwt = CreateJSONWebToken(claims, vapid_key);
  if (!jwt)
    return base::nullopt;

  std::string public_key;
  if (!GetRawPublicKey(*vapid_key, &public_key))
    return base::nullopt;

  std::string base64_public_key;
  base::Base64UrlEncode(public_key, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &base64_public_key);

  return base::StringPrintf(kAuthorizationRequestHeaderFormat, jwt->c_str(),
                            base64_public_key.c_str());
}

std::string GetUrgencyHeader(WebPushMessage::Urgency urgency) {
  switch (urgency) {
    case WebPushMessage::Urgency::kVeryLow:
      return "very-low";
    case WebPushMessage::Urgency::kLow:
      return "low";
    case WebPushMessage::Urgency::kNormal:
      return "normal";
    case WebPushMessage::Urgency::kHigh:
      return "high";
  }
}

std::unique_ptr<network::SimpleURLLoader> BuildURLLoader(
    const std::string& fcm_token,
    int time_to_live,
    const std::string& urgency_header,
    const std::string& auth_header,
    const std::string& message) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  std::string server_url =
      base::StringPrintf(kFCMServerUrlFormat, fcm_token.c_str());
  resource_request->url = GURL(server_url);
  resource_request->load_flags =
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES;
  resource_request->method = "POST";
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      auth_header);
  resource_request->headers.SetHeader(kTTL, base::NumberToString(time_to_live));
  resource_request->headers.SetHeader(kContentEncodingProperty,
                                      kContentCodingAes128Gcm);
  resource_request->headers.SetHeader(kUrgency, urgency_header);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("web_push_message", R"(
        semantics {
          sender: "GCMDriver WebPushSender"
          description:
            "Send a request via Firebase to another device that is signed in"
            "with the same account."
          trigger: "Users send data to another owned device."
          data: "Web push message."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "You can enable or disable this feature in Chrome's settings under"
            "'Sync and Google services'."
          policy_exception_justification: "Not implemented."
        }
      )");
  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  loader->AttachStringForUpload(message, kContentEncodingOctetStream);
  loader->SetRetryOptions(1, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  loader->SetAllowHttpErrorResults(true);

  return loader;
}

}  // namespace

WebPushSender::WebPushSender(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {}

WebPushSender::~WebPushSender() = default;

void WebPushSender::SendMessage(const std::string& fcm_token,
                                crypto::ECPrivateKey* vapid_key,
                                const WebPushMessage& message,
                                SendMessageCallback callback) {
  DCHECK(!fcm_token.empty());
  DCHECK(vapid_key);
  DCHECK_LE(message.time_to_live, message.kMaximumTTL);

  base::Optional<std::string> auth_header =
      GetAuthHeader(vapid_key, message.time_to_live);
  if (!auth_header) {
    LOG(ERROR) << "Failed to create JWT";
    LogSendWebPushMessageResult(SendWebPushMessageResult::kCreateJWTFailed);
    std::move(callback).Run(base::nullopt);
    return;
  }

  LogSendWebPushMessagePayloadSize(message.payload.size());
  std::unique_ptr<network::SimpleURLLoader> url_loader = BuildURLLoader(
      fcm_token, message.time_to_live, GetUrgencyHeader(message.urgency),
      *auth_header, message.payload);
  url_loader->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&WebPushSender::OnMessageSent,
                     weak_ptr_factory_.GetWeakPtr(), std::move(url_loader),
                     std::move(callback)),
      kMaximumBodySize);
}

void WebPushSender::OnMessageSent(
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    SendMessageCallback callback,
    std::unique_ptr<std::string> response_body) {
  int net_error = url_loader->NetError();
  if (net_error != net::OK) {
    LOG(ERROR) << "Network Error: " << net_error;
    LogSendWebPushMessageResult(SendWebPushMessageResult::kNetworkError);
    std::move(callback).Run(base::nullopt);
    return;
  }

  scoped_refptr<net::HttpResponseHeaders> response_headers =
      url_loader->ResponseInfo()->headers;
  if (!url_loader->ResponseInfo() || !response_headers) {
    LOG(ERROR) << "Response info not found";
    LogSendWebPushMessageResult(SendWebPushMessageResult::kServerError);
    std::move(callback).Run(base::nullopt);
    return;
  }

  int response_code = response_headers->response_code();
  if (!network::cors::IsOkStatus(response_code)) {
    LOG(ERROR) << "HTTP Error: " << response_code;
    LogSendWebPushMessageResult(SendWebPushMessageResult::kServerError);
    std::move(callback).Run(base::nullopt);
    return;
  }

  std::string location;
  if (!response_headers->EnumerateHeader(nullptr, "location", &location)) {
    LOG(ERROR) << "Failed to get location header from response";
    LogSendWebPushMessageResult(SendWebPushMessageResult::kParseResponseFailed);
    std::move(callback).Run(base::nullopt);
    return;
  }

  size_t slash_pos = location.rfind("/");
  if (slash_pos == std::string::npos) {
    LOG(ERROR) << "Failed to parse message_id from location header";
    LogSendWebPushMessageResult(SendWebPushMessageResult::kParseResponseFailed);
    std::move(callback).Run(base::nullopt);
    return;
  }

  LogSendWebPushMessageResult(SendWebPushMessageResult::kSuccessful);
  std::move(callback).Run(base::make_optional(location.substr(slash_pos + 1)));
}

}  // namespace gcm

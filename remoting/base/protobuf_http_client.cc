// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/protobuf_http_client.h"

#include "base/strings/stringprintf.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/base/protobuf_http_request.h"
#include "remoting/base/protobuf_http_status.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"
#include "url/gurl.h"

namespace {

constexpr char kAuthorizationHeaderFormat[] = "Authorization: Bearer %s";
constexpr int kMaxResponseSizeKb = 512;

}  // namespace

namespace remoting {

ProtobufHttpClient::ProtobufHttpClient(
    const std::string& server_endpoint,
    OAuthTokenGetter* token_getter,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : server_endpoint_(server_endpoint),
      token_getter_(token_getter),
      url_loader_factory_(url_loader_factory) {}

ProtobufHttpClient::~ProtobufHttpClient() = default;

void ProtobufHttpClient::ExecuteRequest(
    std::unique_ptr<ProtobufHttpRequest> request) {
  DCHECK(request->request_message);
  DCHECK(!request->path.empty());
  DCHECK(request->response_callback_);

  if (!request->authenticated) {
    DoExecuteRequest(std::move(request), OAuthTokenGetter::Status::SUCCESS, {},
                     {});
    return;
  }

  DCHECK(token_getter_);
  token_getter_->CallWithToken(
      base::BindOnce(&ProtobufHttpClient::DoExecuteRequest,
                     weak_factory_.GetWeakPtr(), std::move(request)));
}

void ProtobufHttpClient::CancelPendingRequests() {
  weak_factory_.InvalidateWeakPtrs();
}

void ProtobufHttpClient::DoExecuteRequest(
    std::unique_ptr<ProtobufHttpRequest> request,
    OAuthTokenGetter::Status status,
    const std::string& user_email,
    const std::string& access_token) {
  if (status != OAuthTokenGetter::Status::SUCCESS) {
    LOG(ERROR) << "Failed to fetch access token. Status: " << status;
    request->OnResponse(
        ProtobufHttpStatus(net::HttpStatusCode::HTTP_UNAUTHORIZED), nullptr);
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL("https://" + server_endpoint_ + request->path);
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = net::HttpRequestHeaders::kPostMethod;

  if (status == OAuthTokenGetter::Status::SUCCESS && !access_token.empty()) {
    resource_request->headers.AddHeaderFromString(
        base::StringPrintf(kAuthorizationHeaderFormat, access_token.c_str()));
  } else {
    VLOG(1) << "Attempting to execute request without access token";
  }

  std::unique_ptr<network::SimpleURLLoader> send_url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       request->traffic_annotation);
  send_url_loader->SetTimeoutDuration(request->timeout_duration);
  send_url_loader->AttachStringForUpload(
      request->request_message->SerializeAsString(), "application/x-protobuf");
  send_url_loader->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&ProtobufHttpClient::OnResponse,
                     weak_factory_.GetWeakPtr(), std::move(request),
                     std::move(send_url_loader)),
      kMaxResponseSizeKb);
}

void ProtobufHttpClient::OnResponse(
    std::unique_ptr<ProtobufHttpRequest> request,
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    std::unique_ptr<std::string> response_body) {
  net::Error net_error = static_cast<net::Error>(url_loader->NetError());
  if (net_error == net::Error::ERR_HTTP_RESPONSE_CODE_FAILURE &&
      (!url_loader->ResponseInfo() || !url_loader->ResponseInfo()->headers)) {
    LOG(ERROR) << "Can't find response header.";
    net_error = net::Error::ERR_INVALID_RESPONSE;
  }
  ProtobufHttpStatus status =
      (net_error == net::Error::ERR_HTTP_RESPONSE_CODE_FAILURE ||
       net_error == net::Error::OK)
          ? ProtobufHttpStatus(static_cast<net::HttpStatusCode>(
                url_loader->ResponseInfo()->headers->response_code()))
          : ProtobufHttpStatus(net_error);
  request->OnResponse(status, std::move(response_body));
}

}  // namespace remoting

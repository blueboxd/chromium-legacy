// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_PROTOBUF_HTTP_CLIENT_H_
#define REMOTING_BASE_PROTOBUF_HTTP_CLIENT_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/base/oauth_token_getter.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace remoting {

struct ProtobufHttpRequest;

// Helper class for executing REST/Protobuf requests over HTTP.
class ProtobufHttpClient final {
 public:
  // |server_endpoint| is the hostname of the server.
  // |token_getter| is nullable if none of the requests are authenticated.
  // |token_getter| must outlive |this|.
  ProtobufHttpClient(
      const std::string& server_endpoint,
      OAuthTokenGetter* token_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~ProtobufHttpClient();
  ProtobufHttpClient(const ProtobufHttpClient&) = delete;
  ProtobufHttpClient& operator=(const ProtobufHttpClient&) = delete;

  // Executes a unary request. Caller will not be notified of the result if
  // CancelPendingRequests() is called or |this| is destroyed.
  void ExecuteRequest(std::unique_ptr<ProtobufHttpRequest> request);

  // Tries to cancel all pending requests. Note that this prevents request
  // callbacks from being called but does not necessarily stop pending requests
  // from being sent.
  void CancelPendingRequests();

 private:
  void DoExecuteRequest(std::unique_ptr<ProtobufHttpRequest> request,
                        OAuthTokenGetter::Status status,
                        const std::string& user_email,
                        const std::string& access_token);

  void OnResponse(std::unique_ptr<ProtobufHttpRequest> request,
                  std::unique_ptr<network::SimpleURLLoader> url_loader,
                  std::unique_ptr<std::string> response_body);

  std::string server_endpoint_;
  OAuthTokenGetter* token_getter_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  base::WeakPtrFactory<ProtobufHttpClient> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_BASE_PROTOBUF_HTTP_CLIENT_H_
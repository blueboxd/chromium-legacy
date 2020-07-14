// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_PROTOBUF_HTTP_REQUEST_H_
#define REMOTING_BASE_PROTOBUF_HTTP_REQUEST_H_

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/time/time.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/base/protobuf_http_status.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace remoting {

// A simple unary request. Caller needs to set all public members and call
// SetResponseCallback() before passing it to ProtobufHttpClient.
struct ProtobufHttpRequest final {
  template <typename ResponseType>
  using ResponseCallback =
      base::OnceCallback<void(const ProtobufHttpStatus& status,
                              std::unique_ptr<ResponseType> response)>;

  explicit ProtobufHttpRequest(
      const net::NetworkTrafficAnnotationTag& traffic_annotation);
  ~ProtobufHttpRequest();

  const net::NetworkTrafficAnnotationTag traffic_annotation;
  std::unique_ptr<google::protobuf::MessageLite> request_message;
  std::string path;
  bool authenticated = true;
  base::TimeDelta timeout_duration = base::TimeDelta::FromSeconds(30);

  // Sets the response callback. |ResponseType| needs to be a protobuf message
  // type.
  template <typename ResponseType>
  void SetResponseCallback(ResponseCallback<ResponseType> callback) {
    auto response = std::make_unique<ResponseType>();
    response_message_ = response.get();
    response_callback_ = base::BindOnce(
        [](std::unique_ptr<ResponseType> response,
           ResponseCallback<ResponseType> callback,
           const ProtobufHttpStatus& status) {
          if (!status.ok()) {
            response.reset();
          }
          std::move(callback).Run(status, std::move(response));
        },
        std::move(response), std::move(callback));
  }

 private:
  friend class ProtobufHttpClient;

  // To be called by ProtobufHttpClient.
  void OnResponse(const ProtobufHttpStatus& status,
                  std::unique_ptr<std::string> response_body);

  // Parses |response_body| and writes it to |response_message_|.
  ProtobufHttpStatus ParseResponse(std::unique_ptr<std::string> response_body);

  // This is owned by |response_callback_|.
  google::protobuf::MessageLite* response_message_;
  base::OnceCallback<void(const ProtobufHttpStatus&)> response_callback_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_PROTOBUF_HTTP_REQUEST_H_

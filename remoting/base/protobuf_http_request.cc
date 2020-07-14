// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/protobuf_http_request.h"

namespace remoting {

ProtobufHttpRequest::ProtobufHttpRequest(
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : traffic_annotation(traffic_annotation) {}

ProtobufHttpRequest::~ProtobufHttpRequest() = default;

void ProtobufHttpRequest::OnResponse(
    const ProtobufHttpStatus& status,
    std::unique_ptr<std::string> response_body) {
  std::move(response_callback_)
      .Run(status.ok() ? ParseResponse(std::move(response_body)) : status);
}

ProtobufHttpStatus ProtobufHttpRequest::ParseResponse(
    std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    LOG(ERROR) << "Server returned no response body";
    return ProtobufHttpStatus(net::ERR_EMPTY_RESPONSE);
  }
  if (!response_message_->ParseFromString(*response_body)) {
    LOG(ERROR) << "Failed to parse response body";
    return ProtobufHttpStatus(net::ERR_INVALID_RESPONSE);
  }
  return ProtobufHttpStatus::OK;
}

}  // namespace remoting

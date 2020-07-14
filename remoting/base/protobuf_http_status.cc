// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/protobuf_http_status.h"

#include "net/http/http_status_code.h"

namespace remoting {

const ProtobufHttpStatus& ProtobufHttpStatus::OK =
    ProtobufHttpStatus(net::HttpStatusCode::HTTP_OK);

ProtobufHttpStatus::ProtobufHttpStatus(net::HttpStatusCode http_status_code)
    : http_status_code_(http_status_code),
      net_error_(net::Error::ERR_HTTP_RESPONSE_CODE_FAILURE),
      error_message_(net::GetHttpReasonPhrase(http_status_code)) {
  DCHECK_LE(0, http_status_code) << "Invalid http status code";
}

ProtobufHttpStatus::ProtobufHttpStatus(net::Error net_error)
    : http_status_code_(-1),
      net_error_(net_error),
      error_message_(net::ErrorToString(net_error)) {
  DCHECK_NE(net::Error::OK, net_error) << "Use the HttpStatusCode overload";
  DCHECK_NE(net::Error::ERR_HTTP_RESPONSE_CODE_FAILURE, net_error)
      << "Use the HttpStatusCode overload";
}

ProtobufHttpStatus::~ProtobufHttpStatus() = default;

bool ProtobufHttpStatus::ok() const {
  return http_status_code_ == net::HttpStatusCode::HTTP_OK;
}

}  // namespace remoting

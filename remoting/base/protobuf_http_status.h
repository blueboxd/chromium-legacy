// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_PROTOBUF_HTTP_STATUS_H_
#define REMOTING_BASE_PROTOBUF_HTTP_STATUS_H_

#include <string>

#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"

namespace remoting {

class ProtobufHttpStatus {
 public:
  // An OK pre-defined instance.
  static const ProtobufHttpStatus& OK;

  explicit ProtobufHttpStatus(net::HttpStatusCode http_status_code);
  explicit ProtobufHttpStatus(net::Error net_error);
  ~ProtobufHttpStatus();

  // Indicates whether the http request was successful based on the status code.
  bool ok() const;

  // The http status code, or -1 if the request fails to make, in which case
  // the underlying error can be found by calling net_error().
  int http_status_code() const { return http_status_code_; }

  // The net error. If the error is ERR_HTTP_RESPONSE_CODE_FAILURE, the status
  // code can be retrieved by calling http_status_code().
  net::Error net_error() const { return net_error_; }

  // The message that describes the error.
  const std::string& error_message() const { return error_message_; }

 private:
  int http_status_code_;
  net::Error net_error_;
  std::string error_message_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_PROTOBUF_HTTP_STATUS_H_

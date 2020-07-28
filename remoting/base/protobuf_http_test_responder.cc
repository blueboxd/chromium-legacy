// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/protobuf_http_test_responder.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "net/http/http_status_code.h"
#include "remoting/base/protobuf_http_client_messages.pb.h"
#include "remoting/base/protobuf_http_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace remoting {

ProtobufHttpTestResponder::ProtobufHttpTestResponder() = default;

ProtobufHttpTestResponder::~ProtobufHttpTestResponder() = default;

scoped_refptr<network::SharedURLLoaderFactory>
ProtobufHttpTestResponder::GetUrlLoaderFactory() {
  return base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
      &test_url_loader_factory_);
}

void ProtobufHttpTestResponder::AddResponse(
    const std::string& url,
    const google::protobuf::MessageLite& response_message) {
  test_url_loader_factory_.AddResponse(url,
                                       response_message.SerializeAsString());
}

void ProtobufHttpTestResponder::AddResponseToMostRecentRequestUrl(
    const google::protobuf::MessageLite& response_message) {
  AddResponse(GetMostRecentRequestUrl(), response_message);
}

void ProtobufHttpTestResponder::AddError(
    const std::string& url,
    const ProtobufHttpStatus& error_status) {
  protobufhttpclient::Status status;
  status.set_code(static_cast<int>(error_status.error_code()));
  status.set_message(error_status.error_message());
  test_url_loader_factory_.AddResponse(url, status.SerializeAsString(),
                                       net::HTTP_INTERNAL_SERVER_ERROR);
}

void ProtobufHttpTestResponder::AddErrorToMostRecentRequestUrl(
    const ProtobufHttpStatus& error_status) {
  AddError(GetMostRecentRequestUrl(), error_status);
}

int ProtobufHttpTestResponder::GetNumPending() {
  base::RunLoop().RunUntilIdle();
  return test_url_loader_factory_.pending_requests()->size();
}

network::TestURLLoaderFactory::PendingRequest&
ProtobufHttpTestResponder::GetPendingRequest(size_t index) {
  base::RunLoop().RunUntilIdle();
  DCHECK_LT(index, test_url_loader_factory_.pending_requests()->size());
  return (*test_url_loader_factory_.pending_requests())[index];
}

std::string ProtobufHttpTestResponder::GetMostRecentRequestUrl() {
  int num_pending = GetNumPending();
  DCHECK_LT(0, num_pending);
  return GetPendingRequest(num_pending - 1).request.url.spec();
}

}  // namespace remoting

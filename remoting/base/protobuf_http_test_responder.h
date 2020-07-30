// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_PROTOBUF_HTTP_TEST_RESPONDER_H_
#define REMOTING_BASE_PROTOBUF_HTTP_TEST_RESPONDER_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

namespace google {
namespace protobuf {
class MessageLite;
}  // namespace protobuf
}  // namespace google

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {

class ProtobufHttpStatus;

// Helper class to send responses to Protobuf HTTP requests.
class ProtobufHttpTestResponder final {
 public:
  ProtobufHttpTestResponder();
  ~ProtobufHttpTestResponder();

  ProtobufHttpTestResponder(const ProtobufHttpTestResponder&) = delete;
  ProtobufHttpTestResponder& operator=(const ProtobufHttpTestResponder&) =
      delete;

  // Returns the URL loader factory to be used to create the ProtobufHttpClient.
  // Note that the returned factory *can't be used* after |this| is deleted.
  scoped_refptr<network::SharedURLLoaderFactory> GetUrlLoaderFactory();

  // Note that if you have multiple requests with the same URL, all of them will
  // be resolved with the same response/error.
  void AddResponse(const std::string& url,
                   const google::protobuf::MessageLite& response_message);
  void AddResponseToMostRecentRequestUrl(
      const google::protobuf::MessageLite& response_message);
  void AddError(const std::string& url, const ProtobufHttpStatus& error_status);
  void AddErrorToMostRecentRequestUrl(const ProtobufHttpStatus& error_status);

  // Gets number of pending requests. Unlike
  // network::TestURLLoaderFactory::NumPending(), this method also counts
  // pending but cancelled requests.
  int GetNumPending();

  // Returns the PendingRequest instance available at the given index |index|
  // (including cancelled requests) or null if not existing.
  network::TestURLLoaderFactory::PendingRequest& GetPendingRequest(
      size_t index);

  network::TestURLLoaderFactory::PendingRequest& GetMostRecentPendingRequest();

  std::string GetMostRecentRequestUrl();

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_PROTOBUF_HTTP_TEST_RESPONDER_H_

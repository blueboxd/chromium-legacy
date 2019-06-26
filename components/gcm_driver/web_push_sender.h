// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_WEB_PUSH_SENDER_H_
#define COMPONENTS_GCM_DRIVER_WEB_PUSH_SENDER_H_

#include "base/macros.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace crypto {
class ECPrivateKey;
}

namespace gcm {

struct WebPushMessage;

// Class for sending a message via Firebase Cloud Messaging (FCM) Web Push.
class WebPushSender {
 public:
  using SendMessageCallback =
      base::OnceCallback<void(const std::string&, bool)>;

  explicit WebPushSender(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~WebPushSender();

  // Sends a WebPushMessage via FCM Web Push. Authenticates with FCM server
  // using Voluntary Application Server Identification for Web Push (VAPID)
  // protocol.
  // |fcm_token|: FCM registration token for receiving end.
  // |vapid_key|: Private key to sign VAPID header.
  // |message|: WebPushMessage to be sent.
  // |callback|: To be called once the asynchronous operation is done.
  void SendMessage(const std::string& fcm_token,
                   crypto::ECPrivateKey* vapid_key,
                   const WebPushMessage& message,
                   SendMessageCallback callback);

 private:
  void OnMessageSent(const std::string& message_id,
                     std::unique_ptr<network::SimpleURLLoader> url_loader,
                     SendMessageCallback callback,
                     std::unique_ptr<std::string> response_body);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  base::WeakPtrFactory<WebPushSender> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebPushSender);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_WEB_PUSH_SENDER_H_

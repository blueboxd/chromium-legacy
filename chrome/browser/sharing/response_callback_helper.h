// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_RESPONSE_CALLBACK_HELPER_H_
#define CHROME_BROWSER_SHARING_RESPONSE_CALLBACK_HELPER_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"

namespace chrome_browser_sharing {
enum MessageType : int;
class ResponseMessage;
}  // namespace chrome_browser_sharing

enum class SharingSendMessageResult;

// ResponseCallbackHelper stores callbacks for every message sent. The
// registered callback is run when an ack is received for the same message_guid.
class ResponseCallbackHelper {
 public:
  using SharingResponseMessage = chrome_browser_sharing::ResponseMessage;
  using ResponseCallback =
      base::OnceCallback<void(SharingSendMessageResult,
                              std::unique_ptr<SharingResponseMessage>)>;

  ResponseCallbackHelper();
  virtual ~ResponseCallbackHelper();

  // Registers a |callback| to be run when a response is received for
  // |message_guid|.
  void RegisterCallback(const std::string& message_guid,
                        ResponseCallback callback);

  // Runs the registered |callback| for |message_guid|. Multiple calls for the
  // same guid or calling RunCallback without registering a callback results in
  // noops.
  void RunCallback(const std::string& message_guid,
                   chrome_browser_sharing::MessageType message_type,
                   SharingSendMessageResult result,
                   std::unique_ptr<SharingResponseMessage> response);

  // Called after FCM has sent the message successfully or if an error was
  // encountered while sending the message.
  void OnFCMMessageSent(base::TimeTicks start_time,
                        const std::string& message_guid,
                        chrome_browser_sharing::MessageType message_type,
                        SharingSendMessageResult result,
                        base::Optional<std::string> fcm_message_id);

  // Called when an AckMessage is received from FCM.
  virtual void OnFCMAckReceived(
      chrome_browser_sharing::MessageType message_type,
      const std::string& fcm_message_id,
      std::unique_ptr<SharingResponseMessage> response);

 private:
  // Maps FCM id to random message GUID.
  std::map<std::string, std::string> message_guids_;
  // Maps GUID to the message sending time.
  std::map<std::string, base::TimeTicks> message_sent_time_;
  // Maps GUID to a callback that is run when a response is received.
  std::map<std::string, ResponseCallback> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(ResponseCallbackHelper);
};

#endif  // CHROME_BROWSER_SHARING_RESPONSE_CALLBACK_HELPER_H_

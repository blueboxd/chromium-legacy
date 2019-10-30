// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_ACK_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_SHARING_ACK_MESSAGE_HANDLER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chrome/browser/sharing/response_callback_helper.h"
#include "chrome/browser/sharing/sharing_message_handler.h"

// Class to managae ack message and notify observers.
class AckMessageHandler : public SharingMessageHandler {
 public:
  explicit AckMessageHandler(ResponseCallbackHelper* response_callback_helper);
  ~AckMessageHandler() override;

  // SharingMessageHandler implementation:
  void OnMessage(chrome_browser_sharing::SharingMessage message,
                 SharingMessageHandler::DoneCallback done_callback) override;

 private:
  ResponseCallbackHelper* response_callback_helper_;

  DISALLOW_COPY_AND_ASSIGN(AckMessageHandler);
};

#endif  // CHROME_BROWSER_SHARING_ACK_MESSAGE_HANDLER_H_

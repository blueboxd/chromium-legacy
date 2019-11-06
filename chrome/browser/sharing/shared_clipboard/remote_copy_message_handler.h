// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_REMOTE_COPY_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_REMOTE_COPY_MESSAGE_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/sharing/sharing_message_handler.h"

class Profile;

// Handles incoming messages for the remote copy feature.
class RemoteCopyMessageHandler : public SharingMessageHandler {
 public:
  explicit RemoteCopyMessageHandler(Profile* profile);
  ~RemoteCopyMessageHandler() override;

  // SharingMessageHandler implementation:
  void OnMessage(chrome_browser_sharing::SharingMessage message,
                 DoneCallback done_callback) override;

 private:
  void ShowNotification(const std::string& device_name);

  Profile* profile_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(RemoteCopyMessageHandler);
};

#endif  // CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_REMOTE_COPY_MESSAGE_HANDLER_H_

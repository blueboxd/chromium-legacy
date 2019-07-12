// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_SHARING_SHARING_MESSAGE_HANDLER_H_

namespace chrome_browser_sharing {
class SharingMessage;
}  // namespace chrome_browser_sharing

// Interface for handling incoming SharingMessage.
class SharingMessageHandler {
 public:
  virtual ~SharingMessageHandler() = default;

  // Called when a SharingMessage has been received.
  virtual void OnMessage(
      const chrome_browser_sharing::SharingMessage& message) = 0;
};

#endif  // CHROME_BROWSER_SHARING_SHARING_MESSAGE_HANDLER_H_

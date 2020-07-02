// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_WEBRTC_INCOMING_MESSAGES_LISTENER_H_
#define CHROME_SERVICES_SHARING_NEARBY_WEBRTC_INCOMING_MESSAGES_LISTENER_H_

#include <string>

#include "base/callback.h"
#include "chrome/services/sharing/public/mojom/webrtc_signaling_messenger.mojom.h"

namespace sharing {

class IncomingMessagesListener : public mojom::IncomingMessagesListener {
 public:
  explicit IncomingMessagesListener(
      base::RepeatingCallback<void(const std::string& message)> listener);
  ~IncomingMessagesListener() override;

  // mojom::IncomingMessagesListener:
  void OnMessage(const std::string& message) override;

 private:
  base::RepeatingCallback<void(const std::string& message)> listener_;
};

}  // namespace sharing

#endif  // CHROME_SERVICES_SHARING_NEARBY_WEBRTC_INCOMING_MESSAGES_LISTENER_H_
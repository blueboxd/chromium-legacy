// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_ACK_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_SHARING_ACK_MESSAGE_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/sharing/sharing_message_handler.h"

// Class to managae ack message and notify observers.
class AckMessageHandler : public SharingMessageHandler {
 public:
  // Interface for objects observing ack message received events.
  class AckMessageObserver : public base::CheckedObserver {
   public:
    // Called when an ack message is received, where the identifier of original
    // message is |message_id|.
    virtual void OnAckReceived(const std::string& message_id) = 0;
  };

  AckMessageHandler();
  ~AckMessageHandler() override;

  // Add an observer ack message received events. An observer should not be
  // added more than once.
  void AddObserver(AckMessageObserver* observer);

  // Removes the given observer from ack message received events. Does nothing
  // if this observer has not been added.
  void RemoveObserver(AckMessageObserver* observer);

  // SharingMessageHandler implementation:
  void OnMessage(
      const chrome_browser_sharing::SharingMessage& message) override;

 private:
  base::ObserverList<AckMessageObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AckMessageHandler);
};

#endif  // CHROME_BROWSER_SHARING_ACK_MESSAGE_HANDLER_H_

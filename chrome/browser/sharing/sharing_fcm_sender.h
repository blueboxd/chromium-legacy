// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_FCM_SENDER_H_
#define CHROME_BROWSER_SHARING_SHARING_FCM_SENDER_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"

// Responsible for sending FCM messages within Sharing infrastructure.
class SharingFCMSender {
 public:
  SharingFCMSender();
  virtual ~SharingFCMSender();

  // Sends a |message| to device identified by |device_guid|.
  virtual bool SendMessage(
      const std::string& device_guid,
      const chrome_browser_sharing::SharingMessage& message);

 private:
  DISALLOW_COPY_AND_ASSIGN(SharingFCMSender);
};

#endif  // CHROME_BROWSER_SHARING_SHARING_FCM_SENDER_H_

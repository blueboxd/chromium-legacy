// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_METRICS_H_
#define CHROME_BROWSER_SHARING_SHARING_METRICS_H_

#include "chrome/browser/sharing/proto/sharing_message.pb.h"

// Logs the |payload_case| to UMA. This should be called when a SharingMessage
// is received.
void LogSharingMessageReceived(
    chrome_browser_sharing::SharingMessage::PayloadCase payload_case);

// Logs the number of available devices that are about to be shown in a UI for
// picking a device to start a phone call on.
void LogClickToCallDevicesToShow(int count);

// Logs the number of available apps that are about to be shown in a UI for
// picking an app to start a phone call with.
void LogClickToCallAppsToShow(int count);

#endif  // CHROME_BROWSER_SHARING_SHARING_METRICS_H_

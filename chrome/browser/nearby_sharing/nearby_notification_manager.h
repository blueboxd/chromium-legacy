// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_NOTIFICATION_MANAGER_H_

#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/share_target.h"
#include "chrome/browser/nearby_sharing/transfer_metadata.h"

class Profile;

// Manages notifications shown for Nearby Share. Only a single notification will
// be shown as simultaneous connections are not supported. All methods should be
// called from the UI thread.
class NearbyNotificationManager {
 public:
  explicit NearbyNotificationManager(Profile* profile);
  ~NearbyNotificationManager();

  void ShowProgress(const ShareTarget& share_target,
                    const TransferMetadata& transfer_metadata);

 private:
  Profile* profile_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_NOTIFICATION_MANAGER_H_

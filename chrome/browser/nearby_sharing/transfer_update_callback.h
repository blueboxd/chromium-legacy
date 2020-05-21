// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_TRANSFER_UPDATE_CALLBACK_H_
#define CHROME_BROWSER_NEARBY_SHARING_TRANSFER_UPDATE_CALLBACK_H_

#include "base/observer_list_types.h"
#include "chrome/browser/nearby_sharing/share_target.h"
#include "chrome/browser/nearby_sharing/transfer_metadata.h"

// Reports the transfer status for an ongoing transfer with a |ShareTarget|.
class TransferUpdateCallback : public base::CheckedObserver {
 public:
  virtual void OnTransferUpdate(ShareTarget shareTarget,
                                TransferMetadata transferMetadata) = 0;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_TRANSFER_UPDATE_CALLBACK_H_

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_TRANSFER_METADATA_H_
#define CHROME_BROWSER_NEARBY_SHARING_TRANSFER_METADATA_H_

#include <string>

#include "base/optional.h"
#include "url/gurl.h"

// Metadata about an ongoing transfer. Wraps transient data like status and
// progress.
class TransferMetadata {
 public:
  enum class Status {
    kUnknown,
    kConnecting,
    kAwaitingLocalConfirmation,
    kAwaitingRemoteAcceptance,
    kAwaitingRemoteAcceptanceFailed,
    kInProgress,
    kComplete,
    kFailed,
    kRejected,
    kCancelled,
    kTimedOut,
    kMediaUnavailable,
    kMediaDownloading,
    kNotEnoughSpace,
    kUnsupportedAttachmentType,
    kExternalProviderLaunched,
    kMaxValue = kExternalProviderLaunched
  };

  TransferMetadata(Status status,
                   float progress,
                   base::Optional<std::string> token,
                   bool is_original,
                   bool is_final_status);
  ~TransferMetadata();

  Status status() { return status_; }
  float progress() { return progress_; }

  // Represents the UKey2 token from Nearby Connection. base::nullopt if no
  // UKey2 comparison is needed for this transfer.
  const base::Optional<std::string>& token() { return token_; }

  // True if this |TransferMetadata| has not been seen.
  bool is_original() { return is_original_; }

  // True if this |TransferMetadata| is the last status for this transfer.
  bool is_final_status() { return is_final_status_; }

 private:
  Status status_;
  float progress_;
  base::Optional<std::string> token_;
  bool is_original_;
  bool is_final_status_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_TRANSFER_METADATA_H_

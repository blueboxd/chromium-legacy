// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_TRANSFER_METADATA_BUILDER_H_
#define CHROME_BROWSER_NEARBY_SHARING_TRANSFER_METADATA_BUILDER_H_

#include <string>

#include "base/optional.h"
#include "chrome/browser/nearby_sharing/transfer_metadata.h"

class TransferMetadataBuilder {
 public:
  TransferMetadataBuilder();
  ~TransferMetadataBuilder();

  TransferMetadataBuilder& set_is_final_status(bool is_final_status);

  TransferMetadataBuilder& set_progress(double progress);

  TransferMetadataBuilder& set_status(TransferMetadata::Status status);

  TransferMetadataBuilder& set_token(base::Optional<std::string> token);

  TransferMetadata build() const;

 private:
  bool is_final_status_ = false;
  double progress_ = 0;
  TransferMetadata::Status status_ = TransferMetadata::Status::kInProgress;
  base::Optional<std::string> token_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_TRANSFER_METADATA_BUILDER_H_

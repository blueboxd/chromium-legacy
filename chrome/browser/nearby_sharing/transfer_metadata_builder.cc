// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/transfer_metadata_builder.h"

TransferMetadataBuilder::TransferMetadataBuilder() = default;

TransferMetadataBuilder::~TransferMetadataBuilder() = default;

TransferMetadataBuilder& TransferMetadataBuilder::set_is_final_status(
    bool is_final_status) {
  is_final_status_ = is_final_status;
  return *this;
}

TransferMetadataBuilder& TransferMetadataBuilder::set_progress(
    double progress) {
  progress_ = progress;
  return *this;
}

TransferMetadataBuilder& TransferMetadataBuilder::set_status(
    TransferMetadata::Status status) {
  status_ = status;
  return *this;
}

TransferMetadataBuilder& TransferMetadataBuilder::set_token(
    base::Optional<std::string> token) {
  token_ = std::move(token);
  return *this;
}

TransferMetadata TransferMetadataBuilder::build() const {
  return TransferMetadata(status_, progress_, token_,
                          /*is_original=*/false, is_final_status_);
}

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/transfer_metadata.h"

TransferMetadata::TransferMetadata(Status status,
                                   float progress,
                                   base::Optional<std::string> token,
                                   bool is_original,
                                   bool is_final_status)
    : status_(status),
      progress_(progress),
      token_(std::move(token)),
      is_original_(is_original),
      is_final_status_(is_final_status) {}

TransferMetadata::~TransferMetadata() = default;

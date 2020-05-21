// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/share_target.h"

#include "base/atomic_sequence_num.h"

namespace {

// The last ID given to the previously created ShareTarget.
base::AtomicSequenceNumber g_id_generator;

}  // namespace

ShareTarget::ShareTarget(std::string device_name,
                         GURL image_url,
                         Type type,
                         std::vector<TextAttachment> text_attachments,
                         std::vector<FileAttachment> file_attachments,
                         bool is_incoming,
                         base::Optional<std::string> full_name,
                         bool is_known)
    : id_(g_id_generator.GetNext()),
      device_name_(std::move(device_name)),
      image_url_(std::move(image_url)),
      type_(type),
      text_attachments_(std::move(text_attachments)),
      file_attachments_(std::move(file_attachments)),
      is_incoming_(is_incoming),
      full_name_(std::move(full_name)),
      is_known_(is_known) {}

ShareTarget::~ShareTarget() = default;

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/share_target_builder.h"

ShareTargetBuilder::ShareTargetBuilder() = default;

ShareTargetBuilder::~ShareTargetBuilder() = default;

ShareTargetBuilder& ShareTargetBuilder::set_device_name(
    const std::string& device_name) {
  device_name_ = device_name;
  return *this;
}

ShareTargetBuilder& ShareTargetBuilder::set_is_incoming(bool is_incoming) {
  is_incoming_ = is_incoming;
  return *this;
}

ShareTargetBuilder& ShareTargetBuilder::add_attachment(
    TextAttachment attachment) {
  text_attachments_.push_back(std::move(attachment));
  return *this;
}

ShareTargetBuilder& ShareTargetBuilder::add_attachment(
    FileAttachment attachment) {
  file_attachments_.push_back(std::move(attachment));
  return *this;
}

ShareTarget ShareTargetBuilder::build() const {
  return ShareTarget(device_name_,
                     /*image_url=*/GURL(), ShareTarget::Type::kPhone,
                     text_attachments_, file_attachments_, is_incoming_,
                     /*full_name=*/base::nullopt,
                     /*is_known=*/false);
}

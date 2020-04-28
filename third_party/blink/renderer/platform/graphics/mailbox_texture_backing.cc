// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/mailbox_texture_backing.h"

namespace blink {

MailboxTextureBacking::MailboxTextureBacking(sk_sp<SkImage> sk_image)
    : sk_image_(std::move(sk_image)) {}

const SkImageInfo& MailboxTextureBacking::GetSkImageInfo() {
  return sk_image_->imageInfo();
}

gpu::Mailbox MailboxTextureBacking::GetMailbox() const {
  return mailbox_;
}

sk_sp<SkImage> MailboxTextureBacking::GetAcceleratedSkImage() {
  return sk_image_;
}

}  // namespace blink

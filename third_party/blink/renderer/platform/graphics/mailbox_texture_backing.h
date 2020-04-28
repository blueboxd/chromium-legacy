// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_image.h"

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_MAILBOX_TEXTURE_BACKING_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_MAILBOX_TEXTURE_BACKING_H_

#include "gpu/command_buffer/common/mailbox.h"

namespace blink {

class MailboxTextureBacking : public TextureBacking {
 public:
  explicit MailboxTextureBacking(sk_sp<SkImage> sk_image);
  const SkImageInfo& GetSkImageInfo() override;
  gpu::Mailbox GetMailbox() const override;
  sk_sp<SkImage> GetAcceleratedSkImage() override;

 private:
  const sk_sp<SkImage> sk_image_;
  const gpu::Mailbox mailbox_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_MAILBOX_TEXTURE_BACKING_H__

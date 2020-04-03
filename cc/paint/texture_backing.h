// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_TEXTURE_BACKING_H_
#define CC_PAINT_TEXTURE_BACKING_H_

#include "cc/paint/paint_export.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace gpu {
class Mailbox;
}  // namespace gpu

namespace cc {

// Used for storing mailboxes in a PaintImage.
// This class must be created, used and destroyed on the same thread.
class CC_PAINT_EXPORT TextureBacking : public SkRefCnt {
 public:
  TextureBacking(const TextureBacking&) = delete;
  ~TextureBacking() override = default;

  TextureBacking& operator=(const TextureBacking&) = delete;

  // Returns the metadata for the associated texture.
  virtual const SkImageInfo& GetSkImageInfo() = 0;

  // Returns the shared image mailbox backing for this texture.
  virtual gpu::Mailbox GetMailbox() const = 0;

  // Returns a texture backed SkImage wrapping the mailbox. Only supported if
  // the context used to create this image has a valid GrContext.
  virtual sk_sp<SkImage> GetAcceleratedSkImage() = 0;
};

}  // namespace cc

#endif  // CC_PAINT_TEXTURE_BACKING_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_CONTENT_SIZE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_CONTENT_SIZE_H_

#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ContentSize {
  DISALLOW_NEW();

 public:
  ContentSize() : width_(Length::kMaxSizeNone), height_(Length::kMaxSizeNone) {}
  ContentSize(const Length& width, const Length& height)
      : width_(width), height_(height) {}

  bool IsNone() const { return width_.IsMaxSizeNone(); }
  const Length& GetWidth() const {
    DCHECK(!width_.IsMaxSizeNone());
    return width_;
  }

  const Length& GetHeight() const {
    DCHECK(!height_.IsMaxSizeNone());
    return height_;
  }

  bool operator==(const ContentSize& other) const {
    return width_ == other.width_ && height_ == other.height_;
  }

  bool operator!=(const ContentSize& other) const { return !(*this == other); }

 private:
  Length width_;
  Length height_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_CONTENT_SIZE_H_

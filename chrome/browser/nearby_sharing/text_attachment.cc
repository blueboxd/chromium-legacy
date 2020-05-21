// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/text_attachment.h"

TextAttachment::TextAttachment(std::string text_body, Type type, int64_t size)
    : text_body_(std::move(text_body)), type_(type), size_(size) {}

TextAttachment::~TextAttachment() = default;

int64_t TextAttachment::size() const {
  return size_;
}

Attachment::Family TextAttachment::family() const {
  return Attachment::Family::kText;
}

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/strings/strcat.h"
#include "chrome/browser/nearby_sharing/text_attachment.h"

namespace {

constexpr size_t kMaxPreviewTextLength = 32;

std::string GetTextTitle(const std::string& text_body,
                         TextAttachment::Type type) {
  // TODO(crbug.com/1085067): Improve title based on |type|.
  if (text_body.size() > kMaxPreviewTextLength)
    return base::StrCat({text_body.substr(0, kMaxPreviewTextLength), "…"});

  return text_body;
}

}  // namespace

TextAttachment::TextAttachment(Type type, std::string text_body)
    : Attachment(Attachment::Family::kText, text_body.size()),
      type_(type),
      text_title_(GetTextTitle(text_body, type)),
      text_body_(std::move(text_body)) {}

TextAttachment::TextAttachment(int64_t id,
                               Type type,
                               std::string text_title,
                               int64_t size)
    : Attachment(id, Attachment::Family::kText, size),
      type_(type),
      text_title_(std::move(text_title)) {}

TextAttachment::TextAttachment(const TextAttachment&) = default;

TextAttachment& TextAttachment::operator=(const TextAttachment&) = default;

TextAttachment::~TextAttachment() = default;

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_TEXT_ATTACHMENT_H_
#define CHROME_BROWSER_NEARBY_SHARING_TEXT_ATTACHMENT_H_

#include <string>

#include "base/optional.h"
#include "chrome/browser/nearby_sharing/attachment.h"

// Represents a text attachment.
class TextAttachment : public Attachment {
 public:
  // Different types are used to offer richer experiences on Receiver side,
  // mainly for: 1. displaying notification of attachment types, 2. opening
  // different types with different apps. Remember to update Notifications,
  // ShareTarget, etc once more types are introduced here.
  enum class Type {
    kText,
    kUrl,
    kAddress,
    kPhoneNumber,
    kMaxValue = kPhoneNumber
  };

  TextAttachment(std::string text_body, Type type, int64_t size);
  ~TextAttachment() override;

  // Attachment:
  int64_t size() const override;
  Attachment::Family family() const override;

  const std::string& text_body() const { return text_body_; }
  Type type() const { return type_; }

 private:
  std::string text_body_;
  Type type_;
  int64_t size_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_TEXT_ATTACHMENT_H_

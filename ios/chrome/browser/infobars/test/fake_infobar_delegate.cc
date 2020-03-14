// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/test/fake_infobar_delegate.h"

#include "base/strings/utf_string_conversions.h"

FakeInfobarDelegate::FakeInfobarDelegate()
    : identifier_(infobars::InfoBarDelegate::InfoBarIdentifier::TEST_INFOBAR),
      message_text_(base::ASCIIToUTF16("FakeInfobarDelegate")) {}

FakeInfobarDelegate::FakeInfobarDelegate(
    infobars::InfoBarDelegate::InfoBarIdentifier identifier)
    : identifier_(identifier),
      message_text_(base::ASCIIToUTF16("FakeInfobarDelegate")) {}

FakeInfobarDelegate::FakeInfobarDelegate(base::string16 message_text)
    : identifier_(infobars::InfoBarDelegate::InfoBarIdentifier::TEST_INFOBAR),
      message_text_(message_text) {}

FakeInfobarDelegate::~FakeInfobarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
FakeInfobarDelegate::GetIdentifier() const {
  return identifier_;
}

// Returns the message string to be displayed for the Infobar.
base::string16 FakeInfobarDelegate::GetMessageText() const {
  return message_text_;
}

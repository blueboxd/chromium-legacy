// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/test/fake_infobar_delegate.h"

FakeInfobarDelegate::FakeInfobarDelegate() = default;

FakeInfobarDelegate::~FakeInfobarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
FakeInfobarDelegate::GetIdentifier() const {
  return TEST_INFOBAR;
}

bool FakeInfobarDelegate::EqualsDelegate(
    infobars::InfoBarDelegate* delegate) const {
  return false;
}

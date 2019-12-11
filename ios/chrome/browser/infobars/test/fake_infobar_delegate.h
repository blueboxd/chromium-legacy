// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_TEST_FAKE_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_INFOBARS_TEST_FAKE_INFOBAR_DELEGATE_H_

#include "components/infobars/core/infobar_delegate.h"

// Fake version of InfoBarDelegate.
class FakeInfobarDelegate : public infobars::InfoBarDelegate {
 public:
  FakeInfobarDelegate();
  ~FakeInfobarDelegate() override;

  // Returns InfoBarIdentifier::TEST_INFOBAR.
  InfoBarIdentifier GetIdentifier() const override;
  // Returns false by default.
  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_TEST_FAKE_INFOBAR_DELEGATE_H_

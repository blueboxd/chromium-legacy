// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/test/fake_infobar_ios.h"

#include "base/logging.h"
#include "ios/chrome/browser/infobars/test/fake_infobar_delegate.h"
#import "ios/chrome/browser/ui/infobars/test/fake_infobar_ui_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FakeInfobarIOS::FakeInfobarIOS(base::string16 message_text)
    : InfoBarIOS([[FakeInfobarUIDelegate alloc] init],
                 std::make_unique<FakeInfobarDelegate>(message_text)),
      fake_ui_delegate_(
          static_cast<FakeInfobarUIDelegate*>(InfobarUIDelegate())),
      fake_delegate_(static_cast<FakeInfobarDelegate*>(delegate())) {
  DCHECK([fake_ui_delegate_ isKindOfClass:[FakeInfobarUIDelegate class]]);
  DCHECK(fake_delegate_);
}

FakeInfobarIOS::FakeInfobarIOS(
    std::unique_ptr<FakeInfobarDelegate> fake_delegate)
    : InfoBarIOS([[FakeInfobarUIDelegate alloc] init],
                 std::move(fake_delegate)),
      fake_ui_delegate_(
          static_cast<FakeInfobarUIDelegate*>(InfobarUIDelegate())),
      fake_delegate_(static_cast<FakeInfobarDelegate*>(delegate())) {
  DCHECK([fake_ui_delegate_ isKindOfClass:[FakeInfobarUIDelegate class]]);
  DCHECK(fake_delegate_);
}

FakeInfobarIOS::~FakeInfobarIOS() = default;

// static
std::unique_ptr<FakeInfobarIOS> FakeInfobarIOS::Create(
    InfobarType type,
    bool has_badge,
    base::string16 message_text) {
  std::unique_ptr<FakeInfobarIOS> infobar =
      std::make_unique<FakeInfobarIOS>(message_text);
  infobar->fake_ui_delegate().infobarType = type;
  infobar->fake_ui_delegate().hasBadge = has_badge;
  return infobar;
}

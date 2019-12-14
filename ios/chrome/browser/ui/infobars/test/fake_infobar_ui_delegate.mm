// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/test/fake_infobar_ui_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FakeInfobarUIDelegate

// Synthesize InfobarUIDelegate properties.
@synthesize delegate = _delegate;
@synthesize presented = _presented;
@synthesize hasBadge = _hasBadge;
@synthesize infobarType = _infobarType;
@synthesize view = _view;

- (void)removeView {
}

- (void)detachView {
}

@end

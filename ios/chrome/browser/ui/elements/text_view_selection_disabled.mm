// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/elements/text_view_selection_disabled.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TextViewSelectionDisabled

- (BOOL)canBecomeFirstResponder {
  if (@available(iOS 14.0, *))
    return NO;

  // On iOS 13, the whole string responds to a tap if
  // canBecomeFirstResponder returns NO.
  return YES;
}

@end

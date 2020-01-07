// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "OpenInChromeIntent.h"

#if __has_include(<Intents/Intents.h>) && (!TARGET_OS_OSX || TARGET_OS_IOSMAC) && !TARGET_OS_TV

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation OpenInChromeIntent

@dynamic url;

@end

@interface OpenInChromeIntentResponse ()

@property(readwrite, NS_NONATOMIC_IOSONLY) OpenInChromeIntentResponseCode code;

@end

@implementation OpenInChromeIntentResponse

@synthesize code = _code;

- (instancetype)initWithCode:(OpenInChromeIntentResponseCode)code
                userActivity:(nullable NSUserActivity*)userActivity {
  self = [super init];
  if (self) {
    _code = code;
    self.userActivity = userActivity;
  }
  return self;
}

@end

#endif

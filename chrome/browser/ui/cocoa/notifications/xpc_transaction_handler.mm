// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#import "chrome/browser/ui/cocoa/notifications/xpc_transaction_handler.h"

@class NSUserNotificationCenter;

@implementation XPCTransactionHandler {
  bool _transactionOpen;
}

- (instancetype)init {
  if ((self = [super init])) {
    _transactionOpen = false;
  }
  return self;
}

- (void)openTransactionIfNeeded {
  @synchronized(self) {
    if (_transactionOpen) {
      return;
    }
    xpc_transaction_begin();
    _transactionOpen = true;
  }
}

- (void)closeTransactionIfNeeded {
  @synchronized(self) {
    NSUserNotificationCenter* notificationCenter =
        [NSUserNotificationCenter defaultUserNotificationCenter];
    NSUInteger showing = [[notificationCenter deliveredNotifications] count];
    if (showing == 0 && _transactionOpen) {
      xpc_transaction_end();
      _transactionOpen = false;
    }
  }
}
@end

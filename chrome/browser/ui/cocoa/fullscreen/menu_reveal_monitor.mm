// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/fullscreen/menu_reveal_monitor.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_block.h"
#include "base/mac/scoped_nsobject.h"

static NSString* const kRevealAmountKeyPath = @"revealAmount";

@implementation MenuRevealMonitor {
  base::mac::ScopedBlock<void (^)(double)> _change_handler;
  base::scoped_nsobject<NSTitlebarAccessoryViewController> _accVC;
}

- (instancetype)initWithWindow:(NSWindow*)window
                 changeHandler:(void (^)(double))handler {
  if ((self = [super init])) {
    _change_handler.reset([handler copy]);
    _accVC.reset([[NSTitlebarAccessoryViewController alloc] init]);
    auto* accVC = _accVC.get();
    accVC.view = [[[NSView alloc] initWithFrame:NSZeroRect] autorelease];
    [accVC addObserver:self
            forKeyPath:kRevealAmountKeyPath
               options:NSKeyValueObservingOptionNew
               context:nil];
    [window addTitlebarAccessoryViewController:accVC];
  }
  return self;
}

- (void)dealloc {
  [_accVC removeObserver:self forKeyPath:kRevealAmountKeyPath];
  [_accVC removeFromParentViewController];
  [super dealloc];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id>*)change
                       context:(void*)context {
  double revealAmount =
      base::mac::ObjCCastStrict<NSNumber>(change[NSKeyValueChangeNewKey])
          .doubleValue;
  _change_handler.get()(revealAmount);
}
@end

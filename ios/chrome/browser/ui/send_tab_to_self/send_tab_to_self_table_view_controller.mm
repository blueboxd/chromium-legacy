// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_table_view_controller.h"

#include "base/logging.h"
#include "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierDevicesToSend = kSectionIdentifierEnumZero,
};

@implementation SendTabToSelfTableViewController

- (instancetype)init {
  return [super initWithTableViewStyle:UITableViewStylePlain
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
}

#pragma mark - ViewController Lifecycle

- (void)viewDidLoad {
  NOTIMPLEMENTED();
}

@end

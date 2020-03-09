// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_site_security_view_controller.h"

#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PageInfoSiteSecurityViewController ()

@property(nonatomic, strong)
    PageInfoSiteSecurityDescription* pageInfoSecurityDescription;

@end

@implementation PageInfoSiteSecurityViewController

- (instancetype)initWitDescription:
    (PageInfoSiteSecurityDescription*)description {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _pageInfoSecurityDescription = description;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  // TODO(crbug.com/1038919): Implement this.
  [super viewDidLoad];

  self.title = l10n_util::GetNSString(IDS_IOS_PAGE_INFO_SITE_SECURITY);
}

@end

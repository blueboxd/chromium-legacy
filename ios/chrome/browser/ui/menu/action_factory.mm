// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/menu/action_factory.h"

#import "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/ui/util/pasteboard_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ActionFactory ()

@property(nonatomic, assign) const char* histogram;

@end

@implementation ActionFactory

- (instancetype)initWithHistogram:(const char*)histogram {
  if (self = [super init]) {
    _histogram = histogram;
  }
  return self;
}

- (UIAction*)actionWithTitle:(NSString*)title
                       image:(UIImage*)image
                        type:(MenuActionType)type
                       block:(ProceduralBlock)block {
  // Capture only the histogram name's pointer to be copied by the block.
  const char* histogram = self.histogram;
  return [UIAction actionWithTitle:title
                             image:image
                        identifier:nil
                           handler:^(UIAction* action) {
                             UMA_HISTOGRAM_ENUMERATION(histogram, type);
                             if (block) {
                               block();
                             }
                           }];
}

- (UIAction*)actionToCopyURL:(const GURL)URL {
  return [self actionWithTitle:l10n_util::GetNSString(IDS_IOS_COPY_ACTION_TITLE)
                         image:[UIImage systemImageNamed:@"doc.on.doc"]
                          type:MenuActionType::Copy
                         block:^{
                           StoreURLInPasteboard(URL);
                         }];
}

@end

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/data/chrome_activity_url_source.h"

#import <MobileCoreServices/MobileCoreServices.h>

#include "base/logging.h"
#import "ios/chrome/browser/ui/activity_services/data/chrome_activity_item_thumbnail_generator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ChromeActivityURLSource () {
  NSString* _subject;
  ChromeActivityItemThumbnailGenerator* _thumbnailGenerator;
}

// URL to be shared with share extensions.
@property(nonatomic, strong) NSURL* shareURL;

@end

@implementation ChromeActivityURLSource

- (instancetype)initWithShareURL:(NSURL*)shareURL
                         subject:(NSString*)subject
              thumbnailGenerator:
                  (ChromeActivityItemThumbnailGenerator*)thumbnailGenerator {
  DCHECK(shareURL);
  DCHECK(subject);
  DCHECK(thumbnailGenerator);
  self = [super init];
  if (self) {
    _shareURL = shareURL;
    _subject = [subject copy];
    _thumbnailGenerator = thumbnailGenerator;
  }
  return self;
}

#pragma mark - ChromeActivityItemSource

- (NSSet*)excludedActivityTypes {
  return [NSSet setWithArray:@[
    UIActivityTypeAddToReadingList, UIActivityTypeCopyToPasteboard,
    UIActivityTypePrint, UIActivityTypeSaveToCameraRoll
  ]];
}

#pragma mark - UIActivityItemSource

- (id)activityViewControllerPlaceholderItem:
    (UIActivityViewController*)activityViewController {
  // Return the current URL as a placeholder
  return self.shareURL;
}

- (NSString*)activityViewController:
                 (UIActivityViewController*)activityViewController
             subjectForActivityType:(NSString*)activityType {
  return _subject;
}

- (id)activityViewController:(UIActivityViewController*)activityViewController
         itemForActivityType:(NSString*)activityType {
  return self.shareURL;
}

- (NSString*)activityViewController:
                 (UIActivityViewController*)activityViewController
    dataTypeIdentifierForActivityType:(NSString*)activityType {
  return (NSString*)kUTTypeURL;
}

- (UIImage*)activityViewController:
                (UIActivityViewController*)activityViewController
     thumbnailImageForActivityType:(UIActivityType)activityType
                     suggestedSize:(CGSize)size {
  return [_thumbnailGenerator thumbnailWithSize:size];
}

@end

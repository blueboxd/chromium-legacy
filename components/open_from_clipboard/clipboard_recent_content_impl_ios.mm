// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/open_from_clipboard/clipboard_recent_content_impl_ios.h"

#import <MobileCoreServices/MobileCoreServices.h>
#import <UIKit/UIKit.h>

#import "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/system/sys_info.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Key used to store the pasteboard's current change count. If when resuming
// chrome the pasteboard's change count is different from the stored one, then
// it means that the pasteboard's content has changed.
NSString* const kPasteboardChangeCountKey = @"PasteboardChangeCount";
// Key used to store the last date at which it was detected that the pasteboard
// changed. It is used to evaluate the age of the pasteboard's content.
NSString* const kPasteboardChangeDateKey = @"PasteboardChangeDate";

}  // namespace

@interface ClipboardRecentContentImplIOS ()

// The user defaults from the app group used to optimize the pasteboard change
// detection.
@property(nonatomic, strong) NSUserDefaults* sharedUserDefaults;
// The pasteboard's change count. Increases everytime the pasteboard changes.
@property(nonatomic) NSInteger lastPasteboardChangeCount;
// Contains the authorized schemes for URLs.
@property(nonatomic, readonly) NSSet* authorizedSchemes;
// Delegate for metrics.
@property(nonatomic, strong) id<ClipboardRecentContentDelegate> delegate;
// Maximum age of clipboard in seconds.
@property(nonatomic, readonly) NSTimeInterval maximumAgeOfClipboard;

// If the content of the pasteboard has changed, updates the change count
// and change date.
- (void)updateIfNeeded;

// Returns whether the pasteboard changed since the last time a pasteboard
// change was detected.
- (BOOL)hasPasteboardChanged;

// Loads information from the user defaults about the latest pasteboard entry.
- (void)loadFromUserDefaults;

// Returns the URL contained in the clipboard (if any).
- (NSURL*)URLFromPasteboard;

// Returns the uptime.
- (NSTimeInterval)uptime;

// Returns whether the value of the clipboard should be returned.
- (BOOL)shouldReturnValueOfClipboard;

@end

@implementation ClipboardRecentContentImplIOS

@synthesize lastPasteboardChangeCount = _lastPasteboardChangeCount;
@synthesize lastPasteboardChangeDate = _lastPasteboardChangeDate;
@synthesize sharedUserDefaults = _sharedUserDefaults;
@synthesize authorizedSchemes = _authorizedSchemes;
@synthesize delegate = _delegate;
@synthesize maximumAgeOfClipboard = _maximumAgeOfClipboard;

- (instancetype)initWithMaxAge:(NSTimeInterval)maxAge
             authorizedSchemes:(NSSet<NSString*>*)authorizedSchemes
                  userDefaults:(NSUserDefaults*)groupUserDefaults
                      delegate:(id<ClipboardRecentContentDelegate>)delegate {
  self = [super init];
  if (self) {
    _maximumAgeOfClipboard = maxAge;
    _delegate = delegate;
    _authorizedSchemes = authorizedSchemes;
    _sharedUserDefaults = groupUserDefaults;

    _lastPasteboardChangeCount = NSIntegerMax;
    [self loadFromUserDefaults];
    [self updateIfNeeded];

    // Makes sure |last_pasteboard_change_count_| was properly initialized.
    DCHECK_NE(_lastPasteboardChangeCount, NSIntegerMax);
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(didBecomeActive:)
               name:UIApplicationDidBecomeActiveNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)didBecomeActive:(NSNotification*)notification {
  [self loadFromUserDefaults];
  [self updateIfNeeded];
}

- (BOOL)hasPasteboardChanged {
  return UIPasteboard.generalPasteboard.changeCount !=
         self.lastPasteboardChangeCount;
}

- (NSURL*)recentURLFromClipboard {
  [self updateIfNeeded];

  if (![self shouldReturnValueOfClipboard])
    return nil;

  return [self URLFromPasteboard];
}

- (NSString*)recentTextFromClipboard {
  [self updateIfNeeded];

  if (![self shouldReturnValueOfClipboard])
    return nil;

  return [UIPasteboard generalPasteboard].string;
}

- (UIImage*)recentImageFromClipboard {
  [self updateIfNeeded];

  if (![self shouldReturnValueOfClipboard])
    return nil;

  return [UIPasteboard generalPasteboard].image;
}

- (NSTimeInterval)clipboardContentAge {
  return -[self.lastPasteboardChangeDate timeIntervalSinceNow];
}

- (BOOL)shouldReturnValueOfClipboard {
  if ([self clipboardContentAge] > self.maximumAgeOfClipboard)
    return NO;

  // It is the common convention on iOS that password managers tag confidential
  // data with the flavor "org.nspasteboard.ConcealedType". Obey this
  // convention; the user doesn't want for their confidential data to be
  // suggested as a search, anyway. See http://nspasteboard.org/ for more info.
  NSArray<NSString*>* types =
      [[UIPasteboard generalPasteboard] pasteboardTypes];
  if ([types containsObject:@"org.nspasteboard.ConcealedType"])
    return NO;

  return YES;
}

- (void)suppressClipboardContent {
  // User cleared the user data. The pasteboard entry must be removed from the
  // omnibox list. Force entry expiration by setting copy date to 1970.
  self.lastPasteboardChangeDate =
      [[NSDate alloc] initWithTimeIntervalSince1970:0];
  [self saveToUserDefaults];
}

- (void)updateIfNeeded {
  if (![self hasPasteboardChanged]) {
    return;
  }

  [self.delegate onClipboardChanged];

  self.lastPasteboardChangeDate = [NSDate date];
  self.lastPasteboardChangeCount = [UIPasteboard generalPasteboard].changeCount;

  [self saveToUserDefaults];
}

- (NSURL*)URLFromPasteboard {
  NSURL* url = [UIPasteboard generalPasteboard].URL;
  // Usually, even if the user copies plaintext, if it looks like a URL, the URL
  // property is filled. Sometimes, this doesn't happen, for instance when the
  // pasteboard is sync'd from a Mac to the iOS simulator. In this case,
  // fallback and manually check whether the pasteboard contains a url-like
  // string.
  if (!url) {
    url = [NSURL URLWithString:UIPasteboard.generalPasteboard.string];
  }
  if (![self.authorizedSchemes containsObject:url.scheme]) {
    return nil;
  }
  return url;
}

- (void)loadFromUserDefaults {
  self.lastPasteboardChangeCount =
      [self.sharedUserDefaults integerForKey:kPasteboardChangeCountKey];
  self.lastPasteboardChangeDate = base::mac::ObjCCastStrict<NSDate>(
      [self.sharedUserDefaults objectForKey:kPasteboardChangeDateKey]);
}

- (void)saveToUserDefaults {
  [self.sharedUserDefaults setInteger:self.lastPasteboardChangeCount
                               forKey:kPasteboardChangeCountKey];
  [self.sharedUserDefaults setObject:self.lastPasteboardChangeDate
                              forKey:kPasteboardChangeDateKey];
}

- (NSTimeInterval)uptime {
  return base::SysInfo::Uptime().InSecondsF();
}

@end

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/open_from_clipboard/clipboard_recent_content_impl_ios.h"

#import <CommonCrypto/CommonDigest.h>
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
// Key used to store the hash of the content of the pasteboard. Whenever the
// hash changed, the pasteboard content is considered to have changed.
NSString* const kPasteboardEntryMD5Key = @"PasteboardEntryMD5";

// Compute a hash consisting of the first 4 bytes of the MD5 hash of |string|,
// |image_data|, and |url|. This value is used to detect pasteboard content
// change. Keeping only 4 bytes is a privacy requirement to introduce collision
// and allow deniability of having copied a given string, image, or url.
//
// |image_data| is passed in as NSData instead of UIImage because converting
// UIImage to NSData can be slow for large images and getting NSData directly
// from the pasteboard is quicker.
NSData* WeakMD5FromPasteboardData(NSString* string,
                                  NSData* image_data,
                                  NSURL* url) {
  CC_MD5_CTX ctx;
  CC_MD5_Init(&ctx);

  const std::string clipboard_string = base::SysNSStringToUTF8(string);
  const char* c_string = clipboard_string.c_str();
  CC_MD5_Update(&ctx, c_string, strlen(c_string));

  // This hash is used only to tell if the image has changed, so
  // limit the number of bytes to hash to prevent slowdown.
  NSUInteger bytes_to_hash = fmin([image_data length], 1000000);
  if (bytes_to_hash > 0) {
    CC_MD5_Update(&ctx, [image_data bytes], bytes_to_hash);
  }

  const std::string url_string = base::SysNSStringToUTF8([url absoluteString]);
  const char* url_c_string = url_string.c_str();
  CC_MD5_Update(&ctx, url_c_string, strlen(url_c_string));

  unsigned char hash[CC_MD5_DIGEST_LENGTH];
  CC_MD5_Final(hash, &ctx);

  NSData* data = [NSData dataWithBytes:hash length:4];
  return data;
}

}  // namespace

@interface ClipboardRecentContentImplIOS ()

// The user defaults from the app group used to optimize the pasteboard change
// detection.
@property(nonatomic, strong) NSUserDefaults* sharedUserDefaults;
// The pasteboard's change count. Increases everytime the pasteboard changes.
@property(nonatomic) NSInteger lastPasteboardChangeCount;
// MD5 hash of the last registered pasteboard entry.
@property(nonatomic, strong) NSData* lastPasteboardEntryMD5;
// Contains the authorized schemes for URLs.
@property(nonatomic, readonly) NSSet* authorizedSchemes;
// Delegate for metrics.
@property(nonatomic, strong) id<ClipboardRecentContentDelegate> delegate;
// Maximum age of clipboard in seconds.
@property(nonatomic, readonly) NSTimeInterval maximumAgeOfClipboard;

// If the content of the pasteboard has changed, updates the change count,
// change date, and md5 of the latest pasteboard entry if necessary.
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

@end

@implementation ClipboardRecentContentImplIOS

@synthesize lastPasteboardChangeCount = _lastPasteboardChangeCount;
@synthesize lastPasteboardChangeDate = _lastPasteboardChangeDate;
@synthesize lastPasteboardEntryMD5 = _lastPasteboardEntryMD5;
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

- (NSData*)getCurrentMD5 {
  NSString* pasteboardString = [UIPasteboard generalPasteboard].string;
  NSData* pasteboardImageData = [[UIPasteboard generalPasteboard]
      dataForPasteboardType:(NSString*)kUTTypeImage];
  NSURL* pasteboardURL = [UIPasteboard generalPasteboard].URL;
  NSData* md5 = WeakMD5FromPasteboardData(pasteboardString, pasteboardImageData,
                                          pasteboardURL);

  return md5;
}

- (BOOL)hasPasteboardChanged {
  // If |MD5Changed|, we know for sure there has been at least one pasteboard
  // copy since last time it was checked.
  // If the pasteboard content is still the same but the device was not
  // rebooted, the change count can be checked to see if it changed.
  // Note: due to a mismatch between the actual behavior and documentation, and
  // lack of consistency on different reboot scenarios, the change count cannot
  // be checked after a reboot.
  // See radar://21833556 for more information.
  BOOL deviceRebooted = [self clipboardContentAge] >= [self uptime];

  // On iOS 13, there is a bug where every time a UITextField is opened, the
  // changeCount increases by 2. Thus, if the difference in counts is even,
  // it is unknown whether there is a real change or the user just focused some
  // UITextFields.
  // See radar://7619972 or crbug.com/1058487 for more information.
  NSInteger changeCount = [UIPasteboard generalPasteboard].changeCount;
  BOOL changeCountChanged = changeCount != self.lastPasteboardChangeCount;
  if (@available(iOS 13, *)) {
    // No-op. This should be if !available(13), but that is not supported.
  } else {
    if (!deviceRebooted) {
      return changeCountChanged;
    }
  }

  // If there was no reboot, and the number hasn't changed, the pasteboard
  // definitely hasn't changed.
  if (!deviceRebooted && !changeCountChanged) {
    return NO;
  }

  // If there was no reboot and the size of the change is odd , the pasteboard
  // definitely has changed.
  BOOL changeCountIncreasedIsOdd =
      (changeCount - self.lastPasteboardChangeCount) % 2 != 0;
  if (!deviceRebooted && changeCountIncreasedIsOdd) {
    return YES;
  }

  // Otherwise, it is unknown whether or not there was a real change, so
  // fallback to looking at the MD5.
  self.lastPasteboardChangeCount = changeCount;
  BOOL md5Changed =
      ![[self getCurrentMD5] isEqualToData:self.lastPasteboardEntryMD5];
  return md5Changed;
}

- (NSURL*)recentURLFromClipboard {
  [self updateIfNeeded];
  if ([self clipboardContentAge] > self.maximumAgeOfClipboard) {
    return nil;
  }
  return [self URLFromPasteboard];
}

- (NSString*)recentTextFromClipboard {
  [self updateIfNeeded];
  if ([self clipboardContentAge] > self.maximumAgeOfClipboard) {
    return nil;
  }
  return [UIPasteboard generalPasteboard].string;
}

- (UIImage*)recentImageFromClipboard {
  [self updateIfNeeded];
  if ([self clipboardContentAge] > self.maximumAgeOfClipboard) {
    return nil;
  }
  return [UIPasteboard generalPasteboard].image;
}

- (NSTimeInterval)clipboardContentAge {
  return -[self.lastPasteboardChangeDate timeIntervalSinceNow];
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
  self.lastPasteboardEntryMD5 = [self getCurrentMD5];

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
  self.lastPasteboardEntryMD5 = base::mac::ObjCCastStrict<NSData>(
      [self.sharedUserDefaults objectForKey:kPasteboardEntryMD5Key]);
}

- (void)saveToUserDefaults {
  [self.sharedUserDefaults setInteger:self.lastPasteboardChangeCount
                               forKey:kPasteboardChangeCountKey];
  [self.sharedUserDefaults setObject:self.lastPasteboardChangeDate
                              forKey:kPasteboardChangeDateKey];
  [self.sharedUserDefaults setObject:self.lastPasteboardEntryMD5
                              forKey:kPasteboardEntryMD5Key];
}

- (NSTimeInterval)uptime {
  return base::SysInfo::Uptime().InSecondsF();
}

@end

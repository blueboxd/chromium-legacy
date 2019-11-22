// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/coordinators/infobar_translate_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_translate_modal_consumer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfobarTranslateMediator ()

// Delegate that holds the Translate Infobar information and actions.
@property(nonatomic, assign, readonly)
    translate::TranslateInfoBarDelegate* translateInfobarDelegate;

@end

@implementation InfobarTranslateMediator

- (instancetype)initWithInfoBarDelegate:
    (translate::TranslateInfoBarDelegate*)infobarDelegate {
  self = [super init];
  if (self) {
    DCHECK(infobarDelegate);
    _translateInfobarDelegate = infobarDelegate;
  }
  return self;
}

- (void)setModalConsumer:(id<InfobarTranslateModalConsumer>)modalConsumer {
  _modalConsumer = modalConsumer;

  NSDictionary* prefs = @{
    kSourceLanguagePrefKey : base::SysUTF16ToNSString(
        self.translateInfobarDelegate->original_language_name()),
    kTargetLanguagePrefKey : base::SysUTF16ToNSString(
        self.translateInfobarDelegate->target_language_name()),
    kShouldAlwaysTranslatePrefKey :
        @(self.translateInfobarDelegate->ShouldAlwaysTranslate()),
    kIsTranslatableLanguagePrefKey :
        @(self.translateInfobarDelegate->IsTranslatableLanguageByPrefs()),
    kIsSiteBlacklistedPrefKey :
        @(self.translateInfobarDelegate->IsSiteBlacklisted()),
  };
  [self.modalConsumer setupModalViewControllerWithPrefs:prefs];
}

@end

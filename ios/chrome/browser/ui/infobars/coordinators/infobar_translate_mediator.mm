// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/coordinators/infobar_translate_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_translate_language_selection_consumer.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_translate_modal_consumer.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const int kInvalidLanguageIndex = -1;
}  // namespace

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

- (void)setSourceLanguageSelectionConsumer:
    (id<InfobarTranslateLanguageSelectionConsumer>)
        sourceLanguageSelectionConsumer {
  _sourceLanguageSelectionConsumer = sourceLanguageSelectionConsumer;
  NSArray<TableViewTextItem*>* items =
      [self loadTranslateLanguageItemsForSelectingLanguage:YES];
  [self.sourceLanguageSelectionConsumer setTranslateLanguageItems:items];
}

- (void)setTargetLanguageSelectionConsumer:
    (id<InfobarTranslateLanguageSelectionConsumer>)
        targetLanguageSelectionConsumer {
  _targetLanguageSelectionConsumer = targetLanguageSelectionConsumer;
  NSArray<TableViewTextItem*>* items =
      [self loadTranslateLanguageItemsForSelectingLanguage:NO];
  [self.targetLanguageSelectionConsumer setTranslateLanguageItems:items];
}

- (NSArray<TableViewTextItem*>*)loadTranslateLanguageItemsForSelectingLanguage:
    (BOOL)sourceLanguage {
  int originalLanguageIndex = kInvalidLanguageIndex;
  int targetLanguageIndex = kInvalidLanguageIndex;
  NSMutableArray<TableViewTextItem*>* items = [NSMutableArray array];
  for (size_t i = 0; i < self.translateInfobarDelegate->num_languages(); ++i) {
    if (self.translateInfobarDelegate->language_code_at(i) ==
        self.translateInfobarDelegate->original_language_code()) {
      originalLanguageIndex = i;
      if (!sourceLanguage) {
        // Skip creating item for source language if selecting the target
        // language to prevent same language translation.
        continue;
      }
    }
    if (self.translateInfobarDelegate->language_code_at(i) ==
        self.translateInfobarDelegate->target_language_code()) {
      targetLanguageIndex = i;
      if (sourceLanguage) {
        // Skip creating item for target language if selecting the source
        // language to prevent same language translation.
        continue;
      }
    }

    // TODO(crbug.com/1014959): Add selected index property
    TableViewTextItem* item =
        [[TableViewTextItem alloc] initWithType:kItemTypeEnumZero];
    item.text = base::SysUTF16ToNSString(
        self.translateInfobarDelegate->language_name_at((int(i))));
    [items addObject:item];
  }
  DCHECK_GT(originalLanguageIndex, kInvalidLanguageIndex);
  DCHECK_GT(targetLanguageIndex, kInvalidLanguageIndex);

  return items;
}

@end

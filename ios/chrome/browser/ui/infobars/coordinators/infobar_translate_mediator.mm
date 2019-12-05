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

  BOOL currentStepBeforeTranslate =
      self.currentStep ==
      translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE;

  [self.modalConsumer
      setupModalViewControllerWithPrefs:
          [self createPrefDictionaryForSourceLanguage:
                    base::SysUTF16ToNSString(
                        self.translateInfobarDelegate->original_language_name())
                                       targetLanguage:
                                           base::SysUTF16ToNSString(
                                               self.translateInfobarDelegate
                                                   ->target_language_name())
                               translateButtonEnabled:
                                   currentStepBeforeTranslate]];
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
    if ((sourceLanguage && originalLanguageIndex == (int)i) ||
        (!sourceLanguage && targetLanguageIndex == (int)i)) {
      item.checked = YES;
    }
    [items addObject:item];
  }
  DCHECK_GT(originalLanguageIndex, kInvalidLanguageIndex);
  DCHECK_GT(targetLanguageIndex, kInvalidLanguageIndex);

  return items;
}

#pragma mark - InfobarTranslateLanguageSelectionDelegate

- (void)didSelectSourceLanguageIndex:(int)languageIndex
                            withName:(NSString*)languageName {
  // Sanity check that |languageIndex| matches the languageName selected.
  DCHECK([languageName
      isEqualToString:base::SysUTF16ToNSString(
                          self.translateInfobarDelegate->language_name_at(
                              languageIndex))]);
  DCHECK(self.modalConsumer);

  [self.modalConsumer
      setupModalViewControllerWithPrefs:
          [self createPrefDictionaryForSourceLanguage:
                    base::SysUTF16ToNSString(
                        self.translateInfobarDelegate->language_name_at(
                            languageIndex))
                                       targetLanguage:
                                           base::SysUTF16ToNSString(
                                               self.translateInfobarDelegate
                                                   ->target_language_name())
                               translateButtonEnabled:YES]];
}

- (void)didSelectTargetLanguageIndex:(int)languageIndex
                            withName:(NSString*)languageName {
  // Sanity check that |languageIndex| matches the languageName selected.
  DCHECK([languageName
      isEqualToString:base::SysUTF16ToNSString(
                          self.translateInfobarDelegate->language_name_at(
                              languageIndex))]);
  DCHECK(self.modalConsumer);

  [self.modalConsumer
      setupModalViewControllerWithPrefs:
          [self createPrefDictionaryForSourceLanguage:
                    base::SysUTF16ToNSString(
                        self.translateInfobarDelegate->original_language_name())
                                       targetLanguage:
                                           base::SysUTF16ToNSString(
                                               self.translateInfobarDelegate
                                                   ->language_name_at(
                                                       languageIndex))
                               translateButtonEnabled:YES]];
}

#pragma mark - Private

// Returns a dictionary of prefs to send to the modalConsumer depending on
// |sourceLanguage|, |targetLanguage|, |translateButtonEnabled|, and
// |self.currentStep|.
- (NSDictionary*)createPrefDictionaryForSourceLanguage:(NSString*)sourceLanguage
                                        targetLanguage:(NSString*)targetLanguage
                                translateButtonEnabled:
                                    (BOOL)translateButtonEnabled {
  BOOL currentStepBeforeTranslate =
      self.currentStep ==
      translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE;
  BOOL currentStepAfterTranslate =
      self.currentStep ==
      translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE;

  return @{
    kSourceLanguagePrefKey : sourceLanguage,
    kTargetLanguagePrefKey : targetLanguage,
    kEnableTranslateButtonPrefKey : @(translateButtonEnabled),
    kEnableAndDisplayShowOriginalButtonPrefKey : @(currentStepAfterTranslate),
    kShouldAlwaysTranslatePrefKey :
        @(self.translateInfobarDelegate->ShouldAlwaysTranslate()),
    kDisplayNeverTranslateLanguagePrefKey : @(currentStepBeforeTranslate),
    kDisplayNeverTranslateSiteButtonPrefKey : @(currentStepBeforeTranslate),
    kIsTranslatableLanguagePrefKey :
        @(self.translateInfobarDelegate->IsTranslatableLanguageByPrefs()),
    kIsSiteBlacklistedPrefKey :
        @(self.translateInfobarDelegate->IsSiteBlacklisted()),
  };
}

@end

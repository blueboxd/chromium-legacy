// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_view_controller.h"

#include "base/feature_list.h"
#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/autofill/cells/autofill_edit_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierName = kSectionIdentifierEnumZero,
  SectionIdentifierCreditCardDetails,
  SectionIdentifierCameraButton,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeName = kItemTypeEnumZero,
  ItemTypeCardNumber,
  ItemTypeExpirationMonth,
  ItemTypeExpirationYear,
  ItemTypeUseCameraButton,
};

}  // namespace

@interface AutofillAddCreditCardViewController ()

// The AddCreditCardViewControllerDelegate for this ViewController.
@property(nonatomic, weak) id<AddCreditCardViewControllerDelegate> delegate;

@end

@implementation AutofillAddCreditCardViewController

- (instancetype)initWithDelegate:
    (id<AddCreditCardViewControllerDelegate>)delegate {
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;
  self = [super initWithTableViewStyle:style
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];

  if (self) {
    _delegate = delegate;
  }

  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.allowsSelection = NO;
  self.view.backgroundColor = UIColor.cr_systemGroupedBackgroundColor;

  self.navigationItem.title = l10n_util::GetNSString(
      IDS_IOS_CREDIT_CARD_SETTINGS_ADD_PAYMENT_METHOD_TITLE);

  // Adds 'Cancel' and 'Add' buttons to Navigation bar.
  self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_CANCEL_BUTTON)
              style:UIBarButtonItemStyleDone
             target:self
             action:@selector(handleCancelButton:)];

  self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_ADD_BUTTON)
              style:UIBarButtonItemStyleDone
             target:self
             action:@selector(didTapAddButton:)];
  [self loadModel];
}

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierName];
  [model addSectionWithIdentifier:SectionIdentifierCreditCardDetails];
  [model addSectionWithIdentifier:SectionIdentifierCameraButton];

  AutofillEditItem* cardHolderNameItem =
      [self createTableViewItemWithType:ItemTypeName
                          textFieldName:l10n_util::GetNSString(
                                            IDS_IOS_AUTOFILL_CARDHOLDER)
                   textFieldPlaceholder:
                       l10n_util::GetNSString(
                           IDS_IOS_AUTOFILL_DIALOG_PLACEHOLDER_CARD_HOLDER_NAME)
                           keyboardType:UIKeyboardTypeDefault
                         autofillUIType:AutofillUITypeCreditCardHolderFullName];
  [model addItem:cardHolderNameItem
      toSectionWithIdentifier:SectionIdentifierName];

  AutofillEditItem* cardNumberItem =
      [self createTableViewItemWithType:ItemTypeCardNumber
                          textFieldName:l10n_util::GetNSString(
                                            IDS_IOS_AUTOFILL_CARD_NUMBER)
                   textFieldPlaceholder:
                       l10n_util::GetNSString(
                           IDS_IOS_AUTOFILL_DIALOG_PLACEHOLDER_CARD_NUMBER)
                           keyboardType:UIKeyboardTypeNumberPad
                         autofillUIType:AutofillUITypeCreditCardNumber];
  [model addItem:cardNumberItem
      toSectionWithIdentifier:SectionIdentifierCreditCardDetails];

  AutofillEditItem* expirationMonthItem =
      [self createTableViewItemWithType:ItemTypeExpirationMonth
                          textFieldName:l10n_util::GetNSString(
                                            IDS_IOS_AUTOFILL_EXP_MONTH)
                   textFieldPlaceholder:
                       l10n_util::GetNSString(
                           IDS_IOS_AUTOFILL_DIALOG_PLACEHOLDER_EXPIRY_MONTH)
                           keyboardType:UIKeyboardTypeNumberPad
                         autofillUIType:AutofillUITypeCreditCardExpMonth];
  [model addItem:expirationMonthItem
      toSectionWithIdentifier:SectionIdentifierCreditCardDetails];

  AutofillEditItem* expirationYearItem =
      [self createTableViewItemWithType:ItemTypeExpirationYear
                          textFieldName:l10n_util::GetNSString(
                                            IDS_IOS_AUTOFILL_EXP_YEAR)
                   textFieldPlaceholder:
                       l10n_util::GetNSString(
                           IDS_IOS_AUTOFILL_DIALOG_PLACEHOLDER_EXPIRATION_YEAR)
                           keyboardType:UIKeyboardTypeNumberPad
                         autofillUIType:AutofillUITypeCreditCardExpYear];
  [model addItem:expirationYearItem
      toSectionWithIdentifier:SectionIdentifierCreditCardDetails];

  TableViewTextButtonItem* cameraButtonItem =
      [[TableViewTextButtonItem alloc] initWithType:ItemTypeUseCameraButton];
  cameraButtonItem.buttonBackgroundColor = UIColor.cr_systemBackgroundColor;
  cameraButtonItem.buttonTextColor = [UIColor colorNamed:kBlueColor];
  cameraButtonItem.buttonText = l10n_util::GetNSString(
      IDS_IOS_AUTOFILL_ADD_CREDIT_CARD_OPEN_CAMERA_BUTTON_LABEL);
  cameraButtonItem.textAlignment = NSTextAlignmentNatural;
  [model addItem:cameraButtonItem
      toSectionWithIdentifier:SectionIdentifierCameraButton];
}

#pragma mark - Private

// Reads the data from text fields and sends it to the mediator.
- (void)didTapAddButton:(id)sender {
  NSString* cardHolderName = [self readTextFromItemtype:ItemTypeName
                                      sectionIdentifier:SectionIdentifierName];

  NSString* cardNumber =
      [self readTextFromItemtype:ItemTypeCardNumber
               sectionIdentifier:SectionIdentifierCreditCardDetails];

  NSString* expirationMonth =
      [self readTextFromItemtype:ItemTypeExpirationMonth
               sectionIdentifier:SectionIdentifierCreditCardDetails];

  NSString* expirationYear =
      [self readTextFromItemtype:ItemTypeExpirationYear
               sectionIdentifier:SectionIdentifierCreditCardDetails];

  [self.delegate addCreditCardViewController:self
                 addCreditCardWithHolderName:cardHolderName
                                  cardNumber:cardNumber
                             expirationMonth:expirationMonth
                              expirationYear:expirationYear];
}

// Reads and returns the data from the item with passed |itemType| and
// |sectionIdentifier|.
- (NSString*)readTextFromItemtype:(NSInteger)itemType
                sectionIdentifier:(NSInteger)sectionIdentifier {
  NSIndexPath* path =
      [self.tableViewModel indexPathForItemType:itemType
                              sectionIdentifier:sectionIdentifier];
  AutofillEditItem* item = base::mac::ObjCCastStrict<AutofillEditItem>(
      [self.tableViewModel itemAtIndexPath:path]);
  NSString* text = item.textFieldValue;
  return text;
}

// Dimisses this view controller.
- (void)handleCancelButton:(id)sender {
  [self.delegate addCreditCardViewControllerDidCancel:self];
}

// Returns initialized tableViewItem with passed arguments.
- (AutofillEditItem*)createTableViewItemWithType:(NSInteger)itemType
                                   textFieldName:(NSString*)textFieldName
                            textFieldPlaceholder:(NSString*)textFieldPlaceholder
                                    keyboardType:(UIKeyboardType)keyboardType
                                  autofillUIType:
                                      (AutofillUIType)autofillUIType {
  AutofillEditItem* item = [[AutofillEditItem alloc] initWithType:itemType];
  item.textFieldName = textFieldName;
  item.textFieldPlaceholder = textFieldPlaceholder;
  item.keyboardType = keyboardType;
  item.hideEditIcon = NO;
  item.textFieldEnabled = YES;
  item.autofillUIType = autofillUIType;
  return item;
}

@end

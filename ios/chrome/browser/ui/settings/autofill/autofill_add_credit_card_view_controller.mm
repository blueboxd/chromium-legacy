// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_view_controller.h"

#include "base/feature_list.h"
#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/autofill/cells/autofill_edit_item.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_view_controller_delegate.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
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

// The card holder name updated with the text in tableview cell.
@property(nonatomic, strong) NSString* cardHolderName;

// The card number set from the CreditCardConsumer protocol, used to update the
// UI.
@property(nonatomic, strong) NSString* cardNumber;

// The expiration month set from the CreditCardConsumer protocol, used to update
// the UI.
@property(nonatomic, strong) NSString* expirationMonth;

// The expiration year set from the CreditCardConsumer protocol, used to update
// the UI.
@property(nonatomic, strong) NSString* expirationYear;

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

  self.view.backgroundColor = UIColor.cr_systemGroupedBackgroundColor;

  self.navigationItem.title = l10n_util::GetNSString(
      IDS_IOS_CREDIT_CARD_SETTINGS_ADD_PAYMENT_METHOD_TITLE);

  // Adds 'Cancel' and 'Add' buttons to Navigation bar.
  self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_CANCEL_BUTTON)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(handleCancelButton:)];

  self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_ADD_BUTTON)
              style:UIBarButtonItemStyleDone
             target:self
             action:@selector(didTapAddButton:)];
  [self loadModel];
}

- (BOOL)tableViewHasUserInput {
  [self updateCreditCardData];

  return self.cardHolderName.length || self.cardNumber.length ||
         self.expirationMonth.length || self.expirationYear.length;
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierName];
  [model addSectionWithIdentifier:SectionIdentifierCreditCardDetails];

  AutofillEditItem* cardHolderNameItem =
      [self createTableViewItemWithType:ItemTypeName
                          textFieldName:l10n_util::GetNSString(
                                            IDS_IOS_AUTOFILL_CARDHOLDER)
                         textFieldValue:self.cardHolderName
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
                         textFieldValue:self.cardNumber
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
                         textFieldValue:self.expirationMonth
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
                         textFieldValue:self.expirationYear
                   textFieldPlaceholder:
                       l10n_util::GetNSString(
                           IDS_IOS_AUTOFILL_DIALOG_PLACEHOLDER_EXPIRATION_YEAR)
                           keyboardType:UIKeyboardTypeNumberPad
                         autofillUIType:AutofillUITypeCreditCardExpYear];
  [model addItem:expirationYearItem
      toSectionWithIdentifier:SectionIdentifierCreditCardDetails];

  TableViewTextItem* cameraButtonItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeUseCameraButton];
  cameraButtonItem.textColor = [UIColor colorNamed:kBlueColor];
  cameraButtonItem.text = l10n_util::GetNSString(
      IDS_IOS_AUTOFILL_ADD_CREDIT_CARD_OPEN_CAMERA_BUTTON_LABEL);
  cameraButtonItem.textAlignment = NSTextAlignmentCenter;
  if (@available(iOS 13, *)) {
    [model addSectionWithIdentifier:SectionIdentifierCameraButton];
    [model addItem:cameraButtonItem
        toSectionWithIdentifier:SectionIdentifierCameraButton];
  }
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];
  if ([self.tableViewModel itemTypeForIndexPath:indexPath] ==
      ItemTypeUseCameraButton) {
    [self handleCameraButton];
  }
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (NSIndexPath*)tableView:(UITableView*)tableView
    willSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView willSelectRowAtIndexPath:indexPath];
  if ([self.tableViewModel itemTypeForIndexPath:indexPath] ==
      ItemTypeUseCameraButton) {
    return indexPath;
  }
  return nil;
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];

  // Use |ObjCCast| because |cell| might not be |TableViewTextEditCell|.
  // Set the delegate and style for only |TableViewTextEditCell| type of cell
  // not other types.
  TableViewTextEditCell* editCell =
      base::mac::ObjCCast<TableViewTextEditCell>(cell);
  editCell.textField.delegate = self;
  editCell.selectionStyle = UITableViewCellSelectionStyleNone;

  return cell;
}

#pragma mark - CreditCardConsumer

// TODO(crbug.com/984545): This method will be called from
// CreditCardScannerMediator after it is implemented.
- (void)setCreditCardNumber:(NSString*)cardNumber
            expirationMonth:(NSString*)expirationMonth
             expirationYear:(NSString*)expirationYear {
  if (cardNumber) {
    [self updateCellForItemType:ItemTypeCardNumber
            inSectionIdentifier:SectionIdentifierCreditCardDetails
                       withText:cardNumber];
  }

  if (expirationMonth) {
    [self updateCellForItemType:ItemTypeExpirationMonth
            inSectionIdentifier:SectionIdentifierCreditCardDetails
                       withText:expirationMonth];
  }

  if (expirationYear) {
    [self updateCellForItemType:ItemTypeExpirationYear
            inSectionIdentifier:SectionIdentifierCreditCardDetails
                       withText:expirationYear];
  }
}

#pragma mark - Private

// Handles Add button to add a new credit card.
- (void)didTapAddButton:(id)sender {
  [self updateCreditCardData];

  [self.delegate addCreditCardViewController:self
                 addCreditCardWithHolderName:self.cardHolderName
                                  cardNumber:self.cardNumber
                             expirationMonth:self.expirationMonth
                              expirationYear:self.expirationYear];
}

// Updates credit card data properties with the text in TableView cells.
- (void)updateCreditCardData {
  self.cardHolderName = [self readTextFromItemtype:ItemTypeName
                                 sectionIdentifier:SectionIdentifierName];

  self.cardNumber =
      [self readTextFromItemtype:ItemTypeCardNumber
               sectionIdentifier:SectionIdentifierCreditCardDetails];

  self.expirationMonth =
      [self readTextFromItemtype:ItemTypeExpirationMonth
               sectionIdentifier:SectionIdentifierCreditCardDetails];

  self.expirationYear =
      [self readTextFromItemtype:ItemTypeExpirationYear
               sectionIdentifier:SectionIdentifierCreditCardDetails];
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

// Updates TableView cell of |itemType| in |sectionIdentifier| textfieldValue
// with |text|.
- (void)updateCellForItemType:(NSInteger)itemType
          inSectionIdentifier:(NSInteger)sectionIdentifier
                     withText:(NSString*)text {
  NSIndexPath* path =
      [self.tableViewModel indexPathForItemType:itemType
                              sectionIdentifier:sectionIdentifier];
  AutofillEditItem* item = base::mac::ObjCCastStrict<AutofillEditItem>(
      [self.tableViewModel itemAtIndexPath:path]);
  item.textFieldValue = text;
  [self reconfigureCellsForItems:@[ item ]];
}

// Dimisses this view controller when Cancel button is tapped.
- (void)handleCancelButton:(id)sender {
  [self.delegate addCreditCardViewControllerDidCancel:self];
}

// Returns initialized tableViewItem with passed arguments.
- (AutofillEditItem*)createTableViewItemWithType:(NSInteger)itemType
                                   textFieldName:(NSString*)textFieldName
                                  textFieldValue:(NSString*)textFieldValue
                            textFieldPlaceholder:(NSString*)textFieldPlaceholder
                                    keyboardType:(UIKeyboardType)keyboardType
                                  autofillUIType:
                                      (AutofillUIType)autofillUIType {
  AutofillEditItem* item = [[AutofillEditItem alloc] initWithType:itemType];
  item.textFieldName = textFieldName;
  item.textFieldValue = textFieldValue;
  item.textFieldPlaceholder = textFieldPlaceholder;
  item.keyboardType = keyboardType;
  item.hideEditIcon = NO;
  item.textFieldEnabled = YES;
  item.autofillUIType = autofillUIType;
  return item;
}

// Presents the credit card scanner camera screen.
- (void)handleCameraButton {
  [self.delegate addCreditCardViewControllerDidUseCamera:self];
}

@end

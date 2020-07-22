// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/password_details_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_view_controller_delegate.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Padding used between the image and the text labels.
const CGFloat kWarningIconSize = 20;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierPassword = kSectionIdentifierEnumZero,
  SectionIdentifierCompromisedInfo
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeWebsite = kItemTypeEnumZero,
  ItemTypeUsername,
  ItemTypePassword,
  ItemTypeChangePasswordButton,
  ItemTypeChangePasswordRecommendation,
};

}  // namespace

@interface PasswordDetailsViewController ()

// Password which is shown on the screen.
@property(nonatomic, strong) PasswordDetails* password;

@end

@implementation PasswordDetailsViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kPasswordDetailsViewControllerId;
  self.tableView.allowsSelectionDuringEditing = YES;

  [self loadModel];
}

- (void)viewDidDisappear:(BOOL)animated {
  [self.handler passwordDetailsViewControllerDidDisappear];
  [super viewDidDisappear:animated];
}

#pragma mark - ChromeTableViewController

- (void)editButtonPressed {
  [super editButtonPressed];

  if (!self.tableView.editing) {
    // TODO:(crbug.com/1075494) - Update |_password| accordingly.
    [self.delegate passwordDetailsViewController:self
                          didEditPasswordDetails:self.password];
  }

  [self loadModel];
  [self reconfigureCellsForItems:
            [self.tableViewModel
                itemsInSectionWithIdentifier:SectionIdentifierPassword]];
  [self reconfigureCellsForItems:
            [self.tableViewModel
                itemsInSectionWithIdentifier:SectionIdentifierCompromisedInfo]];
}

- (void)loadModel {
  [super loadModel];
  self.title = self.password.origin;

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierPassword];

  [model addItem:[self websiteItem]
      toSectionWithIdentifier:SectionIdentifierPassword];

  [model addItem:[self usernameItem]
      toSectionWithIdentifier:SectionIdentifierPassword];

  [model addItem:[self passwordItem]
      toSectionWithIdentifier:SectionIdentifierPassword];

  if (self.password.isCompromised) {
    [model addSectionWithIdentifier:SectionIdentifierCompromisedInfo];

    [model addItem:[self changePasswordItem]
        toSectionWithIdentifier:SectionIdentifierCompromisedInfo];

    [model addItem:[self changePasswordRecommendationItem]
        toSectionWithIdentifier:SectionIdentifierCompromisedInfo];
  }
}

#pragma mark - Items

- (TableViewTextEditItem*)websiteItem {
  TableViewTextEditItem* item =
      [[TableViewTextEditItem alloc] initWithType:ItemTypeWebsite];
  item.textFieldName = l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_SITE);
  item.textFieldValue = self.password.website;
  item.textFieldEnabled = NO;
  item.hideIcon = YES;
  return item;
}

- (TableViewTextEditItem*)usernameItem {
  TableViewTextEditItem* item =
      [[TableViewTextEditItem alloc] initWithType:ItemTypeUsername];
  item.textFieldName =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME);
  item.textFieldValue = self.password.username;
  item.textFieldEnabled = NO;
  item.hideIcon = YES;
  return item;
}

- (TableViewTextEditItem*)passwordItem {
  TableViewTextEditItem* item =
      [[TableViewTextEditItem alloc] initWithType:ItemTypePassword];
  item.textFieldName =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD);
  item.textFieldValue = kMaskedPassword;
  item.textFieldEnabled = self.tableView.editing;
  item.hideIcon = !self.tableView.editing;
  item.autoCapitalizationType = UITextAutocapitalizationTypeNone;
  item.keyboardType = UIKeyboardTypeURL;
  item.returnKeyType = UIReturnKeyDone;
  // TODO:(crbug.com/1075494) - Add eye icon to view password.
  return item;
}

- (TableViewTextItem*)changePasswordItem {
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:ItemTypeChangePasswordButton];
  item.text = l10n_util::GetNSString(IDS_IOS_CHANGE_COMPROMISED_PASSWORD);
  item.textColor = self.tableView.editing ? UIColor.cr_secondaryLabelColor
                                          : [UIColor colorNamed:kBlueColor];
  item.accessibilityTraits = UIAccessibilityTraitButton;
  return item;
}

- (SettingsImageDetailTextItem*)changePasswordRecommendationItem {
  SettingsImageDetailTextItem* item = [[SettingsImageDetailTextItem alloc]
      initWithType:ItemTypeChangePasswordRecommendation];
  item.detailText =
      l10n_util::GetNSString(IDS_IOS_CHANGE_COMPROMISED_PASSWORD_DESCRIPTION);
  item.image = [self getCompromisedIcon];
  return item;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewModel* model = self.tableViewModel;
  NSInteger itemType = [model itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeWebsite:
    case ItemTypeUsername:
    case ItemTypeChangePasswordRecommendation:
      break;
    case ItemTypePassword: {
      if (self.tableView.editing) {
        UITableViewCell* cell =
            [self.tableView cellForRowAtIndexPath:indexPath];
        TableViewTextEditCell* textFieldCell =
            base::mac::ObjCCastStrict<TableViewTextEditCell>(cell);
        [textFieldCell.textField becomeFirstResponder];
      }
      break;
    }
    case ItemTypeChangePasswordButton:
      if (!self.tableView.editing) {
        DCHECK(self.commandsDispatcher);
        GURL URL(base::SysNSStringToUTF8(self.password.website));
        DCHECK(URL.is_valid());
        OpenNewTabCommand* command =
            [OpenNewTabCommand commandWithURLFromChrome:URL];
        [self.commandsDispatcher closeSettingsUIAndOpenURL:command];
      }
      break;
  }
}

- (UITableViewCellEditingStyle)tableView:(UITableView*)tableView
           editingStyleForRowAtIndexPath:(NSIndexPath*)indexPath {
  return UITableViewCellEditingStyleNone;
}

- (BOOL)tableView:(UITableView*)tableview
    shouldIndentWhileEditingRowAtIndexPath:(NSIndexPath*)indexPath {
  return NO;
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];

  cell.selectionStyle = UITableViewCellSelectionStyleNone;

  // TODO:(crbug.com/1075494) - Add action to Show/Hide password when user tap
  // eye icon.
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypePassword: {
      TableViewTextEditCell* textFieldCell =
          base::mac::ObjCCastStrict<TableViewTextEditCell>(cell);
      textFieldCell.textField.delegate = self;
      return textFieldCell;
    }
    case ItemTypeWebsite:
    case ItemTypeUsername:
    case ItemTypeChangePasswordButton:
    case ItemTypeChangePasswordRecommendation:
      break;
  }
  return cell;
}

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeWebsite:
    case ItemTypeUsername:
      return NO;
    case ItemTypePassword:
      return YES;
  }
  return NO;
}

#pragma mark - PasswordDetailsConsumer

- (void)setPassword:(PasswordDetails*)password {
  _password = password;
  [self reloadData];
}

#pragma mark - Private

// Called when user tapped Delete button during editing. It means presented
// password should be deleted.
- (void)deleteItems:(NSArray<NSIndexPath*>*)indexPaths {
  // TODO:(crbug.com/1075494) - Show Confirmation dialog and delete password.
}

- (BOOL)shouldHideToolbar {
  return !self.editing;
}

// Applies tint colour and resizes image.
- (UIImage*)getCompromisedIcon {
  UIImage* image = [UIImage imageNamed:@"settings_unsafe_state"];
  UIImage* newImage =
      [image imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  UIGraphicsBeginImageContextWithOptions(
      CGSizeMake(kWarningIconSize, kWarningIconSize), NO, 0.0);
  [UIColor.cr_secondaryLabelColor set];
  [newImage drawInRect:CGRectMake(0, 0, kWarningIconSize, kWarningIconSize)];
  newImage = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();
  return newImage;
}

@end

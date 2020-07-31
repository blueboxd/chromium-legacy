// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/password_details_view_controller.h"

#include <memory>

#include "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_view_controller_delegate.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/test/app/password_test_util.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test class that conforms to PasswordDetailsHanler in order to test the
// presenter methods are called correctly.
@interface FakePasswordDetailsHandler : NSObject <PasswordDetailsHandler>

@property(nonatomic, assign) BOOL deletionCalled;

@property(nonatomic, assign) BOOL editingCalled;

@end

@implementation FakePasswordDetailsHandler

// Called when the view controller was dismissed.
- (void)passwordDetailsViewControllerDidDisappear {
}

- (void)showPasscodeDialog {
}

- (void)showPasswordDeleteDialogWithOrigin:(NSString*)origin {
  self.deletionCalled = YES;
}

- (void)showPasswordEditDialogWithOrigin:(NSString*)origin {
  self.editingCalled = YES;
}

@end

// Test class that conforms to PasswordDetailsViewControllerDelegate in order to
// test the delegate methods are called correctly.
@interface FakePasswordDetailsDelegate
    : NSObject <PasswordDetailsViewControllerDelegate>

@property(nonatomic, strong) PasswordDetails* password;

@end

@implementation FakePasswordDetailsDelegate

- (void)passwordDetailsViewController:
            (PasswordDetailsViewController*)viewController
               didEditPasswordDetails:(PasswordDetails*)password {
  self.password = password;
}

@end

// Unit tests for PasswordIssuesTableViewController.
class PasswordDetailsViewControllerTest : public ChromeTableViewControllerTest {
 protected:
  PasswordDetailsViewControllerTest() {
    handler_ = [[FakePasswordDetailsHandler alloc] init];
    delegate_ = [[FakePasswordDetailsDelegate alloc] init];
    reauthentication_module_ = [[MockReauthenticationModule alloc] init];
    reauthentication_module_.expectedResult = ReauthenticationResult::kSuccess;
  }

  ChromeTableViewController* InstantiateController() override {
    PasswordDetailsViewController* controller =
        [[PasswordDetailsViewController alloc]
            initWithStyle:UITableViewStylePlain];
    controller.handler = handler_;
    controller.delegate = delegate_;
    controller.reauthModule = reauthentication_module_;
    return controller;
  }

  void SetPassword(bool isCompromised = false) {
    auto form = autofill::PasswordForm();
    form.url = GURL("http://www.example.com/");
    form.action = GURL("http://www.example.com/accounts/Login");
    form.username_element = base::ASCIIToUTF16("Email");
    form.username_value = base::ASCIIToUTF16("test@egmail.com");
    form.password_element = base::ASCIIToUTF16("Passwd");
    form.password_value = base::ASCIIToUTF16("test");
    form.submit_element = base::ASCIIToUTF16("signIn");
    form.signon_realm = "http://www.example.com/";
    form.scheme = autofill::PasswordForm::Scheme::kHtml;
    PasswordDetails* password =
        [[PasswordDetails alloc] initWithPasswordForm:form];
    password.compromised = isCompromised;

    PasswordDetailsViewController* passwords_controller =
        static_cast<PasswordDetailsViewController*>(controller());
    [passwords_controller setPassword:password];
  }

  void CheckEditCellText(NSString* expected_text, int section, int item) {
    TableViewTextEditItem* cell =
        static_cast<TableViewTextEditItem*>(GetTableViewItem(section, item));
    EXPECT_NSEQ(expected_text, cell.textFieldValue);
  }

  void CheckDetailItemTextWithId(int expected_detail_text_id,
                                 int section,
                                 int item) {
    SettingsImageDetailTextItem* cell =
        static_cast<SettingsImageDetailTextItem*>(
            GetTableViewItem(section, item));
    EXPECT_NSEQ(l10n_util::GetNSString(expected_detail_text_id),
                cell.detailText);
  }

  FakePasswordDetailsHandler* handler() { return handler_; }
  FakePasswordDetailsDelegate* delegate() { return delegate_; }
  MockReauthenticationModule* reauth() { return reauthentication_module_; }

 private:
  FakePasswordDetailsHandler* handler_;
  FakePasswordDetailsDelegate* delegate_;
  MockReauthenticationModule* reauthentication_module_;
};

// Tests PasswordDetailsViewController is set up with appropriate items
// and sections.
TEST_F(PasswordDetailsViewControllerTest, TestModel) {
  CreateController();
  CheckController();
  EXPECT_EQ(1, NumberOfSections());

  EXPECT_EQ(3, NumberOfItemsInSection(0));
}

// Tests that password is displayed properly.
TEST_F(PasswordDetailsViewControllerTest, TestPassword) {
  SetPassword();
  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(3, NumberOfItemsInSection(0));

  CheckEditCellText(@"http://www.example.com/", 0, 0);
  CheckEditCellText(@"test@egmail.com", 0, 1);
  CheckEditCellText(kMaskedPassword, 0, 2);
}

// Tests that compromised password is displayed properly.
TEST_F(PasswordDetailsViewControllerTest, TestCompromisedPassword) {
  SetPassword(true);
  EXPECT_EQ(2, NumberOfSections());
  EXPECT_EQ(3, NumberOfItemsInSection(0));
  EXPECT_EQ(2, NumberOfItemsInSection(1));

  CheckEditCellText(@"http://www.example.com/", 0, 0);
  CheckEditCellText(@"test@egmail.com", 0, 1);
  CheckEditCellText(kMaskedPassword, 0, 2);

  CheckTextCellTextWithId(IDS_IOS_CHANGE_COMPROMISED_PASSWORD, 1, 0);
  CheckDetailItemTextWithId(IDS_IOS_CHANGE_COMPROMISED_PASSWORD_DESCRIPTION, 1,
                            1);
}

// Tests that password is shown/hidden.
TEST_F(PasswordDetailsViewControllerTest, TestShowHidePassword) {
  SetPassword();
  CheckEditCellText(kMaskedPassword, 0, 2);

  NSIndexPath* indexOfPassword = [NSIndexPath indexPathForRow:2 inSection:0];
  TableViewTextEditCell* textFieldCell =
      base::mac::ObjCCastStrict<TableViewTextEditCell>([controller()
                      tableView:controller().tableView
          cellForRowAtIndexPath:indexOfPassword]);
  EXPECT_TRUE(textFieldCell);
  [textFieldCell.identifyingIconButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];

  CheckEditCellText(@"test", 0, 2);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_REAUTH_REASON_SHOW),
      reauth().localizedReasonForAuthentication);

  [textFieldCell.identifyingIconButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];
  CheckEditCellText(kMaskedPassword, 0, 2);
}

// Tests that passwords was not shown in case reauth failed.
TEST_F(PasswordDetailsViewControllerTest, TestShowPasswordReauthFailed) {
  SetPassword();

  CheckEditCellText(kMaskedPassword, 0, 2);

  reauth().expectedResult = ReauthenticationResult::kFailure;
  NSIndexPath* indexOfPassword = [NSIndexPath indexPathForRow:2 inSection:0];
  TableViewTextEditCell* textFieldCell =
      base::mac::ObjCCastStrict<TableViewTextEditCell>([controller()
                      tableView:controller().tableView
          cellForRowAtIndexPath:indexOfPassword]);
  EXPECT_TRUE(textFieldCell);
  [textFieldCell.identifyingIconButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];

  CheckEditCellText(kMaskedPassword, 0, 2);
}

// Tests that password was revealed during editing.
TEST_F(PasswordDetailsViewControllerTest, TestPasswordShownDuringEditing) {
  SetPassword();
  CheckEditCellText(kMaskedPassword, 0, 2);

  PasswordDetailsViewController* passwordDetails =
      base::mac::ObjCCastStrict<PasswordDetailsViewController>(controller());
  [passwordDetails editButtonPressed];
  EXPECT_TRUE(passwordDetails.tableView.editing);
  CheckEditCellText(@"test", 0, 2);

  [passwordDetails editButtonPressed];
  EXPECT_FALSE(passwordDetails.tableView.editing);
  CheckEditCellText(kMaskedPassword, 0, 2);
}

// Tests that editing mode was not entered because reauth failed.
TEST_F(PasswordDetailsViewControllerTest, TestEditingReauthFailed) {
  SetPassword();
  CheckEditCellText(kMaskedPassword, 0, 2);

  reauth().expectedResult = ReauthenticationResult::kFailure;
  PasswordDetailsViewController* passwordDetails =
      base::mac::ObjCCastStrict<PasswordDetailsViewController>(controller());
  [passwordDetails editButtonPressed];
  EXPECT_FALSE(passwordDetails.tableView.editing);
  CheckEditCellText(kMaskedPassword, 0, 2);
}

// Tests that delete button trigger showing password delete dialog.
TEST_F(PasswordDetailsViewControllerTest, TestPasswordDelete) {
  SetPassword();

  EXPECT_FALSE(handler().deletionCalled);
  PasswordDetailsViewController* passwordDetails =
      base::mac::ObjCCastStrict<PasswordDetailsViewController>(controller());
  [passwordDetails editButtonPressed];
  [[UIApplication sharedApplication]
      sendAction:passwordDetails.deleteButton.action
              to:passwordDetails.deleteButton.target
            from:nil
        forEvent:nil];
  EXPECT_TRUE(handler().deletionCalled);
}

// Tests password editing. User confirmed this action.
TEST_F(PasswordDetailsViewControllerTest, TestEditPasswordConfirmed) {
  SetPassword();

  PasswordDetailsViewController* passwordDetails =
      base::mac::ObjCCastStrict<PasswordDetailsViewController>(controller());
  [passwordDetails editButtonPressed];
  EXPECT_FALSE(handler().editingCalled);
  EXPECT_FALSE(delegate().password);
  EXPECT_TRUE(passwordDetails.tableView.editing);

  TableViewTextEditItem* cell =
      static_cast<TableViewTextEditItem*>(GetTableViewItem(0, 2));
  cell.textFieldValue = @"new_password";

  [passwordDetails editButtonPressed];
  EXPECT_TRUE(handler().editingCalled);

  [passwordDetails passwordEditingConfirmed];
  EXPECT_TRUE(delegate().password);

  EXPECT_NSEQ(@"new_password", delegate().password.password);
  EXPECT_FALSE(passwordDetails.tableView.editing);
}

// Tests  password editing. User cancelled this action.
TEST_F(PasswordDetailsViewControllerTest, TestEditPasswordCancel) {
  SetPassword();

  PasswordDetailsViewController* passwordDetails =
      base::mac::ObjCCastStrict<PasswordDetailsViewController>(controller());
  [passwordDetails editButtonPressed];
  EXPECT_FALSE(delegate().password);
  EXPECT_TRUE(passwordDetails.tableView.editing);

  TableViewTextEditItem* cell =
      static_cast<TableViewTextEditItem*>(GetTableViewItem(0, 2));
  cell.textFieldValue = @"new_password";

  [passwordDetails editButtonPressed];
  EXPECT_FALSE(delegate().password);
  EXPECT_TRUE(passwordDetails.tableView.editing);
}

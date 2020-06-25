// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/passwords_table_view_controller.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/password_form.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/mock_bulk_leak_check_service.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/passwords/ios_chrome_bulk_leak_check_service_factory.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#include "ios/chrome/browser/passwords/password_check_observer_bridge.h"
#include "ios/chrome/browser/passwords/save_passwords_consumer.h"
#import "ios/chrome/browser/ui/settings/cells/settings_password_check_item.h"
#import "ios/chrome/browser/ui/settings/password/password_details_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/passwords_consumer.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_text_item.h"
#include "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::MockPasswordStore;
using password_manager::MockBulkLeakCheckService;

// Declaration to conformance to SavePasswordsConsumerDelegate and keep tests in
// this file working.
@interface PasswordsTableViewController (Test) <UISearchBarDelegate,
                                                SavePasswordsConsumerDelegate,
                                                PasswordsConsumer>
- (void)updateExportPasswordsButton;
@end

namespace {

typedef struct {
  bool password_check_enabled;
} PasswordCheckFeatureStatus;

enum PasswordsSections {
  SavePasswordsSwitch = 0,
  PasswordCheck,
  SavedPasswords,
  Blocked,
  ExportPasswordsButton,
};

class PasswordsTableViewControllerTest
    : public ChromeTableViewControllerTest,
      public ::testing::WithParamInterface<PasswordCheckFeatureStatus> {
 protected:
  PasswordsTableViewControllerTest() = default;

  void SetUp() override {
    // TODO(crbug.com/1096986): Remove parametrized tests once the feature is
    // enabled.
    if (GetParam().password_check_enabled) {
      scoped_feature_list_.InitAndEnableFeature(
          password_manager::features::kPasswordCheck);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          password_manager::features::kPasswordCheck);
    }

    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    ChromeTableViewControllerTest::SetUp();
    IOSChromePasswordStoreFactory::GetInstance()->SetTestingFactory(
        chrome_browser_state_.get(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<web::BrowserState,
                                                  MockPasswordStore>));

    IOSChromeBulkLeakCheckServiceFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            chrome_browser_state_.get(),
            base::BindLambdaForTesting([](web::BrowserState*) {
              return std::unique_ptr<KeyedService>(
                  std::make_unique<MockBulkLeakCheckService>());
            }));

    CreateController();
  }

  int GetSectionIndex(PasswordsSections section) {
    switch (section) {
      case SavePasswordsSwitch:
      case PasswordCheck:
        return section;
      case SavedPasswords:
        return GetParam().password_check_enabled ? 2 : 1;
      case Blocked:
        return GetParam().password_check_enabled ? 3 : 2;
      case ExportPasswordsButton:
        return GetParam().password_check_enabled ? 3 : 2;
    }
  }

  int SectionsOffset() { return GetParam().password_check_enabled ? 1 : 0; }

  MockPasswordStore& GetMockStore() {
    return *static_cast<MockPasswordStore*>(
        IOSChromePasswordStoreFactory::GetForBrowserState(
            chrome_browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS)
            .get());
  }

  MockBulkLeakCheckService& GetMockPasswordCheckService() {
    return *static_cast<MockBulkLeakCheckService*>(
        IOSChromeBulkLeakCheckServiceFactory::GetForBrowserState(
            chrome_browser_state_.get()));
  }

  ChromeTableViewController* InstantiateController() override {
    return [[PasswordsTableViewController alloc]
        initWithBrowserState:chrome_browser_state_.get()];
  }

  void ChangePasswordCheckState(PasswordCheckUIState state) {
    PasswordsTableViewController* passwords_controller =
        static_cast<PasswordsTableViewController*>(controller());
    [passwords_controller setPasswordCheckUIState:state];
  }

  // Adds a form to PasswordsTableViewController.
  void AddPasswordForm(std::unique_ptr<autofill::PasswordForm> form) {
    PasswordsTableViewController* passwords_controller =
        static_cast<PasswordsTableViewController*>(controller());
    std::vector<std::unique_ptr<autofill::PasswordForm>> passwords;
    passwords.push_back(std::move(form));
    [passwords_controller onGetPasswordStoreResults:std::move(passwords)];
  }

  // Creates and adds a saved password form.
  void AddSavedForm1() {
    auto form = std::make_unique<autofill::PasswordForm>();
    form->url = GURL("http://www.example.com/accounts/LoginAuth");
    form->action = GURL("http://www.example.com/accounts/Login");
    form->username_element = base::ASCIIToUTF16("Email");
    form->username_value = base::ASCIIToUTF16("test@egmail.com");
    form->password_element = base::ASCIIToUTF16("Passwd");
    form->password_value = base::ASCIIToUTF16("test");
    form->submit_element = base::ASCIIToUTF16("signIn");
    form->signon_realm = "http://www.example.com/";
    form->scheme = autofill::PasswordForm::Scheme::kHtml;
    form->blacklisted_by_user = false;
    AddPasswordForm(std::move(form));
  }

  // Creates and adds a saved password form.
  void AddSavedForm2() {
    auto form = std::make_unique<autofill::PasswordForm>();
    form->url = GURL("http://www.example2.com/accounts/LoginAuth");
    form->action = GURL("http://www.example2.com/accounts/Login");
    form->username_element = base::ASCIIToUTF16("Email");
    form->username_value = base::ASCIIToUTF16("test@egmail.com");
    form->password_element = base::ASCIIToUTF16("Passwd");
    form->password_value = base::ASCIIToUTF16("test");
    form->submit_element = base::ASCIIToUTF16("signIn");
    form->signon_realm = "http://www.example2.com/";
    form->scheme = autofill::PasswordForm::Scheme::kHtml;
    form->blacklisted_by_user = false;
    AddPasswordForm(std::move(form));
  }

  // Creates and adds a blocked site form to never offer to save
  // user's password to those sites.
  void AddBlockedForm1() {
    auto form = std::make_unique<autofill::PasswordForm>();
    form->url = GURL("http://www.secret.com/login");
    form->action = GURL("http://www.secret.com/action");
    form->username_element = base::ASCIIToUTF16("email");
    form->username_value = base::ASCIIToUTF16("test@secret.com");
    form->password_element = base::ASCIIToUTF16("password");
    form->password_value = base::ASCIIToUTF16("cantsay");
    form->submit_element = base::ASCIIToUTF16("signIn");
    form->signon_realm = "http://www.secret.com/";
    form->scheme = autofill::PasswordForm::Scheme::kHtml;
    form->blacklisted_by_user = true;
    AddPasswordForm(std::move(form));
  }

  // Creates and adds another blocked site form to never offer to save
  // user's password to those sites.
  void AddBlockedForm2() {
    auto form = std::make_unique<autofill::PasswordForm>();
    form->url = GURL("http://www.secret2.com/login");
    form->action = GURL("http://www.secret2.com/action");
    form->username_element = base::ASCIIToUTF16("email");
    form->username_value = base::ASCIIToUTF16("test@secret2.com");
    form->password_element = base::ASCIIToUTF16("password");
    form->password_value = base::ASCIIToUTF16("cantsay");
    form->submit_element = base::ASCIIToUTF16("signIn");
    form->signon_realm = "http://www.secret2.com/";
    form->scheme = autofill::PasswordForm::Scheme::kHtml;
    form->blacklisted_by_user = true;
    AddPasswordForm(std::move(form));
  }

  // Deletes the item at (row, section) and wait util condition returns true or
  // timeout.
  bool deleteItemAndWait(int section, int row, ConditionBlock condition) {
    PasswordsTableViewController* passwords_controller =
        static_cast<PasswordsTableViewController*>(controller());
    [passwords_controller
        deleteItems:@[ [NSIndexPath indexPathForRow:row inSection:section] ]];
    return base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForUIElementTimeout, condition);
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests default case has no saved sites and no blocked sites.
TEST_P(PasswordsTableViewControllerTest, TestInitialization) {
  CheckController();
  EXPECT_EQ(2 + SectionsOffset(), NumberOfSections());
}

// Tests adding one item in saved password section.
TEST_P(PasswordsTableViewControllerTest, AddSavedPasswords) {
  AddSavedForm1();

  EXPECT_EQ(3 + SectionsOffset(), NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(GetSectionIndex(SavedPasswords)));
}

// Tests adding one item in blocked password section.
TEST_P(PasswordsTableViewControllerTest, AddBlockedPasswords) {
  AddBlockedForm1();

  EXPECT_EQ(3 + SectionsOffset(), NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(GetSectionIndex(Blocked)));
}

// Tests adding one item in saved password section, and two items in blocked
// password section.
TEST_P(PasswordsTableViewControllerTest, AddSavedAndBlocked) {
  AddSavedForm1();
  AddBlockedForm1();
  AddBlockedForm2();

  // There should be two sections added.
  EXPECT_EQ(4 + SectionsOffset(), NumberOfSections());

  // There should be 1 row in saved password section.
  EXPECT_EQ(1, NumberOfItemsInSection(GetSectionIndex(SavedPasswords)));
  // There should be 2 rows in blocked password section.
  EXPECT_EQ(2, NumberOfItemsInSection(GetSectionIndex(Blocked)));
}

// Tests the order in which the saved passwords are displayed.
TEST_P(PasswordsTableViewControllerTest, TestSavedPasswordsOrder) {
  AddSavedForm2();

  CheckTextCellTextAndDetailText(@"example2.com", @"test@egmail.com",
                                 GetSectionIndex(SavedPasswords), 0);

  AddSavedForm1();
  CheckTextCellTextAndDetailText(@"example.com", @"test@egmail.com",
                                 GetSectionIndex(SavedPasswords), 0);
  CheckTextCellTextAndDetailText(@"example2.com", @"test@egmail.com",
                                 GetSectionIndex(SavedPasswords), 1);
}

// Tests the order in which the blocked passwords are displayed.
TEST_P(PasswordsTableViewControllerTest, TestBlockedPasswordsOrder) {
  AddBlockedForm2();
  CheckTextCellText(@"secret2.com", GetSectionIndex(SavedPasswords), 0);

  AddBlockedForm1();
  CheckTextCellText(@"secret.com", GetSectionIndex(SavedPasswords), 0);
  CheckTextCellText(@"secret2.com", GetSectionIndex(SavedPasswords), 1);
}

// Tests displaying passwords in the saved passwords section when there are
// duplicates in the password store.
TEST_P(PasswordsTableViewControllerTest, AddSavedDuplicates) {
  AddSavedForm1();
  AddSavedForm1();

  EXPECT_EQ(3 + SectionsOffset(), NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(GetSectionIndex(SavedPasswords)));
}

// Tests displaying passwords in the blocked passwords section when there
// are duplicates in the password store.
TEST_P(PasswordsTableViewControllerTest, AddBlockedDuplicates) {
  AddBlockedForm1();
  AddBlockedForm1();

  EXPECT_EQ(3 + SectionsOffset(), NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(GetSectionIndex(SavedPasswords)));
}

// Tests deleting items from saved passwords and blocked passwords sections.
TEST_P(PasswordsTableViewControllerTest, DeleteItems) {
  AddSavedForm1();
  AddBlockedForm1();
  AddBlockedForm2();

  // Delete item in save passwords section.
  ASSERT_TRUE(deleteItemAndWait(GetSectionIndex(SavedPasswords), 0, ^{
    return NumberOfSections() == (3 + SectionsOffset());
  }));
  // Section 2 should now be the blocked passwords section, and should still
  // have both its items.
  EXPECT_EQ(2, NumberOfItemsInSection(GetSectionIndex(SavedPasswords)));

  // Delete item in blocked passwords section.
  ASSERT_TRUE(deleteItemAndWait(GetSectionIndex(SavedPasswords), 0, ^{
    return NumberOfItemsInSection(GetSectionIndex(SavedPasswords)) == 1;
  }));
  // There should be no password sections remaining and no search bar.
  EXPECT_TRUE(deleteItemAndWait(GetSectionIndex(SavedPasswords), 0, ^{
    return NumberOfSections() == (2 + +SectionsOffset());
  }));
}

// Tests deleting items from saved passwords and blocked passwords sections
// when there are duplicates in the store.
TEST_P(PasswordsTableViewControllerTest, DeleteItemsWithDuplicates) {
  AddSavedForm1();
  AddSavedForm1();
  AddBlockedForm1();
  AddBlockedForm1();
  AddBlockedForm2();

  // Delete item in save passwords section.
  ASSERT_TRUE(deleteItemAndWait(GetSectionIndex(SavedPasswords), 0, ^{
    return NumberOfSections() == (3 + SectionsOffset());
  }));
  // Section 2 should now be the blocked passwords section, and should still
  // have both its items.
  EXPECT_EQ(2, NumberOfItemsInSection(GetSectionIndex(Blocked) - 1));

  // Delete item in blocked passwords section.
  ASSERT_TRUE(deleteItemAndWait(GetSectionIndex(Blocked) - 1, 0, ^{
    return NumberOfItemsInSection(GetSectionIndex(Blocked) - 1) == 1;
  }));
  // There should be no password sections remaining and no search bar.
  EXPECT_TRUE(deleteItemAndWait(GetSectionIndex(Blocked) - 1, 0, ^{
    return NumberOfSections() == (2 + SectionsOffset());
  }));
}

TEST_P(PasswordsTableViewControllerTest,
       TestExportButtonDisabledNoSavedPasswords) {
  PasswordsTableViewController* passwords_controller =
      static_cast<PasswordsTableViewController*>(controller());
  [passwords_controller updateExportPasswordsButton];

  TableViewDetailTextItem* exportButton =
      GetTableViewItem(GetSectionIndex(SavedPasswords), 0);
  CheckTextCellTextWithId(IDS_IOS_EXPORT_PASSWORDS,
                          GetSectionIndex(SavedPasswords), 0);

  EXPECT_NSEQ(UIColor.cr_labelColor, exportButton.textColor);
  EXPECT_TRUE(exportButton.accessibilityTraits &
              UIAccessibilityTraitNotEnabled);

  // Add blocked form.
  AddBlockedForm1();
  // The export button should still be disabled as exporting blocked forms
  // is not currently supported.
  EXPECT_NSEQ(UIColor.cr_labelColor, exportButton.textColor);
  EXPECT_TRUE(exportButton.accessibilityTraits &
              UIAccessibilityTraitNotEnabled);
}

TEST_P(PasswordsTableViewControllerTest,
       TestExportButtonEnabledWithSavedPasswords) {
  PasswordsTableViewController* passwords_controller =
      static_cast<PasswordsTableViewController*>(controller());
  AddSavedForm1();
  [passwords_controller updateExportPasswordsButton];

  TableViewDetailTextItem* exportButton =
      GetTableViewItem(GetSectionIndex(ExportPasswordsButton), 0);

  CheckTextCellTextWithId(IDS_IOS_EXPORT_PASSWORDS,
                          GetSectionIndex(ExportPasswordsButton), 0);

  EXPECT_NSEQ([UIColor colorNamed:kBlueColor], exportButton.textColor);
  EXPECT_FALSE(exportButton.accessibilityTraits &
               UIAccessibilityTraitNotEnabled);
}

// Tests that the "Export Passwords..." button is greyed out in edit mode.
TEST_P(PasswordsTableViewControllerTest, TestExportButtonDisabledEditMode) {
  PasswordsTableViewController* passwords_controller =
      static_cast<PasswordsTableViewController*>(controller());
  AddSavedForm1();
  [passwords_controller updateExportPasswordsButton];

  TableViewDetailTextItem* exportButton =
      GetTableViewItem(GetSectionIndex(ExportPasswordsButton), 0);
  CheckTextCellTextWithId(IDS_IOS_EXPORT_PASSWORDS,
                          GetSectionIndex(ExportPasswordsButton), 0);

  [passwords_controller setEditing:YES animated:NO];

  EXPECT_NSEQ(UIColor.cr_labelColor, exportButton.textColor);
  EXPECT_TRUE(exportButton.accessibilityTraits &
              UIAccessibilityTraitNotEnabled);
}

// Tests that the "Export Passwords..." button is enabled after exiting
// edit mode.
TEST_P(PasswordsTableViewControllerTest,
       TestExportButtonEnabledWhenEdittingFinished) {
  PasswordsTableViewController* passwords_controller =
      static_cast<PasswordsTableViewController*>(controller());
  AddSavedForm1();
  [passwords_controller updateExportPasswordsButton];

  TableViewDetailTextItem* exportButton =
      GetTableViewItem(GetSectionIndex(ExportPasswordsButton), 0);
  CheckTextCellTextWithId(IDS_IOS_EXPORT_PASSWORDS,
                          GetSectionIndex(ExportPasswordsButton), 0);

  [passwords_controller setEditing:YES animated:NO];
  [passwords_controller setEditing:NO animated:NO];

  EXPECT_NSEQ([UIColor colorNamed:kBlueColor], exportButton.textColor);
  EXPECT_FALSE(exportButton.accessibilityTraits &
               UIAccessibilityTraitNotEnabled);
}

TEST_P(PasswordsTableViewControllerTest, PropagateDeletionToStore) {
  PasswordsTableViewController* passwords_controller =
      static_cast<PasswordsTableViewController*>(controller());
  autofill::PasswordForm form;
  form.url = GURL("http://www.example.com/accounts/LoginAuth");
  form.action = GURL("http://www.example.com/accounts/Login");
  form.username_element = base::ASCIIToUTF16("Email");
  form.username_value = base::ASCIIToUTF16("test@egmail.com");
  form.password_element = base::ASCIIToUTF16("Passwd");
  form.password_value = base::ASCIIToUTF16("test");
  form.submit_element = base::ASCIIToUTF16("signIn");
  form.signon_realm = "http://www.example.com/";
  form.scheme = autofill::PasswordForm::Scheme::kHtml;
  form.blacklisted_by_user = false;

  AddPasswordForm(std::make_unique<autofill::PasswordForm>(form));

  EXPECT_CALL(GetMockStore(), RemoveLogin(form));
  [passwords_controller passwordDetailsTableViewController:nil
                                            deletePassword:form];
}

// Tests filtering of items.
TEST_P(PasswordsTableViewControllerTest, FilterItems) {
  AddSavedForm1();
  AddSavedForm2();
  AddBlockedForm1();
  AddBlockedForm2();

  EXPECT_EQ(4 + SectionsOffset(), NumberOfSections());

  PasswordsTableViewController* passwords_controller =
      static_cast<PasswordsTableViewController*>(controller());
  UISearchBar* bar =
      passwords_controller.navigationItem.searchController.searchBar;

  // Force the initial data to be rendered into view first, before doing any
  // new filtering (avoids mismatch when reloadSections is called).
  [passwords_controller searchBar:bar textDidChange:@""];

  // Search item in save passwords section.
  [passwords_controller searchBar:bar textDidChange:@"example.com"];
  // Only one item in saved passwords should remain.
  EXPECT_EQ(1, NumberOfItemsInSection(GetSectionIndex(SavedPasswords)));
  EXPECT_EQ(0, NumberOfItemsInSection(GetSectionIndex(Blocked)));
  CheckTextCellTextAndDetailText(@"example.com", @"test@egmail.com",
                                 GetSectionIndex(SavedPasswords), 0);

  [passwords_controller searchBar:bar textDidChange:@"test@egmail.com"];
  // Only two items in saved passwords should remain.
  EXPECT_EQ(2, NumberOfItemsInSection(GetSectionIndex(SavedPasswords)));
  EXPECT_EQ(0, NumberOfItemsInSection(GetSectionIndex(Blocked)));
  CheckTextCellTextAndDetailText(@"example.com", @"test@egmail.com",
                                 GetSectionIndex(SavedPasswords), 0);
  CheckTextCellTextAndDetailText(@"example2.com", @"test@egmail.com",
                                 GetSectionIndex(SavedPasswords), 1);

  [passwords_controller searchBar:bar textDidChange:@"secret"];
  // Only two blocked items should remain.
  EXPECT_EQ(0, NumberOfItemsInSection(GetSectionIndex(SavedPasswords)));
  EXPECT_EQ(2, NumberOfItemsInSection(GetSectionIndex(Blocked)));
  CheckTextCellText(@"secret.com", GetSectionIndex(Blocked), 0);
  CheckTextCellText(@"secret2.com", GetSectionIndex(Blocked), 1);

  [passwords_controller searchBar:bar textDidChange:@""];
  // All items should be back.
  EXPECT_EQ(2, NumberOfItemsInSection(GetSectionIndex(SavedPasswords)));
  EXPECT_EQ(2, NumberOfItemsInSection(GetSectionIndex(Blocked)));
}

// Test verifies disabled state of password check cell.
TEST_P(PasswordsTableViewControllerTest, PasswordCheckStateDisabled) {
  if (!GetParam().password_check_enabled)
    return;
  ChangePasswordCheckState(PasswordCheckStateDisabled);

  CheckDetailItemTextWithIds(IDS_IOS_CHECK_PASSWORDS,
                             IDS_IOS_CHECK_PASSWORDS_DESCRIPTION,
                             GetSectionIndex(PasswordCheck), 0);
  SettingsPasswordCheckItem* checkPassword =
      GetTableViewItem(GetSectionIndex(PasswordCheck), 0);
  EXPECT_FALSE(checkPassword.enabled);
  EXPECT_TRUE(checkPassword.indicatorHidden);
  EXPECT_FALSE(checkPassword.image);
}

// Test verifies default state of password check cell.
TEST_P(PasswordsTableViewControllerTest, PasswordCheckStateDefault) {
  if (!GetParam().password_check_enabled)
    return;

  ChangePasswordCheckState(PasswordCheckStateDefault);

  CheckDetailItemTextWithIds(IDS_IOS_CHECK_PASSWORDS,
                             IDS_IOS_CHECK_PASSWORDS_DESCRIPTION,
                             GetSectionIndex(PasswordCheck), 0);
  SettingsPasswordCheckItem* checkPassword =
      GetTableViewItem(GetSectionIndex(PasswordCheck), 0);
  EXPECT_TRUE(checkPassword.enabled);
  EXPECT_TRUE(checkPassword.indicatorHidden);
  EXPECT_FALSE(checkPassword.image);
}

// Test verifies running state of password check cell.
TEST_P(PasswordsTableViewControllerTest, PasswordCheckStateRunning) {
  if (!GetParam().password_check_enabled)
    return;

  ChangePasswordCheckState(PasswordCheckStateRunning);

  CheckDetailItemTextWithIds(IDS_IOS_CHECK_PASSWORDS,
                             IDS_IOS_CHECK_PASSWORDS_DESCRIPTION,
                             GetSectionIndex(PasswordCheck), 0);
  SettingsPasswordCheckItem* checkPassword =
      GetTableViewItem(GetSectionIndex(PasswordCheck), 0);
  EXPECT_TRUE(checkPassword.enabled);
  EXPECT_FALSE(checkPassword.indicatorHidden);
  EXPECT_FALSE(checkPassword.image);
}

// Test verifies clicking start triggers correct function in service.
TEST_P(PasswordsTableViewControllerTest, PasswordCheckStart) {
  if (!GetParam().password_check_enabled)
    return;

  PasswordsTableViewController* passwords_controller =
      static_cast<PasswordsTableViewController*>(controller());

  EXPECT_CALL(GetMockPasswordCheckService(), CheckUsernamePasswordPairs);

  [passwords_controller tableView:passwords_controller.tableView
          didSelectRowAtIndexPath:[NSIndexPath
                                      indexPathForItem:1
                                             inSection:GetSectionIndex(
                                                           PasswordCheck)]];
}

const std::vector<PasswordCheckFeatureStatus> kPasswordCheckFeatureStatusCases{
    // Passwords export disabled
    {FALSE},
    // Passwords export enabled
    {TRUE}};

INSTANTIATE_TEST_SUITE_P(PasswordCheckDisabledAndEnabled,
                         PasswordsTableViewControllerTest,
                         ::testing::ValuesIn(kPasswordCheckFeatureStatusCases));

}  // namespace

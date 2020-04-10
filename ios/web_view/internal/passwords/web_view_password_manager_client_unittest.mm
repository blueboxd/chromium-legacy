// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/passwords/web_view_password_manager_client.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/mock_password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "ios/web/public/test/scoped_testing_web_client.h"
#include "ios/web/public/test/web_task_environment.h"
#include "ios/web/public/web_client.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#include "ios/web_view/test/test_with_locale_and_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CWVTestPasswordManagerClientDelegate
    : NSObject <CWVPasswordManagerClientDelegate>
@end

@implementation CWVTestPasswordManagerClientDelegate {
  GURL _emptyURL;
  std::unique_ptr<ios_web_view::WebViewBrowserState> _browserState;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _browserState = std::make_unique<ios_web_view::WebViewBrowserState>(
        /*off_the_record=*/false);
  }
  return self;
}

- (void)showSavePasswordInfoBar:
    (std::unique_ptr<password_manager::PasswordFormManagerForUI>)formToSave {
}

- (void)showUpdatePasswordInfoBar:
    (std::unique_ptr<password_manager::PasswordFormManagerForUI>)formToUpdate {
}

- (void)showAutosigninNotification:
    (std::unique_ptr<autofill::PasswordForm>)formSignedIn {
}

- (ios_web_view::WebViewBrowserState*)browserState {
  return _browserState.get();
}

- (web::WebState*)webState {
  return nullptr;
}

- (password_manager::PasswordManager*)passwordManager {
  return nullptr;
}

- (const GURL&)lastCommittedURL {
  return _emptyURL;
}

@end

namespace ios_web_view {

using testing::_;
using testing::Invoke;
using testing::Return;

class WebViewPasswordManagerClientTest : public TestWithLocaleAndResources {
 protected:
  WebViewPasswordManagerClientTest()
      : web_client_(std::make_unique<web::WebClient>()) {}
  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
};

TEST_F(WebViewPasswordManagerClientTest, NoPromptIfBlacklisted) {
  CWVTestPasswordManagerClientDelegate* test_delegate =
      [[CWVTestPasswordManagerClientDelegate alloc] init];
  WebViewPasswordManagerClient client(test_delegate);
  auto password_manager_for_ui =
      std::make_unique<password_manager::MockPasswordFormManagerForUI>();

  EXPECT_CALL(*password_manager_for_ui, IsBlacklisted()).WillOnce(Return(true));
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      password_manager::features::kEnablePasswordsAccountStorage);

  EXPECT_FALSE(client.PromptUserToSaveOrUpdatePassword(
      std::move(password_manager_for_ui), /*update_password=*/false));
}

TEST_F(WebViewPasswordManagerClientTest, NoPromptIfNotOptedInToAccountStorage) {
  CWVTestPasswordManagerClientDelegate* test_delegate =
      [[CWVTestPasswordManagerClientDelegate alloc] init];
  WebViewPasswordManagerClient client(test_delegate);
  auto password_manager_for_ui =
      std::make_unique<password_manager::MockPasswordFormManagerForUI>();

  EXPECT_CALL(*password_manager_for_ui, IsBlacklisted())
      .WillOnce(Return(false));
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndDisableFeature(
      password_manager::features::kEnablePasswordsAccountStorage);

  EXPECT_FALSE(client.PromptUserToSaveOrUpdatePassword(
      std::move(password_manager_for_ui), /*update_password=*/false));
}

// TODO(crbug.com/1069338): Write test that verifies it prompts if all
// conditions passes.

}  // namespace ios_web_view

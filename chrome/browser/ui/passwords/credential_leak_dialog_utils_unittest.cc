// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/credential_leak_dialog_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/origin.h"

using password_manager::CredentialLeakFlags;
using password_manager::CredentialLeakType;

namespace leak_dialog_utils {

namespace {

// Contains information that should be displayed on the leak dialog for
// specified |leak_type|.
const struct {
  // Specifies the test case.
  CredentialLeakType leak_type;
  // The rest of the fields specify what should be displayed for this test case.
  int accept_button_id;
  int close_button_id;
  int leak_message_id;
  int leak_title_id;
  bool should_show_close_button;
  bool should_check_passwords;
} kLeakTypesTestCases[] = {
    {password_manager::CreateLeakTypeFromBools(
         /*is_saved=*/false,
         /*is_reused=*/false,
         /*is_syncing=*/false),
     IDS_OK, IDS_CLOSE, IDS_CREDENTIAL_LEAK_CURRENT_SITE_MESSAGE,
     IDS_CREDENTIAL_LEAK_CURRENT_SITE_TITLE, false, false},
    {password_manager::CreateLeakTypeFromBools(
         /*is_saved=*/false,
         /*is_reused=*/false,
         /*is_syncing=*/true),
     IDS_OK, IDS_CLOSE, IDS_CREDENTIAL_LEAK_CURRENT_SITE_MESSAGE,
     IDS_CREDENTIAL_LEAK_CURRENT_SITE_TITLE, false, false},
    {password_manager::CreateLeakTypeFromBools(
         /*is_saved=*/false,
         /*is_reused=*/true,
         /*is_syncing=*/true),
     IDS_LEAK_CHECK_CREDENTIALS, IDS_CLOSE,
     IDS_CREDENTIAL_LEAK_NOT_SAVED_MULTIPLE_SITES_MESSAGE,
     IDS_CREDENTIAL_LEAK_MULTIPLE_SITES_TITLE, true, true},
    {password_manager::CreateLeakTypeFromBools(
         /*is_saved=*/true,
         /*is_reused=*/false,
         /*is_syncing=*/true),
     IDS_OK, IDS_CLOSE, IDS_CREDENTIAL_LEAK_CURRENT_SITE_MESSAGE,
     IDS_CREDENTIAL_LEAK_CURRENT_SITE_TITLE, false, false},
    {password_manager::CreateLeakTypeFromBools(
         /*is_saved=*/true,
         /*is_reused=*/true,
         /*is_syncing=*/true),
     IDS_LEAK_CHECK_CREDENTIALS, IDS_CLOSE,
     IDS_CREDENTIAL_LEAK_SAVED_MULTIPLE_SITES_MESSAGE,
     IDS_CREDENTIAL_LEAK_MULTIPLE_SITES_TITLE, true, true}};
}  // namespace

TEST(CredentialLeakDialogUtilsTest, GetAcceptButtonLabel) {
  for (size_t i = 0; i < base::size(kLeakTypesTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    EXPECT_EQ(
        l10n_util::GetStringUTF16(kLeakTypesTestCases[i].accept_button_id),
        GetAcceptButtonLabel(kLeakTypesTestCases[i].leak_type));
  }
}

TEST(CredentialLeakDialogUtilsTest, GetCloseButtonLabel) {
  for (size_t i = 0; i < base::size(kLeakTypesTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    EXPECT_EQ(l10n_util::GetStringUTF16(kLeakTypesTestCases[i].close_button_id),
              GetCloseButtonLabel());
  }
}

TEST(CredentialLeakDialogUtilsTest, GetDescription) {
  GURL origin("https://example.com");
  for (size_t i = 0; i < base::size(kLeakTypesTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    auto expected_message =
        kLeakTypesTestCases[i].leak_message_id ==
                IDS_CREDENTIAL_LEAK_SAVED_MULTIPLE_SITES_MESSAGE
            ? l10n_util::GetStringUTF16(kLeakTypesTestCases[i].leak_message_id)
            : l10n_util::GetStringFUTF16(
                  kLeakTypesTestCases[i].leak_message_id,
                  url_formatter::FormatOriginForSecurityDisplay(
                      url::Origin::Create(origin),
                      url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS));
    EXPECT_EQ(expected_message,
              GetDescription(kLeakTypesTestCases[i].leak_type, origin));
  }
}

TEST(CredentialLeakDialogUtilsTest, GetTitle) {
  for (size_t i = 0; i < base::size(kLeakTypesTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    EXPECT_EQ(l10n_util::GetStringUTF16(kLeakTypesTestCases[i].leak_title_id),
              GetTitle(kLeakTypesTestCases[i].leak_type));
  }
}

TEST(CredentialLeakDialogUtilsTest, ShouldCheckPasswords) {
  for (size_t i = 0; i < base::size(kLeakTypesTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    EXPECT_EQ(kLeakTypesTestCases[i].should_check_passwords,
              ShouldCheckPasswords(kLeakTypesTestCases[i].leak_type));
  }
}

TEST(CredentialLeakDialogUtilsTest, ShouldShowCloseButton) {
  for (size_t i = 0; i < base::size(kLeakTypesTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    EXPECT_EQ(kLeakTypesTestCases[i].should_show_close_button,
              ShouldShowCloseButton(kLeakTypesTestCases[i].leak_type));
  }
}

}  // namespace leak_dialog_utils

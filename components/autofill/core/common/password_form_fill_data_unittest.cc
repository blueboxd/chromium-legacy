// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/password_form_fill_data.h"

#include <map>
#include <memory>
#include <ostream>
#include <utility>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/renderer_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using base::UTF16ToASCII;
using Store = autofill::PasswordForm::Store;

namespace autofill {

namespace {

constexpr char kPreferredUsername[] = "test@gmail.com";
constexpr char kPreferredPassword[] = "password";
constexpr char kPreferredAlternatePassword[] = "new_password";

constexpr char kDuplicateLocalUsername[] = "local@gmail.com";
constexpr char kDuplicateLocalPassword[] = "local_password";

constexpr char kSyncedUsername[] = "synced@gmail.com";
constexpr char kSyncedPassword[] = "password";

PasswordForm CreateForm(std::string username,
                        std::string password,
                        Store store) {
  PasswordForm form;
  form.username_value = ASCIIToUTF16(username);
  form.password_value = ASCIIToUTF16(password);
  form.in_store = store;
  return form;
}

MATCHER_P3(IsLogin, username, password, uses_account_store, std::string()) {
  return UTF16ToASCII(arg.username) == username &&
         UTF16ToASCII(arg.password) == password &&
         arg.uses_account_store == uses_account_store;
}

PasswordFormFillData::LoginCollection::const_iterator FindPasswordByUsername(
    const std::vector<autofill::PasswordAndMetadata>& logins,
    const base::string16& username) {
  return std::find_if(logins.begin(), logins.end(),
                      [&username](const autofill::PasswordAndMetadata& login) {
                        return login.username == username;
                      });
}

}  // namespace

void PrintTo(const PasswordAndMetadata& login, std::ostream* out) {
  *out << "(" + UTF16ToASCII(login.username) + ", " +
              UTF16ToASCII(login.password) + ", " +
              (login.uses_account_store ? "true" : "false") + ")";
}

void PrintTo(PasswordFormFillData::LoginCollection logins, std::ostream* out) {
  *out << "[\n";
  for (const PasswordAndMetadata& login : logins) {
    PrintTo(login, out);
    *out << ",\n";
  }
  *out << "]";
}

// Tests that the when there is a single preferred match, and no extra
// matches, the PasswordFormFillData is filled in correctly.
TEST(PasswordFormFillDataTest, TestSinglePreferredMatch) {
  // Create the current form on the page.
  PasswordForm form_on_page;
  form_on_page.origin = GURL("https://foo.com/");
  form_on_page.action = GURL("https://foo.com/login");
  form_on_page.username_element = ASCIIToUTF16("username");
  form_on_page.username_value = ASCIIToUTF16(kPreferredUsername);
  form_on_page.password_element = ASCIIToUTF16("password");
  form_on_page.password_value = ASCIIToUTF16(kPreferredPassword);
  form_on_page.submit_element = ASCIIToUTF16("");
  form_on_page.signon_realm = "https://foo.com/";
  form_on_page.scheme = PasswordForm::Scheme::kHtml;

  // Create an exact match in the database.
  PasswordForm preferred_match;
  preferred_match.origin = GURL("https://foo.com/");
  preferred_match.action = GURL("https://foo.com/login");
  preferred_match.username_element = ASCIIToUTF16("username");
  preferred_match.username_value = ASCIIToUTF16(kPreferredUsername);
  preferred_match.password_element = ASCIIToUTF16("password");
  preferred_match.password_value = ASCIIToUTF16(kPreferredPassword);
  preferred_match.submit_element = ASCIIToUTF16("");
  preferred_match.signon_realm = "https://foo.com/";
  preferred_match.scheme = PasswordForm::Scheme::kHtml;

  std::vector<const PasswordForm*> matches;

  PasswordFormFillData result(form_on_page, matches, preferred_match, true);

  // |wait_for_username| should reflect the |wait_for_username| argument passed
  // to the constructor, which in this case is true.
  EXPECT_TRUE(result.wait_for_username);
  // The preferred realm should be empty since it's the same as the realm of
  // the form.
  EXPECT_EQ(std::string(), result.preferred_realm);

  PasswordFormFillData result2(form_on_page, matches, preferred_match, false);

  // |wait_for_username| should reflect the |wait_for_username| argument passed
  // to the constructor, which in this case is false.
  EXPECT_FALSE(result2.wait_for_username);
}

// Tests that constructing a PasswordFormFillData behaves correctly when there
// is a preferred match that was found using public suffix matching, an
// additional result that also used public suffix matching, and a third result
// that was found without using public suffix matching.
TEST(PasswordFormFillDataTest, TestPublicSuffixDomainMatching) {
  // Create the current form on the page.
  PasswordForm form_on_page;
  form_on_page.origin = GURL("https://foo.com/");
  form_on_page.action = GURL("https://foo.com/login");
  form_on_page.username_element = ASCIIToUTF16("username");
  form_on_page.username_value = ASCIIToUTF16(kPreferredUsername);
  form_on_page.password_element = ASCIIToUTF16("password");
  form_on_page.password_value = ASCIIToUTF16(kPreferredPassword);
  form_on_page.submit_element = ASCIIToUTF16("");
  form_on_page.signon_realm = "https://foo.com/";
  form_on_page.scheme = PasswordForm::Scheme::kHtml;

  // Create a match from the database that matches using public suffix.
  PasswordForm preferred_match;
  preferred_match.origin = GURL("https://mobile.foo.com/");
  preferred_match.action = GURL("https://mobile.foo.com/login");
  preferred_match.username_element = ASCIIToUTF16("username");
  preferred_match.username_value = ASCIIToUTF16(kPreferredUsername);
  preferred_match.password_element = ASCIIToUTF16("password");
  preferred_match.password_value = ASCIIToUTF16(kPreferredPassword);
  preferred_match.submit_element = ASCIIToUTF16("");
  preferred_match.signon_realm = "https://foo.com/";
  preferred_match.is_public_suffix_match = true;
  preferred_match.scheme = PasswordForm::Scheme::kHtml;

  // Create a match that matches exactly, so |is_public_suffix_match| has a
  // default value false.
  PasswordForm exact_match;
  exact_match.origin = GURL("https://foo.com/");
  exact_match.action = GURL("https://foo.com/login");
  exact_match.username_element = ASCIIToUTF16("username");
  exact_match.username_value = ASCIIToUTF16("test1@gmail.com");
  exact_match.password_element = ASCIIToUTF16("password");
  exact_match.password_value = ASCIIToUTF16(kPreferredPassword);
  exact_match.submit_element = ASCIIToUTF16("");
  exact_match.signon_realm = "https://foo.com/";
  exact_match.scheme = PasswordForm::Scheme::kHtml;

  // Create a match that was matched using public suffix, so
  // |is_public_suffix_match| == true.
  PasswordForm public_suffix_match;
  public_suffix_match.origin = GURL("https://foo.com/");
  public_suffix_match.action = GURL("https://foo.com/login");
  public_suffix_match.username_element = ASCIIToUTF16("username");
  public_suffix_match.username_value = ASCIIToUTF16("test2@gmail.com");
  public_suffix_match.password_element = ASCIIToUTF16("password");
  public_suffix_match.password_value = ASCIIToUTF16(kPreferredPassword);
  public_suffix_match.submit_element = ASCIIToUTF16("");
  public_suffix_match.is_public_suffix_match = true;
  public_suffix_match.signon_realm = "https://foo.com/";
  public_suffix_match.scheme = PasswordForm::Scheme::kHtml;

  // Add one exact match and one public suffix match.
  std::vector<const PasswordForm*> matches = {&exact_match,
                                              &public_suffix_match};

  PasswordFormFillData result(form_on_page, matches, preferred_match, true);
  EXPECT_TRUE(result.wait_for_username);
  // The preferred realm should match the signon realm from the
  // preferred match so the user can see where the result came from.
  EXPECT_EQ(preferred_match.signon_realm, result.preferred_realm);

  // The realm of the exact match should be empty.
  PasswordFormFillData::LoginCollection::const_iterator iter =
      FindPasswordByUsername(result.additional_logins,
                             exact_match.username_value);
  EXPECT_EQ(std::string(), iter->realm);

  // The realm of the public suffix match should be set to the original signon
  // realm so the user can see where the result came from.
  iter = FindPasswordByUsername(result.additional_logins,
                                public_suffix_match.username_value);
  EXPECT_EQ(iter->realm, public_suffix_match.signon_realm);
}

// Tests that the constructing a PasswordFormFillData behaves correctly when
// there is a preferred match that was found using affiliation based matching,
// an additional result that also used affiliation based matching, and a third
// result that was found without using affiliation based matching.
TEST(PasswordFormFillDataTest, TestAffiliationMatch) {
  // Create the current form on the page.
  PasswordForm form_on_page;
  form_on_page.origin = GURL("https://foo.com/");
  form_on_page.action = GURL("https://foo.com/login");
  form_on_page.username_element = ASCIIToUTF16("username");
  form_on_page.username_value = ASCIIToUTF16(kPreferredUsername);
  form_on_page.password_element = ASCIIToUTF16("password");
  form_on_page.password_value = ASCIIToUTF16(kPreferredPassword);
  form_on_page.submit_element = ASCIIToUTF16("");
  form_on_page.signon_realm = "https://foo.com/";
  form_on_page.scheme = PasswordForm::Scheme::kHtml;

  // Create a match from the database that matches using affiliation.
  PasswordForm preferred_match;
  preferred_match.origin = GURL("android://hash@foo.com/");
  preferred_match.username_value = ASCIIToUTF16(kPreferredUsername);
  preferred_match.password_value = ASCIIToUTF16(kPreferredPassword);
  preferred_match.signon_realm = "android://hash@foo.com/";
  preferred_match.is_affiliation_based_match = true;

  // Create a match that matches exactly, so |is_affiliation_based_match| has a
  // default value false.
  PasswordForm exact_match;
  exact_match.origin = GURL("https://foo.com/");
  exact_match.action = GURL("https://foo.com/login");
  exact_match.username_element = ASCIIToUTF16("username");
  exact_match.username_value = ASCIIToUTF16("test1@gmail.com");
  exact_match.password_element = ASCIIToUTF16("password");
  exact_match.password_value = ASCIIToUTF16(kPreferredPassword);
  exact_match.submit_element = ASCIIToUTF16("");
  exact_match.signon_realm = "https://foo.com/";
  exact_match.scheme = PasswordForm::Scheme::kHtml;

  // Create a match that was matched using public suffix, so
  // |is_public_suffix_match| == true.
  PasswordForm affiliated_match;
  affiliated_match.origin = GURL("android://hash@foo1.com/");
  affiliated_match.username_value = ASCIIToUTF16("test2@gmail.com");
  affiliated_match.password_value = ASCIIToUTF16(kPreferredPassword);
  affiliated_match.is_affiliation_based_match = true;
  affiliated_match.signon_realm = "https://foo1.com/";
  affiliated_match.scheme = PasswordForm::Scheme::kHtml;

  // Add one exact match and one affiliation based match.
  std::vector<const PasswordForm*> matches = {&exact_match, &affiliated_match};

  PasswordFormFillData result(form_on_page, matches, preferred_match, false);
  EXPECT_FALSE(result.wait_for_username);
  // The preferred realm should match the signon realm from the
  // preferred match so the user can see where the result came from.
  EXPECT_EQ(preferred_match.signon_realm, result.preferred_realm);

  // The realm of the exact match should be empty.
  PasswordFormFillData::LoginCollection::const_iterator iter =
      FindPasswordByUsername(result.additional_logins,
                             exact_match.username_value);
  EXPECT_EQ(std::string(), iter->realm);

  // The realm of the affiliation based match should be set to the original
  // signon realm so the user can see where the result came from.
  iter = FindPasswordByUsername(result.additional_logins,
                                affiliated_match.username_value);
  EXPECT_EQ(iter->realm, affiliated_match.signon_realm);
}

// Tests that renderer ids are passed correctly.
TEST(PasswordFormFillDataTest, RendererIDs) {
  // Create the current form on the page.
  PasswordForm form_on_page;
  form_on_page.origin = GURL("https://foo.com/");
  form_on_page.action = GURL("https://foo.com/login");
  form_on_page.username_element = ASCIIToUTF16("username");
  form_on_page.password_element = ASCIIToUTF16("password");
  form_on_page.username_may_use_prefilled_placeholder = true;

  // Create an exact match in the database.
  PasswordForm preferred_match = form_on_page;
  preferred_match.username_value = ASCIIToUTF16(kPreferredUsername);
  preferred_match.password_value = ASCIIToUTF16(kPreferredPassword);

  // Set renderer id related fields.
  FormData form_data;
  form_data.unique_renderer_id = FormRendererId(42);
  form_data.is_form_tag = true;
  form_on_page.form_data = form_data;
  form_on_page.has_renderer_ids = true;
  form_on_page.username_element_renderer_id = FieldRendererId(123);
  form_on_page.password_element_renderer_id = FieldRendererId(456);

  PasswordFormFillData result(form_on_page, {}, preferred_match, true);

  EXPECT_EQ(form_data.unique_renderer_id, result.form_renderer_id);
  EXPECT_EQ(form_on_page.has_renderer_ids, result.has_renderer_ids);
  EXPECT_EQ(form_on_page.username_element_renderer_id,
            result.username_field.unique_renderer_id);
  EXPECT_EQ(form_on_page.password_element_renderer_id,
            result.password_field.unique_renderer_id);
  EXPECT_TRUE(result.username_may_use_prefilled_placeholder);
}

// Tests that nor username nor password fields are set when password element is
// not found.
TEST(PasswordFormFillDataTest, NoPasswordElement) {
  // Create the current form on the page.
  PasswordForm form_on_page;
  form_on_page.origin = GURL("https://foo.com/");
  form_on_page.has_renderer_ids = true;
  form_on_page.username_element_renderer_id = FieldRendererId(123);
  // Set no password element.
  form_on_page.password_element_renderer_id = FieldRendererId();
  form_on_page.new_password_element_renderer_id = FieldRendererId(456);

  // Create an exact match in the database.
  PasswordForm preferred_match = form_on_page;
  preferred_match.username_value = ASCIIToUTF16(kPreferredUsername);
  preferred_match.password_value = ASCIIToUTF16(kPreferredPassword);

  FormData form_data;
  form_data.unique_renderer_id = FormRendererId(42);
  form_data.is_form_tag = true;
  form_on_page.form_data = form_data;

  PasswordFormFillData result(form_on_page, {} /* matches */, preferred_match,
                              true);

  // Check that nor username nor password fields are set.
  EXPECT_EQ(true, result.has_renderer_ids);
  EXPECT_TRUE(result.username_field.unique_renderer_id.is_null());
  EXPECT_TRUE(result.password_field.unique_renderer_id.is_null());
}

// Tests that matches are retained without duplicates.
TEST(PasswordFormFillDataTest, DeduplicatesFillData) {
  // Create the current form on the page.
  PasswordForm form;
  form.username_element = ASCIIToUTF16("username");
  form.password_element = ASCIIToUTF16("password");

  // Create an exact match in the database.
  PasswordForm preferred_match = form;
  preferred_match.username_value = ASCIIToUTF16(kPreferredUsername);
  preferred_match.password_value = ASCIIToUTF16(kPreferredPassword);
  preferred_match.in_store = Store::kProfileStore;

  // Create two discarded and one retained duplicate.
  const PasswordForm duplicate_of_preferred =
      CreateForm(kPreferredUsername, kPreferredPassword, Store::kProfileStore);
  const PasswordForm account_duplicate_of_preferred =
      CreateForm(kPreferredUsername, kPreferredPassword, Store::kAccountStore);
  const PasswordForm non_duplicate_of_preferred = CreateForm(
      kPreferredUsername, kPreferredAlternatePassword, Store::kAccountStore);

  // Create a local password and its discarded duplicate.
  const PasswordForm local = CreateForm(
      kDuplicateLocalUsername, kDuplicateLocalPassword, Store::kProfileStore);
  const PasswordForm duplicate_of_local = local;

  // Create a synced password and its discarded local duplicate.
  const PasswordForm remote =
      CreateForm(kSyncedUsername, kSyncedPassword, Store::kProfileStore);
  const PasswordForm duplicate_of_remote =
      CreateForm(kSyncedUsername, kSyncedPassword, Store::kAccountStore);

  PasswordFormFillData result(
      form,
      {&duplicate_of_preferred, &account_duplicate_of_preferred,
       &non_duplicate_of_preferred, &local, &duplicate_of_local, &remote,
       &duplicate_of_remote},
      preferred_match, true);

  EXPECT_EQ(preferred_match.username_value, result.username_field.value);
  EXPECT_EQ(preferred_match.password_value, result.password_field.value);
  EXPECT_TRUE(result.uses_account_store);
  EXPECT_THAT(
      result.additional_logins,
      testing::ElementsAre(
          IsLogin(kPreferredUsername, kPreferredAlternatePassword, true),
          IsLogin(kDuplicateLocalUsername, kDuplicateLocalPassword, false),
          IsLogin(kSyncedUsername, kSyncedPassword, true)));
}

}  // namespace autofill

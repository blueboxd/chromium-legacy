// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/password_form_fill_data.h"

#include <tuple>
#include <unordered_set>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

namespace {

bool IsPublicSuffixMatchOrAffiliationBasedMatch(const PasswordForm& form) {
  return form.is_public_suffix_match || form.is_affiliation_based_match;
}

// Finds any suggestion in |login| whose username and password match the |form|.
PasswordFormFillData::LoginCollection::iterator FindDuplicate(
    PasswordFormFillData::LoginCollection* logins,
    const PasswordForm& form) {
  return std::find_if(logins->begin(), logins->end(),
                      [&form](const PasswordAndMetadata& login) {
                        return (form.username_value == login.username &&
                                form.password_value == login.password);
                      });
}

// This function takes a |duplicate_form| and the realm and uses_account_store
// properties of an existing suggestion. Both suggestions have identical
// username and password.
// If the duplicate should replace the existing suggestion, this method
// overrides the realm and uses_account_store properties to achieve that.
void MaybeReplaceRealmAndStoreWithDuplicate(const PasswordForm& duplicate_form,
                                            std::string* existing_realm,
                                            bool* existing_uses_account_store) {
  DCHECK(existing_realm);
  DCHECK(existing_uses_account_store);
  if (*existing_uses_account_store)
    return;  // No need to replace existing account-stored suggestion.
  if (!duplicate_form.IsUsingAccountStore())
    return;  // No need to replace a local suggestion with identical other one.
  if (IsPublicSuffixMatchOrAffiliationBasedMatch(duplicate_form))
    return;  // Never replace a possibly exact match with a PSL match.
  *existing_uses_account_store = duplicate_form.IsUsingAccountStore();
  existing_realm->clear();  // Reset realm since form cannot be a psl match.
}

}  // namespace

PasswordAndMetadata::PasswordAndMetadata() = default;
PasswordAndMetadata::PasswordAndMetadata(const PasswordAndMetadata&) = default;
PasswordAndMetadata::PasswordAndMetadata(PasswordAndMetadata&&) = default;
PasswordAndMetadata& PasswordAndMetadata::operator=(
    const PasswordAndMetadata&) = default;
PasswordAndMetadata& PasswordAndMetadata::operator=(PasswordAndMetadata&&) =
    default;
PasswordAndMetadata::~PasswordAndMetadata() = default;

PasswordFormFillData::PasswordFormFillData() = default;

PasswordFormFillData::PasswordFormFillData(
    const PasswordForm& form_on_page,
    const std::vector<const PasswordForm*>& matches,
    const PasswordForm& preferred_match,
    bool wait_for_username)
    : form_renderer_id(form_on_page.form_data.unique_renderer_id),
      name(form_on_page.form_data.name),
      origin(form_on_page.origin),
      action(form_on_page.action),
      uses_account_store(preferred_match.IsUsingAccountStore()),
      wait_for_username(wait_for_username),
      has_renderer_ids(form_on_page.has_renderer_ids) {
  // Note that many of the |FormFieldData| members are not initialized for
  // |username_field| and |password_field| because they are currently not used
  // by the password autocomplete code.
  username_field.value = preferred_match.username_value;
  password_field.value = preferred_match.password_value;
  if (!form_on_page.only_for_fallback &&
      (form_on_page.HasPasswordElement() || form_on_page.IsSingleUsername())) {
    // Fill fields identifying information only for non-fallback case when
    // password element is found. In other cases a fill popup is shown on
    // clicking on each password field so no need in any field identifiers.
    username_field.name = form_on_page.username_element;
    username_field.unique_renderer_id =
        form_on_page.username_element_renderer_id;
    username_may_use_prefilled_placeholder =
        form_on_page.username_may_use_prefilled_placeholder;

    password_field.name = form_on_page.password_element;
    password_field.unique_renderer_id =
        form_on_page.password_element_renderer_id;
    password_field.form_control_type = "password";

    // On iOS, use the unique_id field to refer to elements.
#if defined(OS_IOS)
    username_field.unique_id = form_on_page.username_element;
    password_field.unique_id = form_on_page.password_element;
#endif
  }

  if (IsPublicSuffixMatchOrAffiliationBasedMatch(preferred_match))
    preferred_realm = preferred_match.signon_realm;

  // Copy additional username/value pairs.
  for (const PasswordForm* match : matches) {
    // If any already retained suggestion matches the login, discard the login
    // or override the existing duplicate with the account-stored match.
    if (match->username_value == preferred_match.username_value &&
        match->password_value == preferred_match.password_value) {
      MaybeReplaceRealmAndStoreWithDuplicate(*match, &preferred_realm,
                                             &uses_account_store);
      continue;
    }
    auto duplicate_iter = FindDuplicate(&additional_logins, *match);
    if (duplicate_iter != additional_logins.end()) {
      MaybeReplaceRealmAndStoreWithDuplicate(
          *match, &duplicate_iter->realm, &duplicate_iter->uses_account_store);
      continue;
    }
    PasswordAndMetadata value;
    value.username = match->username_value;
    value.password = match->password_value;
    value.uses_account_store = match->IsUsingAccountStore();
    if (IsPublicSuffixMatchOrAffiliationBasedMatch(*match))
      value.realm = match->signon_realm;
    additional_logins.push_back(std::move(value));
  }
}

PasswordFormFillData::PasswordFormFillData(const PasswordFormFillData&) =
    default;

PasswordFormFillData& PasswordFormFillData::operator=(
    const PasswordFormFillData&) = default;

PasswordFormFillData::PasswordFormFillData(PasswordFormFillData&&) = default;

PasswordFormFillData& PasswordFormFillData::operator=(PasswordFormFillData&&) =
    default;

PasswordFormFillData::~PasswordFormFillData() = default;

PasswordFormFillData MaybeClearPasswordValues(
    const PasswordFormFillData& data) {
  // In case when there is a username on a page (for example in a hidden field),
  // credentials from |additional_logins| could be used for filling on load. So
  // in case of filling on load nor |password_field| nor |additional_logins|
  // can't be cleared
  bool is_fallback =
      data.has_renderer_ids && data.password_field.unique_renderer_id.is_null();
  if (!data.wait_for_username && !is_fallback)
    return data;
  PasswordFormFillData result(data);
  result.password_field.value.clear();
  for (auto& credentials : result.additional_logins)
    credentials.password.clear();
  return result;
}

}  // namespace autofill

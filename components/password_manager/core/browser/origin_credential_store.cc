// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/origin_credential_store.h"

#include <utility>
#include <vector>

#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "url/gurl.h"

using autofill::PasswordForm;

namespace password_manager {
namespace {

GURL GetAndroidOrOriginURL(const GURL& url) {
  if (IsValidAndroidFacetURI(url.spec()))
    return url;  // Pass android origins as they are.
  return url.GetOrigin();
}

}  // namespace

UiCredential::UiCredential(base::string16 username,
                           base::string16 password,
                           const GURL& origin_url,
                           IsPublicSuffixMatch is_public_suffix_match)
    : username_(std::move(username)),
      password_(std::move(password)),
      origin_url_(GetAndroidOrOriginURL(origin_url)),
      is_public_suffix_match_(is_public_suffix_match) {}

UiCredential::UiCredential(const PasswordForm& form)
    : username_(form.username_value),
      password_(form.password_value),
      origin_url_(GetAndroidOrOriginURL(form.origin)),
      is_public_suffix_match_(form.is_public_suffix_match) {}

UiCredential::UiCredential(UiCredential&&) = default;
UiCredential::UiCredential(const UiCredential&) = default;
UiCredential& UiCredential::operator=(UiCredential&&) = default;
UiCredential& UiCredential::operator=(const UiCredential&) = default;
UiCredential::~UiCredential() = default;

bool operator==(const UiCredential& lhs, const UiCredential& rhs) {
  return lhs.username() == rhs.username() && lhs.password() == rhs.password() &&
         lhs.origin_url() == rhs.origin_url() &&
         lhs.is_public_suffix_match() == rhs.is_public_suffix_match();
}

std::ostream& operator<<(std::ostream& os, const UiCredential& credential) {
  os << "(user: \"" << credential.username() << "\", "
     << "pwd: \"" << credential.password() << "\", "
     << "origin: \"" << credential.origin_url() << "\", "
     << (credential.is_public_suffix_match() ? "PSL-" : "exact origin ")
     << "match)";
  return os;
}

OriginCredentialStore::OriginCredentialStore(url::Origin origin)
    : origin_(std::move(origin)) {}
OriginCredentialStore::~OriginCredentialStore() = default;

void OriginCredentialStore::SaveCredentials(
    std::vector<UiCredential> credentials) {
  credentials_ = std::move(credentials);
}

base::span<const UiCredential> OriginCredentialStore::GetCredentials() const {
  return credentials_;
}

void OriginCredentialStore::ClearCredentials() {
  credentials_.clear();
}

}  // namespace password_manager

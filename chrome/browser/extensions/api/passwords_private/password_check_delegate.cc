// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/password_check_delegate.h"
#include <memory>

#include "base/containers/flat_set.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "chrome/grit/generated_resources.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/ui/compromised_credentials_provider.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "url/gurl.h"

namespace extensions {

namespace {

using password_manager::CredentialWithPassword;
using ui::TimeFormat;
using CompromisedCredentialsView =
    password_manager::CompromisedCredentialsProvider::CredentialsView;
using SavedPasswordsView =
    password_manager::SavedPasswordsPresenter::SavedPasswordsView;

api::passwords_private::CompromiseType ConvertCompromiseType(
    password_manager::CompromiseType type) {
  switch (type) {
    case password_manager::CompromiseType::kLeaked:
      return api::passwords_private::COMPROMISE_TYPE_LEAKED;
    case password_manager::CompromiseType::kPhished:
      return api::passwords_private::COMPROMISE_TYPE_PHISHED;
  }

  NOTREACHED();
  return api::passwords_private::COMPROMISE_TYPE_NONE;
}

// Computes a map that matches compromised credentials with corresponding saved
// passwords in the store. This is needed to reflect changes to the underlying
// password store when a compromised credential gets modified or removed through
// the UI. Also it allows to inject affiliation information to be displayed in
// the UI.
PasswordCheckDelegate::CredentialPasswordsMap
MapCompromisedCredentialsToSavedPasswords(
    CompromisedCredentialsView compromised_credentials_view,
    SavedPasswordsView saved_passwords) {
  // Create a set to turn queries to look up a matching credential from O(n) to
  // O(log n).
  base::flat_set<CredentialWithPassword,
                 password_manager::PasswordCredentialLess>
      compromised_credentials(compromised_credentials_view.begin(),
                              compromised_credentials_view.end());

  // Populate the map. The values are vectors, because it is possible that
  // multiple saved passwords match to the same compromised credential. In most
  // cases there should be a 1-1 relationship, though.
  PasswordCheckDelegate::CredentialPasswordsMap passwords_map;
  for (const auto& saved_password : saved_passwords) {
    auto it = compromised_credentials.find(
        password_manager::CredentialView(saved_password));
    if (it != compromised_credentials.end()) {
      passwords_map[*it].push_back(saved_password);
    }
  }

  return passwords_map;
}

}  // namespace

PasswordCheckDelegate::PasswordCheckDelegate(Profile* profile)
    : profile_(profile),
      password_store_(PasswordStoreFactory::GetForProfile(
          profile,
          ServiceAccessType::EXPLICIT_ACCESS)),
      saved_passwords_presenter_(password_store_),
      compromised_credentials_provider_(password_store_,
                                        &saved_passwords_presenter_) {
  observed_compromised_credentials_provider_.Add(
      &compromised_credentials_provider_);

  // Instructs the presenter and provider to initialize and built their caches.
  // This will soon after invoke OnCompromisedCredentialsChanged(), which then
  // initializes |credentials_to_forms_| as well. Calls to
  // GetCompromisedCredentialsInfo() that might happen until then will return an
  // empty list.
  saved_passwords_presenter_.Init();
  compromised_credentials_provider_.Init();
}

PasswordCheckDelegate::~PasswordCheckDelegate() = default;

api::passwords_private::CompromisedCredentialsInfo
PasswordCheckDelegate::GetCompromisedCredentialsInfo() {
  CompromisedCredentialsView compromised_credentials_view =
      compromised_credentials_provider_.GetCompromisedCredentials();
  std::vector<CredentialWithPassword> ordered_compromised_credentials(
      compromised_credentials_view.begin(), compromised_credentials_view.end());
  std::sort(
      ordered_compromised_credentials.begin(),
      ordered_compromised_credentials.end(),
      [](const CredentialWithPassword& lhs, const CredentialWithPassword& rhs) {
        return std::tie(lhs.compromise_type, lhs.create_time) >
               std::tie(rhs.compromise_type, rhs.create_time);
      });

  api::passwords_private::CompromisedCredentialsInfo credentials_info;
  credentials_info.compromised_credentials.reserve(
      ordered_compromised_credentials.size());
  for (const auto& credential : ordered_compromised_credentials) {
    api::passwords_private::CompromisedCredential api_credential;
    auto facet = password_manager::FacetURI::FromPotentiallyInvalidSpec(
        credential.signon_realm);
    if (facet.IsValidAndroidFacetURI()) {
      // |formatted_orgin| and |change_password_url| need special handling for
      // Android. Here we use affiliation information instead of the
      // signon_realm.
      const autofill::PasswordForm& android_form =
          credentials_to_forms_.at(credential).at(0);
      api_credential.formatted_origin = android_form.app_display_name;
      api_credential.change_password_url =
          std::make_unique<std::string>(android_form.affiliated_web_realm);

      // In case no affiliation information could be obtained show the formatted
      // package name to the user. An empty change_password_url will be handled
      // by the frontend, by not including a link in this case.
      if (api_credential.formatted_origin.empty()) {
        api_credential.formatted_origin = l10n_util::GetStringFUTF8(
            IDS_SETTINGS_PASSWORDS_ANDROID_APP,
            base::UTF8ToUTF16(facet.android_package_name()));
      }
    } else {
      api_credential.formatted_origin =
          base::UTF16ToUTF8(url_formatter::FormatUrl(
              GURL(credential.signon_realm),
              url_formatter::kFormatUrlOmitDefaults |
                  url_formatter::kFormatUrlOmitHTTPS |
                  url_formatter::kFormatUrlOmitTrivialSubdomains |
                  url_formatter::kFormatUrlTrimAfterHost,
              net::UnescapeRule::SPACES, nullptr, nullptr, nullptr));
      api_credential.change_password_url =
          std::make_unique<std::string>(credential.signon_realm);
    }

    api_credential.id =
        compromised_credential_id_generator_.GenerateId(credential);
    api_credential.signon_realm = credential.signon_realm;
    api_credential.username = base::UTF16ToUTF8(credential.username);
    api_credential.elapsed_time_since_compromise = base::UTF16ToUTF8(
        TimeFormat::Simple(TimeFormat::FORMAT_ELAPSED, TimeFormat::LENGTH_LONG,
                           base::Time::Now() - credential.create_time));
    api_credential.compromise_type =
        ConvertCompromiseType(credential.compromise_type);
    credentials_info.compromised_credentials.push_back(
        std::move(api_credential));
  }

  return credentials_info;
}

base::Optional<api::passwords_private::CompromisedCredential>
PasswordCheckDelegate::GetPlaintextCompromisedPassword(
    api::passwords_private::CompromisedCredential credential) const {
  const CredentialWithPassword* compromised_credential =
      compromised_credential_id_generator_.TryGetKey(credential.id);
  if (!compromised_credential)
    return base::nullopt;

  if (credential.signon_realm != compromised_credential->signon_realm ||
      credential.username !=
          base::UTF16ToUTF8(compromised_credential->username)) {
    return base::nullopt;
  }

  credential.password = std::make_unique<std::string>(
      base::UTF16ToUTF8(compromised_credential->password));
  return credential;
}

void PasswordCheckDelegate::OnCompromisedCredentialsChanged(
    CompromisedCredentialsView credentials) {
  credentials_to_forms_ = MapCompromisedCredentialsToSavedPasswords(
      credentials, saved_passwords_presenter_.GetSavedPasswords());
  if (auto* event_router =
          PasswordsPrivateEventRouterFactory::GetForProfile(profile_)) {
    event_router->OnCompromisedCredentialsInfoChanged(
        GetCompromisedCredentialsInfo());
  }
}

}  // namespace extensions

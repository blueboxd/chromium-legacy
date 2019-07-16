// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_util.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/blacklisted_credentials_cleaner.h"
#include "components/password_manager/core/browser/credentials_cleaner.h"
#include "components/password_manager/core/browser/credentials_cleaner_runner.h"
#include "components/password_manager/core/browser/http_credentials_cleaner.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"

using autofill::PasswordForm;

namespace password_manager_util {
namespace {

// Return true if
// 1.|lhs| is non-PSL match, |rhs| is PSL match or
// 2.|lhs| and |rhs| have the same value of |is_public_suffix_match|, and |lhs|
// is preferred while |rhs| is not preferred.
bool IsBetterMatch(const PasswordForm* lhs, const PasswordForm* rhs) {
  return std::make_pair(!lhs->is_public_suffix_match, lhs->preferred) >
         std::make_pair(!rhs->is_public_suffix_match, rhs->preferred);
}

}  // namespace

// Update |credential| to reflect usage.
void UpdateMetadataForUsage(PasswordForm* credential) {
  ++credential->times_used;

  // Remove alternate usernames. At this point we assume that we have found
  // the right username.
  credential->all_possible_usernames.clear();
}

password_manager::SyncState GetPasswordSyncState(
    const syncer::SyncService* sync_service) {
  if (sync_service && sync_service->GetUserSettings()->IsFirstSetupComplete() &&
      sync_service->IsSyncFeatureActive() &&
      sync_service->GetActiveDataTypes().Has(syncer::PASSWORDS)) {
    return sync_service->GetUserSettings()->IsUsingSecondaryPassphrase()
               ? password_manager::SYNCING_WITH_CUSTOM_PASSPHRASE
               : password_manager::SYNCING_NORMAL_ENCRYPTION;
  }
  return password_manager::NOT_SYNCING;
}

bool IsSyncingWithNormalEncryption(const syncer::SyncService* sync_service) {
  return GetPasswordSyncState(sync_service) ==
         password_manager::SYNCING_NORMAL_ENCRYPTION;
}

void FindDuplicates(std::vector<std::unique_ptr<PasswordForm>>* forms,
                    std::vector<std::unique_ptr<PasswordForm>>* duplicates,
                    std::vector<std::vector<PasswordForm*>>* tag_groups) {
  if (forms->empty())
    return;

  // Linux backends used to treat the first form as a prime oneamong the
  // duplicates. Therefore, the caller should try to preserve it.
  std::stable_sort(forms->begin(), forms->end(), autofill::LessThanUniqueKey());

  std::vector<std::unique_ptr<PasswordForm>> unique_forms;
  unique_forms.push_back(std::move(forms->front()));
  if (tag_groups) {
    tag_groups->clear();
    tag_groups->push_back(std::vector<PasswordForm*>());
    tag_groups->front().push_back(unique_forms.front().get());
  }
  for (auto it = forms->begin() + 1; it != forms->end(); ++it) {
    if (ArePasswordFormUniqueKeysEqual(**it, *unique_forms.back())) {
      if (tag_groups)
        tag_groups->back().push_back(it->get());
      duplicates->push_back(std::move(*it));
    } else {
      if (tag_groups)
        tag_groups->push_back(std::vector<PasswordForm*>(1, it->get()));
      unique_forms.push_back(std::move(*it));
    }
  }
  forms->swap(unique_forms);
}

void TrimUsernameOnlyCredentials(
    std::vector<std::unique_ptr<PasswordForm>>* android_credentials) {
  // Remove username-only credentials which are not federated.
  base::EraseIf(*android_credentials,
                [](const std::unique_ptr<PasswordForm>& form) {
                  return form->scheme == PasswordForm::Scheme::kUsernameOnly &&
                         form->federation_origin.opaque();
                });

  // Set "skip_zero_click" on federated credentials.
  std::for_each(android_credentials->begin(), android_credentials->end(),
                [](const std::unique_ptr<PasswordForm>& form) {
                  if (form->scheme == PasswordForm::Scheme::kUsernameOnly)
                    form->skip_zero_click = true;
                });
}

bool IsLoggingActive(const password_manager::PasswordManagerClient* client) {
  const autofill::LogManager* log_manager = client->GetLogManager();
  return log_manager && log_manager->IsLoggingActive();
}

bool ManualPasswordGenerationEnabled(
    password_manager::PasswordManagerDriver* driver) {
#if defined(OS_ANDROID)
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kManualPasswordGenerationAndroid))
    return false;
#endif  // defined(OS_ANDROID)
  password_manager::PasswordGenerationFrameHelper* password_generation_manager =
      driver ? driver->GetPasswordGenerationHelper() : nullptr;
  if (!password_generation_manager ||
      !password_generation_manager->IsGenerationEnabled(false /*logging*/)) {
    return false;
  }

  LogPasswordGenerationEvent(
      autofill::password_generation::PASSWORD_GENERATION_CONTEXT_MENU_SHOWN);
  return true;
}

bool ShowAllSavedPasswordsContextMenuEnabled(
    password_manager::PasswordManagerDriver* driver) {
  password_manager::PasswordManager* password_manager =
      driver ? driver->GetPasswordManager() : nullptr;
  if (!password_manager)
    return false;

  password_manager::PasswordManagerClient* client = password_manager->client();
  if (!client ||
      !client->IsFillingFallbackEnabled(driver->GetLastCommittedURL()))
    return false;

  LogContextOfShowAllSavedPasswordsShown(
      password_manager::metrics_util::
          SHOW_ALL_SAVED_PASSWORDS_CONTEXT_CONTEXT_MENU);

  return true;
}

void UserTriggeredManualGenerationFromContextMenu(
    password_manager::PasswordManagerClient* password_manager_client) {
  password_manager_client->GeneratePassword();
  LogPasswordGenerationEvent(
      autofill::password_generation::PASSWORD_GENERATION_CONTEXT_MENU_PRESSED);
}

// TODO(http://crbug.com/890318): Add unitests to check cleaners are correctly
// created.
void RemoveUselessCredentials(
    scoped_refptr<password_manager::PasswordStore> store,
    PrefService* prefs,
    int delay_in_seconds,
    base::RepeatingCallback<network::mojom::NetworkContext*()>
        network_context_getter) {
  auto cleaning_tasks_runner =
      std::make_unique<password_manager::CredentialsCleanerRunner>();

#if !defined(OS_IOS)
  // Can be null for some unittests.
  if (!network_context_getter.is_null()) {
    cleaning_tasks_runner->MaybeAddCleaningTask(
        std::make_unique<password_manager::HttpCredentialCleaner>(
            store, network_context_getter, prefs));
  }
#endif  // !defined(OS_IOS)

  cleaning_tasks_runner->MaybeAddCleaningTask(
      std::make_unique<password_manager::BlacklistedCredentialsCleaner>(store,
                                                                        prefs));

  if (cleaning_tasks_runner->HasPendingTasks()) {
    // The runner will delete itself once the clearing tasks are done, thus we
    // are releasing ownership here.
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &password_manager::CredentialsCleanerRunner::StartCleaning,
            base::Unretained(cleaning_tasks_runner.release())),
        base::TimeDelta::FromSeconds(delay_in_seconds));
  }
}

base::StringPiece GetSignonRealmWithProtocolExcluded(const PasswordForm& form) {
  base::StringPiece signon_realm_protocol_excluded = form.signon_realm;

  // Find the web origin (with protocol excluded) in the signon_realm.
  const size_t after_protocol =
      signon_realm_protocol_excluded.find(form.origin.host_piece());
  DCHECK_NE(after_protocol, base::StringPiece::npos);

  // Keep the string starting with position |after_protocol|.
  signon_realm_protocol_excluded =
      signon_realm_protocol_excluded.substr(after_protocol);
  return signon_realm_protocol_excluded;
}

void FindBestMatches(
    std::vector<const PasswordForm*> matches,
    std::map<base::string16, const PasswordForm*>* best_matches,
    std::vector<const PasswordForm*>* not_best_matches,
    const PasswordForm** preferred_match) {
  DCHECK(std::all_of(
      matches.begin(), matches.end(),
      [](const PasswordForm* match) { return !match->blacklisted_by_user; }));
  DCHECK(best_matches);
  DCHECK(not_best_matches);
  DCHECK(preferred_match);

  *preferred_match = nullptr;
  best_matches->clear();
  not_best_matches->clear();

  if (matches.empty())
    return;

  // Sort matches using IsBetterMatch predicate.
  std::sort(matches.begin(), matches.end(), IsBetterMatch);
  for (const auto* match : matches) {
    const base::string16& username = match->username_value;
    // The first match for |username| in the sorted array is best match.
    if (best_matches->find(username) == best_matches->end())
      best_matches->insert(std::make_pair(username, match));
    else
      not_best_matches->push_back(match);
  }

  *preferred_match = *matches.begin();
}

const PasswordForm* GetMatchForUpdating(
    const PasswordForm& submitted_form,
    const std::map<base::string16, const PasswordForm*>& credentials) {
  // This is the case for the credential management API. It should not depend on
  // form managers. Once that's the case, this should be turned into a DCHECK.
  // TODO(crbug/947030): turn it into a DCHECK.
  if (!submitted_form.federation_origin.opaque())
    return nullptr;

  // Try to return form with matching |username_value|.
  auto it = credentials.find(submitted_form.username_value);
  if (it != credentials.end()) {
    if (!it->second->is_public_suffix_match)
      return it->second;

    const auto& password_to_save = submitted_form.new_password_value.empty()
                                       ? submitted_form.password_value
                                       : submitted_form.new_password_value;
    // Normally, the copy of the PSL matched credentials, adapted for the
    // current domain, is saved automatically without asking the user, because
    // the copy likely represents the same account, i.e., the one for which
    // the user already agreed to store a password.
    //
    // However, if the user changes the suggested password, it might indicate
    // that the autofilled credentials and |submitted_password_form|
    // actually correspond to two different accounts (see
    // http://crbug.com/385619).
    return password_to_save == it->second->password_value ? it->second
                                                          : nullptr;
  }

  // Next attempt is to find a match by password value. It should not be tried
  // when the username was actually detected.
  if (submitted_form.type == PasswordForm::Type::kApi ||
      !submitted_form.username_value.empty())
    return nullptr;

  for (const auto& stored_match : credentials) {
    if (stored_match.second->password_value == submitted_form.password_value)
      return stored_match.second;
  }

  // Last try. The submitted form had no username but a password. Assume that
  // it's an existing credential.
  return credentials.empty() ? nullptr : credentials.begin()->second;
}

autofill::PasswordForm MakeNormalizedBlacklistedForm(
    password_manager::PasswordStore::FormDigest digest) {
  autofill::PasswordForm result;
  result.blacklisted_by_user = true;
  result.scheme = std::move(digest.scheme);
  result.signon_realm = std::move(digest.signon_realm);
  // In case |digest| corresponds to an Android credential copy the origin as
  // is, otherwise clear out the path by calling GetOrigin().
  if (password_manager::FacetURI::FromPotentiallyInvalidSpec(
          digest.origin.spec())
          .IsValidAndroidFacetURI()) {
    result.origin = std::move(digest.origin);
  } else {
    // GetOrigin() will return an empty GURL if the origin is not valid or
    // standard. DCHECK that this will not happen.
    DCHECK(digest.origin.is_valid());
    DCHECK(digest.origin.IsStandard());
    result.origin = digest.origin.GetOrigin();
  }
  return result;
}

}  // namespace password_manager_util

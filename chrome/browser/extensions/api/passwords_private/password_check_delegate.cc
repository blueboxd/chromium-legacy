// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/password_check_delegate.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_utils.h"
#include "chrome/browser/password_manager/bulk_leak_check_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/password_form.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/bulk_leak_check_service.h"
#include "components/password_manager/core/browser/compromised_credentials_table.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"
#include "components/password_manager/core/browser/ui/compromised_credentials_provider.h"
#include "components/password_manager/core/browser/ui/credential_utils.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "url/gurl.h"

namespace extensions {

namespace {

using autofill::PasswordForm;
using password_manager::CanonicalizeUsername;
using password_manager::CompromiseType;
using password_manager::CredentialWithPassword;
using password_manager::LeakCheckCredential;
using password_manager::PasswordCredentialLess;
using ui::TimeFormat;

using CompromisedCredentialsView =
    password_manager::CompromisedCredentialsProvider::CredentialsView;
using SavedPasswordsView =
    password_manager::SavedPasswordsPresenter::SavedPasswordsView;
using State = password_manager::BulkLeakCheckService::State;

using CompromisedCredentialSet =
    base::flat_set<CredentialWithPassword, PasswordCredentialLess>;

}  // namespace

// Key used to attach UserData to a LeakCheckCredential.
constexpr char kPasswordCheckDataKey[] = "password-check-data-key";

// Class remembering the state required to update the progress of an ongoing
// Password Check.
class PasswordCheckProgress : public base::RefCounted<PasswordCheckProgress> {
 public:
  base::WeakPtr<PasswordCheckProgress> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  size_t remaining_in_queue() const { return remaining_in_queue_; }
  size_t already_processed() const { return already_processed_; }

  // Increments the counts corresponding to |password|. Intended to be called
  // for each credential that is passed to the bulk check.
  void IncrementCounts(const PasswordForm& password) {
    ++remaining_in_queue_;
    ++counts_[password];
  }

  // Updates the counts after a |credential| has been processed by the bulk
  // check.
  void OnProcessed(const LeakCheckCredential& credential) {
    auto it = counts_.find(credential);
    const int num_matching = it != counts_.end() ? it->second : 0;
    already_processed_ += num_matching;
    remaining_in_queue_ -= num_matching;
  }

 private:
  friend class base::RefCounted<PasswordCheckProgress>;
  ~PasswordCheckProgress() = default;

  // Count variables needed to correctly show the progress of the check to the
  // user. |already_processed_| contains the number of credentials that have
  // been checked already, while |remaining_in_queue_| remembers how many
  // passwords still need to be checked.
  // Since the bulk leak check tries to be as efficient as possible, it performs
  // a deduplication step before starting to check passwords. In this step it
  // canonicalizes each credential, and only processes the combinations that are
  // unique. Since this number likely does not match the total number of saved
  // passwords, we remember in |counts_| how many saved passwords a given
  // canonicalized credential corresponds to.
  size_t already_processed_ = 0;
  size_t remaining_in_queue_ = 0;
  std::map<password_manager::CanonicalizedCredential, size_t> counts_;

  base::WeakPtrFactory<PasswordCheckProgress> weak_ptr_factory_{this};
};

namespace {

// A class attached to each LeakCheckCredential that holds a shared handle to
// the PasswordCheckProgress and is able to update the progress accordingly.
class PasswordCheckData : public LeakCheckCredential::Data {
 public:
  explicit PasswordCheckData(scoped_refptr<PasswordCheckProgress> progress)
      : progress_(std::move(progress)) {}
  ~PasswordCheckData() override = default;

  std::unique_ptr<Data> Clone() override {
    return std::make_unique<PasswordCheckData>(progress_);
  }

 private:
  scoped_refptr<PasswordCheckProgress> progress_;
};

api::passwords_private::PasswordCheckState ConvertPasswordCheckState(
    State state) {
  switch (state) {
    case State::kIdle:
      return api::passwords_private::PASSWORD_CHECK_STATE_IDLE;
    case State::kRunning:
      return api::passwords_private::PASSWORD_CHECK_STATE_RUNNING;
    case State::kCanceled:
      return api::passwords_private::PASSWORD_CHECK_STATE_CANCELED;
    case State::kSignedOut:
      return api::passwords_private::PASSWORD_CHECK_STATE_SIGNED_OUT;
    case State::kNetworkError:
      return api::passwords_private::PASSWORD_CHECK_STATE_OFFLINE;
    case State::kQuotaLimit:
      return api::passwords_private::PASSWORD_CHECK_STATE_QUOTA_LIMIT;
    case State::kTokenRequestFailure:
    case State::kHashingFailure:
    case State::kServiceError:
      return api::passwords_private::PASSWORD_CHECK_STATE_OTHER_ERROR;
  }

  NOTREACHED();
  return api::passwords_private::PASSWORD_CHECK_STATE_NONE;
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
  CompromisedCredentialSet compromised_credentials(
      compromised_credentials_view.begin(), compromised_credentials_view.end());

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

std::string FormatElapsedTime(base::Time time) {
  const base::TimeDelta elapsed_time = base::Time::Now() - time;
  if (elapsed_time < base::TimeDelta::FromMinutes(1))
    return l10n_util::GetStringUTF8(IDS_SETTINGS_PASSWORDS_JUST_NOW);

  return base::UTF16ToUTF8(TimeFormat::SimpleWithMonthAndYear(
      TimeFormat::FORMAT_ELAPSED, TimeFormat::LENGTH_LONG, elapsed_time, true));
}

// Helper struct that bundles a CredentialWithPassword with a corresponding
// passwords_private::CompromiseType. This is necessary to support the both
// PHISHED_AND_LEAKED case, which does not exists in password_manager's
// CompromiseType.
struct CompromisedCredentialAndType {
  CredentialWithPassword credential;
  api::passwords_private::CompromiseType type;
};

// Orders |compromised_credentials| in such a way that phished credentials
// precede leaked credentials, and that credentials of the same compromise type
// are ordered by recency. Furthermore it de-duplicates credentials that are
// both phished and leaked, making sure they only appear once in the final list.
std::vector<CompromisedCredentialAndType> OrderCompromisedCredentials(
    CompromisedCredentialsView compromised_credentials) {
  // Partition the compromised credentials into phished and leaked.
  std::vector<CredentialWithPassword> phished_storage;
  std::vector<CredentialWithPassword> leaked_storage;
  std::partition_copy(
      compromised_credentials.begin(), compromised_credentials.end(),
      std::back_inserter(phished_storage), std::back_inserter(leaked_storage),
      [](const auto& credential) {
        return credential.compromise_type == CompromiseType::kPhished;
      });

  // Perform a set intersection to find credentials that are both phished and
  // leaked. Operate on flat_sets, since they provide a more convenient API.
  CompromisedCredentialSet phished = std::move(phished_storage);
  CompromisedCredentialSet leaked = std::move(leaked_storage);
  CompromisedCredentialSet phished_and_leaked;
  std::set_intersection(
      phished.begin(), phished.end(), leaked.begin(), leaked.end(),
      std::inserter(phished_and_leaked, phished_and_leaked.end()),
      PasswordCredentialLess());

  // Iterate through the phished and leaked credentials and update their
  // timestamp to be the most recent compromise event.
  for (auto& phished_and_leaked : phished_and_leaked) {
    phished_and_leaked.create_time =
        std::max(phished.find(phished_and_leaked)->create_time,
                 leaked.find(phished_and_leaked)->create_time);
  }

  // Erase phished and leaked credentials from the other two sets.
  auto is_phished_and_leaked = [&](const auto& credential) {
    return phished_and_leaked.contains(credential);
  };
  base::EraseIf(phished, is_phished_and_leaked);
  base::EraseIf(leaked, is_phished_and_leaked);

  // Finally move all credentials into a single list, associating with the
  // corresponding CompromiseType.
  const size_t num_phished = phished.size() + phished_and_leaked.size();
  std::vector<CompromisedCredentialAndType> results;
  results.reserve(compromised_credentials.size());
  for (auto& credential : std::move(phished).extract()) {
    results.push_back({std::move(credential),
                       api::passwords_private::COMPROMISE_TYPE_PHISHED});
  }

  for (auto& credential : std::move(phished_and_leaked).extract()) {
    results.push_back(
        {std::move(credential),
         api::passwords_private::COMPROMISE_TYPE_PHISHED_AND_LEAKED});
  }

  for (auto& credential : std::move(leaked).extract()) {
    results.push_back({std::move(credential),
                       api::passwords_private::COMPROMISE_TYPE_LEAKED});
  }

  // By construction the phished credentials precede the leaked credentials in
  // |results|. Now sort both groups by their creation date so that most recent
  // compromises appear first in both lists.
  auto create_time_cmp = [](const auto& lhs, const auto& rhs) {
    return lhs.credential.create_time > rhs.credential.create_time;
  };
  std::sort(results.begin(), results.begin() + num_phished, create_time_cmp);
  std::sort(results.begin() + num_phished, results.end(), create_time_cmp);
  return results;
}

}  // namespace

PasswordCheckDelegate::PasswordCheckDelegate(Profile* profile)
    : profile_(profile),
      password_store_(PasswordStoreFactory::GetForProfile(
          profile,
          ServiceAccessType::EXPLICIT_ACCESS)),
      saved_passwords_presenter_(password_store_),
      compromised_credentials_provider_(password_store_,
                                        &saved_passwords_presenter_),
      bulk_leak_check_service_adapter_(
          &saved_passwords_presenter_,
          BulkLeakCheckServiceFactory::GetForProfile(profile_),
          profile_->GetPrefs()) {
  observed_saved_passwords_presenter_.Add(&saved_passwords_presenter_);
  observed_compromised_credentials_provider_.Add(
      &compromised_credentials_provider_);
  observed_bulk_leak_check_service_.Add(
      BulkLeakCheckServiceFactory::GetForProfile(profile_));

  // Instructs the presenter and provider to initialize and built their caches.
  // This will soon after invoke OnCompromisedCredentialsChanged(), which then
  // initializes |credentials_to_forms_| as well. Calls to
  // GetCompromisedCredentials() that might happen until then will return an
  // empty list.
  saved_passwords_presenter_.Init();
  compromised_credentials_provider_.Init();
}

PasswordCheckDelegate::~PasswordCheckDelegate() = default;

std::vector<api::passwords_private::CompromisedCredential>
PasswordCheckDelegate::GetCompromisedCredentials() {
  std::vector<CompromisedCredentialAndType>
      ordered_compromised_credential_and_types = OrderCompromisedCredentials(
          compromised_credentials_provider_.GetCompromisedCredentials());

  std::vector<api::passwords_private::CompromisedCredential>
      compromised_credentials;
  compromised_credentials.reserve(
      ordered_compromised_credential_and_types.size());
  for (const auto& credential_and_type :
       ordered_compromised_credential_and_types) {
    const auto& credential = credential_and_type.credential;
    api::passwords_private::CompromisedCredential api_credential;
    auto facet = password_manager::FacetURI::FromPotentiallyInvalidSpec(
        credential.signon_realm);
    if (facet.IsValidAndroidFacetURI()) {
      api_credential.is_android_credential = true;
      // |formatted_orgin|, |detailed_origin| and |change_password_url| need
      // special handling for Android. Here we use affiliation information
      // instead of the signon_realm.
      const PasswordForm& android_form =
          credentials_to_forms_.at(credential).at(0);
      if (!android_form.app_display_name.empty()) {
        api_credential.formatted_origin = android_form.app_display_name;
        api_credential.detailed_origin = android_form.app_display_name;
        api_credential.change_password_url =
            std::make_unique<std::string>(android_form.affiliated_web_realm);
      } else {
        // In case no affiliation information could be obtained show the
        // formatted package name to the user. An empty change_password_url will
        // be handled by the frontend, by not including a link in this case.
        api_credential.formatted_origin = l10n_util::GetStringFUTF8(
            IDS_SETTINGS_PASSWORDS_ANDROID_APP,
            base::UTF8ToUTF16(facet.android_package_name()));
        api_credential.detailed_origin = facet.android_package_name();
      }
    } else {
      api_credential.is_android_credential = false;
      api_credential.formatted_origin =
          base::UTF16ToUTF8(url_formatter::FormatUrl(
              GURL(credential.signon_realm),
              url_formatter::kFormatUrlOmitDefaults |
                  url_formatter::kFormatUrlOmitHTTPS |
                  url_formatter::kFormatUrlOmitTrivialSubdomains |
                  url_formatter::kFormatUrlTrimAfterHost,
              net::UnescapeRule::SPACES, nullptr, nullptr, nullptr));
      api_credential.detailed_origin =
          base::UTF16ToUTF8(url_formatter::FormatUrlForSecurityDisplay(
              GURL(credential.signon_realm)));
      api_credential.change_password_url =
          std::make_unique<std::string>(credential.signon_realm);
    }

    api_credential.id =
        compromised_credential_id_generator_.GenerateId(credential);
    api_credential.signon_realm = credential.signon_realm;
    api_credential.username = base::UTF16ToUTF8(credential.username);
    api_credential.compromise_time =
        credential.create_time.ToJsTimeIgnoringNull();
    api_credential.compromise_type = credential_and_type.type;
    api_credential.elapsed_time_since_compromise =
        FormatElapsedTime(credential.create_time);
    compromised_credentials.push_back(std::move(api_credential));
  }

  return compromised_credentials;
}

base::Optional<api::passwords_private::CompromisedCredential>
PasswordCheckDelegate::GetPlaintextCompromisedPassword(
    api::passwords_private::CompromisedCredential credential) const {
  const CredentialWithPassword* compromised_credential =
      FindMatchingCompromisedCredential(credential);
  if (!compromised_credential)
    return base::nullopt;

  credential.password = std::make_unique<std::string>(
      base::UTF16ToUTF8(compromised_credential->password));
  return credential;
}

bool PasswordCheckDelegate::ChangeCompromisedCredential(
    const api::passwords_private::CompromisedCredential& credential,
    base::StringPiece new_password) {
  // Try to obtain the original CredentialWithPassword and try to find it in
  // |credentials_to_forms_|. Return false if either one fails.
  const CredentialWithPassword* compromised_credential =
      FindMatchingCompromisedCredential(credential);
  if (!compromised_credential)
    return false;

  auto it = credentials_to_forms_.find(*compromised_credential);
  if (it == credentials_to_forms_.end())
    return false;

  // Make sure there are matching password forms. Also erase duplicates if there
  // are any.
  const auto& forms = it->second;
  if (forms.empty())
    return false;

  for (size_t i = 1; i < forms.size(); ++i)
    password_store_->RemoveLogin(forms[i]);

  // Note: We Invoke EditPassword on the presenter rather than UpdateLogin() on
  // the store, so that observers of the presenter get notified of this event.
  return saved_passwords_presenter_.EditPassword(
      forms[0], base::UTF8ToUTF16(new_password));
}

bool PasswordCheckDelegate::RemoveCompromisedCredential(
    const api::passwords_private::CompromisedCredential& credential) {
  // Try to obtain the original CredentialWithPassword and try to find it in
  // |credentials_to_forms_|. Return false if either one fails.
  const CredentialWithPassword* compromised_credential =
      FindMatchingCompromisedCredential(credential);
  if (!compromised_credential)
    return false;

  auto it = credentials_to_forms_.find(*compromised_credential);
  if (it == credentials_to_forms_.end())
    return false;

  // Erase all matching credentials from the store. Return whether any
  // credentials were deleted.
  SavedPasswordsView saved_passwords = it->second;
  for (const PasswordForm& saved_password : saved_passwords)
    password_store_->RemoveLogin(saved_password);

  return !saved_passwords.empty();
}

void PasswordCheckDelegate::StartPasswordCheck(
    StartPasswordCheckCallback callback) {
  // If the delegate isn't initialized yet, enqueue the callback and return
  // early.
  if (!is_initialized_) {
    start_check_callbacks_.push_back(std::move(callback));
    return;
  }

  // Also return early if the check is already running.
  if (bulk_leak_check_service_adapter_.GetBulkLeakCheckState() ==
      State::kRunning) {
    std::move(callback).Run(State::kRunning);
    return;
  }

  auto progress = base::MakeRefCounted<PasswordCheckProgress>();
  for (const auto& password : saved_passwords_presenter_.GetSavedPasswords())
    progress->IncrementCounts(password);

  password_check_progress_ = progress->GetWeakPtr();
  PasswordCheckData data(std::move(progress));
  is_check_running_ = bulk_leak_check_service_adapter_.StartBulkLeakCheck(
      kPasswordCheckDataKey, &data);
  DCHECK(is_check_running_);
  std::move(callback).Run(
      bulk_leak_check_service_adapter_.GetBulkLeakCheckState());
}

void PasswordCheckDelegate::StopPasswordCheck() {
  if (!is_initialized_) {
    for (auto&& callback : std::exchange(start_check_callbacks_, {}))
      std::move(callback).Run(State::kIdle);
    return;
  }

  bulk_leak_check_service_adapter_.StopBulkLeakCheck();
}

api::passwords_private::PasswordCheckStatus
PasswordCheckDelegate::GetPasswordCheckStatus() const {
  api::passwords_private::PasswordCheckStatus result;

  // Obtain the timestamp of the last completed check. This is 0.0 in case the
  // check never completely ran before.
  const double last_check_completed = profile_->GetPrefs()->GetDouble(
      password_manager::prefs::kLastTimePasswordCheckCompleted);
  if (last_check_completed) {
    result.elapsed_time_since_last_check = std::make_unique<std::string>(
        FormatElapsedTime(base::Time::FromDoubleT(last_check_completed)));
  }

  State state = bulk_leak_check_service_adapter_.GetBulkLeakCheckState();
  SavedPasswordsView saved_passwords =
      saved_passwords_presenter_.GetSavedPasswords();

  // Handle the currently running case first, only then consider errors.
  if (state == State::kRunning) {
    result.state = api::passwords_private::PASSWORD_CHECK_STATE_RUNNING;

    if (password_check_progress_) {
      result.already_processed =
          std::make_unique<int>(password_check_progress_->already_processed());
      result.remaining_in_queue =
          std::make_unique<int>(password_check_progress_->remaining_in_queue());
    } else {
      result.already_processed = std::make_unique<int>(0);
      result.remaining_in_queue = std::make_unique<int>(0);
    }

    return result;
  }

  if (saved_passwords.empty()) {
    result.state = api::passwords_private::PASSWORD_CHECK_STATE_NO_PASSWORDS;
    return result;
  }

  result.state = ConvertPasswordCheckState(state);
  return result;
}

void PasswordCheckDelegate::OnSavedPasswordsChanged(SavedPasswordsView) {
  // Getting the first notification about a change in saved passwords implies
  // that the delegate is initialized, and start check callbacks can be invoked,
  // if any.
  if (!std::exchange(is_initialized_, true)) {
    for (auto&& callback : std::exchange(start_check_callbacks_, {}))
      StartPasswordCheck(std::move(callback));
  }

  // A change in the saved passwords might result in leaving or entering the
  // NO_PASSWORDS state, thus we need to trigger a notification.
  NotifyPasswordCheckStatusChanged();
}

void PasswordCheckDelegate::OnCompromisedCredentialsChanged(
    CompromisedCredentialsView credentials) {
  credentials_to_forms_ = MapCompromisedCredentialsToSavedPasswords(
      credentials, saved_passwords_presenter_.GetSavedPasswords());
  if (auto* event_router =
          PasswordsPrivateEventRouterFactory::GetForProfile(profile_)) {
    event_router->OnCompromisedCredentialsChanged(GetCompromisedCredentials());
  }
}

void PasswordCheckDelegate::OnStateChanged(State state) {
  if (state == State::kIdle && std::exchange(is_check_running_, false)) {
    // When the service transitions from running into idle it has finished a
    // check.
    profile_->GetPrefs()->SetDouble(
        password_manager::prefs::kLastTimePasswordCheckCompleted,
        base::Time::Now().ToDoubleT());

    // In case the check run to completion delay the last Check Status update by
    // a second. This avoids flickering of the UI if the full check ran from
    // start to finish almost immediately.
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PasswordCheckDelegate::NotifyPasswordCheckStatusChanged,
                       weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(1));
    return;
  }

  // NotifyPasswordCheckStatusChanged() invokes GetPasswordCheckStatus()
  // obtaining the relevant information. Thus there is no need to forward the
  // arguments passed to OnStateChanged().
  NotifyPasswordCheckStatusChanged();
}

void PasswordCheckDelegate::OnCredentialDone(
    const LeakCheckCredential& credential,
    password_manager::IsLeaked is_leaked) {
  if (is_leaked) {
    // In case the credential is leaked, iterate over all currently saved
    // credentials and mark those as compromised that have the same
    // canonicalized username and password.
    const base::string16 canocalized_username =
        CanonicalizeUsername(credential.username());
    for (const PasswordForm& saved_password :
         saved_passwords_presenter_.GetSavedPasswords()) {
      if (saved_password.password_value == credential.password() &&
          CanonicalizeUsername(saved_password.username_value) ==
              canocalized_username) {
        password_store_->AddCompromisedCredentials({
            .signon_realm = saved_password.signon_realm,
            .username = saved_password.username_value,
            .create_time = base::Time::Now(),
            .compromise_type = CompromiseType::kLeaked,
        });
      }
    }
  }

  // Update the progress in case there is one.
  if (password_check_progress_)
    password_check_progress_->OnProcessed(credential);

  // While the check is still running trigger an update of the check status,
  // considering that the progress has changed.
  if (bulk_leak_check_service_adapter_.GetBulkLeakCheckState() ==
      State::kRunning) {
    NotifyPasswordCheckStatusChanged();
  }
}

const CredentialWithPassword*
PasswordCheckDelegate::FindMatchingCompromisedCredential(
    const api::passwords_private::CompromisedCredential& credential) const {
  const CredentialWithPassword* compromised_credential =
      compromised_credential_id_generator_.TryGetKey(credential.id);
  if (!compromised_credential)
    return nullptr;

  if (credential.signon_realm != compromised_credential->signon_realm ||
      credential.username !=
          base::UTF16ToUTF8(compromised_credential->username) ||
      (credential.password &&
       *credential.password !=
           base::UTF16ToUTF8(compromised_credential->password))) {
    return nullptr;
  }

  return compromised_credential;
}

void PasswordCheckDelegate::NotifyPasswordCheckStatusChanged() {
  if (auto* event_router =
          PasswordsPrivateEventRouterFactory::GetForProfile(profile_)) {
    event_router->OnPasswordCheckStatusChanged(GetPasswordCheckStatus());
  }
}

}  // namespace extensions

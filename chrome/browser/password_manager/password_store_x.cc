// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_x.h"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_notification_types.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_service.h"

using autofill::PasswordForm;
using password_manager::PasswordStoreChange;
using password_manager::PasswordStoreChangeList;
using password_manager::PasswordStoreDefault;

namespace {

bool AddLoginToBackend(
    const std::unique_ptr<PasswordStoreX::NativeBackend>& backend,
    const PasswordForm& form,
    PasswordStoreChangeList* changes) {
  *changes = backend->AddLogin(form);
  return (!changes->empty() &&
          changes->back().type() == PasswordStoreChange::ADD);
}

bool RemoveLoginsByURLAndTimeFromBackend(
    PasswordStoreX::NativeBackend* backend,
    const base::Callback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    PasswordStoreChangeList* changes) {
  std::vector<std::unique_ptr<PasswordForm>> forms;
  if (!backend->GetAllLogins(&forms))
    return false;

  for (const auto& form : forms) {
    if (url_filter.Run(form->origin) && form->date_created >= delete_begin &&
        (delete_end.is_null() || form->date_created < delete_end) &&
        !backend->RemoveLogin(*form, changes))
      return false;
  }

  return true;
}

// Disables encryption on |login_db|, if the migration to encryption has not
// been performed yet.
std::unique_ptr<password_manager::LoginDatabase> DisableEncryption(
    std::unique_ptr<password_manager::LoginDatabase> login_db,
    PrefService* prefs) {
  if (prefs->GetInteger(password_manager::prefs::kMigrationToLoginDBStep) !=
      PasswordStoreX::LOGIN_DB_REPLACED) {
    login_db->disable_encryption();
  }
  return login_db;
}

// Returns the password_manager::metrics_util::LinuxBackendMigrationStatus
// equivalent for |step|.
password_manager::metrics_util::LinuxBackendMigrationStatus StepForMetrics(
    PasswordStoreX::MigrationToLoginDBStep step) {
  using password_manager::metrics_util::LinuxBackendMigrationStatus;
  switch (step) {
    case PasswordStoreX::NOT_ATTEMPTED:
      return LinuxBackendMigrationStatus::kNotAttempted;
    case PasswordStoreX::DEPRECATED_FAILED:
      return LinuxBackendMigrationStatus::kDeprecatedFailed;
    case PasswordStoreX::COPIED_ALL:
      return LinuxBackendMigrationStatus::kCopiedAll;
    case PasswordStoreX::LOGIN_DB_REPLACED:
      return LinuxBackendMigrationStatus::kLoginDBReplaced;
    case PasswordStoreX::STARTED:
      return LinuxBackendMigrationStatus::kStarted;
    case PasswordStoreX::POSTPONED:
      return LinuxBackendMigrationStatus::kPostponed;
    case PasswordStoreX::DEPRECATED_FAILED_CREATE_ENCRYPTED:
      return LinuxBackendMigrationStatus::kDeprecatedFailedCreatedEncrypted;
    case PasswordStoreX::FAILED_ACCESS_NATIVE:
      return LinuxBackendMigrationStatus::kFailedAccessNative;
    case PasswordStoreX::FAILED_REPLACE:
      return LinuxBackendMigrationStatus::kFailedReplace;
    case PasswordStoreX::FAILED_INIT_ENCRYPTED:
      return LinuxBackendMigrationStatus::kFailedInitEncrypted;
    case PasswordStoreX::FAILED_RECREATE_ENCRYPTED:
      return LinuxBackendMigrationStatus::kFailedRecreateEncrypted;
    case PasswordStoreX::FAILED_WRITE_TO_ENCRYPTED:
      return LinuxBackendMigrationStatus::kFailedWriteToEncrypted;
  }
  NOTREACHED();
  return LinuxBackendMigrationStatus::kNotAttempted;
}

// Remove |forms| from |backend|. If |forms| is empty, |backend| will be cleared
// entirely. This must be called on |runner|, which has to be the same as
// |backend|'s.
void ClearNativeBackend(scoped_refptr<base::SequencedTaskRunner> runner,
                        std::unique_ptr<PasswordStoreX::NativeBackend> backend,
                        std::vector<std::unique_ptr<PasswordForm>> forms) {
  if (forms.empty()) {
    if (!backend->GetAllLogins(&forms))
      return;
  }

  if (!forms.empty()) {
    PasswordStoreChangeList changes;
    backend->RemoveLogin(*forms.back(), &changes);
    forms.pop_back();
    if (!forms.empty()) {
      // We yield on the task runner between deletes, because this is a
      // background cleanup which has to happen on the native backend's
      // preferred thread, and in the case of gnome-keyring it's the main
      // thread.
      runner->PostTask(FROM_HERE,
                       base::BindOnce(&ClearNativeBackend, runner,
                                      std::move(backend), std::move(forms)));
    }
  }
}

}  // namespace

PasswordStoreX::PasswordStoreX(
    std::unique_ptr<password_manager::LoginDatabase> login_db,
    base::FilePath login_db_file,
    base::FilePath encrypted_login_db_file,
    std::unique_ptr<NativeBackend> backend,
    PrefService* prefs)
    : PasswordStoreDefault(DisableEncryption(std::move(login_db), prefs)),
      backend_(std::move(backend)),
      login_db_file_(std::move(login_db_file)),
      encrypted_login_db_file_(std::move(encrypted_login_db_file)),
      migration_checked_(false),
      allow_fallback_(false) {
  migration_step_pref_.Init(password_manager::prefs::kMigrationToLoginDBStep,
                            prefs);
  migration_to_login_db_step_ =
      static_cast<MigrationToLoginDBStep>(migration_step_pref_.GetValue());

  base::UmaHistogramEnumeration(
      "PasswordManager.LinuxBackendMigration.Adoption",
      StepForMetrics(migration_to_login_db_step_));

  // No |backend_| means serving from PasswordStoreDefault.
  if (migration_to_login_db_step_ == LOGIN_DB_REPLACED)
    backend_.reset();
}

PasswordStoreX::~PasswordStoreX() {}

scoped_refptr<base::SequencedTaskRunner>
PasswordStoreX::CreateBackgroundTaskRunner() const {
  scoped_refptr<base::SequencedTaskRunner> result =
      backend_ ? backend_->GetBackgroundTaskRunner() : nullptr;
  return result ? result : PasswordStoreDefault::CreateBackgroundTaskRunner();
}

PasswordStoreChangeList PasswordStoreX::AddLoginImpl(
    const PasswordForm& form,
    password_manager::AddLoginError* error) {
  CheckMigration();
  PasswordStoreChangeList changes;
  if (use_native_backend() && AddLoginToBackend(backend_, form, &changes)) {
    allow_fallback_ = false;
  } else if (allow_default_store()) {
    changes = PasswordStoreDefault::AddLoginImpl(form, error);
  }
  return changes;
}

PasswordStoreChangeList PasswordStoreX::UpdateLoginImpl(
    const PasswordForm& form,
    password_manager::UpdateLoginError* error) {
  CheckMigration();
  PasswordStoreChangeList changes;
  if (use_native_backend() && backend_->UpdateLogin(form, &changes)) {
    allow_fallback_ = false;
  } else if (allow_default_store()) {
    changes = PasswordStoreDefault::UpdateLoginImpl(form, error);
  }
  return changes;
}

PasswordStoreChangeList PasswordStoreX::RemoveLoginImpl(
    const PasswordForm& form) {
  CheckMigration();
  PasswordStoreChangeList changes;
  if (use_native_backend() && backend_->RemoveLogin(form, &changes)) {
    allow_fallback_ = false;
  } else if (allow_default_store()) {
    changes = PasswordStoreDefault::RemoveLoginImpl(form);
  }
  return changes;
}

PasswordStoreChangeList PasswordStoreX::RemoveLoginsByURLAndTimeImpl(
    const base::Callback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end) {
  CheckMigration();

  PasswordStoreChangeList changes;
  if (use_native_backend() &&
      RemoveLoginsByURLAndTimeFromBackend(backend_.get(), url_filter,
                                          delete_begin, delete_end, &changes)) {
    allow_fallback_ = false;
  } else if (allow_default_store()) {
    changes = PasswordStoreDefault::RemoveLoginsByURLAndTimeImpl(
        url_filter, delete_begin, delete_end);
  }

  return changes;
}

PasswordStoreChangeList PasswordStoreX::RemoveLoginsCreatedBetweenImpl(
    base::Time delete_begin,
    base::Time delete_end) {
  CheckMigration();
  PasswordStoreChangeList changes;
  if (use_native_backend() &&
      backend_->RemoveLoginsCreatedBetween(
          delete_begin, delete_end, &changes)) {
    allow_fallback_ = false;
  } else if (allow_default_store()) {
    changes = PasswordStoreDefault::RemoveLoginsCreatedBetweenImpl(delete_begin,
                                                                   delete_end);
  }
  return changes;
}

PasswordStoreChangeList PasswordStoreX::DisableAutoSignInForOriginsImpl(
    const base::Callback<bool(const GURL&)>& origin_filter) {
  CheckMigration();
  PasswordStoreChangeList changes;
  if (use_native_backend() &&
      backend_->DisableAutoSignInForOrigins(origin_filter, &changes)) {
    allow_fallback_ = false;
  } else if (allow_default_store()) {
    changes =
        PasswordStoreDefault::DisableAutoSignInForOriginsImpl(origin_filter);
  }
  return changes;
}

namespace {

// Sorts |list| by origin, like the ORDER BY clause in login_database.cc.
void SortLoginsByOrigin(std::vector<std::unique_ptr<PasswordForm>>* list) {
  std::sort(list->begin(), list->end(),
            [](const std::unique_ptr<PasswordForm>& a,
               const std::unique_ptr<PasswordForm>& b) {
              return a->origin < b->origin;
            });
}

}  // anonymous namespace

std::vector<std::unique_ptr<PasswordForm>> PasswordStoreX::FillMatchingLogins(
    const FormDigest& form) {
  CheckMigration();
  std::vector<std::unique_ptr<PasswordForm>> matched_forms;
  if (use_native_backend() && backend_->GetLogins(form, &matched_forms)) {
    SortLoginsByOrigin(&matched_forms);
    // The native backend may succeed and return no data even while locked, if
    // the query did not match anything stored. So we continue to allow fallback
    // until we perform a write operation, or until a read returns actual data.
    if (!matched_forms.empty())
      allow_fallback_ = false;
    return matched_forms;
  }
  if (allow_default_store())
    return PasswordStoreDefault::FillMatchingLogins(form);
  return std::vector<std::unique_ptr<PasswordForm>>();
}

bool PasswordStoreX::FillAutofillableLogins(
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  CheckMigration();
  if (use_native_backend() && backend_->GetAutofillableLogins(forms)) {
    SortLoginsByOrigin(forms);
    // See GetLoginsImpl() for why we disallow fallback conditionally here.
    if (!forms->empty())
      allow_fallback_ = false;
    return true;
  }
  if (allow_default_store())
    return PasswordStoreDefault::FillAutofillableLogins(forms);
  return false;
}

bool PasswordStoreX::FillBlacklistLogins(
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  CheckMigration();
  if (use_native_backend() && backend_->GetBlacklistLogins(forms)) {
    // See GetLoginsImpl() for why we disallow fallback conditionally here.
    SortLoginsByOrigin(forms);
    if (!forms->empty())
      allow_fallback_ = false;
    return true;
  }
  if (allow_default_store())
    return PasswordStoreDefault::FillBlacklistLogins(forms);
  return false;
}

void PasswordStoreX::CheckMigration() {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());

  if (migration_checked_ || !backend_.get())
    return;
  migration_checked_ = true;
  DCHECK_NE(migration_to_login_db_step_, LOGIN_DB_REPLACED);

  base::Time migration_to_native_started = base::Time::Now();

  ssize_t migrated = MigrateToNativeBackend();
  if (migrated > 0) {
    VLOG(1) << "Migrated " << migrated << " passwords to native store.";
  } else if (migrated == 0) {
    // As long as we are able to migrate some passwords, we know the native
    // store is working. But if there is nothing to migrate, the "migration"
    // can succeed even when the native store would fail. In this case we
    // allow a later fallback to the default store. Once any later operation
    // succeeds on the native store, we will no longer allow fallback.
    allow_fallback_ = true;
  } else {
    LOG(WARNING) << "Native password store migration failed! "
                 << "Falling back on default (unencrypted) store.";
    backend_.reset();
  }

  base::UmaHistogramLongTimes(
      "PasswordManager.LinuxBackendMigration.TimeIntoNative",
      base::Time::Now() - migration_to_native_started);

  if (base::FeatureList::IsEnabled(
          password_manager::features::kMigrateLinuxToLoginDB)) {
    // Copy passwords from the backend into the login database, using
    // encryption.
    if (backend_) {
      UpdateMigrationToLoginDBStep(STARTED);
      MigrateToEncryptedLoginDB();
    } else {
      UpdateMigrationToLoginDBStep(POSTPONED);
    }

    base::UmaHistogramEnumeration(
        "PasswordManager.LinuxBackendMigration.AttemptResult",
        StepForMetrics(migration_to_login_db_step_));
  }
}

bool PasswordStoreX::allow_default_store() {
  if (allow_fallback_) {
    LOG(WARNING) << "Native password store failed! " <<
                 "Falling back on default (unencrypted) store.";
    backend_.reset();
    // Don't warn again. We'll use the default store because backend_ is NULL.
    allow_fallback_ = false;
  }
  return !backend_.get();
}

ssize_t PasswordStoreX::MigrateToNativeBackend() {
  DCHECK(backend_.get());
  std::vector<std::unique_ptr<PasswordForm>> forms;
  std::vector<std::unique_ptr<PasswordForm>> blacklist_forms;
  bool ok = PasswordStoreDefault::FillAutofillableLogins(&forms) &&
            PasswordStoreDefault::FillBlacklistLogins(&blacklist_forms);
  const size_t autofillable_forms_count = forms.size();
  forms.resize(autofillable_forms_count + blacklist_forms.size());
  std::move(blacklist_forms.begin(), blacklist_forms.end(),
            forms.begin() + autofillable_forms_count);
  if (ok) {
    // We add all the passwords (and blacklist entries) to the native backend
    // before attempting to remove any from the login database, to make sure we
    // don't somehow end up with some of the passwords in one store and some in
    // another. We'll always have at least one intact store this way.
    for (size_t i = 0; i < forms.size(); ++i) {
      PasswordStoreChangeList changes;
      if (!AddLoginToBackend(backend_, *forms[i], &changes)) {
        ok = false;
        break;
      }
    }
    if (ok) {
      for (size_t i = 0; i < forms.size(); ++i) {
        // If even one of these calls to RemoveLoginImpl() succeeds, then we
        // should prefer the native backend to the now-incomplete login
        // database. Thus we want to return a success status even in the case
        // where some fail. The only real problem with this is that we might
        // leave passwords in the login database and never come back to clean
        // them out if any of these calls do fail.
        PasswordStoreDefault::RemoveLoginImpl(*forms[i]);
      }
      // Finally, delete the database file itself. We remove the passwords from
      // it before deleting the file just in case there is some problem deleting
      // the file (e.g. directory is not writable, but file is), which would
      // otherwise cause passwords to re-migrate next (or maybe every) time.
      DeleteAndRecreateDatabaseFile();
    }
  }
  ssize_t result = ok ? forms.size() : -1;
  return result;
}

void PasswordStoreX::MigrateToEncryptedLoginDB() {
  base::Time migration_to_encrypted_started = base::Time::Now();

  // Initialise the temporary database.
  auto encrypted_login_db = std::make_unique<password_manager::LoginDatabase>(
      encrypted_login_db_file_);
  if (!encrypted_login_db->Init()) {
    VLOG(1) << "Failed to init the encrypted database file. Migration "
               "aborted.";
    UpdateMigrationToLoginDBStep(FAILED_INIT_ENCRYPTED);
    return;  // Serve from the native backend.
  }

  // Copy everything from the backend to the temporary database.
  VLOG(1) << "Migrating all passwords to the encrypted database. Last status: "
          << migration_to_login_db_step_;
  UpdateMigrationToLoginDBStep(CopyBackendToLoginDB(encrypted_login_db.get()));
  if (migration_to_login_db_step_ != COPIED_ALL) {
    VLOG(1) << "Migration to encryption failed.";
    base::DeleteFile(encrypted_login_db_file_, false);
    return;
  }

  base::UmaHistogramLongTimes(
      "PasswordManager.LinuxBackendMigration.TimeIntoEncrypted",
      base::Time::Now() - migration_to_encrypted_started);

  // Dispose of the databases, so that we release the databases' locks.
  PasswordStoreDefault::SetLoginDB(nullptr);
  encrypted_login_db.reset();
  // Move the new database onto the old.
  if (!base::Move(encrypted_login_db_file_, login_db_file_)) {
    LOG(ERROR) << "Could not replace login database.";
    UpdateMigrationToLoginDBStep(FAILED_REPLACE);
    base::DeleteFile(encrypted_login_db_file_, false);
    return;  // Serve from the native backend.
  }
  UpdateMigrationToLoginDBStep(LOGIN_DB_REPLACED);

  auto login_db =
      std::make_unique<password_manager::LoginDatabase>(login_db_file_);
  if (login_db->Init()) {
    PasswordStoreDefault::SetLoginDB(std::move(login_db));
  } else {
    // The password manager is disabled because PasswordStoreDefault is left
    // with no LoginDatabase and |backend_| will be disposed of.
    LOG(ERROR) << "Could not initialise database after migration. Password "
                  "Manager is disabled.";
  }

  // Cleanup the native backend on the background, while we serve from
  // PasswordStoreDefault. PasswordStoreX will use the PasswordStoreDefault
  // behaviour, because we move |backend_|.
  auto task_runner = CreateBackgroundTaskRunner();
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&ClearNativeBackend, task_runner, std::move(backend_),
                     std::vector<std::unique_ptr<autofill::PasswordForm>>()));
}

PasswordStoreX::MigrationToLoginDBStep PasswordStoreX::CopyBackendToLoginDB(
    password_manager::LoginDatabase* login_db) {
  DCHECK(backend_);
  DCHECK(login_db);

  if (!login_db->DeleteAndRecreateDatabaseFile()) {
    LOG(ERROR) << "Failed to create the encrypted login database file";
    return FAILED_RECREATE_ENCRYPTED;
  }

  std::vector<std::unique_ptr<PasswordForm>> forms;
  if (!backend_->GetAllLogins(&forms))
    return FAILED_ACCESS_NATIVE;

  for (auto& form : forms) {
    PasswordStoreChangeList changes = login_db->AddLogin(*form);
    if (changes.empty() || changes.back().type() != PasswordStoreChange::ADD) {
      // AddLogin() would fail if the form has empty |origin|, empty
      // |signon_realm|, is a duplicate blacklisting or there was an IO error.
      // All of these cases are not supported and can be dropped.
      if (form->signon_realm.empty() || form->origin.is_empty() ||
          form->blacklisted_by_user) {
        LOG(WARNING) << "Dropped a credential during migration away from the "
                        "native backend";
      } else {
        return FAILED_WRITE_TO_ENCRYPTED;
      }
    }
  }

  return COPIED_ALL;
}

void PasswordStoreX::UpdateMigrationToLoginDBStep(MigrationToLoginDBStep step) {
  migration_to_login_db_step_ = step;
  main_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&PasswordStoreX::UpdateMigrationPref,
                                weak_ptr_factory_.GetWeakPtr(),
                                migration_to_login_db_step_));
}

void PasswordStoreX::UpdateMigrationPref(MigrationToLoginDBStep step) {
  migration_step_pref_.SetValue(step);
}

void PasswordStoreX::ShutdownOnUIThread() {
  migration_step_pref_.Destroy();
  PasswordStoreDefault::ShutdownOnUIThread();
}

password_manager::FormRetrievalResult PasswordStoreX::ReadAllLogins(
    password_manager::PrimaryKeyToFormMap* key_to_form_map) {
  // This method is called from the PasswordSyncBridge which supports only
  // PasswordStoreDefault. Therefore, on Linux, it should be called only if the
  // client is using LogainDatabase instead of the NativeBackend's. It's the
  // responsibility of the caller to guarantee that.
  if (use_native_backend()) {
    NOTREACHED();
  }
  return PasswordStoreDefault::ReadAllLogins(key_to_form_map);
}

PasswordStoreChangeList PasswordStoreX::RemoveLoginByPrimaryKeySync(
    int primary_key) {
  // This method is called from the PasswordSyncBridge which supports only
  // PasswordStoreDefault. Therefore, on Linux, it should be called only if the
  // client is using LogainDatabase instead of the NativeBackend's. It's the
  // responsibility of the caller to guarantee that.
  if (use_native_backend()) {
    NOTREACHED();
  }
  return PasswordStoreDefault::RemoveLoginByPrimaryKeySync(primary_key);
}

password_manager::PasswordStoreSync::MetadataStore*
PasswordStoreX::GetMetadataStore() {
  // This method is called from the PasswordSyncBridge which supports only
  // PasswordStoreDefault. Therefore, on Linux, it should be called only if the
  // client is using LogainDatabase instead of the NativeBackend's. It's the
  // responsibility of the caller to guarantee that.
  if (use_native_backend()) {
    NOTREACHED();
  }
  return PasswordStoreDefault::GetMetadataStore();
}

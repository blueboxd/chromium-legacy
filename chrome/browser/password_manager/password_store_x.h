// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_X_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_X_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_store_default.h"
#include "components/prefs/pref_member.h"

namespace password_manager {
class LoginDatabase;
}

// PasswordStoreX is used on Linux and other non-Windows, non-Mac OS X
// operating systems. It uses a "native backend" to actually store the password
// data when such a backend is available, and otherwise falls back to using the
// login database like PasswordStoreDefault. It also handles automatically
// migrating password data to a native backend from the login database.
//
// There are currently native backends for GNOME Keyring and KWallet.
class PasswordStoreX : public password_manager::PasswordStoreDefault {
 public:
  // The state of the migration from native backends and an unencrypted loginDB
  // to an encrypted loginDB.
  enum MigrationToLoginDBStep {
    // Neither started nor failed.
    NOT_ATTEMPTED = 0,
    // The last attempt was not completed.
    DEPRECATED_FAILED,
    // All the data is in the temporary encrypted loginDB.
    COPIED_ALL,
    // The standard login database is encrypted.
    LOGIN_DB_REPLACED,
    // The migration is about to be attempted. This value was deprecated and
    // replaced by more price entries. It may still be store in users'
    // preferences.
    STARTED,
    // No access to the native backend.
    POSTPONED,
    // Could not create or write into the temporary file.
    DEPRECATED_FAILED_CREATE_ENCRYPTED,
    // Could not read from the native backend.
    FAILED_ACCESS_NATIVE,
    // Could not replace old database.
    FAILED_REPLACE,
    // Could not initialise the temporary encrypted database.
    FAILED_INIT_ENCRYPTED,
    // Could not reset th temporary encrypted database.
    FAILED_RECREATE_ENCRYPTED,
    // Could not add entries into the temporary encrypted database.
    FAILED_WRITE_TO_ENCRYPTED,
  };

  // NativeBackends more or less implement the PaswordStore interface, but
  // with return values rather than implicit consumer notification.
  class NativeBackend {
   public:
    virtual ~NativeBackend() {}

    virtual bool Init() = 0;

    virtual password_manager::PasswordStoreChangeList AddLogin(
        const autofill::PasswordForm& form) = 0;
    // Updates |form| and appends the changes to |changes|. |changes| shouldn't
    // be null. Returns false iff the operation failed due to a system backend
    // error.
    virtual bool UpdateLogin(
        const autofill::PasswordForm& form,
        password_manager::PasswordStoreChangeList* changes) = 0;
    // Removes |form| and appends the changes to |changes|. |changes| shouldn't
    // be null. Returns false iff the operation failed due to a system backend
    // error.
    virtual bool RemoveLogin(
        const autofill::PasswordForm& form,
        password_manager::PasswordStoreChangeList* changes) = 0;

    // Removes all logins created/synced from |delete_begin| onwards (inclusive)
    // and before |delete_end|. You may use a null Time value to do an unbounded
    // delete in either direction.
    virtual bool RemoveLoginsCreatedBetween(
        base::Time delete_begin,
        base::Time delete_end,
        password_manager::PasswordStoreChangeList* changes) = 0;

    // Sets the 'skip_zero_click' flag to 'true' for all logins in the database
    // that match |origin_filter|.
    virtual bool DisableAutoSignInForOrigins(
        const base::Callback<bool(const GURL&)>& origin_filter,
        password_manager::PasswordStoreChangeList* changes) = 0;

    // The three methods below overwrite |forms| with all stored credentials
    // matching |form|, all stored non-blacklisted credentials, and all stored
    // blacklisted credentials, respectively. On success, they return true.
    virtual bool GetLogins(const FormDigest& form,
                           std::vector<std::unique_ptr<autofill::PasswordForm>>*
                               forms) WARN_UNUSED_RESULT = 0;
    virtual bool GetAutofillableLogins(
        std::vector<std::unique_ptr<autofill::PasswordForm>>* forms)
        WARN_UNUSED_RESULT = 0;
    virtual bool GetBlacklistLogins(
        std::vector<std::unique_ptr<autofill::PasswordForm>>* forms)
        WARN_UNUSED_RESULT = 0;
    virtual bool GetAllLogins(
        std::vector<std::unique_ptr<autofill::PasswordForm>>* forms)
        WARN_UNUSED_RESULT = 0;

    // Returns the background thread in case the backend uses one, or null.
    virtual scoped_refptr<base::SequencedTaskRunner>
    GetBackgroundTaskRunner() = 0;
  };

  // |backend| may be NULL in which case this PasswordStoreX will act the same
  // as PasswordStoreDefault. |login_db| is the default location and does not
  // use encryption. |login_db_file| is the location of |login_db|.
  // |encrypted_login_db_file| is a separate file and is used for the migration
  // to encryption.
  PasswordStoreX(std::unique_ptr<password_manager::LoginDatabase> login_db,
                 base::FilePath login_db_file,
                 base::FilePath encrypted_login_db_file,
                 std::unique_ptr<NativeBackend> backend,
                 PrefService* prefs);

  // RefcountedKeyedService:
  void ShutdownOnUIThread() override;

 protected:
  // Implements PasswordStoreSync interface.
  password_manager::FormRetrievalResult ReadAllLogins(
      password_manager::PrimaryKeyToFormMap* key_to_form_map) override;
  password_manager::PasswordStoreChangeList RemoveLoginByPrimaryKeySync(
      int primary_key) override;
  password_manager::PasswordStoreSync::MetadataStore* GetMetadataStore()
      override;

 private:
  friend class PasswordStoreXTest;

  ~PasswordStoreX() override;

  // Implements PasswordStore interface.
  scoped_refptr<base::SequencedTaskRunner> CreateBackgroundTaskRunner()
      const override;
  password_manager::PasswordStoreChangeList AddLoginImpl(
      const autofill::PasswordForm& form,
      password_manager::AddLoginError* error = nullptr) override;
  password_manager::PasswordStoreChangeList UpdateLoginImpl(
      const autofill::PasswordForm& form,
      password_manager::UpdateLoginError* error = nullptr) override;
  password_manager::PasswordStoreChangeList RemoveLoginImpl(
      const autofill::PasswordForm& form) override;
  password_manager::PasswordStoreChangeList RemoveLoginsByURLAndTimeImpl(
      const base::Callback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end) override;
  password_manager::PasswordStoreChangeList RemoveLoginsCreatedBetweenImpl(
      base::Time delete_begin,
      base::Time delete_end) override;
  password_manager::PasswordStoreChangeList DisableAutoSignInForOriginsImpl(
      const base::Callback<bool(const GURL&)>& origin_filter) override;
  std::vector<std::unique_ptr<autofill::PasswordForm>> FillMatchingLogins(
      const FormDigest& form) override;
  bool FillAutofillableLogins(
      std::vector<std::unique_ptr<autofill::PasswordForm>>* forms) override;
  bool FillBlacklistLogins(
      std::vector<std::unique_ptr<autofill::PasswordForm>>* forms) override;

  // Check to see whether migration from the unencrypted loginDB is necessary,
  // and perform it if so. Additionally, if the migration to encryption is
  // enabled, then the passwords will also be copied into the encrypted login
  // database and PasswordStoreX will serve from there. If this migration was
  // completed in a previous run, CheckMigration will simply enable serving from
  // the encrypted login database.
  void CheckMigration();

  // Return true if we should try using the native backend.
  bool use_native_backend() { return !!backend_.get(); }

  // Return true if we can fall back on the default store, warning the first
  // time we call it when falling back is necessary. See |allow_fallback_|.
  bool allow_default_store();

  // Synchronously migrates all the passwords stored in the login database
  // (PasswordStoreDefault) to the native backend. If successful, the login
  // database will be left with no stored passwords, and the number of passwords
  // migrated will be returned. (This might be 0 if migration was not
  // necessary.) Returns < 0 on failure.
  ssize_t MigrateToNativeBackend();

  // Moves the passwords from the backend to a temporary login database, using
  // encryption, and then moves them over to the standard location. This
  // operation can take a significant amount of time.
  void MigrateToEncryptedLoginDB();

  // Synchronously copies everything from the |backend_| to |login_db|. Returns
  // COPIED_ALL on success and FAILED on error.
  MigrationToLoginDBStep CopyBackendToLoginDB(
      password_manager::LoginDatabase* login_db);

  // Update |migration_to_login_db_step_| and |migration_step_pref_|.
  void UpdateMigrationToLoginDBStep(MigrationToLoginDBStep step);

  // Update |migration_step_pref_|. It must be executed on the preference's
  // thread.
  void UpdateMigrationPref(MigrationToLoginDBStep step);

  // The native backend in use, or NULL if none.
  std::unique_ptr<NativeBackend> backend_;
  // The location of the PasswordStoreDefault's database.
  const base::FilePath login_db_file_;
  // A second login database, which will hold encrypted values during migration.
  const base::FilePath encrypted_login_db_file_;
  // Whether we have already attempted migration to the native store.
  bool migration_checked_;
  // Whether we should allow falling back to the default store. If there is
  // nothing to migrate, then the first attempt to use the native store will
  // be the first time we try to use it and we should allow falling back. If
  // we have migrated successfully, then we do not allow falling back.
  bool allow_fallback_;
  // Tracks the last completed step in the migration from the native backends to
  // LoginDB.
  IntegerPrefMember migration_step_pref_;
  MigrationToLoginDBStep migration_to_login_db_step_ = NOT_ATTEMPTED;

  base::WeakPtrFactory<PasswordStoreX> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreX);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_X_H_

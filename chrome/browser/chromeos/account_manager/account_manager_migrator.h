// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_MIGRATOR_H_
#define CHROME_BROWSER_CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_MIGRATOR_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/account_manager/account_migration_runner.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace chromeos {

class AccountManagerMigrator : public KeyedService {
 public:
  explicit AccountManagerMigrator(Profile* profile);
  ~AccountManagerMigrator() override;

  // Starts migrating accounts to Chrome OS Account Manager.
  void Start();

  // Gets the current status of migration.
  AccountMigrationRunner::Status GetStatus() const;

 private:
  // Returns whether migrations should be run or skipped.
  bool ShouldRunMigrations() const;

  // Adds the necessary migration steps to |migration_runner_|.
  void AddMigrationSteps();

  void OnMigrationRunComplete(
      const AccountMigrationRunner::MigrationResult& result);

  // Runs tasks that must be completed regardless of the success / failure /
  // no-op of migrations.
  void RunCleanupTasks();

  // A non-owning pointer to |Profile|.
  Profile* const profile_;

  // Used for running migration steps.
  chromeos::AccountMigrationRunner migration_runner_;

  // Stores if any migration steps were actually run. It is possible for the
  // migration flow to be a no-op, in which case this will be |false|.
  bool ran_migration_steps_ = false;

  base::WeakPtrFactory<AccountManagerMigrator> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(AccountManagerMigrator);
};

class AccountManagerMigratorFactory : public BrowserContextKeyedServiceFactory {
 public:
  static AccountManagerMigrator* GetForBrowserContext(
      content::BrowserContext* context);
  static AccountManagerMigratorFactory* GetInstance();

 private:
  friend class base::NoDestructor<AccountManagerMigratorFactory>;

  AccountManagerMigratorFactory();
  ~AccountManagerMigratorFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(AccountManagerMigratorFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_MIGRATOR_H_
